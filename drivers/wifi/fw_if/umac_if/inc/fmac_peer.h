/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @brief Header containing peer handling specific declarations for the
 * FMAC IF Layer of the Wi-Fi driver.
 */

#ifndef __FMAC_PEER_H__
#define __FMAC_PEER_H__

#include "fmac_structs.h"

int nvlsi_wlan_fmac_peer_get_id(struct nvlsi_wlan_fmac_dev_ctx *fmac_ctx,
				const unsigned char *mac_addr);

int nvlsi_wlan_fmac_peer_add(struct nvlsi_wlan_fmac_dev_ctx *fmac_ctx,
			     unsigned char nvlsi_vif_idx,
			     const unsigned char *mac_addr,
			     unsigned char is_legacy,
			     unsigned char qos_supported);

void nvlsi_wlan_fmac_peer_remove(struct nvlsi_wlan_fmac_dev_ctx *fmac_ctx,
				 unsigned char nvlsi_vif_idx,
				 int peer_id);

void nvlsi_wlan_fmac_peers_flush(struct nvlsi_wlan_fmac_dev_ctx *fmac_ctx,
				 unsigned char nvlsi_vif_idx);

#endif /* __FMAC_PEER_H__ */
