/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 * @brief WiFi QT control app main file.
 */

#include <zephyr/sys/printk.h>
#include <zephyr/kernel.h>
#if defined(CLOCK_FEATURE_HFCLK_DIVIDE_PRESENT) || NRF_CLOCK_HAS_HFCLK192M
#include <nrfx_clock.h>
#endif
#include <zephyr/device.h>
#include <zephyr/net/net_config.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <zephyr/net/net_ip.h>

#include <ctrl_iface_zephyr.h>
#include <supp_main.h>

#include <utils.h>
#include <common/wpa_ctrl.h>

#ifdef CONFIG_USB_DEVICE_STACK
#include <zephyr/usb/usb_device.h>

int init_usb(void)
{
	int ret;

	ret = usb_enable(NULL);
	if (ret != 0) {
		printk("Cannot enable USB (%d)", ret);
		return ret;
	}

	return 0;
}
#endif /* CONFIG_USB_DEVICE_STACK */

static int run_qt_command(const char *cmd)
{
	char buffer[64] = {0}, response[16] = {0};
	struct wpa_ctrl *w = NULL;
	struct wpa_supplicant *wpa_s = NULL;
	size_t resp_len = sizeof(response);
	int status = 0;
	int retry_count = 0;
	int ret;

retry:
	wpa_s = z_wpas_get_handle_by_ifname("wlan0");
	if (!wpa_s && retry_count++ < 5) {
		indigo_logger(LOG_LEVEL_ERROR,
			      "%s: Unable to get wpa_s handle for %s\n",
			      __func__, "wlan0");
		goto retry;
	}

	if (!wpa_s) {
		goto done;
	}

	w = wpa_ctrl_open(wpa_s->ctrl_iface->sock_pair[0]);
	if (!w) {
		indigo_logger(LOG_LEVEL_ERROR, "Failed to connect to wpa_supplicant");
		goto done;
	}

	ret = snprintf(buffer, sizeof(buffer), "%s", cmd);
	if (ret < 0 || ret >= (int)sizeof(buffer)) {
		indigo_logger(LOG_LEVEL_ERROR, "Failed to execute command");
		goto done;
	}

	wpa_ctrl_request(w, buffer, sizeof(buffer), response, &resp_len, NULL);
	if (resp_len == 0) {
		indigo_logger(LOG_LEVEL_ERROR, "Failed to execute command");
		goto done;
	}

	indigo_logger(LOG_LEVEL_DEBUG, "Response: %s", response);

done:
	return status;
}


int qt_wpa3_assoc_tb(void)
{
	char buffer[512];

	BUILD_ASSERT_MSG(sizeof(CONFIG_WIFI_SSID) > 1, "Invalid SSID");
	BUILD_ASSERT_MSG(sizeof(CONFIG_WIFI_PSK) > 1, "Invalid PSK");

	run_qt_command("REMOVE_NETWORK all");
	run_qt_command("ADD_NETWORK");
	snprintf(buffer, sizeof(buffer), "SET_NETWORK 0 ssid \"%s\"", CONFIG_WIFI_SSID);
	run_qt_command(buffer);
	run_qt_command("SET_NETWORK 0 key_mgmt SAE");
	snprintf(buffer, sizeof(buffer), "SET_NETWORK 0 psk \"%s\"", CONFIG_WIFI_PSK);
	run_qt_command(buffer);
	run_qt_command("SET_NETWORK 0 proto RSN");
	run_qt_command("SET_NETWORK 0 pairwise CCMP");
	run_qt_command("SET_NETWORK 0 group CCMP");
	run_qt_command("SET_NETWORK 0 ieee80211w 2");
	run_qt_command("ENABLE_NETWORK 0");
	run_qt_command("SELECT_NETWORK 0");

	return 0;
}

int main(void)
{
	struct in_addr addr;

#if defined(CLOCK_FEATURE_HFCLK_DIVIDE_PRESENT) || NRF_CLOCK_HAS_HFCLK192M
	/* For now hardcode to 128MHz */
	nrfx_clock_divider_set(NRF_CLOCK_DOMAIN_HFCLK,
			       NRF_CLOCK_HFCLK_DIV_1);
#endif
	printk("Starting %s with CPU frequency: %d MHz\n", CONFIG_BOARD, SystemCoreClock/MHZ(1));

#ifdef CONFIG_USB_DEVICE_STACK
	init_usb();
	/* Redirect static IP address to netusb*/
	const struct device *usb_dev = device_get_binding("eth_netusb");
	struct net_if *iface = net_if_lookup_by_dev(usb_dev);

	if (!iface) {
		printk("Cannot find network interface: %s", "eth_netusb");
		return -1;
	}
	if (sizeof(CONFIG_NET_CONFIG_USB_IPV4_ADDR) > 1) {
		if (net_addr_pton(AF_INET, CONFIG_NET_CONFIG_USB_IPV4_ADDR, &addr)) {
			printk("Invalid address: %s", CONFIG_NET_CONFIG_USB_IPV4_ADDR);
			return -1;
		}
		net_if_ipv4_addr_add(iface, &addr, NET_ADDR_MANUAL, 0);
	}
	if (sizeof(CONFIG_NET_CONFIG_MY_IPV4_NETMASK) > 1) {
		/* If not empty */
		if (net_addr_pton(AF_INET, CONFIG_NET_CONFIG_MY_IPV4_NETMASK, &addr)) {
			printk("Invalid netmask: %s", CONFIG_NET_CONFIG_MY_IPV4_NETMASK);
		} else {
			net_if_ipv4_set_netmask(iface, &addr);
		}
	}
#endif /* CONFIG_USB_DEVICE_STACK */

#ifdef CONFIG_SLIP
	const struct device *slip_dev = device_get_binding(CONFIG_SLIP_DRV_NAME);
	struct net_if *slip_iface = net_if_lookup_by_dev(slip_dev);

	if (!slip_iface) {
		printk("Cannot find network interface: %s", CONFIG_SLIP_DRV_NAME);
		return -1;
	}

	if (sizeof(CONFIG_NET_CONFIG_SLIP_IPV4_ADDR) > 1) {
		if (net_addr_pton(AF_INET, CONFIG_NET_CONFIG_SLIP_IPV4_ADDR, &addr)) {
			printk("Invalid address: %s", CONFIG_NET_CONFIG_SLIP_IPV4_ADDR);
			return -1;
		}
		net_if_ipv4_addr_add(slip_iface, &addr, NET_ADDR_MANUAL, 0);
	}

	if (sizeof(CONFIG_NET_CONFIG_SLIP_IPV4_MASK) > 1) {
		/* If not empty */
		if (net_addr_pton(AF_INET, CONFIG_NET_CONFIG_SLIP_IPV4_MASK, &addr)) {
			printk("Invalid netmask: %s", CONFIG_NET_CONFIG_SLIP_IPV4_MASK);
		} else {
			net_if_ipv4_set_netmask(slip_iface, &addr);
		}
	}
#endif /* CONFIG_SLIP */

#ifdef CONFIG_NET_CONFIG_SETTINGS
	/* Without this, DHCPv4 starts on first interface and if that is not Wi-Fi or
	 * only supports IPv6, then its an issue. (E.g., OpenThread)
	 *
	 * So, we start DHCPv4 on Wi-Fi interface always, independent of the ordering.
	 */
	/* TODO: Replace device name with DTS settings later */
	const struct device *dev = device_get_binding("wlan0");
	struct net_if *wifi_iface = net_if_lookup_by_dev(dev);

	if (!wifi_iface) {
		printk("Cannot find network interface: %s", "wlan0");
		return -1;
	}
	if (sizeof(CONFIG_NET_CONFIG_MY_IPV4_ADDR) > 1) {
		if (net_addr_pton(AF_INET, CONFIG_NET_CONFIG_MY_IPV4_ADDR, &addr)) {
			printk("Invalid address: %s", CONFIG_NET_CONFIG_MY_IPV4_ADDR);
			return -1;
		}
		net_if_ipv4_addr_add(wifi_iface, &addr, NET_ADDR_MANUAL, 0);
	}
	if (sizeof(CONFIG_NET_CONFIG_MY_IPV4_NETMASK) > 1) {
		/* If not empty */
		if (net_addr_pton(AF_INET, CONFIG_NET_CONFIG_MY_IPV4_NETMASK, &addr)) {
			printk("Invalid netmask: %s", CONFIG_NET_CONFIG_MY_IPV4_NETMASK);
		} else {
			net_if_ipv4_set_netmask(wifi_iface, &addr);
		}
	}

	/* As both are Ethernet, we need to set specific interface*/
	net_if_set_default(wifi_iface);

	net_config_init_app(dev, "Initializing network");
#endif

	k_sleep(K_MSEC(1000));

	qt_wpa3_assoc_tb();

	return 0;
}


#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/init.h>

#include "wpa_cli_zephyr.h"

static int cmd_wpa_cli(const struct shell *sh,
			  size_t argc,
			  const char *argv[])
{
	malloc_stats();
}

/* Persisting with "wpa_cli" naming for compatibility with Wi-Fi
 * certification applications and scripts.
 */
SHELL_CMD_REGISTER(ms,
		   NULL,
		   "ms commands (only for internal use)",
		   cmd_wpa_cli);
