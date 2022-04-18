/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @brief Header containing network stack interface specific declarations for
 * the Zephyr OS layer of the Wi-Fi driver.
 */

#ifndef __ZEPHYR_NET_IF_H__
#define __ZEPHYR_NET_IF_H__
#include <device.h>
#include <net/net_pkt.h>
#include <net/net_if.h>
#include <net/ethernet.h>


void nvlsi_if_init(struct net_if *iface);
enum ethernet_hw_caps nvlsi_if_caps_get(const struct device *dev);
int nvlsi_if_send(const struct device *dev,
		  struct net_pkt *pkt);
#endif /* __ZEPHYR_NET_IF_H__ */
