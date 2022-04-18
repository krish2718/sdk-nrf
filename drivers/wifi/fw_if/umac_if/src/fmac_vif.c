/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @brief File containing VIF handling specific definitions for the
 * FMAC IF Layer of the Wi-Fi driver.
 */

#include "fmac_vif.h"
#include "host_rpu_umac_if.h"


int nvlsi_wlan_fmac_vif_check_if_limit(struct nvlsi_wlan_fmac_dev_ctx *fmac_dev_ctx,
				       int if_type)
{
	switch (if_type) {
	case IMG_IFTYPE_STATION:
	case IMG_IFTYPE_P2P_CLIENT:
		if (fmac_dev_ctx->num_sta >= MAX_NUM_STAS) {
			nvlsi_rpu_osal_log_err(fmac_dev_ctx->fpriv->opriv,
					       "%s: Maximum STA Interface type exceeded\n",
					       __func__);
			return -1;
		}
		break;
	case IMG_IFTYPE_AP:
	case IMG_IFTYPE_P2P_GO:
		if (fmac_dev_ctx->num_ap >= MAX_NUM_APS) {
			nvlsi_rpu_osal_log_err(fmac_dev_ctx->fpriv->opriv,
					       "%s: Maximum AP Interface type exceeded\n",
					       __func__);
			return -1;
		}
		break;
	default:
		nvlsi_rpu_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				       "%s: Interface type not supported\n",
				       __func__);
		return -1;
	}

	return 0;
}


void nvlsi_wlan_fmac_vif_incr_if_type(struct nvlsi_wlan_fmac_dev_ctx *fmac_dev_ctx,
				      int if_type)
{
	switch (if_type) {
	case IMG_IFTYPE_STATION:
	case IMG_IFTYPE_P2P_CLIENT:
		fmac_dev_ctx->num_sta++;
		break;
	case IMG_IFTYPE_AP:
	case IMG_IFTYPE_P2P_GO:
		fmac_dev_ctx->num_ap++;
		break;
	default:
		nvlsi_rpu_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				       "%s:Unsupported VIF type\n",
				       __func__);
	}
}


void nvlsi_wlan_fmac_vif_decr_if_type(struct nvlsi_wlan_fmac_dev_ctx *fmac_dev_ctx,
				      int if_type)
{
	switch (if_type) {
	case IMG_IFTYPE_STATION:
	case IMG_IFTYPE_P2P_CLIENT:
		fmac_dev_ctx->num_sta--;
		break;
	case IMG_IFTYPE_AP:
	case IMG_IFTYPE_P2P_GO:
		fmac_dev_ctx->num_ap--;
		break;
	default:
		nvlsi_rpu_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				       "%s:Unsupported VIF type\n",
				       __func__);
	}
}


void nvlsi_wlan_fmac_vif_clear_ctx(void *nvlsi_fmac_dev_ctx,
				   unsigned char nvlsi_vif_idx)
{
	struct nvlsi_wlan_fmac_dev_ctx *fmac_dev_ctx = NULL;
	struct nvlsi_wlan_fmac_vif_ctx *vif_ctx = NULL;

	fmac_dev_ctx = (struct nvlsi_wlan_fmac_dev_ctx *)nvlsi_fmac_dev_ctx;

	vif_ctx =  fmac_dev_ctx->vif_ctx[nvlsi_vif_idx];

	nvlsi_rpu_osal_mem_free(fmac_dev_ctx->fpriv->opriv,
				vif_ctx);
	fmac_dev_ctx->vif_ctx[nvlsi_vif_idx] = NULL;
}


void nvlsi_wlan_fmac_vif_update_if_type(void *nvlsi_fmac_dev_ctx,
					unsigned char nvlsi_vif_idx,
					int if_type)
{
	struct nvlsi_wlan_fmac_dev_ctx *fmac_dev_ctx = NULL;
	struct nvlsi_wlan_fmac_vif_ctx *vif_ctx = NULL;

	fmac_dev_ctx = (struct nvlsi_wlan_fmac_dev_ctx *)nvlsi_fmac_dev_ctx;

	vif_ctx  = fmac_dev_ctx->vif_ctx[nvlsi_vif_idx];

	nvlsi_wlan_fmac_vif_decr_if_type(fmac_dev_ctx,
					 vif_ctx->if_type);

	nvlsi_wlan_fmac_vif_incr_if_type(fmac_dev_ctx,
					 if_type);

	vif_ctx->if_type = if_type;
}
