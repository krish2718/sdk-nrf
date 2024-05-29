/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/net/net_event.h>
#include <supp_events.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <net/library.h>

LOG_MODULE_REGISTER(wifi_ready_events_handling, CONFIG_WIFI_READY_EVENT_HANDLING_LOG_LEVEL);

static app_callbacks_t callbacks;
static struct net_mgmt_event_callback net_wpa_supp_cb;

static void handle_wpa_supp_ready(struct net_mgmt_event_callback *cb)
{
	LOG_INF("Printing iface name: %s", "nordic_wlan0");
	callbacks.start_app();		
}

static void wpa_supp_event_handler(struct net_mgmt_event_callback *cb,
	uint32_t mgmt_event, struct net_if *iface)
{
	/* TODO: Handle other events */
	switch (mgmt_event) {
	case NET_EVENT_WPA_SUPP_READY:
		handle_wpa_supp_ready(cb);
		break;
	default:
		LOG_DBG("Unhandled event (%d)", mgmt_event);
		break;
	}
}

int register_events(app_callbacks_t cb)
{
	callbacks = cb;

	net_mgmt_init_event_callback(&net_wpa_supp_cb, wpa_supp_event_handler, WPA_SUPP_EVENTS);
	net_mgmt_add_event_callback(&net_wpa_supp_cb);

	return 0;
}
