/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/*
 * Ethernet L2 sink: frames are counted and freed at send(). Unlike the
 * loopback (dummy L2), this exercises the Ethernet L2 path and lets the
 * stack skip software checksums when offload is advertised.
 */

#include <zephyr/kernel.h>
#include <zephyr/net/ethernet.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_pkt.h>

struct null_eth_ctx {
	uint8_t mac[6];
	uint32_t tx_pkts;
};

static struct null_eth_ctx null_eth_data = {
	.mac = {0x02, 0x00, 0x5e, 0x10, 0x00, 0x01},
};

static enum ethernet_hw_caps null_eth_caps(const struct device *dev, struct net_if *iface)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(iface);

	if (IS_ENABLED(CONFIG_BENCH_NULL_ETH_CSUM_OFFLOAD)) {
		return ETHERNET_HW_TX_CHKSUM_OFFLOAD | ETHERNET_HW_RX_CHKSUM_OFFLOAD;
	}

	return 0;
}

static int null_eth_send(const struct device *dev, struct net_pkt *pkt)
{
	struct null_eth_ctx *ctx = dev->data;

	ARG_UNUSED(pkt);
	ctx->tx_pkts++;

	return 0;
}

static void null_eth_iface_init(struct net_if *iface)
{
	const struct device *dev = net_if_get_device(iface);
	struct null_eth_ctx *ctx = dev->data;

	net_if_set_link_addr(iface, ctx->mac, sizeof(ctx->mac), NET_LINK_ETHERNET);
	ethernet_init(iface);
}

static const struct ethernet_api null_eth_api = {
	.iface_api.init = null_eth_iface_init,
	.get_capabilities = null_eth_caps,
	.send = null_eth_send,
};

ETH_NET_DEVICE_INIT(null_eth, "null_eth", NULL, NULL, &null_eth_data, NULL,
		    CONFIG_ETH_INIT_PRIORITY, &null_eth_api, NET_ETH_MTU);
