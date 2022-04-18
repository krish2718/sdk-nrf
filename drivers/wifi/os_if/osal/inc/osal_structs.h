/*
 * File Name: osal_structs.h
 *
 * Copyright (c) 2011-2020 Imagination Technologies Ltd.
 * All rights reserved
 *
 */
#ifndef __OSAL_STRUCTS_H__
#define __OSAL_STRUCTS_H__

#include <stddef.h>

/**
 * enum nvlsi_rpu_status - The status of an operation performed by the
 *                       Imagination RPU driver.
 * @NVLSI_RPU_STATUS_SUCCESS: The operation was successful.
 * @NVLSI_RPU_STATUS_FAIL: The operation failed.
 *
 * This enum lists the possible outcomes of an operation performed by the
 * Imagination RPU driver.
 */
enum nvlsi_rpu_status {
	NVLSI_RPU_STATUS_SUCCESS,
	NVLSI_RPU_STATUS_FAIL = -1,
};


/**
 * enum nvlsi_rpu_osal_dma_dir - DMA direction for a DMA operation
 * @NVLSI_RPU_OSAL_DMA_DIR_TO_DEV: Data needs to be DMAed to the device.
 * @NVLSI_RPU_DMA_DIR_FROM_DEV: Data needs to be DMAed from the device.
 * @NVLSI_RPU_DMA_DIR_BIDI: Data can be DMAed in either direction i.e to
 *                        or from the device.
 *
 * This enum lists the possible directions for a DMA operation i.e whether the
 * DMA operation is for transferring data to or from a device
 */
enum nvlsi_rpu_osal_dma_dir {
	NVLSI_RPU_OSAL_DMA_DIR_TO_DEV,
	NVLSI_RPU_OSAL_DMA_DIR_FROM_DEV,
	NVLSI_RPU_OSAL_DMA_DIR_BIDI
};


struct nvlsi_rpu_osal_host_map {
	unsigned long addr;
	unsigned long size;
};


struct nvlsi_rpu_osal_priv {
	struct nvlsi_rpu_osal_ops *ops;
};
#endif /* __OSAL_STRUCTS_H__ */
