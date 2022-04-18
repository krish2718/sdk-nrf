/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @brief Header containing patch loader specific declarations for the
 * HAL Layer of the Wi-Fi driver.
 */

#ifndef __HAL_FW_PATCH_LOADER_H__
#define __HAL_FW_PATCH_LOADER_H__

#include "hal_structs.h"

enum nvlsi_wlan_fw_patch_type {
	NVLSI_WLAN_FW_PATCH_TYPE_PRI,
	NVLSI_WLAN_FW_PATCH_TYPE_SEC,
	NVLSI_WLAN_FW_PATCH_TYPE_MAX
};


/*
 * Downloads a firmware patch into RPU memory.
 */
enum nvlsi_rpu_status nvlsi_rpu_hal_fw_patch_load(struct nvlsi_rpu_hal_dev_ctx *hal_dev_ctx,
						  enum RPU_PROC_TYPE rpu_proc,
						  void *fw_pri_patch_data,
						  unsigned int fw_pri_patch_size,
						  void *fw_sec_patch_data,
						  unsigned int fw_sec_patch_size);

enum nvlsi_rpu_status nvlsi_rpu_hal_fw_patch_boot(struct nvlsi_rpu_hal_dev_ctx *hal_dev_ctx,
						  enum RPU_PROC_TYPE rpu_proc,
						  bool is_patch_present);
#endif /* __HAL_FW_PATCH_LOADER_H__ */
