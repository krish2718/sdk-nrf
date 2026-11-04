/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/*
 * Raw Ethernet frame TX into the null driver, bypassing IP/UDP/TCP. Prices
 * what a network processor forwarding host frames would pay per frame:
 *
 *   sock  AF_PACKET SOCK_RAW sendto(): socket layer + one copy into net_buf
 *   copy  net_pkt built here, frame copied in, net_if_send_data(): no socket
 *   zc    frame wrapped in an external-data net_buf, no socket and no copy
 */

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/net/ethernet.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_pkt.h>
#include <zephyr/net/socket.h>
#include <zephyr/net_buf.h>

#include <stdlib.h>
#include <string.h>

#define RAW_ETHERTYPE 0x88B5 /* IEEE 802 local experimental */
#define RAW_MAX_FRAME 1514

uint32_t bench_cpu_hz(void);

/* Descriptors only: data stays in the caller's buffer (NET_BUF_EXTERNAL_DATA). */
NET_BUF_POOL_HEAP_DEFINE(raw_ext_pool, 8, 0, NULL);

static uint8_t frame[RAW_MAX_FRAME] __aligned(4);
static const uint8_t peer_mac[NET_ETH_ADDR_LEN] = {0x02, 0x00, 0x5e, 0x10, 0x00, 0x02};

enum raw_mode {
	RAW_SOCK,
	RAW_COPY,
	RAW_ZC,
};

static void frame_init(struct net_if *iface, size_t len)
{
	struct net_eth_hdr *hdr = (struct net_eth_hdr *)frame;

	memcpy(hdr->dst.addr, peer_mac, sizeof(hdr->dst.addr));
	memcpy(hdr->src.addr, net_if_get_link_addr(iface)->addr, sizeof(hdr->src.addr));
	hdr->type = net_htons(RAW_ETHERTYPE);

	for (size_t i = sizeof(*hdr); i < len; i++) {
		frame[i] = (uint8_t)i;
	}
}

static int send_direct(struct net_if *iface, size_t len, bool zero_copy)
{
	struct net_pkt *pkt;
	struct net_buf *buf;

	if (zero_copy) {
		buf = net_buf_alloc_with_data(&raw_ext_pool, frame, len, K_NO_WAIT);
		if (!buf) {
			return -ENOBUFS;
		}

		pkt = net_pkt_alloc_on_iface(iface, K_NO_WAIT);
		if (!pkt) {
			net_buf_unref(buf);
			return -ENOMEM;
		}

		net_pkt_append_buffer(pkt, buf);
		net_pkt_set_family(pkt, NET_AF_PACKET);
	} else {
		pkt = net_pkt_alloc_with_buffer(iface, len, NET_AF_PACKET, 0, K_NO_WAIT);
		if (!pkt) {
			return -ENOMEM;
		}

		if (net_pkt_write(pkt, frame, len) < 0) {
			net_pkt_unref(pkt);
			return -ENOBUFS;
		}
	}

	net_pkt_cursor_init(pkt);

	if (net_if_send_data(iface, pkt) == NET_DROP) {
		net_pkt_unref(pkt);
		return -EIO;
	}

	return 0;
}

static int cmd_raw(const struct shell *sh, size_t argc, char **argv)
{
	struct net_if *iface = net_if_get_first_by_type(&NET_L2_GET_NAME(ETHERNET));
	struct net_sockaddr_ll ll = {0};
	size_t len = strtoul(argv[2], NULL, 10);
	uint32_t secs = strtoul(argv[3], NULL, 10);
	uint64_t frames = 0, errors = 0;
	uint64_t t0, dt, end;
	enum raw_mode mode;
	uint32_t hz;
	int fd = -1;

	if (!strcmp(argv[1], "sock")) {
		mode = RAW_SOCK;
	} else if (!strcmp(argv[1], "copy")) {
		mode = RAW_COPY;
	} else if (!strcmp(argv[1], "zc")) {
		mode = RAW_ZC;
	} else {
		shell_error(sh, "mode must be sock, copy or zc");
		return -EINVAL;
	}

	if (!iface || len < sizeof(struct net_eth_hdr) || len > RAW_MAX_FRAME || secs == 0) {
		return -EINVAL;
	}

	frame_init(iface, len);

	if (mode == RAW_SOCK) {
		/* SOCK_RAW only accepts 0, ETH_P_ALL and a few fixed types; 0 is TX only. */
		fd = zsock_socket(NET_AF_PACKET, NET_SOCK_RAW, 0);
		if (fd < 0) {
			shell_error(sh, "socket: %d", errno);
			return -errno;
		}

		ll.sll_family = NET_AF_PACKET;
		ll.sll_protocol = net_htons(RAW_ETHERTYPE);
		ll.sll_ifindex = net_if_get_by_iface(iface);
		ll.sll_halen = NET_ETH_ADDR_LEN;
		memcpy(ll.sll_addr, peer_mac, NET_ETH_ADDR_LEN);

		if (zsock_bind(fd, (struct net_sockaddr *)&ll, sizeof(ll)) < 0) {
			shell_error(sh, "bind: %d", errno);
			zsock_close(fd);
			return -errno;
		}
	}

	hz = bench_cpu_hz();
	t0 = k_cycle_get_64();
	end = t0 + (uint64_t)secs * sys_clock_hw_cycles_per_sec();

	do {
		/* Check the clock every 32 frames to keep it out of the per-frame cost. */
		for (int i = 0; i < 32; i++) {
			int ret;

			if (mode == RAW_SOCK) {
				ret = zsock_sendto(fd, frame, len, 0, (struct net_sockaddr *)&ll,
						   sizeof(ll));
				ret = ret == (int)len ? 0 : -EIO;
			} else {
				ret = send_direct(iface, len, mode == RAW_ZC);
			}

			if (ret) {
				errors++;
			} else {
				frames++;
			}
		}
	} while (k_cycle_get_64() < end);

	dt = k_cycle_get_64() - t0;

	if (fd >= 0) {
		zsock_close(fd);
	}

	/* Fixed-point x100 for Mbps to keep float printing out of the shell. */
	shell_print(sh, "raw mode=%s len=%zu cpu_hz=%u frames=%llu errors=%llu "
		    "cycles_per_frame=%llu centi_mbps=%llu",
		    argv[1], len, hz, frames, errors,
		    frames ? (uint64_t)hz * dt / sys_clock_hw_cycles_per_sec() / frames : 0,
		    frames * len * 8ULL * 100ULL * sys_clock_hw_cycles_per_sec() / dt / 1000000ULL);

	return errors && !frames ? -EIO : 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(raw_cmds,
	SHELL_CMD_ARG(tx, NULL, "Raw frame TX: raw tx <sock|copy|zc> <frame_len> <secs>",
		      cmd_raw, 4, 0),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(raw, &raw_cmds, "Raw Ethernet frame TX benchmark", NULL);
