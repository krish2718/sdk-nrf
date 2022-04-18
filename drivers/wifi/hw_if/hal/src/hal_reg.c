/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @brief File containing register read/write specific definitions for the
 * HAL Layer of the Wi-Fi driver.
 */

#include "pal.h"
#include "hal_api.h"
#include "hal_common.h"


static bool hal_rpu_is_reg(unsigned int addr_val)
{
	unsigned int addr_base = (addr_val & RPU_ADDR_MASK_BASE);

	if ((addr_base == RPU_ADDR_SBUS_START) ||
	    (addr_base == RPU_ADDR_PBUS_START))
		return true;
	else
		return false;
}


enum nvlsi_rpu_status hal_rpu_reg_read(struct nvlsi_rpu_hal_dev_ctx *hal_dev_ctx,
				       unsigned int *val,
				       unsigned int rpu_reg_addr)
{
	enum nvlsi_rpu_status status = NVLSI_RPU_STATUS_FAIL;
	unsigned long addr_offset = 0;

	if (!hal_dev_ctx)
		return status;

	if ((val == NULL) ||
	    !hal_rpu_is_reg(rpu_reg_addr)) {
		nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
				       "%s: Invalid params, val = %p, rpu_reg (0x%x)\n",
				       __func__,
				       val,
				       rpu_reg_addr);
		return status;
	}

	status = pal_rpu_addr_offset_get(hal_dev_ctx->hpriv->opriv,
					 rpu_reg_addr,
					 &addr_offset);

	if (status != NVLSI_RPU_STATUS_SUCCESS) {
		nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
				       "%s: pal_rpu_addr_offset_get failed\n",
				       __func__);
		return status;
	}

	*val = nvlsi_rpu_bal_read_word(hal_dev_ctx->bal_dev_ctx,
				       addr_offset);

	if (*val == 0xFFFFFFFF) {
		nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
				       "%s: Error !! Value read at addr_offset = %x is = %X\n",
				       __func__,
				       addr_offset,
				       *val);
		status = NVLSI_RPU_STATUS_FAIL;
		goto out;
	}

	status = NVLSI_RPU_STATUS_SUCCESS;
out:
	return status;
}

enum nvlsi_rpu_status hal_rpu_reg_write(struct nvlsi_rpu_hal_dev_ctx *hal_dev_ctx,
					unsigned int rpu_reg_addr,
					unsigned int val)
{
	enum nvlsi_rpu_status status = NVLSI_RPU_STATUS_FAIL;
	unsigned long addr_offset = 0;

	if (!hal_dev_ctx)
		return status;

	if (!hal_rpu_is_reg(rpu_reg_addr)) {
		nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
				       "%s: Invalid params, rpu_reg_addr (0x%X)\n",
				       __func__,
				       rpu_reg_addr);
		return status;
	}

	status = pal_rpu_addr_offset_get(hal_dev_ctx->hpriv->opriv,
					 rpu_reg_addr,
					 &addr_offset);

	if (status != NVLSI_RPU_STATUS_SUCCESS) {
		nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
				       "%s: pal_rpu_get_region_offset failed\n",
				       __func__);
		return status;
	}

	nvlsi_rpu_bal_write_word(hal_dev_ctx->bal_dev_ctx,
				 addr_offset,
				 val);

	status = NVLSI_RPU_STATUS_SUCCESS;

	return status;
}
