/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @brief File containing utility function definitions for the
 * FMAC IF Layer of the Wi-Fi driver.
 */

#include "osal_api.h"
#include "fmac_util.h"
#include "host_rpu_umac_if.h"

/* Ethernet-II snap header (RFC1042 for most EtherTypes) */
unsigned char llc_header[]  = { 0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00 };

/* Bridge-Tunnel header (for EtherTypes ETH_P_AARP and ETH_P_IPX) */
unsigned char aarp_ipx_header[] = { 0xaa, 0xaa, 0x03, 0x00, 0x00, 0xf8 };


bool nvlsi_wlan_util_is_multicast_addr(const unsigned char *addr)
{
	return (0x01 & *addr);
}


bool nvlsi_wlan_util_is_unicast_addr(const unsigned char *addr)
{
	return !nvlsi_wlan_util_is_multicast_addr(addr);
}


bool nvlsi_wlan_util_ether_addr_equal(const unsigned char *addr_1,
				      const unsigned char *addr_2)
{
	const unsigned short *a = (const unsigned short *)addr_1;
	const unsigned short *b = (const unsigned short *)addr_2;

	return ((a[0] ^ b[0]) | (a[1] ^ b[1]) | (a[2] ^ b[2])) == 0;
}


unsigned short nvlsi_wlan_util_rx_get_eth_type(struct nvlsi_wlan_fmac_dev_ctx *fmac_dev_ctx,
					       void *nwb)
{
	unsigned  char *payload = NULL;

	payload = (unsigned char *)nwb;
	return payload[6] << 8 | payload[7];
}


unsigned short nvlsi_wlan_util_tx_get_eth_type(struct nvlsi_wlan_fmac_dev_ctx *fmac_dev_ctx,
					       void *nwb)
{
	unsigned  char *payload = NULL;

	payload = (unsigned char *)nwb;
	return payload[12] << 8 | payload[13];
}


int nvlsi_wlan_util_get_skip_header_bytes(unsigned short eth_type)
{
	int skip_header_bytes = sizeof(eth_type);

	if (eth_type == NVLSI_WLAN_FMAC_ETH_P_AARP ||
	    eth_type == NVLSI_WLAN_FMAC_ETH_P_IPX)
		skip_header_bytes += sizeof(aarp_ipx_header);
	else if (eth_type >= NVLSI_WLAN_FMAC_ETH_P_802_3_MIN)
		skip_header_bytes += sizeof(llc_header);

	return skip_header_bytes;
}


void nvlsi_wlan_util_convert_to_eth(struct nvlsi_wlan_fmac_dev_ctx *fmac_dev_ctx,
				    void *nwb,
				    struct nvlsi_wlan_fmac_ieee80211_hdr *hdr,
				    unsigned short eth_type)
{

	struct nvlsi_wlan_fmac_eth_hdr *ehdr = NULL;
	unsigned int len = 0;

	len = nvlsi_rpu_osal_nbuf_data_size(fmac_dev_ctx->fpriv->opriv,
					    nwb);

	ehdr = (struct nvlsi_wlan_fmac_eth_hdr *)
		nvlsi_rpu_osal_nbuf_data_push(fmac_dev_ctx->fpriv->opriv,
					      nwb,
					      sizeof(struct nvlsi_wlan_fmac_eth_hdr));

	switch (hdr->fc & (NVLSI_WLAN_FCTL_TODS | NVLSI_WLAN_FCTL_FROMDS)) {
	case (NVLSI_WLAN_FCTL_TODS | NVLSI_WLAN_FCTL_FROMDS):
		nvlsi_rpu_osal_mem_cpy(fmac_dev_ctx->fpriv->opriv,
				       ehdr->src,
				       hdr->addr_4,
				       NVLSI_WLAN_FMAC_ETH_ADDR_LEN);

		nvlsi_rpu_osal_mem_cpy(fmac_dev_ctx->fpriv->opriv,
				       ehdr->dst,
				       hdr->addr_1,
				       NVLSI_WLAN_FMAC_ETH_ADDR_LEN);
		break;
	case (NVLSI_WLAN_FCTL_FROMDS):
		nvlsi_rpu_osal_mem_cpy(fmac_dev_ctx->fpriv->opriv,
				       ehdr->src,
				       hdr->addr_3,
				       NVLSI_WLAN_FMAC_ETH_ADDR_LEN);
		nvlsi_rpu_osal_mem_cpy(fmac_dev_ctx->fpriv->opriv,
				       ehdr->dst,
				       hdr->addr_1,
				       NVLSI_WLAN_FMAC_ETH_ADDR_LEN);
		break;
	case (NVLSI_WLAN_FCTL_TODS):
		nvlsi_rpu_osal_mem_cpy(fmac_dev_ctx->fpriv->opriv,
				       ehdr->src,
				       hdr->addr_2,
				       NVLSI_WLAN_FMAC_ETH_ADDR_LEN);
		nvlsi_rpu_osal_mem_cpy(fmac_dev_ctx->fpriv->opriv,
				       ehdr->dst,
				       hdr->addr_3,
				       NVLSI_WLAN_FMAC_ETH_ADDR_LEN);
		break;
	default:
		/* Both FROM and TO DS bit is zero*/
		nvlsi_rpu_osal_mem_cpy(fmac_dev_ctx->fpriv->opriv,
				       ehdr->src,
				       hdr->addr_2,
				       NVLSI_WLAN_FMAC_ETH_ADDR_LEN);
		nvlsi_rpu_osal_mem_cpy(fmac_dev_ctx->fpriv->opriv,
				       ehdr->dst,
				       hdr->addr_1,
				       NVLSI_WLAN_FMAC_ETH_ADDR_LEN);

	}

	if (eth_type >= NVLSI_WLAN_FMAC_ETH_P_802_3_MIN)
		ehdr->proto = ((eth_type >> 8) | (eth_type << 8));
	else
		ehdr->proto = len;
}

void nvlsi_wlan_util_rx_convert_amsdu_to_eth(struct nvlsi_wlan_fmac_dev_ctx *fmac_dev_ctx,
					     void *nwb)
{
	struct nvlsi_wlan_fmac_eth_hdr *ehdr = NULL;
	struct nvlsi_wlan_fmac_amsdu_hdr amsdu_hdr;
	unsigned int len = 0;
	unsigned short eth_type = 0;
	unsigned char amsdu_hdr_len = sizeof(struct nvlsi_wlan_fmac_amsdu_hdr);

	nvlsi_rpu_osal_mem_cpy(fmac_dev_ctx->fpriv->opriv,
			       &amsdu_hdr,
			       nvlsi_rpu_osal_nbuf_data_get(fmac_dev_ctx->fpriv->opriv,
							    nwb),
			       amsdu_hdr_len);

	eth_type = nvlsi_wlan_util_rx_get_eth_type(fmac_dev_ctx,
						   (void *)((char *)nvlsi_rpu_osal_nbuf_data_get(fmac_dev_ctx->fpriv->opriv,
												 nwb) + amsdu_hdr_len));

	nvlsi_rpu_osal_nbuf_data_pull(fmac_dev_ctx->fpriv->opriv,
				      nwb,
				      (amsdu_hdr_len +
				       nvlsi_wlan_util_get_skip_header_bytes(eth_type)));

	len = nvlsi_rpu_osal_nbuf_data_size(fmac_dev_ctx->fpriv->opriv,
					    nwb);

	ehdr = (struct nvlsi_wlan_fmac_eth_hdr *)
		nvlsi_rpu_osal_nbuf_data_push(fmac_dev_ctx->fpriv->opriv,
					      nwb,
					      sizeof(struct nvlsi_wlan_fmac_eth_hdr));

	nvlsi_rpu_osal_mem_cpy(fmac_dev_ctx->fpriv->opriv,
			       ehdr->src,
			       amsdu_hdr.src,
			       NVLSI_WLAN_FMAC_ETH_ADDR_LEN);

	nvlsi_rpu_osal_mem_cpy(fmac_dev_ctx->fpriv->opriv,
			       ehdr->dst,
			       amsdu_hdr.dst,
			       NVLSI_WLAN_FMAC_ETH_ADDR_LEN);

	if (eth_type >= NVLSI_WLAN_FMAC_ETH_P_802_3_MIN)
		ehdr->proto = ((eth_type >> 8) | (eth_type << 8));
	else
		ehdr->proto = len;
}

int nvlsi_wlan_util_get_tid(struct nvlsi_wlan_fmac_dev_ctx *fmac_dev_ctx,
			    void *nwb)
{
	unsigned short ether_type = 0;
	int priority = 0;
	unsigned short vlan_tci = 0;
	unsigned char vlan_priority = 0;
	unsigned int mpls_hdr = 0;
	unsigned char mpls_tc_qos = 0;
	unsigned char tos = 0;
	unsigned char dscp = 0;
	unsigned short ipv6_hdr = 0;
	unsigned char *nwb_data = NULL;

	ether_type = nvlsi_wlan_util_tx_get_eth_type(fmac_dev_ctx,
						     nvlsi_rpu_osal_nbuf_data_get(fmac_dev_ctx->fpriv->opriv,
										  nwb));
	nwb_data = (unsigned char *)nvlsi_rpu_osal_nbuf_data_get(fmac_dev_ctx->fpriv->opriv,
								 nwb) + NVLSI_WLAN_FMAC_ETH_HDR_LEN;

	switch (ether_type & NVLSI_WLAN_FMAC_ETH_TYPE_MASK) {
	/* If VLAN 802.1Q (0x8100) ||
	 * 802.1AD(0x88A8) FRAME calculate priority accordingly
	 */
	case NVLSI_WLAN_FMAC_ETH_P_8021Q: /* ETH_P_8021Q: */
	case NVLSI_WLAN_FMAC_ETH_P_8021AD: /* ETH_P_8021AD: */
		vlan_tci = (((unsigned char *)nwb_data)[4] << 8) |
			(((unsigned char *)nwb_data)[5]);
		vlan_priority = ((vlan_tci & NVLSI_WLAN_FMAC_VLAN_PRIO_MASK)
				 >> NVLSI_WLAN_FMAC_VLAN_PRIO_SHIFT);
		priority = vlan_priority;
		break;

		/* If MPLS MC(0x8840) / UC(0x8847) frame calculate priority
		 * accordingly
		 */
	case NVLSI_WLAN_FMAC_ETH_P_MPLS_UC: /*ETH_P_MPLS_UC:*/
	case NVLSI_WLAN_FMAC_ETH_P_MPLS_MC: /*ETH_P_MPLS_MC:*/
		mpls_hdr = (((unsigned char *)nwb_data)[0] << 24) |
			(((unsigned char *)nwb_data)[1] << 16) |
			(((unsigned char *)nwb_data)[2] << 8)  |
			(((unsigned char *)nwb_data)[3]);
		mpls_tc_qos = (mpls_hdr & (NVLSI_WLAN_FMAC_MPLS_LS_TC_MASK)
			       >> NVLSI_WLAN_FMAC_MPLS_LS_TC_SHIFT);
		priority = mpls_tc_qos;
		break;
		/* If IP (0x0800) frame calculate priority accordingly */
	case NVLSI_WLAN_FMAC_ETH_P_IP:/*ETH_P_IP:*/
		/*get the tos filed*//*DA+SA+ETH+(VER+IHL)*/
		tos = (((unsigned char *)nwb_data)[1]);
		/*get the dscp value */
		dscp = (tos & 0xfc);
		priority = dscp >> 5;
		break;
	case NVLSI_WLAN_FMAC_ETH_P_IPV6: /*ETH_P_IPV6:*/
		/*get the tos filed*//*DA+SA+ETH*/
		ipv6_hdr = (((unsigned char *)nwb_data)[0] << 8) |
			((unsigned char *)nwb_data)[1];
		dscp = (((ipv6_hdr & NVLSI_WLAN_FMAC_IPV6_TOS_MASK)
			 >> NVLSI_WLAN_FMAC_IPV6_TOS_SHIFT) & 0xfc);
		priority = dscp >> 5;
		break;
		/* If Media Independent (0x8917)
		 * frame calculate priority accordingly.
		 */
	case NVLSI_WLAN_FMAC_ETH_P_80221: /* ETH_P_80221 */
		/* 802.21 is always network control traffic */
		priority = 0x07;
		break;
	default:
		priority = 0;
	}

	return priority;
}


int nvlsi_wlan_util_get_vif_indx(struct nvlsi_wlan_fmac_dev_ctx *fmac_dev_ctx,
				 const unsigned char *mac_addr)
{
	int i = 0;
	int vif_index = -1;

	for (i = 0; i < MAX_PEERS; i++) {
		if (!nvlsi_wlan_util_ether_addr_equal(
						      fmac_dev_ctx->tx_config.peers[i].ra_addr,
						      mac_addr)) {
			vif_index = fmac_dev_ctx->tx_config.peers[i].nvlsi_vif_idx;
			break;
		}
	}
	if (vif_index == -1)
		nvlsi_rpu_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				       "%s: Invalid vif_index = %d",
				       __func__,
				       vif_index);

	return vif_index;
}


unsigned char *nvlsi_wlan_util_get_dest(struct nvlsi_wlan_fmac_dev_ctx *fmac_dev_ctx,
					void *nwb)
{
	return nvlsi_rpu_osal_nbuf_data_get(fmac_dev_ctx->fpriv->opriv,
					    nwb);
}


unsigned char *nvlsi_wlan_util_get_ra(struct nvlsi_wlan_fmac_vif_ctx *vif,
				      void *nwb)
{
	if (vif->if_type == IMG_IFTYPE_STATION)
		return vif->bssid;

	return nvlsi_rpu_osal_nbuf_data_get(vif->fmac_dev_ctx->fpriv->opriv,
					    nwb);
}


unsigned char *nvlsi_wlan_util_get_src(struct nvlsi_wlan_fmac_dev_ctx *fmac_dev_ctx,
				       void *nwb)
{
	return (unsigned char *)nvlsi_rpu_osal_nbuf_data_get(fmac_dev_ctx->fpriv->opriv,
							     nwb) + NVLSI_WLAN_FMAC_ETH_ADDR_LEN;
}


bool nvlsi_wlan_util_is_arr_zero(unsigned char *arr,
				 unsigned int arr_sz)
{
	unsigned int i = 0;

	for (i = 0; i < arr_sz; i++) {
		if (arr[i] != 0)
			return false;
	}

	return true;
}
