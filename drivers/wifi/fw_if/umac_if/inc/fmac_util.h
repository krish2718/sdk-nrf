/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#ifndef __FMAC_UTIL_H__
#define __FMAC_UTIL_H__

#define PACK(__Declaration__) __Declaration__ __attribute__((__packed__))

#include <stdbool.h>
#include "fmac_structs.h"

#define NVLSI_WLAN_FMAC_ETH_ADDR_LEN 6
#define NVLSI_WLAN_FMAC_ETH_HDR_LEN 14

#define NVLSI_WLAN_FMAC_FTYPE_DATA 0x0008
#define NVLSI_WLAN_FMAC_STYPE_DATA 0x0000
#define NVLSI_WLAN_FMAC_STYPE_QOS_DATA 0x0080

#define NVLSI_WLAN_FMAC_FCTL_FTYPE 0x000c
#define NVLSI_WLAN_FMAC_FCTL_PROTECTED 0x4000
#define NVLSI_WLAN_FMAC_FCTL_TODS 0x0100
#define NVLSI_WLAN_FMAC_FCTL_FROMDS 0x0200

#define NVLSI_WLAN_FMAC_CIPHER_SUITE_WEP40 0x000FAC01
#define NVLSI_WLAN_FMAC_CIPHER_SUITE_WEP104 0x000FAC05
#define NVLSI_WLAN_FMAC_CIPHER_SUITE_TKIP 0x000FAC02
#define NVLSI_WLAN_FMAC_CIPHER_SUITE_CCMP 0x000FAC04
#define NVLSI_WLAN_FMAC_CIPHER_SUITE_CCMP_256 0x000FAC0A
#define NVLSI_WLAN_FMAC_CIPHER_SUITE_OPEN 0x0
#define NVLSI_WLAN_FMAC_CIPHER_SUITE_SMS4 0x00147201

#define NVLSI_WLAN_FMAC_CCMP_HDR_LEN 8
#define NVLSI_WLAN_FMAC_CCMP_256_HDR_LEN 8
#define NVLSI_WLAN_FMAC_SMS4_HDR_LEN 18

#define NVLSI_WLAN_FMAC_WEP_IV_LEN 4
#define NVLSI_WLAN_FMAC_TKIP_IV_LEN 8

#define NVLSI_WLAN_FCTL_TODS 0x0100
#define NVLSI_WLAN_FCTL_FROMDS 0x0200
#define NVLSI_WLAN_FMAC_ETH_P_8021Q     0x8100  /* 802.1Q VLAN Extended Header */
#define NVLSI_WLAN_FMAC_ETH_P_8021AD    0x88A8  /* 802.1ad Service VLAN */
#define NVLSI_WLAN_FMAC_ETH_P_MPLS_UC   0x8847  /* MPLS Unicast traffic */
#define NVLSI_WLAN_FMAC_ETH_P_MPLS_MC   0x8848  /* MPLS Multicast traffic */
#define NVLSI_WLAN_FMAC_ETH_P_IP        0x0800  /* Internet Protocol packet */
#define NVLSI_WLAN_FMAC_ETH_P_IPV6      0x86DD  /* IPv6 over bluebook */
#define NVLSI_WLAN_FMAC_ETH_P_80221     0x8917  /* IEEE 802.21 Media Independent Handover Protocol */
#define NVLSI_WLAN_FMAC_ETH_P_AARP 0x80F3       /* Appletalk AARP */
#define NVLSI_WLAN_FMAC_ETH_P_IPX  0x8137       /* IPX over DIX */
#define NVLSI_WLAN_FMAC_ETH_P_802_3_MIN 0x0600  /* If the value in the ethernet type is less than this value
						 * then the frame is Ethernet II. Else it is 802.3
						 */
#define NVLSI_WLAN_FMAC_VLAN_PRIO_SHIFT	0x0D  /* 13 bit */
#define NVLSI_WLAN_FMAC_VLAN_PRIO_MASK	0xE000
#define NVLSI_WLAN_FMAC_MPLS_LS_TC_MASK	0x00000E00
#define NVLSI_WLAN_FMAC_MPLS_LS_TC_SHIFT	0x09
#define NVLSI_WLAN_FMAC_IPV6_TOS_MASK	0x0FF0
#define NVLSI_WLAN_FMAC_IPV6_TOS_SHIFT	0x04  /* 4bit */
#define NVLSI_WLAN_FMAC_ETH_TYPE_MASK	0xFFFF

PACK(
struct wifi_nrf_wlan_fmac_ieee80211_hdr {
	unsigned short fc;
	unsigned short dur_id;
	unsigned char addr_1[NVLSI_WLAN_FMAC_ETH_ADDR_LEN];
	unsigned char addr_2[NVLSI_WLAN_FMAC_ETH_ADDR_LEN];
	unsigned char addr_3[NVLSI_WLAN_FMAC_ETH_ADDR_LEN];
	unsigned short seq_ctrl;
	unsigned char addr_4[NVLSI_WLAN_FMAC_ETH_ADDR_LEN];
	});

PACK(
struct wifi_nrf_wlan_fmac_eth_hdr {
	unsigned char dst[NVLSI_WLAN_FMAC_ETH_ADDR_LEN]; /* destination eth addr */
	unsigned char src[NVLSI_WLAN_FMAC_ETH_ADDR_LEN]; /* source ether addr */
	unsigned short proto; /* packet type ID field */
	});

PACK(
struct wifi_nrf_wlan_fmac_amsdu_hdr {
	unsigned char dst[NVLSI_WLAN_FMAC_ETH_ADDR_LEN]; /* destination eth addr */
	unsigned char src[NVLSI_WLAN_FMAC_ETH_ADDR_LEN]; /* source ether addr */
	unsigned short length; /* length*/
	});

bool wifi_nrf_wlan_util_is_multicast_addr(const unsigned char *addr);

bool wifi_nrf_wlan_util_is_unicast_addr(const unsigned char *addr);

bool wifi_nrf_wlan_util_ether_addr_equal(const unsigned char *addr_1,
				      const unsigned char *addr_2);

int wifi_nrf_wlan_util_get_tid(struct wifi_nrf_wlan_fmac_dev_ctx *fmac_dev_ctx,
			    void *nwb);

int wifi_nrf_wlan_util_get_vif_indx(struct wifi_nrf_wlan_fmac_dev_ctx *fmac_dev_ctx,
				 const unsigned char *mac_addr);

unsigned char *wifi_nrf_wlan_util_get_ra(struct wifi_nrf_wlan_fmac_vif_ctx *vif,
				      void *nwb);

unsigned char *wifi_nrf_wlan_util_get_src(struct wifi_nrf_wlan_fmac_dev_ctx *fmac_dev_ctx,
				       void *nwb);

unsigned char *wifi_nrf_wlan_util_get_dest(struct wifi_nrf_wlan_fmac_dev_ctx *fmac_dev_ctx,
					void *nwb);

unsigned short wifi_nrf_wlan_util_tx_get_eth_type(struct wifi_nrf_wlan_fmac_dev_ctx *fmac_dev_ctx,
					       void *nwb);

unsigned short wifi_nrf_wlan_util_rx_get_eth_type(struct wifi_nrf_wlan_fmac_dev_ctx *fmac_dev_ctx,
					       void *nwb);

int wifi_nrf_wlan_util_get_skip_header_bytes(unsigned short eth_type);

void wifi_nrf_wlan_util_convert_to_eth(struct wifi_nrf_wlan_fmac_dev_ctx *fmac_dev_ctx,
				    void *nwb,
				    struct wifi_nrf_wlan_fmac_ieee80211_hdr *hdr,
				    unsigned short eth_type);

void wifi_nrf_wlan_util_rx_convert_amsdu_to_eth(struct wifi_nrf_wlan_fmac_dev_ctx *fmac_dev_ctx,
					     void *nwb);

bool wifi_nrf_wlan_util_is_arr_zero(unsigned char *arr,
				 unsigned int arr_sz);
#endif /* __FMAC_UTIL_H__ */

