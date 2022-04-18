/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @brief API definitions for the Bus Abstraction Layer (BAL) of the Wi-Fi
 * driver.
 */

#include "bal_api.h"

struct nvlsi_rpu_bal_dev_ctx *nvlsi_rpu_bal_dev_add(struct nvlsi_rpu_bal_priv *bpriv,
						    void *hal_dev_ctx)
{
	enum nvlsi_rpu_status status = NVLSI_RPU_STATUS_FAIL;
	struct nvlsi_rpu_bal_dev_ctx *bal_dev_ctx = NULL;

	bal_dev_ctx = nvlsi_rpu_osal_mem_zalloc(bpriv->opriv,
						sizeof(*bal_dev_ctx));

	if (!bal_dev_ctx) {
		nvlsi_rpu_osal_log_err(bpriv->opriv,
				       "%s: Unable to allocate bal_dev_ctx\n", __func__);
		goto out;
	}

	bal_dev_ctx->bpriv = bpriv;
	bal_dev_ctx->hal_dev_ctx = hal_dev_ctx;

	bal_dev_ctx->bus_dev_ctx = bpriv->ops->dev_add(bpriv->bus_priv,
						       bal_dev_ctx);

	if (!bal_dev_ctx->bus_dev_ctx) {
		nvlsi_rpu_osal_log_err(bpriv->opriv,
				       "%s: Bus dev_add failed\n", __func__);
		goto out;
	}

	status = NVLSI_RPU_STATUS_SUCCESS;
out:
	if (status != NVLSI_RPU_STATUS_SUCCESS) {
		if (bal_dev_ctx) {
			nvlsi_rpu_osal_mem_free(bpriv->opriv,
						bal_dev_ctx);
			bal_dev_ctx = NULL;
		}
	}

	return bal_dev_ctx;
}


void nvlsi_rpu_bal_dev_rem(struct nvlsi_rpu_bal_dev_ctx *bal_dev_ctx)
{
	bal_dev_ctx->bpriv->ops->dev_rem(bal_dev_ctx->bus_dev_ctx);

	nvlsi_rpu_osal_mem_free(bal_dev_ctx->bpriv->opriv,
				bal_dev_ctx);
}


enum nvlsi_rpu_status nvlsi_rpu_bal_dev_init(struct nvlsi_rpu_bal_dev_ctx *bal_dev_ctx)
{
	enum nvlsi_rpu_status status = NVLSI_RPU_STATUS_FAIL;

	status = bal_dev_ctx->bpriv->ops->dev_init(bal_dev_ctx->bus_dev_ctx);

	if (status != NVLSI_RPU_STATUS_SUCCESS) {
		nvlsi_rpu_osal_log_err(bal_dev_ctx->bpriv->opriv,
				       "%s: dev_init failed\n", __func__);
		goto out;
	}
out:
	return status;
}


void nvlsi_rpu_bal_dev_deinit(struct nvlsi_rpu_bal_dev_ctx *bal_dev_ctx)
{
	bal_dev_ctx->bpriv->ops->dev_deinit(bal_dev_ctx->bus_dev_ctx);
}


static enum nvlsi_rpu_status nvlsi_rpu_bal_isr(void *ctx)
{
	struct nvlsi_rpu_bal_dev_ctx *bal_dev_ctx = NULL;
	enum nvlsi_rpu_status status = NVLSI_RPU_STATUS_FAIL;

	bal_dev_ctx = (struct nvlsi_rpu_bal_dev_ctx *)ctx;

	status = bal_dev_ctx->bpriv->intr_callbk_fn(bal_dev_ctx->hal_dev_ctx);

	return status;
}


struct nvlsi_rpu_bal_priv *nvlsi_rpu_bal_init(struct nvlsi_rpu_osal_priv *opriv,
					      struct nvlsi_rpu_bal_cfg_params *cfg_params,
					      enum nvlsi_rpu_status (*intr_callbk_fn)(void *hal_dev_ctx))
{
	struct nvlsi_rpu_bal_priv *bpriv = NULL;

	bpriv = nvlsi_rpu_osal_mem_zalloc(opriv,
					  sizeof(*bpriv));

	if (!bpriv) {
		nvlsi_rpu_osal_log_err(opriv,
				       "%s: Unable to allocate memory for bpriv\n", __func__);
		goto out;
	}

	bpriv->opriv = opriv;

	bpriv->intr_callbk_fn = intr_callbk_fn;

	bpriv->ops = get_bus_ops();

	bpriv->bus_priv = bpriv->ops->init(opriv,
					   cfg_params,
					   &nvlsi_rpu_bal_isr);

	if (!bpriv->bus_priv) {
		nvlsi_rpu_osal_log_err(opriv,
				       "%s: Failed\n", __func__);
		nvlsi_rpu_osal_mem_free(opriv,
					bpriv);
		bpriv = NULL;
	}

out:
	return bpriv;
}


void nvlsi_rpu_bal_deinit(struct nvlsi_rpu_bal_priv *bpriv)
{
	bpriv->ops->deinit(bpriv->bus_priv);

	nvlsi_rpu_osal_mem_free(bpriv->opriv,
				bpriv);
}


unsigned int nvlsi_rpu_bal_read_word(void *ctx,
				     unsigned long addr_offset)
{
	struct nvlsi_rpu_bal_dev_ctx *bal_dev_ctx = NULL;
	unsigned int val = 0;

	bal_dev_ctx = (struct nvlsi_rpu_bal_dev_ctx *)ctx;

	val = bal_dev_ctx->bpriv->ops->read_word(bal_dev_ctx->bus_dev_ctx,
						 addr_offset);

	return val;
}


void nvlsi_rpu_bal_write_word(void *ctx,
			      unsigned long addr_offset,
			      unsigned int val)
{
	struct nvlsi_rpu_bal_dev_ctx *bal_dev_ctx = NULL;

	bal_dev_ctx = (struct nvlsi_rpu_bal_dev_ctx *)ctx;

	bal_dev_ctx->bpriv->ops->write_word(bal_dev_ctx->bus_dev_ctx,
					    addr_offset,
					    val);
}


void nvlsi_rpu_bal_read_block(void *ctx,
			      void *dest_addr,
			      unsigned long src_addr_offset,
			      size_t len)
{
	struct nvlsi_rpu_bal_dev_ctx *bal_dev_ctx = NULL;

	bal_dev_ctx = (struct nvlsi_rpu_bal_dev_ctx *)ctx;

	bal_dev_ctx->bpriv->ops->read_block(bal_dev_ctx->bus_dev_ctx,
					    dest_addr,
					    src_addr_offset,
					    len);
}


void nvlsi_rpu_bal_write_block(void *ctx,
			       unsigned long dest_addr_offset,
			       const void *src_addr,
			       size_t len)
{
	struct nvlsi_rpu_bal_dev_ctx *bal_dev_ctx = NULL;

	bal_dev_ctx = (struct nvlsi_rpu_bal_dev_ctx *)ctx;

	bal_dev_ctx->bpriv->ops->write_block(bal_dev_ctx->bus_dev_ctx,
					     dest_addr_offset,
					     src_addr,
					     len);
}


unsigned long nvlsi_rpu_bal_dma_map(void *ctx,
				    unsigned long virt_addr,
				    size_t len,
				    enum nvlsi_rpu_osal_dma_dir dma_dir)
{
	struct nvlsi_rpu_bal_dev_ctx *bal_dev_ctx = NULL;
	unsigned long phy_addr = 0;

	bal_dev_ctx = (struct nvlsi_rpu_bal_dev_ctx *)ctx;

	phy_addr = bal_dev_ctx->bpriv->ops->dma_map(bal_dev_ctx->bus_dev_ctx,
						    virt_addr,
						    len,
						    dma_dir);

	return phy_addr;
}


unsigned long nvlsi_rpu_bal_dma_unmap(void *ctx,
				      unsigned long phy_addr,
				      size_t len,
				      enum nvlsi_rpu_osal_dma_dir dma_dir)
{
	struct nvlsi_rpu_bal_dev_ctx *bal_dev_ctx = NULL;
	unsigned long virt_addr = 0;

	bal_dev_ctx = (struct nvlsi_rpu_bal_dev_ctx *)ctx;

	virt_addr = bal_dev_ctx->bpriv->ops->dma_unmap(bal_dev_ctx->bus_dev_ctx,
						       phy_addr,
						       len,
						       dma_dir);

	return virt_addr;
}
