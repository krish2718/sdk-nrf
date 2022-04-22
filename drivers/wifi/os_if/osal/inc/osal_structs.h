/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#ifndef __OSAL_STRUCTS_H__
#define __OSAL_STRUCTS_H__

#include <stddef.h>

/**
 * enum wifi_nrf_status - The status of an operation performed by the
 *                       RPU driver.
 * @NVLSI_RPU_STATUS_SUCCESS: The operation was successful.
 * @NVLSI_RPU_STATUS_FAIL: The operation failed.
 *
 * This enum lists the possible outcomes of an operation performed by the
 * RPU driver.
 */
enum wifi_nrf_status {
	NVLSI_RPU_STATUS_SUCCESS,
	NVLSI_RPU_STATUS_FAIL = -1,
};


/**
 * enum wifi_nrf_osal_dma_dir - DMA direction for a DMA operation
 * @NVLSI_RPU_OSAL_DMA_DIR_TO_DEV: Data needs to be DMAed to the device.
 * @NVLSI_RPU_DMA_DIR_FROM_DEV: Data needs to be DMAed from the device.
 * @NVLSI_RPU_DMA_DIR_BIDI: Data can be DMAed in either direction i.e to
 *                        or from the device.
 *
 * This enum lists the possible directions for a DMA operation i.e whether the
 * DMA operation is for transferring data to or from a device
 */
enum wifi_nrf_osal_dma_dir {
	NVLSI_RPU_OSAL_DMA_DIR_TO_DEV,
	NVLSI_RPU_OSAL_DMA_DIR_FROM_DEV,
	NVLSI_RPU_OSAL_DMA_DIR_BIDI
};


struct wifi_nrf_osal_host_map {
	unsigned long addr;
	unsigned long size;
};


struct wifi_nrf_osal_priv {
	struct wifi_nrf_osal_ops *ops;
};
#endif /* __OSAL_STRUCTS_H__ */
