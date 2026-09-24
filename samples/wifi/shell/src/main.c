/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 * @brief WiFi shell sample main function
 */

#include <zephyr/sys/printk.h>
#include <zephyr/kernel.h>
#include <zephyr/pm/device.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/printk.h>
#include <zephyr/init.h>
#include <zephyr/pm/device.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/wifi_utils.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/ethernet.h>
#include <zephyr/net/ethernet_mgmt.h>

#if defined(CONFIG_NRFX_CLOCK_HFCLK) &&                                                            \
	(defined(CLOCK_FEATURE_HFCLK_DIVIDE_PRESENT) || NRF_CLOCK_HAS_HFCLK192M)
#include <nrfx_clock_hfclk.h>
#endif
#include <zephyr/device.h>
#include <zephyr/net/net_config.h>

#if defined(CONFIG_USB_DEVICE_STACK) && !defined(CONFIG_BOARD_THINGY91X_NRF5340_CPUAPP)
#define USES_USB_ETH 1
#else
#define USES_USB_ETH 0
#endif

#if USES_USB_ETH
#include <zephyr/usb/usb_device.h>
#endif

#if USES_USB_ETH || defined(CONFIG_SLIP)
static struct in_addr addr = { { { 192, 0, 2, 1 } } };
static struct in_addr mask = { { { 255, 255, 255, 0 } } };
#endif /* CONFIG_USB_DEVICE_STACK || CONFIG_SLIP */

#if USES_USB_ETH
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

static int wifi_scan(void)
{
	struct net_if *iface = net_if_get_default();
	int band_str_len;
	struct wifi_scan_params params = { 0 };


	params.dwell_time_passive = 130;
	params.dwell_time_active = 50;
	params.scan_type = WIFI_SCAN_TYPE_ACTIVE;


	if (net_mgmt(NET_REQUEST_WIFI_SCAN, iface, &params,
			sizeof(struct wifi_scan_params))) {
		return -ENOEXEC;
	}

	printk("Scan requested\n");


	return 0;
}


int main(void)
{
#if defined(CONFIG_NRFX_CLOCK_HFCLK) &&                                                            \
	(defined(CLOCK_FEATURE_HFCLK_DIVIDE_PRESENT) || NRF_CLOCK_HAS_HFCLK192M)
	/* For now hardcode to 128MHz */
	nrfx_clock_hfclk_divider_set(NRF_CLOCK_HFCLK_DIV_1);
#endif
	printk("Starting %s with CPU frequency: %d MHz\n", CONFIG_BOARD, SystemCoreClock/MHZ(1));

#if USES_USB_ETH
	init_usb();

	/* Redirect static IP address to netusb*/
	const struct device *usb_dev = device_get_binding("eth_netusb");
	struct net_if *iface = net_if_lookup_by_dev(usb_dev);

	if (!iface) {
		printk("Cannot find network interface: %s", "eth_netusb");
		return -1;
	}

	net_if_ipv4_addr_add(iface, &addr, NET_ADDR_MANUAL, 0);
	net_if_ipv4_set_netmask_by_addr(iface, &addr, &mask);
#endif

#ifdef CONFIG_SLIP
	const struct device *slip_dev = device_get_binding(CONFIG_SLIP_DRV_NAME);
	struct net_if *slip_iface = net_if_lookup_by_dev(slip_dev);

	if (!slip_iface) {
		printk("Cannot find network interface: %s", CONFIG_SLIP_DRV_NAME);
		return -1;
	}

	net_if_ipv4_addr_add(slip_iface, &addr, NET_ADDR_MANUAL, 0);
	net_if_ipv4_set_netmask_by_addr(slip_iface, &addr, &mask);
#endif /* CONFIG_SLIP */

#ifdef CONFIG_NET_CONFIG_SETTINGS
	/* Without this, DHCPv4 starts on first interface and if that is not Wi-Fi or
	 * only supports IPv6, then its an issue. (E.g., OpenThread)
	 *
	 * So, we start DHCPv4 on Wi-Fi interface always, independent of the ordering.
	 */
	const struct device *dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_wifi));
	struct net_if *wifi_iface = net_if_lookup_by_dev(dev);

	/* As both are Ethernet, we need to set specific interface*/
	net_if_set_default(wifi_iface);

	net_config_init_app(dev, "Initializing network");
#endif

#if 1
	/* Suspend the console (UART) before sleeping -- otherwise it
	 * stays fully active (and keeps whatever clock it depends on
	 * requested) for the whole "idle" window, which is the
	 * single biggest cause of elevated idle current in a sample
	 * like this.
	 */
	const struct device *const console = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
	int err = pm_device_action_run(console, PM_DEVICE_ACTION_SUSPEND);
	if (err != 0) {
		printk("Failed to suspend console: %d\n", err);
		return 0;
	}
#endif
	return 0;
	while (1) {
		wifi_scan();
		k_sleep(K_SECONDS(10));
	}
	return 0;
}
