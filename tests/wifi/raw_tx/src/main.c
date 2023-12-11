/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(wifi_raw_tx, CONFIG_LOG_DEFAULT_LEVEL);

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/types.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>

#include <zephyr/sys/printk.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/socket.h>

#include <nrf_wifi/fw_if/umac_if/inc/default/fmac_structs.h>

static const char test_vector[] = {
#include "../test_vectors/udp_ipv6_zerocsum.vec"
};

static void wifi_set_mode(void)
{
	int ret;
	struct net_if *iface = NULL;
	struct wifi_mode_info mode_info = {0};

	mode_info.oper = WIFI_MGMT_SET;

	iface = net_if_get_first_wifi();
	if (iface == NULL) {
		LOG_ERR("Failed to get Wi-Fi iface");
		return;
	}
	mode_info.if_index = net_if_get_by_iface(iface);

	mode_info.mode =  WIFI_STA_MODE | WIFI_TX_INJECTION_MODE;

	ret = net_mgmt(NET_REQUEST_WIFI_MODE, iface, &mode_info, sizeof(mode_info));
	if (ret) {
		LOG_ERR("Mode setting failed %d", ret);
	}
}

static int setup_raw_pkt_socket(int *sockfd, struct sockaddr_ll *sa)
{
	struct net_if *iface = NULL;
	int ret;

	*sockfd = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW);
	if (*sockfd < 0) {
		LOG_ERR("Unable to create a socket %d", errno);
		return -1;
	}

	iface = net_if_get_first_wifi();
	if (!iface) {
		LOG_ERR("Failed to get Wi-Fi interface");
		return -1;
	}

	sa->sll_family = AF_PACKET;
	sa->sll_ifindex = net_if_get_by_iface(iface);

	/* Bind the socket */
	ret = bind(*sockfd, (struct sockaddr *)sa, sizeof(struct sockaddr_ll));
	if (ret < 0) {
		LOG_ERR("Error: Unable to bind socket to the network interface:%d", errno);
		close(*sockfd);
		return -1;
	}

	return 0;
}

static void fill_raw_tx_pkt_hdr(struct raw_tx_pkt_header *raw_tx_pkt)
{
	/* Raw Tx Packet header */
	raw_tx_pkt->magic_num = NRF_WIFI_MAGIC_NUM_RAWTX;
	raw_tx_pkt->data_rate = 0;
	raw_tx_pkt->packet_length = sizeof(test_vector) / 2;
	raw_tx_pkt->tx_mode = 3;
	raw_tx_pkt->queue = 0;
	/* The byte is reserved and used by the driver */
	raw_tx_pkt->raw_tx_flag = 0;
}

int wifi_send_raw_tx_pkt(int sockfd, char *test_frame,
			size_t buf_length, struct sockaddr_ll *sa)
{
	return sendto(sockfd, test_frame, buf_length, 0,
		(struct sockaddr *)sa, sizeof(*sa));
}

ZTEST(wifi_raw_tx_tests, raw_tx_unconnected)
{
	int ret;
	int sockfd;
	struct sockaddr_ll sa;
	struct raw_tx_pkt_header raw_tx_pkt;
	char *test_frame = NULL;

	wifi_set_mode();

	ret = setup_raw_pkt_socket(&sockfd, &sa);
	zassert_equal(ret, 0, "Socket setup failed");

	fill_raw_tx_pkt_hdr(&raw_tx_pkt);

	test_frame = malloc(sizeof(struct raw_tx_pkt_header) + sizeof(test_vector) / 2);
	zassert_not_null(test_frame, "Failed to allocate memory for test frame");

	memcpy(test_frame, &raw_tx_pkt, sizeof(struct raw_tx_pkt_header));
	net_bytes_from_str(test_frame + sizeof(struct raw_tx_pkt_header),
		sizeof(test_vector) / 2, test_vector);

	ret = wifi_send_raw_tx_pkt(sockfd, (char *)&raw_tx_pkt,
			sizeof(raw_tx_pkt), &sa);
	zassert_equal(ret, 0, "Raw Tx failed");

	close(sockfd);
}


ZTEST_SUITE(wifi_raw_tx_tests, NULL, NULL, NULL, NULL, NULL);
