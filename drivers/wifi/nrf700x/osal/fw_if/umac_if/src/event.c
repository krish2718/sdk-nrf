/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @brief File containing event specific definitions for the
 * FMAC IF Layer of the Wi-Fi driver.
 */

#include "host_rpu_umac_if.h"
#include "hal_mem.h"
#include "fmac_rx.h"
#include "fmac_tx.h"
#include "fmac_peer.h"
#include "fmac_cmd.h"
#include "fmac_ap.h"
#include "fmac_event.h"
#include <string.h>

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

#ifndef CONFIG_NRF700X_RADIO_TEST
static enum wifi_nrf_status umac_event_ctrl_process(struct wifi_nrf_fmac_dev_ctx *fmac_dev_ctx,
						    void *event_data,
						    unsigned int event_len)
{
	enum wifi_nrf_status status = WIFI_NRF_STATUS_SUCCESS;
	struct nrf_wifi_umac_hdr *umac_hdr = NULL;
	struct wifi_nrf_fmac_vif_ctx *vif_ctx = NULL;
	int i = 0;
	
	unsigned char if_id = 0;
	unsigned int event_num = 0;

	umac_hdr = event_data;
	if_id = umac_hdr->ids.wdev_id;
	event_num = umac_hdr->cmd_evnt;

	if (if_id >= MAX_NUM_VIFS) {
		wifi_nrf_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				      "%s: Invalid wdev_id recd from UMAC %d\n",
				      __func__,
				      if_id);

		goto out;
	}

	vif_ctx = fmac_dev_ctx->vif_ctx[if_id];
	if (!vif_ctx) {
		wifi_nrf_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				      "%s: Invalid vif_ctx for wdev_id %d\n",
				      __func__,
				      if_id);
		goto out;
	}

	for (i = 0; i < ARRAY_SIZE(umac_event_handlers); i++) {
		if (umac_event_handlers[i].event_id == event_num) {
			if (umac_event_handlers[i].handler) {
				umac_event_handlers[i].handler(vif_ctx, event_data, event_len);
			} else {
				wifi_nrf_osal_log_err(fmac_dev_ctx->fpriv->opriv,
							"%s: No handler for event %d for wdev_id %d\n",
							__func__,
							event_num,
							if_id);
				goto out;
			}
			break;
		}
	}

	if (i == ARRAY_SIZE(umac_event_handlers)) {
		wifi_nrf_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				      "%s: Unknown/unsupported event %d for wdev_id %d\n",
				      __func__,
				      event_num,
				      if_id);
		goto out;
	}


out:
	return status;
}

static enum wifi_nrf_status
wifi_nrf_fmac_data_event_process(struct wifi_nrf_fmac_dev_ctx *fmac_dev_ctx,
				 void *umac_head)
{
	enum wifi_nrf_status status = WIFI_NRF_STATUS_SUCCESS;
	int event;
	int incoming_event;

	if (!fmac_dev_ctx) {
		goto out;
	}

	if (!umac_head) {
		wifi_nrf_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				      "%s: Invalid parameters\n",
				      __func__);
		goto out;
	}

	incoming_event = ((struct nrf_wifi_umac_head *)umac_head)->event;

	// fmac_internal_data_event_handlers
	for (event = 0; event < ARRAY_SIZE(fmac_internal_data_event_handlers); event++) {
		if (fmac_internal_data_event_handlers[event].event_id == incoming_event) {
			if (fmac_internal_data_event_handlers[event].handler) {
				status = fmac_internal_data_event_handlers[event].handler(fmac_dev_ctx,
											 umac_head);
				if (status != WIFI_NRF_STATUS_SUCCESS) {
					wifi_nrf_osal_log_err(fmac_dev_ctx->fpriv->opriv,
								"%s: Failed to process event %d\n",
								__func__,
								incoming_event);
					goto out;
				}
			} else {
				wifi_nrf_osal_log_err(fmac_dev_ctx->fpriv->opriv,
							"%s: No handler for event %d\n",
							__func__,
							incoming_event);
				goto out;
			}
			break;
		}
	}
out:
	return status;
}


static enum wifi_nrf_status
wifi_nrf_fmac_data_events_process(struct wifi_nrf_fmac_dev_ctx *fmac_dev_ctx,
				  struct host_rpu_msg *rpu_msg)
{
	enum wifi_nrf_status status = WIFI_NRF_STATUS_FAIL;
	unsigned char *umac_head = NULL;
	int host_rpu_length_left = 0;

	if (!fmac_dev_ctx || !rpu_msg) {
		goto out;
	}

	umac_head = (unsigned char *)rpu_msg->msg;
	host_rpu_length_left = rpu_msg->hdr.len - sizeof(struct host_rpu_msg);

	while (host_rpu_length_left > 0) {
		status = wifi_nrf_fmac_data_event_process(fmac_dev_ctx,
							  umac_head);

		if (status != WIFI_NRF_STATUS_SUCCESS) {
			wifi_nrf_osal_log_err(fmac_dev_ctx->fpriv->opriv,
					      "%s: umac_process_data_event failed\n",
					      __func__);
			goto out;
		}

		host_rpu_length_left -= ((struct nrf_wifi_umac_head *)umac_head)->len;
		umac_head += ((struct nrf_wifi_umac_head *)umac_head)->len;
	}
out:
	return status;
}

#else /* CONFIG_NRF700X_RADIO_TEST */
static enum wifi_nrf_status umac_event_rf_test_process(struct wifi_nrf_fmac_dev_ctx *fmac_dev_ctx,
						       void *event)
{
	enum wifi_nrf_status status = WIFI_NRF_STATUS_FAIL;
	struct nrf_wifi_event_rftest *rf_test_event = NULL;
	struct nrf_wifi_temperature_params rf_test_get_temperature;
	struct nrf_wifi_rf_get_rf_rssi rf_get_rf_rssi;
	struct nrf_wifi_rf_test_xo_calib xo_calib_params;
	struct nrf_wifi_rf_get_xo_value rf_get_xo_value_params;

	if (!event) {
		wifi_nrf_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				      "%s: Invalid parameters\n",
				      __func__);
		goto out;
	}

	rf_test_event = ((struct nrf_wifi_event_rftest *)event);

	if (rf_test_event->rf_test_info.rfevent[0] != fmac_dev_ctx->rf_test_type) {
		wifi_nrf_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				      "%s: Invalid event type (%d) recd for RF test type (%d)\n",
				      __func__,
				      rf_test_event->rf_test_info.rfevent[0],
				      fmac_dev_ctx->rf_test_type);
		goto out;
	}

	switch (rf_test_event->rf_test_info.rfevent[0]) {
	case NRF_WIFI_RF_TEST_EVENT_RX_ADC_CAP:
	case NRF_WIFI_RF_TEST_EVENT_RX_STAT_PKT_CAP:
	case NRF_WIFI_RF_TEST_EVENT_RX_DYN_PKT_CAP:
		status = hal_rpu_mem_read(fmac_dev_ctx->hal_dev_ctx,
					  fmac_dev_ctx->rf_test_cap_data,
					  RPU_MEM_RF_TEST_CAP_BASE,
					  fmac_dev_ctx->rf_test_cap_sz);

		break;
	case NRF_WIFI_RF_TEST_EVENT_TX_TONE_START:
	case NRF_WIFI_RF_TEST_EVENT_DPD_ENABLE:
		break;

	case NRF_WIFI_RF_TEST_GET_TEMPERATURE:
		memcpy(&rf_test_get_temperature,
			(const unsigned char *)&rf_test_event->rf_test_info.rfevent[0],
			sizeof(rf_test_get_temperature));

		if (rf_test_get_temperature.readTemperatureStatus) {
			wifi_nrf_osal_log_err(fmac_dev_ctx->fpriv->opriv,
			"Temperature reading failed\n");
		} else {
			wifi_nrf_osal_log_err(fmac_dev_ctx->fpriv->opriv,
			"Temperature reading success: \t");

			wifi_nrf_osal_log_err(fmac_dev_ctx->fpriv->opriv,
			"The temperature is = %d degree celsius\n",
			rf_test_get_temperature.temperature);
		}
		break;
	case NRF_WIFI_RF_TEST_EVENT_RF_RSSI:
		memcpy(&rf_get_rf_rssi,
			(const unsigned char *)&rf_test_event->rf_test_info.rfevent[0],
			sizeof(rf_get_rf_rssi));

		wifi_nrf_osal_log_err(fmac_dev_ctx->fpriv->opriv,
		"RF RSSI value is = %d\n",
		rf_get_rf_rssi.agc_status_val);
		break;
	case NRF_WIFI_RF_TEST_EVENT_XO_CALIB:
		memcpy(&xo_calib_params,
			(const unsigned char *)&rf_test_event->rf_test_info.rfevent[0],
			sizeof(xo_calib_params));

		wifi_nrf_osal_log_err(fmac_dev_ctx->fpriv->opriv,
		"XO value configured is = %d\n",
		xo_calib_params.xo_val);
		break;
	case NRF_WIFI_RF_TEST_XO_TUNE:
		memcpy(&rf_get_xo_value_params,
			(const unsigned char *)&rf_test_event->rf_test_info.rfevent[0],
			sizeof(rf_get_xo_value_params));

		wifi_nrf_osal_log_err(fmac_dev_ctx->fpriv->opriv,
		"Best XO value is = %d\n",
		rf_get_xo_value_params.xo_value);
		break;
	default:
		break;
	}

	fmac_dev_ctx->rf_test_type = NRF_WIFI_RF_TEST_MAX;
	status = WIFI_NRF_STATUS_SUCCESS;

out:
	return status;
}


#endif /* !CONFIG_NRF700X_RADIO_TEST */


static enum wifi_nrf_status umac_event_stats_process(struct wifi_nrf_fmac_dev_ctx *fmac_dev_ctx,
						     void *event)
{
	enum wifi_nrf_status status = WIFI_NRF_STATUS_FAIL;
	struct nrf_wifi_umac_event_stats *stats = NULL;

	if (!event) {
		wifi_nrf_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				      "%s: Invalid parameters\n",
				      __func__);
		goto out;
	}

	if (!fmac_dev_ctx->stats_req) {
		wifi_nrf_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				      "%s: Stats recd when req was not sent!\n",
				      __func__);
		goto out;
	}

	stats = ((struct nrf_wifi_umac_event_stats *)event);

	wifi_nrf_osal_mem_cpy(fmac_dev_ctx->fpriv->opriv,
			      fmac_dev_ctx->fw_stats,
			      &stats->fw,
			      sizeof(*fmac_dev_ctx->fw_stats));

	fmac_dev_ctx->stats_req = false;

	status = WIFI_NRF_STATUS_SUCCESS;

out:
	return status;
}


static enum wifi_nrf_status umac_process_sys_events(struct wifi_nrf_fmac_dev_ctx *fmac_dev_ctx,
						    struct host_rpu_msg *rpu_msg)
{
	enum wifi_nrf_status status = WIFI_NRF_STATUS_FAIL;
	unsigned char *sys_head = NULL;

	sys_head = (unsigned char *)rpu_msg->msg;

	switch (((struct nrf_wifi_sys_head *)sys_head)->cmd_event) {
	case NRF_WIFI_EVENT_STATS:
		status = umac_event_stats_process(fmac_dev_ctx,
						  sys_head);
		break;
	case NRF_WIFI_EVENT_INIT_DONE:
		fmac_dev_ctx->fw_init_done = 1;
		status = WIFI_NRF_STATUS_SUCCESS;
		break;
	case NRF_WIFI_EVENT_DEINIT_DONE:
		fmac_dev_ctx->fw_deinit_done = 1;
		status = WIFI_NRF_STATUS_SUCCESS;
		break;
#ifdef CONFIG_NRF700X_RADIO_TEST
	case NRF_WIFI_EVENT_RF_TEST:
		status = umac_event_rf_test_process(fmac_dev_ctx,
						    sys_head);
		break;
	case NRF_WIFI_EVENT_RADIOCMD_STATUS:
		struct nrf_wifi_umac_event_err_status *umac_status =
			((struct nrf_wifi_umac_event_err_status *)sys_head);
		fmac_dev_ctx->radio_cmd_status = umac_status->status;
		fmac_dev_ctx->radio_cmd_done = true;
		status = WIFI_NRF_STATUS_SUCCESS;
		break;
#endif /* CONFIG_NRF700X_RADIO_TEST */
	default:
		wifi_nrf_osal_log_err(fmac_dev_ctx->fpriv->opriv,
				      "%s: Unknown event recd: %d\n",
				      __func__,
				      ((struct nrf_wifi_sys_head *)sys_head)->cmd_event);
		break;
	}

	return status;
}


enum wifi_nrf_status wifi_nrf_fmac_event_callback(void *mac_dev_ctx,
						  void *rpu_event_data,
						  unsigned int rpu_event_len)
{
	enum wifi_nrf_status status = WIFI_NRF_STATUS_FAIL;
	struct wifi_nrf_fmac_dev_ctx *fmac_dev_ctx = NULL;
	struct host_rpu_msg *rpu_msg = NULL;
	struct nrf_wifi_umac_hdr *umac_hdr = NULL;
	unsigned int umac_msg_len = 0;
	int umac_msg_type = NRF_WIFI_UMAC_EVENT_UNSPECIFIED;

	fmac_dev_ctx = (struct wifi_nrf_fmac_dev_ctx *)mac_dev_ctx;

	rpu_msg = (struct host_rpu_msg *)rpu_event_data;
	umac_hdr = (struct nrf_wifi_umac_hdr *)rpu_msg->msg;
	umac_msg_len = rpu_msg->hdr.len;
	umac_msg_type = umac_hdr->cmd_evnt;
	callbk_fns = &fmac_dev_ctx->fpriv->callbk_fns;

	switch (rpu_msg->type) {
#ifndef CONFIG_NRF700X_RADIO_TEST
	case NRF_WIFI_HOST_RPU_MSG_TYPE_DATA:
		status = wifi_nrf_fmac_data_events_process(fmac_dev_ctx,
							   rpu_msg);
		break;
	case NRF_WIFI_HOST_RPU_MSG_TYPE_UMAC:
		status = umac_event_ctrl_process(fmac_dev_ctx,
						 rpu_msg->msg,
						 rpu_msg->hdr.len);

		if (status != WIFI_NRF_STATUS_SUCCESS) {
			wifi_nrf_osal_log_err(fmac_dev_ctx->fpriv->opriv,
					      "%s: umac_event_ctrl_process failed\n",
					      __func__);
			goto out;
		}
		break;
#endif /* !CONFIG_NRF700X_RADIO_TEST */
	case NRF_WIFI_HOST_RPU_MSG_TYPE_SYSTEM:
		status = umac_process_sys_events(fmac_dev_ctx,
						 rpu_msg);
		break;
	default:
		goto out;
	}

out:
	return status;
}
