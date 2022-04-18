/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @brief File containing FMAC interface specific definitions for the
 * Zephyr OS layer of the Wi-Fi driver.
 */

#include <stdlib.h>
#include <net/ethernet.h>
#include "rpu_fw_patches.h"
#include "fmac_api.h"
#include "zephyr_util.h"
#include "zephyr_fmac_main.h"
#include "zephyr_wpa_supp_if.h"
#include "zephyr_net_if.h"
#include "zephyr_disp_scan.h"

char *base_mac_addr = "0019F5331179";
char *rf_params;

unsigned char aggregation = 1;
unsigned char wmm = 1;
unsigned char max_num_tx_agg_sessions = 4;
unsigned char max_num_rx_agg_sessions = 2;
unsigned char reorder_buf_size = 64;
unsigned char max_rxampdu_size = MAX_RX_AMPDU_SIZE_64KB;

unsigned char max_tx_aggregation = 4;

unsigned int rx1_num_bufs = 21;
unsigned int rx2_num_bufs = 21;
unsigned int rx3_num_bufs = 21;

unsigned int rx1_buf_sz = 1600;
unsigned int rx2_buf_sz = 1600;
unsigned int rx3_buf_sz = 1600;

unsigned char rate_protection_type;

struct nvlsi_rpu_drv_priv_zep rpu_drv_priv_zep;


void nvlsi_event_proc_scan_start_zep(void *if_priv)
{
	struct nvlsi_rpu_vif_ctx_zep *vif_ctx_zep = NULL;

	vif_ctx_zep = if_priv;

	if (vif_ctx_zep->scan_type == SCAN_DISPLAY)
		return;

	nvlsi_wpa_supp_event_proc_scan_start(if_priv);
}


void nvlsi_event_proc_scan_done_zep(void *vif_ctx,
				    struct img_umac_event_trigger_scan *scan_done_event)
{
	enum nvlsi_rpu_status status = NVLSI_RPU_STATUS_FAIL;
	struct nvlsi_rpu_vif_ctx_zep *vif_ctx_zep = NULL;

	vif_ctx_zep = vif_ctx;

	if (!vif_ctx_zep) {
		printk("%s: vif_ctx_zep is NULL\n", __func__);
		return;
	}

	if (vif_ctx_zep->scan_type == SCAN_DISPLAY) {
		status = nvlsi_disp_scan_res_get_zep(vif_ctx_zep);

		if (status != NVLSI_RPU_STATUS_SUCCESS) {
			printk("%s: nvlsi_disp_scan_res_get_zep failed\n", __func__);
			return;
		}
	} else if (vif_ctx_zep->scan_type == SCAN_CONNECT) {
		nvlsi_wpa_supp_event_proc_scan_done(vif_ctx_zep,
						    scan_done_event,
						    0);
	} else {
		printk("%s: Scan type = %d not supported yet\n", __func__, vif_ctx_zep->scan_type);
		return;
	}

	status = NVLSI_RPU_STATUS_SUCCESS;
}


enum nvlsi_rpu_status nvlsi_wlan_fmac_dev_add_zep(struct nvlsi_rpu_drv_priv_zep *drv_priv_zep)
{
	enum nvlsi_rpu_status status = NVLSI_RPU_STATUS_FAIL;
	struct nvlsi_rpu_ctx_zep *rpu_ctx_zep = NULL;
	struct nvlsi_wlan_fmac_fw_info fw_info;
	void *rpu_ctx = NULL;

	rpu_ctx_zep = &drv_priv_zep->rpu_ctx_zep;

	rpu_ctx_zep->drv_priv_zep = drv_priv_zep;

	rpu_ctx = nvlsi_wlan_fmac_dev_add(drv_priv_zep->fmac_priv,
					  rpu_ctx_zep);

	if (!rpu_ctx) {
		printk("%s: nvlsi_wlan_fmac_dev_add failed\n", __func__);
		k_free(rpu_ctx_zep);
		rpu_ctx_zep = NULL;
		goto out;
	}

	rpu_ctx_zep->rpu_ctx = rpu_ctx;

	memset(&fw_info,
	       0,
	       sizeof(fw_info));

	fw_info.lmac_patch_pri.data = nvlsi_wlan_lmac_patch_pri_bimg;
	fw_info.lmac_patch_pri.size = sizeof(nvlsi_wlan_lmac_patch_pri_bimg);
	fw_info.lmac_patch_sec.data = nvlsi_wlan_lmac_patch_sec_bin;
	fw_info.lmac_patch_sec.size = sizeof(nvlsi_wlan_lmac_patch_sec_bin);
	fw_info.umac_patch_pri.data = nvlsi_wlan_umac_patch_pri_bimg;
	fw_info.umac_patch_pri.size = sizeof(nvlsi_wlan_umac_patch_pri_bimg);
	fw_info.umac_patch_sec.data = nvlsi_wlan_umac_patch_sec_bin;
	fw_info.umac_patch_sec.size = sizeof(nvlsi_wlan_umac_patch_sec_bin);

	/* Load the FW patches to the RPU */
	status = nvlsi_wlan_fmac_fw_load(rpu_ctx,
					 &fw_info);

	if (status != NVLSI_RPU_STATUS_SUCCESS) {
		printk("%s: nvlsi_wlan_fmac_fw_load failed\n", __func__);
		goto out;
	}

	status = nvlsi_wlan_fmac_ver_get(rpu_ctx);

	if (status != NVLSI_RPU_STATUS_SUCCESS) {
		printk("%s: nvlsi_wlan_fmac_ver_get failed\n", __func__);
		nvlsi_wlan_fmac_dev_rem(rpu_ctx);
		k_free(rpu_ctx_zep);
		rpu_ctx_zep = NULL;
		goto out;
	}

out:
	return status;
}


void nvlsi_wlan_fmac_dev_rem_zep(struct nvlsi_rpu_ctx_zep *rpu_ctx_zep)
{
	nvlsi_wlan_fmac_dev_rem(rpu_ctx_zep->rpu_ctx);

	k_free(rpu_ctx_zep);
}


enum nvlsi_rpu_status nvlsi_wlan_fmac_def_vif_add_zep(struct nvlsi_rpu_ctx_zep *rpu_ctx_zep,
						      unsigned char vif_idx,
						      const struct device *dev)
{
	enum nvlsi_rpu_status status = NVLSI_RPU_STATUS_FAIL;
	struct nvlsi_rpu_vif_ctx_zep *vif_ctx_zep = NULL;
	struct img_umac_add_vif_info add_vif_info;

	vif_ctx_zep = &rpu_ctx_zep->vif_ctx_zep[vif_idx];

	vif_ctx_zep->rpu_ctx_zep = rpu_ctx_zep;
	vif_ctx_zep->zep_dev_ctx = dev;

	memset(&add_vif_info,
	       0,
	       sizeof(add_vif_info));

	add_vif_info.iftype = IMG_IFTYPE_STATION;

	memcpy(add_vif_info.ifacename,
	       "wlan0",
	       strlen("wlan0"));

	memcpy(add_vif_info.mac_addr,
	       base_mac_addr,
	       sizeof(add_vif_info.mac_addr));

	vif_ctx_zep->vif_idx = nvlsi_wlan_fmac_add_vif(rpu_ctx_zep->rpu_ctx,
						       vif_ctx_zep,
						       &add_vif_info);

	if (vif_ctx_zep->vif_idx != 0) {
		printk("%s: FMAC returned non 0 index for default interface\n", __func__);
		k_free(vif_ctx_zep);
		goto out;
	}

	status = NVLSI_RPU_STATUS_SUCCESS;
out:
	return status;
}


void nvlsi_wlan_fmac_def_vif_rem_zep(struct nvlsi_rpu_vif_ctx_zep *vif_ctx_zep)
{
	struct nvlsi_rpu_ctx_zep *rpu_ctx_zep = NULL;

	rpu_ctx_zep = vif_ctx_zep->rpu_ctx_zep;

	nvlsi_wlan_fmac_del_vif(rpu_ctx_zep->rpu_ctx,
				vif_ctx_zep->vif_idx);

	k_free(vif_ctx_zep);
}



enum nvlsi_rpu_status nvlsi_wlan_fmac_def_vif_state_chg(struct nvlsi_rpu_vif_ctx_zep *vif_ctx_zep,
							unsigned char state)
{
	enum nvlsi_rpu_status status = NVLSI_RPU_STATUS_FAIL;
	struct nvlsi_rpu_ctx_zep *rpu_ctx_zep = NULL;
	struct img_umac_chg_vif_state_info vif_info;

	rpu_ctx_zep = vif_ctx_zep->rpu_ctx_zep;

	memset(&vif_info,
	       0,
	       sizeof(vif_info));

	vif_info.state = state;

	memcpy(vif_info.ifacename,
	       "wlan0",
	       strlen("wlan0"));

	status = nvlsi_wlan_fmac_chg_vif_state(rpu_ctx_zep->rpu_ctx,
					       vif_ctx_zep->vif_idx,
					       &vif_info);

	if (status != NVLSI_RPU_STATUS_SUCCESS) {
		printk("%s: nvlsi_wlan_fmac_chg_vif_state failed\n",
		       __func__);
		goto out;
	}
out:
	return status;
}


enum nvlsi_rpu_status nvlsi_wlan_fmac_dev_init_zep(struct nvlsi_rpu_ctx_zep *rpu_ctx_zep)
{
	enum nvlsi_rpu_status status = NVLSI_RPU_STATUS_FAIL;
	struct nvlsi_wlan_fmac_init_dev_params params;
	int ret = -1;

	memset(&params,
	       0,
	       sizeof(params));

	ret = hex_str_to_val(params.base_mac_addr,
			     sizeof(params.base_mac_addr),
			     base_mac_addr);

	if (ret == -1) {
		printk("%s: hex_str_to_val failed\n", __func__);
		goto out;
	}

	if (rf_params) {
		memset(params.rf_params,
		       0xFF,
		       sizeof(params.rf_params));

		ret = hex_str_to_val(params.rf_params,
				     sizeof(params.rf_params),
				     rf_params);

		if (ret == -1) {
			printk("%s: hex_str_to_val failed\n", __func__);
			goto out;
		}

		params.rf_params_valid = true;
	} else
		params.rf_params_valid = false;

	params.phy_calib = DEF_PHY_CALIB;

	status = nvlsi_wlan_fmac_dev_init(rpu_ctx_zep->rpu_ctx,
					  &params);

	if (status != NVLSI_RPU_STATUS_SUCCESS) {
		printk("%s: nvlsi_wlan_fmac_dev_init failed\n",
		       __func__);
		goto out;
	}
out:
	return status;
}


void nvlsi_wlan_fmac_dev_deinit_zep(struct nvlsi_rpu_ctx_zep *rpu_ctx_zep)
{
	struct nvlsi_wlan_fmac_dev_ctx *fmac_dev_ctx = NULL;
	struct nvlsi_rpu_vif_ctx_zep *vif_ctx_zep = NULL;
	unsigned char vif_idx = MAX_NUM_VIFS;
	unsigned char i = 0;

	fmac_dev_ctx = rpu_ctx_zep->rpu_ctx;

	/* Delete all other interfaces */
	for (i = 1; i < MAX_NUM_VIFS; i++) {
		if (fmac_dev_ctx->vif_ctx[i]) {
			vif_ctx_zep = fmac_dev_ctx->vif_ctx[i]->os_vif_ctx;
			vif_idx = vif_ctx_zep->vif_idx;
			nvlsi_wlan_fmac_del_vif(fmac_dev_ctx,
						vif_idx);
			fmac_dev_ctx->vif_ctx[i] = NULL;
		}
	}

	nvlsi_wlan_fmac_dev_deinit(rpu_ctx_zep->rpu_ctx);
}


static int nvlsi_rpu_drv_main_zep(const struct device *dev)
{
	enum nvlsi_rpu_status status = NVLSI_RPU_STATUS_FAIL;
	struct nvlsi_rpu_vif_ctx_zep *vif_ctx_zep = NULL;
	int ret = -1;
	struct nvlsi_wlan_fmac_callbk_fns callbk_fns;
	struct img_data_config_params data_config;
	struct rx_buf_pool_params rx_buf_pools[MAX_NUM_OF_RX_QUEUES];

	vif_ctx_zep = dev->data;

	if (rf_params) {
		if (strlen(rf_params) != (RF_PARAMS_SIZE * 2)) {
			printk("%s: Invalid length of rf_params. Should consist of %d hex characters\n",
			       __func__,
			       (RF_PARAMS_SIZE * 2));

			goto out;
		}
	}

	data_config.aggregation = aggregation;
	data_config.wmm = wmm;
	data_config.max_num_tx_agg_sessions = max_num_tx_agg_sessions;
	data_config.max_num_rx_agg_sessions = max_num_rx_agg_sessions;
	data_config.max_tx_aggregation = max_tx_aggregation;
	data_config.reorder_buf_size = reorder_buf_size;
	data_config.max_rxampdu_size = max_rxampdu_size;
	data_config.rate_protection_type = rate_protection_type;

	rx_buf_pools[0].num_bufs = rx1_num_bufs;
	rx_buf_pools[1].num_bufs = rx2_num_bufs;
	rx_buf_pools[2].num_bufs = rx3_num_bufs;
	rx_buf_pools[0].buf_sz = rx1_buf_sz;
	rx_buf_pools[1].buf_sz = rx2_buf_sz;
	rx_buf_pools[2].buf_sz = rx3_buf_sz;

	callbk_fns.if_state_chg_callbk_fn = NULL;
	callbk_fns.rx_frm_callbk_fn = NULL;
	callbk_fns.scan_start_callbk_fn = nvlsi_event_proc_scan_start_zep;
	callbk_fns.scan_done_callbk_fn = nvlsi_event_proc_scan_done_zep;
	callbk_fns.scan_res_callbk_fn = nvlsi_wpa_supp_event_proc_scan_res;
	callbk_fns.disp_scan_res_callbk_fn = nvlsi_event_proc_disp_scan_res_zep;
	callbk_fns.auth_resp_callbk_fn = nvlsi_wpa_supp_event_proc_auth_resp;
	callbk_fns.assoc_resp_callbk_fn = nvlsi_wpa_supp_event_proc_assoc_resp;
	callbk_fns.deauth_callbk_fn = nvlsi_wpa_supp_event_proc_deauth;
	callbk_fns.disassoc_callbk_fn = nvlsi_wpa_supp_event_proc_disassoc;

	rpu_drv_priv_zep.fmac_priv = nvlsi_wlan_fmac_init(&data_config,
							  rx_buf_pools,
							  &callbk_fns);

	if (rpu_drv_priv_zep.fmac_priv == NULL) {
		printk("%s: nvlsi_wlan_fmac_init failed\n", __func__);
		goto out;
	}

	status = nvlsi_wlan_fmac_dev_add_zep(&rpu_drv_priv_zep);

	if (status != NVLSI_RPU_STATUS_SUCCESS) {
		printk("%s: nvlsi_wlan_fmac_dev_add_zep failed\n", __func__);
		nvlsi_wlan_fmac_deinit(rpu_drv_priv_zep.fmac_priv);
		goto out;
	}

	status = nvlsi_wlan_fmac_def_vif_add_zep(&rpu_drv_priv_zep.rpu_ctx_zep,
						 0,
						 dev);

	if (status != NVLSI_RPU_STATUS_SUCCESS) {
		printk("%s: nvlsi_wlan_fmac_def_vif_add_zep failed\n", __func__);
		nvlsi_wlan_fmac_dev_rem_zep(&rpu_drv_priv_zep.rpu_ctx_zep);
		nvlsi_wlan_fmac_deinit(rpu_drv_priv_zep.fmac_priv);
		goto out;
	}

	status = nvlsi_wlan_fmac_dev_init_zep(&rpu_drv_priv_zep.rpu_ctx_zep);

	if (status != NVLSI_RPU_STATUS_SUCCESS) {
		printk("%s: nvlsi_wlan_fmac_dev_init_zep failed\n", __func__);
		nvlsi_wlan_fmac_def_vif_rem_zep(vif_ctx_zep);
		nvlsi_wlan_fmac_dev_rem_zep(vif_ctx_zep->rpu_ctx_zep);
		nvlsi_wlan_fmac_deinit(rpu_drv_priv_zep.fmac_priv);
		goto out;
	}

	/* Bring the default interface up in the firmware */
	status  = nvlsi_wlan_fmac_def_vif_state_chg(vif_ctx_zep,
						    1);

	if (status != NVLSI_RPU_STATUS_SUCCESS) {
		printk("%s: nvlsi_wlan_fmac_def_vif_state_chg failed\n",
		       __func__);
		nvlsi_wlan_fmac_dev_deinit_zep(vif_ctx_zep->rpu_ctx_zep);
		nvlsi_wlan_fmac_def_vif_rem_zep(vif_ctx_zep);
		nvlsi_wlan_fmac_dev_rem_zep(vif_ctx_zep->rpu_ctx_zep);
		nvlsi_wlan_fmac_deinit(rpu_drv_priv_zep.fmac_priv);
		goto out;
	}

	ret = 0;
out:
	return ret;
}


static const struct zep_wpa_supp_dev_ops nvlsi_dev_ops = {
	.if_api.iface_api.init = nvlsi_if_init,
	.if_api.get_capabilities = nvlsi_if_caps_get,
	.if_api.send = nvlsi_if_send,

	.off_api.disp_scan = nvlsi_disp_scan_zep,

	.init = nvlsi_wpa_supp_dev_init,
	.deinit = nvlsi_wpa_supp_dev_deinit,
	.scan2 = nvlsi_wpa_supp_scan2,
	.scan_abort = nvlsi_wpa_supp_scan_abort,
	.get_scan_results2 = nvlsi_wpa_supp_scan_results_get,
	.deauthenticate = nvlsi_wpa_supp_deauthenticate,
	.authenticate = nvlsi_wpa_supp_authenticate,
	.associate = nvlsi_wpa_supp_associate,
#ifdef notyet
	.set_key = nvlsi_wpa_supp_set_key,
#endif
};


ETH_NET_DEVICE_DT_INST_DEFINE(0, /* inst */
			      nvlsi_rpu_drv_main_zep, /* init_fn */
			      NULL, /* pm_action_cb */
			      &rpu_drv_priv_zep.rpu_ctx_zep.vif_ctx_zep[0], /* data */
			      NULL, /* cfg */
			      CONFIG_WIFI_INIT_PRIORITY, /* prio */
			      &nvlsi_dev_ops, /* api */
			      1500); /*mtu */
