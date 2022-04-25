/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @brief File containing command specific definitions for the
 * FMAC IF Layer of the Wi-Fi driver.
 */

#include "host_rpu_umac_if.h"
#include "hal_api.h"
#include "fmac_structs.h"

struct host_rpu_msg *umac_cmd_alloc(struct wifi_nrf_fmac_dev_ctx *fmac_dev_ctx,
				    int type,
				    int len)
{
	struct host_rpu_msg *umac_cmd = NULL;

	umac_cmd = wifi_nrf_osal_mem_zalloc(fmac_dev_ctx->fpriv->opriv,
					     sizeof(*umac_cmd) + len);

	if (!umac_cmd) {
		wifi_nrf_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				       "%s: Failed to allocate UMAC cmd\n",
				       __func__);
		goto out;
	}

	umac_cmd->type = type;
	umac_cmd->hdr.len = sizeof(*umac_cmd) + len;

out:
	return umac_cmd;
}


enum wifi_nrf_status umac_cmd_cfg(struct wifi_nrf_fmac_dev_ctx *fmac_dev_ctx,
				   void *params,
				   int len)
{
	enum wifi_nrf_status status = NVLSI_RPU_STATUS_FAIL;
	struct host_rpu_msg *umac_cmd = NULL;

	if (!fmac_dev_ctx->init_done) {
		struct img_umac_hdr *umac_hdr = NULL;

		umac_hdr = (struct img_umac_hdr *)params;
		wifi_nrf_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				       "%s: UMAC buff config not yet done(%d)\n",
				       __func__,
				       umac_hdr->cmd_evnt);
		goto out;
	}

	umac_cmd = umac_cmd_alloc(fmac_dev_ctx,
				  IMG_HOST_RPU_MSG_TYPE_UMAC,
				  len);

	if (!umac_cmd) {
		wifi_nrf_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				       "%s: umac_cmd_alloc failed\n",
				       __func__);
		goto out;
	}

	wifi_nrf_osal_mem_cpy(fmac_dev_ctx->fpriv->opriv,
			       umac_cmd->msg,
			       params,
			       len);

	status = wifi_nrf_hal_ctrl_cmd_send(fmac_dev_ctx->hal_dev_ctx,
					     umac_cmd,
					     (sizeof(*umac_cmd) + len));

out:
	return status;
}


enum wifi_nrf_status umac_cmd_init(struct wifi_nrf_fmac_dev_ctx *fmac_dev_ctx,
				    unsigned char *def_mac_addr,
				    unsigned char def_vif_idx,
				    unsigned char *rf_params,
				    bool rf_params_valid,
				    struct img_data_config_params config,
				    unsigned int phy_calib)
{
	enum wifi_nrf_status status = NVLSI_RPU_STATUS_FAIL;
	struct host_rpu_msg *umac_cmd = NULL;
	struct img_cmd_sys_init *umac_cmd_data = NULL;
	unsigned int len = 0;

	len = sizeof(*umac_cmd_data);

	umac_cmd = umac_cmd_alloc(fmac_dev_ctx,
				  IMG_HOST_RPU_MSG_TYPE_SYSTEM,
				  len);

	if (!umac_cmd) {
		wifi_nrf_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				       "%s: umac_cmd_alloc failed\n",
				       __func__);
		goto out;
	}

	umac_cmd_data = (struct img_cmd_sys_init *)(umac_cmd->msg);

	umac_cmd_data->sys_head.cmd_event = IMG_CMD_INIT;
	umac_cmd_data->sys_head.len = len;

	wifi_nrf_osal_mem_cpy(fmac_dev_ctx->fpriv->opriv,
			       umac_cmd_data->sys_params.mac_addr,
			       def_mac_addr,
			       IMG_ETH_ADDR_LEN);

	umac_cmd_data->wdev_id = def_vif_idx;
	umac_cmd_data->sys_params.rf_params_valid = rf_params_valid;

	if (rf_params_valid)
		wifi_nrf_osal_mem_cpy(fmac_dev_ctx->fpriv->opriv,
				       umac_cmd_data->sys_params.rf_params,
				       rf_params,
				       RF_PARAMS_SIZE);

	umac_cmd_data->sys_params.phy_calib = phy_calib;
	umac_cmd_data->sys_params.hw_bringup_time = HW_DELAY;
	umac_cmd_data->sys_params.sw_bringup_time = SW_DELAY;
	umac_cmd_data->sys_params.bcn_time_out = BCN_TIMEOUT;
	umac_cmd_data->sys_params.calibSleepClk = CALIB_SLEEP_CLOCK_ENABLE;

	wifi_nrf_osal_mem_cpy(fmac_dev_ctx->fpriv->opriv,
			       umac_cmd_data->rx_buf_pools,
			       fmac_dev_ctx->fpriv->rx_buf_pools,
			       sizeof(umac_cmd_data->rx_buf_pools));

	umac_cmd_data->data_config_params = config;
	umac_cmd_data->temp_vbat_config_params.temp_based_calib_en = TEMP_CALIB_ENABLE;
	umac_cmd_data->temp_vbat_config_params.temp_calib_bitmap = DEF_PHY_TEMP_CALIB;
	umac_cmd_data->temp_vbat_config_params.vbat_calibp_bitmap = DEF_PHY_VBAT_CALIB;
	umac_cmd_data->temp_vbat_config_params.temp_vbat_mon_period = TEMP_CALIB_PERIOD;
	umac_cmd_data->temp_vbat_config_params.VthLow = VBAT_LOW;
	umac_cmd_data->temp_vbat_config_params.VthHi = VBAT_HIGH;
	umac_cmd_data->temp_vbat_config_params.temp_threshold = TEMP_CALIB_THRESHOLD;

	status = wifi_nrf_hal_ctrl_cmd_send(fmac_dev_ctx->hal_dev_ctx,
					     umac_cmd,
					     (sizeof(*umac_cmd) + len));

out:
	return status;
}


enum wifi_nrf_status umac_cmd_deinit(struct wifi_nrf_fmac_dev_ctx *fmac_dev_ctx)
{
	enum wifi_nrf_status status = NVLSI_RPU_STATUS_FAIL;
	struct host_rpu_msg *umac_cmd = NULL;
	struct img_cmd_sys_deinit *umac_cmd_data = NULL;
	unsigned int len = 0;

	len = sizeof(*umac_cmd_data);
	umac_cmd = umac_cmd_alloc(fmac_dev_ctx,
				  IMG_HOST_RPU_MSG_TYPE_SYSTEM,
				  len);
	if (!umac_cmd) {
		wifi_nrf_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				       "%s: umac_cmd_alloc failed\n",
				       __func__);
		goto out;
	}
	umac_cmd_data = (struct img_cmd_sys_deinit *)(umac_cmd->msg);
	umac_cmd_data->sys_head.cmd_event = IMG_CMD_DEINIT;
	umac_cmd_data->sys_head.len = len;
	status = wifi_nrf_hal_ctrl_cmd_send(fmac_dev_ctx->hal_dev_ctx,
					     umac_cmd,
					     (sizeof(*umac_cmd) + len));
out:
	return status;
}


enum wifi_nrf_status umac_cmd_btcoex(struct wifi_nrf_fmac_dev_ctx *fmac_dev_ctx,
				      struct rpu_btcoex *params)
{
	enum wifi_nrf_status status = NVLSI_RPU_STATUS_FAIL;
	struct host_rpu_msg *umac_cmd = NULL;
	struct img_cmd_btcoex *umac_cmd_data = NULL;
	int len = 0;

	len = sizeof(*umac_cmd_data);

	umac_cmd = umac_cmd_alloc(fmac_dev_ctx,
				  IMG_HOST_RPU_MSG_TYPE_SYSTEM,
				  len);

	if (!umac_cmd) {
		wifi_nrf_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				       "%s: umac_cmd_alloc failed\n",
				       __func__);
		goto out;
	}

	umac_cmd_data = (struct img_cmd_btcoex *)(umac_cmd->msg);

	umac_cmd_data->sys_head.cmd_event = IMG_CMD_BTCOEX;
	umac_cmd_data->sys_head.len = len;

	wifi_nrf_osal_mem_cpy(fmac_dev_ctx->fpriv->opriv,
			       &umac_cmd_data->conf,
			       params,
			       sizeof(umac_cmd_data->conf));

	status = wifi_nrf_hal_ctrl_cmd_send(fmac_dev_ctx->hal_dev_ctx,
					     umac_cmd,
					     (sizeof(*umac_cmd) + len));
out:
	return status;


}


enum wifi_nrf_status umac_cmd_prog_stats_get(struct wifi_nrf_fmac_dev_ctx *fmac_dev_ctx,
					      enum rpu_stats_type stats_type)
{
	enum wifi_nrf_status status = NVLSI_RPU_STATUS_FAIL;
	struct host_rpu_msg *umac_cmd = NULL;
	struct img_cmd_get_stats *umac_cmd_data = NULL;
	int len = 0;

	len = sizeof(*umac_cmd_data);

	umac_cmd = umac_cmd_alloc(fmac_dev_ctx,
				  IMG_HOST_RPU_MSG_TYPE_SYSTEM,
				  len);

	if (!umac_cmd) {
		wifi_nrf_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				       "%s: umac_cmd_alloc failed\n",
				       __func__);
		goto out;
	}

	umac_cmd_data = (struct img_cmd_get_stats *)(umac_cmd->msg);

	umac_cmd_data->sys_head.cmd_event = IMG_CMD_GET_STATS;
	umac_cmd_data->sys_head.len = len;
	umac_cmd_data->stats_type = stats_type;

	status = wifi_nrf_hal_ctrl_cmd_send(fmac_dev_ctx->hal_dev_ctx,
					     umac_cmd,
					     (sizeof(*umac_cmd) + len));

out:
	return status;
}
