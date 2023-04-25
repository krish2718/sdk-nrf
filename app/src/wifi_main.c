/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 * @brief WiFi shell sample main function
 */

#include <zephyr/sys/printk.h>
#include <nrfx_clock.h>
#include <zephyr/device.h>
#include <zephyr/net/net_config.h>

#include <zephyr/usb/usb_device.h>
#include <zephyr/net/net_config.h>

#include "dut/wfa_dut.h"


#ifdef CONFIG_USB_DEVICE_STACK
#define CONFIG_NET_CONFIG_USB_IPV4_ADDR "192.168.251.150"
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
#endif

void main(void)
{
#ifdef CONFIG_USB_DEVICE_STACK
	struct in_addr addr;
#endif

#ifdef CLOCK_FEATURE_HFCLK_DIVIDE_PRESENT
	/* For now hardcode to 128MHz */
	nrfx_clock_divider_set(NRF_CLOCK_DOMAIN_HFCLK, NRF_CLOCK_HFCLK_DIV_1);
#endif
	printk("Starting %s with CPU frequency: %d MHz\n", CONFIG_BOARD, SystemCoreClock / MHZ(1));

#ifdef CONFIG_USB_DEVICE_STACK
	init_usb();

	/* Redirect static IP address to netusb*/
	const struct device *usb_dev = device_get_binding("eth_netusb");
	struct net_if *iface = net_if_lookup_by_dev(usb_dev);
	if (!iface) {
		printk("Cannot find network interface: %s", "eth_netusb");
		return;
	}
	if (sizeof(CONFIG_NET_CONFIG_USB_IPV4_ADDR) > 1) {
		if (net_addr_pton(AF_INET, CONFIG_NET_CONFIG_USB_IPV4_ADDR, &addr)) {
			printk("Invalid address: %s", CONFIG_NET_CONFIG_USB_IPV4_ADDR);
			return;
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
#endif
	const struct device *dev = device_get_binding("wlan0");
	struct net_if *wifi_iface = net_if_lookup_by_dev(dev);
	if (!wifi_iface) {
		printk("Cannot find network interface: %s", "wlan0");
		return;
	}
	/* As both are Ethernet, we need to set specific interface*/
	net_if_set_default(wifi_iface);

	net_config_init_app(dev, "Initializing network");

	dut_main();
}
