/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/net/conn_mgr_connectivity.h>
#include <zephyr/net/conn_mgr_monitor.h>
#include <zephyr/net/wifi_mgmt.h>

#include "message_channel.h"

/* Register log module */
LOG_MODULE_REGISTER(network, CONFIG_MQTT_SAMPLE_NETWORK_LOG_LEVEL);

/* This module does not subscribe to any channels */

/* Macros used to subscribe to specific Zephyr NET management events. */
#define L4_EVENT_MASK (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)
#define CONN_LAYER_EVENT_MASK (NET_EVENT_CONN_IF_FATAL_ERROR)

/* Zephyr NET management event callback structures. */
static struct net_mgmt_event_callback l4_cb;
static struct net_mgmt_event_callback conn_cb;

static void l4_event_handler(struct net_mgmt_event_callback *cb,
			     uint32_t event,
			     struct net_if *iface)
{
	int err;
	enum network_status status;

	switch (event) {
	case NET_EVENT_L4_CONNECTED:
		LOG_INF("Network connectivity established");
		status = NETWORK_CONNECTED;
		break;
	case NET_EVENT_L4_DISCONNECTED:
		LOG_INF("Network connectivity lost");
		status = NETWORK_DISCONNECTED;
		break;
	default:
		/* Don't care */
		return;
	}

	err = zbus_chan_pub(&NETWORK_CHAN, &status, K_SECONDS(1));
	if (err) {
		LOG_ERR("zbus_chan_pub, error: %d", err);
		SEND_FATAL_ERROR();
	}
}

static void connectivity_event_handler(struct net_mgmt_event_callback *cb,
				       uint32_t event,
				       struct net_if *iface)
{
	if (event == NET_EVENT_CONN_IF_FATAL_ERROR) {
		LOG_ERR("NET_EVENT_CONN_IF_FATAL_ERROR");
		SEND_FATAL_ERROR();
		return;
	}
}

static int lock_core_wifi_config_power_save(void)
{
	struct net_if *iface = net_if_get_default();
	struct wifi_ps_params params = { 0 };

	params.type = WIFI_PS_PARAM_LISTEN_INTERVAL;
	params.listen_interval = 10;

	if (net_mgmt(NET_REQUEST_WIFI_PS, iface, &params, sizeof(params))) {
		if (params.fail_reason == WIFI_PS_PARAM_LISTEN_INTERVAL_RANGE_INVALID) {
			LOG_ERR("Setting listen interval failed. Reason %s",
				wifi_ps_get_config_err_code_str(params.fail_reason));
			LOG_ERR("Hardware support valid range : 3 - 65535");
		} else {
			LOG_ERR("Setting listen interval failed. Reason %s",
				wifi_ps_get_config_err_code_str(params.fail_reason));
		}
		return -ENOEXEC;
	}
	LOG_INF("Listen interval %hu", params.listen_interval);

	params.type = WIFI_PS_PARAM_WAKEUP_MODE;
	if (params.listen_interval == 0) {
		params.wakeup_mode = WIFI_PS_WAKEUP_MODE_DTIM;
	} else {
		params.wakeup_mode = WIFI_PS_WAKEUP_MODE_LISTEN_INTERVAL;
	}

	if (net_mgmt(NET_REQUEST_WIFI_PS, iface, &params, sizeof(params))) {
		LOG_ERR("Setting PS wake up mode to %s failed..Reason %s",
			params.wakeup_mode ? "Listen interval" : "DTIM interval",
			wifi_ps_get_config_err_code_str(params.fail_reason));
		return -ENOEXEC;
	}

	LOG_INF("Wifi power save mode set to %s",
		params.wakeup_mode ? "Listen interval" : "DTIM interval");

	LOG_INF("Wifi power save configs setup success");

	return 0;
};

static void network_task(void)
{
	int err;

	/* Setup handler for Zephyr NET Connection Manager events. */
	net_mgmt_init_event_callback(&l4_cb, l4_event_handler, L4_EVENT_MASK);
	net_mgmt_add_event_callback(&l4_cb);

	/* Setup handler for Zephyr NET Connection Manager Connectivity layer. */
	net_mgmt_init_event_callback(&conn_cb, connectivity_event_handler, CONN_LAYER_EVENT_MASK);
	net_mgmt_add_event_callback(&conn_cb);

	/* Wait for Wi-Fi to be up */
	k_sleep(K_SECONDS(2));

	lock_core_wifi_config_power_save();

	/* Connecting to the configured connectivity layer.
	 * Wi-Fi or LTE depending on the board that the sample was built for.
	 */
	LOG_INF("Bringing network interface up and connecting to the network");

	err = conn_mgr_all_if_up(true);
	if (err) {
		LOG_ERR("conn_mgr_all_if_up, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}

	/* Resend connection status if the sample is built for Native Posix.
	 * This is necessary because the network interface is automatically brought up
	 * at SYS_INIT() before main() is called.
	 * This means that NET_EVENT_L4_CONNECTED fires before the
	 * appropriate handler l4_event_handler() is registered.
	 */
	if (IS_ENABLED(CONFIG_BOARD_NATIVE_POSIX)) {
		conn_mgr_mon_resend_status();
	}
}

K_THREAD_DEFINE(network_task_id,
		CONFIG_MQTT_SAMPLE_NETWORK_THREAD_STACK_SIZE,
		network_task, NULL, NULL, NULL, 3, 0, 0);
