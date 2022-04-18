/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @brief Header containing register read/write specific declarations for the
 * HAL Layer of the Wi-Fi driver.
 */


/*
 * File Name: hal_reg.h
 *
 * HAL APIs to read/write from/to RPU registers.
 *
 * Copyright (c) 2011-2020 Imagination Technologies Ltd.
 * All rights reserved.
 *
 */
#ifndef __HAL_REG_H__
#define __HAL_REG_H__

#include "hal_structs.h"

/**
 * hal_rpu_reg_read() - Read from a RPU register.
 * @hal_ctx: Pointer to HAL context.
 * @val: Pointer to the host memory where the value read from the RPU register
 *       is to be copied.
 * @rpu_reg_addr: Absolute value of RPU register address from which the
 *                value is to be read.
 *
 * This function reads a 4 byte value from a RPU register and copies it
 * to the host memory pointed to by the val parameter.
 *
 * Return: Status
 *		Pass : NVLSI_RPU_STATUS_SUCCESS
 *		Error: NVLSI_RPU_STATUS_FAIL
 */
enum nvlsi_rpu_status hal_rpu_reg_read(struct nvlsi_rpu_hal_dev_ctx *hal_ctx,
				       unsigned int *val,
				       unsigned int rpu_reg_addr);

/**
 * hal_rpu_reg_write() - Write to a RPU register.
 * @hal_ctx: Pointer to HAL context.
 * @rpu_reg_addr: Absolute value of RPU register address to which the
 *                value is to be written.
 * @val: The value which is to be written to the RPU register.
 *
 * This function writes a 4 byte value to a RPU register.
 *
 * Return: Status
 *		Pass : NVLSI_RPU_STATUS_SUCCESS
 *		Error: NVLSI_RPU_STATUS_FAIL
 */
enum nvlsi_rpu_status hal_rpu_reg_write(struct nvlsi_rpu_hal_dev_ctx *hal_ctx,
					unsigned int rpu_reg_addr,
					unsigned int val);
#endif /* __HAL_REG_H__ */
