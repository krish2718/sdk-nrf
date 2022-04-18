/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @brief Header containing display scan specific declarations for the
 * Zephyr OS layer of the Wi-Fi driver.
 */

#ifndef __ZEPHYR_DISP_SCAN_H__
#define __ZEPHYR_DISP_SCAN_H__

#include <device.h>
#include <net/wifi_mgmt.h>
#include "osal_api.h"

int nvlsi_disp_scan_zep(const struct device *dev,
			scan_result_cb_t cb);

enum nvlsi_rpu_status nvlsi_disp_scan_res_get_zep(struct nvlsi_rpu_vif_ctx_zep *vif_ctx_zep);

void nvlsi_event_proc_disp_scan_res_zep(void *vif_ctx,
					struct img_umac_event_new_scan_display_results *scan_res,
					bool is_last);
#endif /*  __ZEPHYR_DISP_SCAN_H__ */
