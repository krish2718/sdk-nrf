/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 * @brief Wi-Fi Raw Tx Packet sample
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(raw_tx_packet, CONFIG_LOG_DEFAULT_LEVEL);

#include <nrfx_clock.h>
#include <zephyr/kernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/printk.h>
#include <zephyr/init.h>

#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/socket.h>
#include <zephyr/drivers/gpio.h>

#include <fmac_structs.h>

#include "net_private.h"

#define WIFI_SHELL_MODULE "wifi"

#define WIFI_SHELL_MGMT_EVENTS (NET_EVENT_WIFI_CONNECT_RESULT |		\
				NET_EVENT_WIFI_DISCONNECT_RESULT)

#define MAX_SSID_LEN        32
#define DHCP_TIMEOUT        70
#define CONNECTION_TIMEOUT  100
#define STATUS_POLLING_MS   300

#define CONTINUOUS_MODE_TRANSMISSION 0
#define FIXED_MODE_TRANSMISSION 1

static struct net_mgmt_event_callback wifi_shell_mgmt_cb;
static struct net_mgmt_event_callback net_shell_mgmt_cb;

static struct {
	const struct shell *sh;
	union {
		struct {
			uint8_t connected	: 1;
			uint8_t connect_result	: 1;
			uint8_t disconnect_requested	: 1;
			uint8_t _unused		: 5;
		};
		uint8_t all;
	};
} context;

struct beacon {
	uint16_t frame_control;
	uint16_t duration;
	uint8_t da[6];
	uint8_t sa[6];
	uint8_t bssid[6];
	uint16_t seq_ctrl;
	uint8_t payload[256];
};

static struct beacon test_beacon_frame = {
	.frame_control = htons(0X8000),
	.duration = 0X0000,
	.da = {0Xff, 0Xff, 0Xff, 0Xff, 0Xff, 0Xff},
	/* Transmitter Address: a0:69:60:e3:52:15 */
	.sa = {0Xa0, 0X69, 0X60, 0Xe3, 0X52, 0X15},
	.bssid = {0Xa0, 0X69, 0X60, 0Xe3, 0X52, 0X15},
	.seq_ctrl = 0X0010,
	/* SSID: NRF_RAW_TX_PACKET_APP */
	.payload = {
		0X0c, 0Xa2, 0X28, 0X00, 0X00, 0X00, 0X00, 0X00, 0X64, 0X00,
		0X11, 0X04, 0X00, 0X15, 0X4E, 0X52, 0X46, 0X5f, 0X52, 0X41,
		0X57, 0X5f, 0X54, 0X58, 0X5f, 0X50, 0X41, 0X43, 0X4b, 0X45,
		0X54, 0X5f, 0X41, 0X50, 0X50, 0X01, 0X08, 0X82, 0X84, 0X8b,
		0X96, 0X0c, 0X12, 0X18, 0X24, 0X03, 0X01, 0X06, 0X05, 0X04,
		0X00, 0X02, 0X00, 0X00, 0X2a, 0X01, 0X04, 0X32, 0X04, 0X30,
		0X48, 0X60, 0X6c, 0X30, 0X14, 0X01, 0X00, 0X00, 0X0f, 0Xac,
		0X04, 0X01, 0X00, 0X00, 0X0f, 0Xac, 0X04, 0X01, 0X00, 0X00,
		0X0f, 0Xac, 0X02, 0X0c, 0X00, 0X3b, 0X02, 0X51, 0X00, 0X2d,
		0X1a, 0X0c, 0X00, 0X17, 0Xff, 0Xff, 0X00, 0X00, 0X00, 0X00,
		0X00, 0X00, 0X00, 0X2c, 0X01, 0X01, 0X00, 0X00, 0X00, 0X00,
		0X00, 0X00, 0X00, 0X00, 0X00, 0X3d, 0X16, 0X06, 0X00, 0X00,
		0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
		0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X7f, 0X08, 0X04, 0X00,
		0X00, 0X02, 0X00, 0X00, 0X00, 0X40, 0Xff, 0X1a, 0X23, 0X01,
		0X78, 0X10, 0X1a, 0X00, 0X00, 0X00, 0X20, 0X0e, 0X09, 0X00,
		0X09, 0X80, 0X04, 0X01, 0Xc4, 0X00, 0Xfa, 0Xff, 0Xfa, 0Xff,
		0X61, 0X1c, 0Xc7, 0X71, 0Xff, 0X07, 0X24, 0Xf0, 0X3f, 0X00,
		0X81, 0Xfc, 0Xff, 0Xdd, 0X18, 0X00, 0X50, 0Xf2, 0X02, 0X01,
		0X01, 0X01, 0X00, 0X03, 0Xa4, 0X00, 0X00, 0X27, 0Xa4, 0X00,
		0X00, 0X42, 0X43, 0X5e, 0X00, 0X62, 0X32, 0X2f, 0X00
	}
};

static int cmd_wifi_status(void)
{
	struct net_if *iface = NULL;
	struct wifi_iface_status status = { 0 };
	int ret;

	iface = net_if_get_first_wifi();
	if (!iface) {
		LOG_ERR("Failed to get Wi-Fi iface");
		return -1;
	}

	ret = net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, iface, &status,
				sizeof(struct wifi_iface_status));
	if (ret) {
		LOG_INF("Status request failed: %d\n", ret);
		return ret;
	}

	LOG_INF("==================");
	LOG_INF("State: %s", wifi_state_txt(status.state));

	if (status.state >= WIFI_STATE_ASSOCIATED) {
		uint8_t mac_string_buf[sizeof("xx:xx:xx:xx:xx:xx")];

		LOG_INF("Interface Mode: %s",
		       wifi_mode_txt(status.iface_mode));
		LOG_INF("Link Mode: %s",
		       wifi_link_mode_txt(status.link_mode));
		LOG_INF("SSID: %-32s", status.ssid);
		LOG_INF("BSSID: %s",
		       net_sprint_ll_addr_buf(
				status.bssid, WIFI_MAC_ADDR_LEN,
				mac_string_buf, sizeof(mac_string_buf)));
		LOG_INF("Band: %s", wifi_band_txt(status.band));
		LOG_INF("Channel: %d", status.channel);
		LOG_INF("Security: %s", wifi_security_txt(status.security));
		LOG_INF("MFP: %s", wifi_mfp_txt(status.mfp));
		LOG_INF("RSSI: %d", status.rssi);
		LOG_INF("TWT: %s", status.twt_capable ? "Supported" : "Not supported");
	}
	return 0;
}

static void handle_wifi_connect_result(struct net_mgmt_event_callback *cb)
{
	const struct wifi_status *status =
		(const struct wifi_status *) cb->info;

	if (context.connected) {
		return;
	}

	if (status->status) {
		LOG_ERR("Connection failed (%d)", status->status);
	} else {
		LOG_INF("Connected");
		context.connected = true;
	}

	context.connect_result = true;
}

static void handle_wifi_disconnect_result(struct net_mgmt_event_callback *cb)
{
	const struct wifi_status *status =
		(const struct wifi_status *) cb->info;

	if (!context.connected) {
		return;
	}

	if (context.disconnect_requested) {
		LOG_INF("Disconnection request %s (%d)",
			 status->status ? "failed" : "done",
					status->status);
		context.disconnect_requested = false;
	} else {
		LOG_INF("Received Disconnected");
		context.connected = false;
	}

	cmd_wifi_status();
}

static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
				     uint32_t mgmt_event, struct net_if *iface)
{
	switch (mgmt_event) {
	case NET_EVENT_WIFI_CONNECT_RESULT:
		handle_wifi_connect_result(cb);
		break;
	case NET_EVENT_WIFI_DISCONNECT_RESULT:
		handle_wifi_disconnect_result(cb);
		break;
	default:
		break;
	}
}

static void print_dhcp_ip(struct net_mgmt_event_callback *cb)
{
	/* Get DHCP info from struct net_if_dhcpv4 and print */
	const struct net_if_dhcpv4 *dhcpv4 = cb->info;
	const struct in_addr *addr = &dhcpv4->requested_ip;
	char dhcp_info[128];

	net_addr_ntop(AF_INET, addr, dhcp_info, sizeof(dhcp_info));

	LOG_INF("DHCP IP address: %s", dhcp_info);
}

static void net_mgmt_event_handler(struct net_mgmt_event_callback *cb,
				    uint32_t mgmt_event, struct net_if *iface)
{
	switch (mgmt_event) {
	case NET_EVENT_IPV4_DHCP_BOUND:
		print_dhcp_ip(cb);
		break;
	default:
		break;
	}
}

#ifdef CONFIG_RAW_TX_PACKET_SAMPLE_CONNECTION_MODE
static int __wifi_args_to_params(struct wifi_connect_req_params *params)
{
	params->timeout = SYS_FOREVER_MS;

	/* SSID */
	params->ssid = CONFIG_RAW_TX_PACKET_SAMPLE_SSID;
	params->ssid_length = strlen(params->ssid);

#if defined(CONFIG_RAW_TX_PACKET_SAMPLE_KEY_MGMT_WPA2)
	params->security = 1;
#elif defined(CONFIG_RAW_TX_PACKET_SAMPLE_KEY_MGMT_WPA2_256)
	params->security = 2;
#elif defined(CONFIG_RAW_TX_PACKET_SAMPLE_KEY_MGMT_WPA3)
	params->security = 3;
#else
	params->security = 0;
#endif

#if !defined(CONFIG_RAW_TX_PACKET_SAMPLE_KEY_MGMT_NONE)
	params->psk = CONFIG_RAW_TX_PACKET_SAMPLE_PASSWORD;
	params->psk_length = strlen(params->psk);
#endif
	params->channel = WIFI_CHANNEL_ANY;

	/* MFP (optional) */
	params->mfp = WIFI_MFP_OPTIONAL;

	return 0;
}

static int wifi_connect(void)
{
	struct net_if *iface = NULL;
	static struct wifi_connect_req_params cnx_params;

	iface = net_if_get_first_wifi();
	if (!iface) {
		LOG_ERR("Failed to get Wi-Fi iface");
		return -1;
	}

	context.connected = false;
	context.connect_result = false;
	__wifi_args_to_params(&cnx_params);

	if (net_mgmt(NET_REQUEST_WIFI_CONNECT, iface,
		     &cnx_params, sizeof(struct wifi_connect_req_params))) {
		LOG_ERR("Connection request failed");

		return -ENOEXEC;
	}

	LOG_INF("Connection requested");

	return 0;
}

static int try_wifi_connect(void)
{
	while (1) {
		wifi_connect();

		while (!context.connect_result) {
			cmd_wifi_status();
			k_sleep(K_MSEC(STATUS_POLLING_MS));
		}

		if (context.connected) {
			break;
		} else if (!context.connect_result) {
			LOG_ERR("Connection Timed Out");
			return -1;
		}
	}

	return 0;
}
#endif

#ifdef CONFIG_RAW_TX_PACKET_SAMPLE_IDLE_MODE
static void wifi_set_channel(void)
{
	struct net_if *iface;
	struct wifi_channel_info channel_info = {0};
	int ret;

	channel_info.oper = WIFI_MGMT_SET;

	iface = net_if_get_first_wifi();
	if (iface == NULL) {
		LOG_ERR("Failed to get Wi-Fi iface");
		return;
	}
	channel_info.if_index = net_if_get_by_iface(iface);

	if (channel_info.oper == WIFI_MGMT_SET) {
		channel_info.channel = CONFIG_RAW_TX_PACKET_SAMPLE_CHANNEL;
		if ((channel_info.channel < WIFI_CHANNEL_MIN) ||
			   (channel_info.channel > WIFI_CHANNEL_MAX)) {
			LOG_ERR("Invalid channel number. Range is (1-233)");
			return;
		}
	}

	ret = net_mgmt(NET_REQUEST_WIFI_CHANNEL, iface,
			&channel_info, sizeof(channel_info));
	if (ret) {
		LOG_ERR(" Channel setting failed %d\n", ret);
			return;
	}

	LOG_INF("Wi-Fi channel set to %d", channel_info.channel);
}
#endif

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
	raw_tx_pkt->data_rate = CONFIG_RAW_TX_PACKET_SAMPLE_RATE_VALUE;
	raw_tx_pkt->packet_length = sizeof(test_beacon_frame);
	raw_tx_pkt->tx_mode = CONFIG_RAW_TX_PACKET_SAMPLE_RATE_FLAGS;
	raw_tx_pkt->queue = CONFIG_RAW_TX_PACKET_SAMPLE_QUEUE_NUM;
	/* The byte is reserved and used by the driver */
	raw_tx_pkt->raw_tx_flag = 0;
}

int wifi_send_raw_tx_pkt(int sockfd, char *test_frame,
			size_t buf_length, struct sockaddr_ll *sa)
{
	int ret = sendto(sockfd, test_frame, buf_length, 0,
			(struct sockaddr *)sa, sizeof(*sa));
	return ret;
}

static int get_pkt_transmit_count(int *num_tx_pkts)
{
#if defined(CONFIG_RAW_TX_PACKET_SAMPLE_FIXED_NUM_PACKETS)
	*num_tx_pkts = CONFIG_RAW_TX_PACKET_SAMPLE_FIXED_NUM_PACKETS;
	if (*num_tx_pkts == 0) {
		LOG_ERR("Can't send %d number of raw tx packets", *num_tx_pkts);
		return -1;
	}
	LOG_INF("Sending %d number of raw tx packets", *num_tx_pkts);
#else
	*num_tx_pkts = -1;
	LOG_INF("Sending raw tx packets continuously");
#endif
	return 0;
}

static void increment_seq_control(void)
{
	uint16_t mask = 0x000F;

	test_beacon_frame.seq_ctrl = (((test_beacon_frame.seq_ctrl >> 4) + 1) << 4
					| (test_beacon_frame.seq_ctrl & mask));
	if (test_beacon_frame.seq_ctrl > 0XFFF0) {
		test_beacon_frame.seq_ctrl = 0X0010;
	}
}

static void wifi_send_raw_tx_packets(void)
{
	struct sockaddr_ll sa;
	int sockfd, ret;
	struct raw_tx_pkt_header packet;
	char *test_frame = NULL;
	int buf_length, num_pkts, transmission_mode;

	ret = setup_raw_pkt_socket(&sockfd, &sa);
	if (ret < 0) {
		LOG_ERR("Setting socket for raw pkt transmission failed %d", errno);
		return;
	}

	fill_raw_tx_pkt_hdr(&packet);

	ret = get_pkt_transmit_count(&num_pkts);
	if (ret < 0) {
		close(sockfd);
		return;
	}

	test_frame = malloc(sizeof(struct raw_tx_pkt_header) + sizeof(test_beacon_frame));
	if (!test_frame) {
		LOG_ERR("Malloc failed for send buffer %d", errno);
		return;
	}

	buf_length = sizeof(struct raw_tx_pkt_header) + sizeof(test_beacon_frame);
	memcpy(test_frame, &packet, sizeof(struct raw_tx_pkt_header));

	if (num_pkts == 1) {
		memcpy(test_frame + sizeof(struct raw_tx_pkt_header),
				&test_beacon_frame, sizeof(test_beacon_frame));

		ret = wifi_send_raw_tx_pkt(sockfd, test_frame, buf_length, &sa);
		if (ret < 0) {
			LOG_ERR("Unable to send beacon frame: %s", strerror(errno));
			close(sockfd);
			free(test_frame);
			return;
		}
	} else {
		transmission_mode = CONFIG_RAW_TX_PACKET_SAMPLE_PACKET_TRANSMISSION_MODE;
		while (1) {
			memcpy(test_frame + sizeof(struct raw_tx_pkt_header),
					&test_beacon_frame, sizeof(test_beacon_frame));

			if (transmission_mode == FIXED_MODE_TRANSMISSION && num_pkts == 0) {
				break;
			}

			ret = sendto(sockfd, test_frame, buf_length, 0,
					(struct sockaddr *)&sa, sizeof(sa));
			if (ret < 0) {
				LOG_ERR("Unable to send beacon frame: %s", strerror(errno));
				close(sockfd);
				free(test_frame);
				return;
			}

			if (transmission_mode == FIXED_MODE_TRANSMISSION && num_pkts > 0) {
				num_pkts--;
			}

			increment_seq_control();

			k_msleep(CONFIG_RAW_TX_PACKET_SAMPLE_INTER_FRAME_DELAY_MS);
		}
	}

	/* close the socket */
	close(sockfd);
	free(test_frame);
}

int main(void)
{
	memset(&context, 0, sizeof(context));

	net_mgmt_init_event_callback(&wifi_shell_mgmt_cb,
				     wifi_mgmt_event_handler,
				     WIFI_SHELL_MGMT_EVENTS);

	net_mgmt_add_event_callback(&wifi_shell_mgmt_cb);

	net_mgmt_init_event_callback(&net_shell_mgmt_cb,
				     net_mgmt_event_handler,
				     NET_EVENT_IPV4_DHCP_BOUND);

	net_mgmt_add_event_callback(&net_shell_mgmt_cb);

	LOG_INF("Starting %s with CPU frequency: %d MHz", CONFIG_BOARD, SystemCoreClock/MHZ(1));
	k_sleep(K_SECONDS(1));

	LOG_INF("Static IP address (overridable): %s/%s -> %s",
		CONFIG_NET_CONFIG_MY_IPV4_ADDR,
		CONFIG_NET_CONFIG_MY_IPV4_NETMASK,
		CONFIG_NET_CONFIG_MY_IPV4_GW);

	wifi_set_mode();

#ifdef CONFIG_RAW_TX_PACKET_SAMPLE_CONNECTION_MODE
	int status;

	status = try_wifi_connect();
	if (status < 0) {
		return status;
	}
#else
	wifi_set_channel();
#endif
	wifi_send_raw_tx_packets();

	return 0;
}
