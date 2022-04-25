/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @brief File containing netowrk stack interface specific definitions for the
 * Zephyr OS layer of the Wi-Fi driver.
 */

#include <stdlib.h>
#include "fmac_api.h"
#include "shim.h"
#include "zephyr_fmac_main.h"
#include "zephyr_net_if.h"

void wifi_nrf_if_init(struct net_if *iface)
{
	struct wifi_nrf_vif_ctx_zep *vif_ctx_zep = NULL;
	const struct device *dev = NULL;
	char mac_addr[] = {0x00, 0x23, 0x23, 0x23, 0x23, 0x23};

	dev = net_if_get_device(iface);

	vif_ctx_zep = dev->data;

	if (!vif_ctx_zep) {
		printk("%s: vif_ctx_zep is NULL\n", __func__);
		return;
	}

	vif_ctx_zep->zep_net_if_ctx = iface;

	ethernet_init(iface);

	net_if_set_link_addr(iface,
			     mac_addr,
			     sizeof(mac_addr),
			     NET_LINK_ETHERNET);
}


enum ethernet_hw_caps wifi_nrf_if_caps_get(const struct device *dev)
{
	return (ETHERNET_LINK_10BASE_T |
		ETHERNET_LINK_100BASE_T |
		ETHERNET_LINK_1000BASE_T);
}


int wifi_nrf_if_send(const struct device *dev,
		  struct net_pkt *pkt)
{
	struct wifi_nrf_vif_ctx_zep *vif_ctx_zep = NULL;
	struct wifi_nrf_ctx_zep *rpu_ctx_zep = NULL;

	vif_ctx_zep = dev->data;
	rpu_ctx_zep = vif_ctx_zep->rpu_ctx_zep;

	if (!vif_ctx_zep) {
		printk("%s: vif_ctx_zep is NULL\n", __func__);
		net_pkt_unref(pkt);
		return -1;
	}

	/* TODO : net_pkt to nbuf ?? */
	return wifi_nrf_fmac_start_xmit(rpu_ctx_zep->rpu_ctx,
					  vif_ctx_zep->vif_idx,
					  net_pkt_to_nbuf(pkt));
}
