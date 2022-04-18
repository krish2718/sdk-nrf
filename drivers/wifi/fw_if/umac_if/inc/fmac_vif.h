/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @brief Header containing virtual interface (VIF) specific declarations for
 * the FMAC IF Layer of the Wi-Fi driver.
 */

#ifndef __FMAC_VIF_H__
#define __FMAC_VIF_H__

#include "fmac_structs.h"

int nvlsi_wlan_fmac_vif_check_if_limit(struct nvlsi_wlan_fmac_dev_ctx *fmac_dev_ctx,
				       int if_type);

void nvlsi_wlan_fmac_vif_incr_if_type(struct nvlsi_wlan_fmac_dev_ctx *fmac_dev_ctx,
				      int if_type);

void nvlsi_wlan_fmac_vif_decr_if_type(struct nvlsi_wlan_fmac_dev_ctx *fmac_dev_ctx,
				      int if_type);

void nvlsi_wlan_fmac_vif_clear_ctx(void *nvlsi_fmac_dev_ctx,
				   unsigned char nvlsi_vif_idx);

void nvlsi_wlan_fmac_vif_update_if_type(void *nvlsi_fmac_dev_ctx,
					unsigned char nvlsi_vif_idx,
					int if_type);
#endif /* __FMAC_VIF_H__ */

