/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @brief File containing QSPI Bus Layer specific function definitions of the
 * Wi-Fi driver.
 */

#include "bal_structs.h"
#include "qspi.h"
#include "pal.h"

#define NVLSI_WLAN_QSPI_DEV_NAME "wlan0"

int nvlsi_wlan_bus_qspi_irq_handler(void *data)
{
	struct nvlsi_wlan_bus_qspi_dev_ctx *dev_ctx = NULL;
	struct nvlsi_wlan_bus_qspi_priv *qspi_priv = NULL;
	int ret = 0;

	dev_ctx = (struct nvlsi_wlan_bus_qspi_dev_ctx *)data;
	qspi_priv = dev_ctx->qspi_priv;

	ret = qspi_priv->intr_callbk_fn(dev_ctx->bal_dev_ctx);

	return ret;
}


void *nvlsi_wlan_bus_qspi_dev_add(void *bus_priv,
				  void *bal_dev_ctx)
{
	enum nvlsi_rpu_status status = NVLSI_RPU_STATUS_FAIL;
	struct nvlsi_wlan_bus_qspi_priv *qspi_priv = NULL;
	struct nvlsi_wlan_bus_qspi_dev_ctx *qspi_dev_ctx = NULL;
	struct nvlsi_rpu_osal_host_map host_map;

	qspi_priv = bus_priv;

	qspi_dev_ctx = nvlsi_rpu_osal_mem_zalloc(qspi_priv->opriv,
						 sizeof(*qspi_dev_ctx));

	if (!qspi_dev_ctx) {
		nvlsi_rpu_osal_log_err(qspi_priv->opriv,
				       "%s: Unable to allocate qspi_dev_ctx\n", __func__);
		goto out;
	}

	qspi_dev_ctx->qspi_priv = qspi_priv;
	qspi_dev_ctx->bal_dev_ctx = bal_dev_ctx;

	qspi_dev_ctx->os_qspi_dev_ctx = nvlsi_rpu_osal_bus_qspi_dev_add(qspi_priv->opriv,
									qspi_priv->os_qspi_priv,
									qspi_dev_ctx);
	;

	if (!qspi_dev_ctx->os_qspi_dev_ctx) {
		nvlsi_rpu_osal_log_err(qspi_priv->opriv,
				       "%s: nvlsi_rpu_osal_bus_qspi_dev_add failed\n", __func__);

		nvlsi_rpu_osal_mem_free(qspi_priv->opriv,
					qspi_dev_ctx);

		qspi_dev_ctx = NULL;

		goto out;
	}

	nvlsi_rpu_osal_bus_qspi_dev_host_map_get(qspi_priv->opriv,
						 qspi_dev_ctx->os_qspi_dev_ctx,
						 &host_map);

	qspi_dev_ctx->host_addr_base = host_map.addr;

	qspi_dev_ctx->addr_pktram_base = qspi_dev_ctx->host_addr_base +
		qspi_priv->cfg_params.addr_pktram_base;

	status = nvlsi_rpu_osal_bus_qspi_dev_intr_reg(qspi_dev_ctx->qspi_priv->opriv,
						      qspi_dev_ctx->os_qspi_dev_ctx,
						      qspi_dev_ctx,
						      &nvlsi_wlan_bus_qspi_irq_handler);

	if (status != NVLSI_RPU_STATUS_SUCCESS) {
		nvlsi_rpu_osal_log_err(qspi_dev_ctx->qspi_priv->opriv,
				       "%s: Unable to register interrupt to the OS\n",
				       __func__);

		nvlsi_rpu_osal_bus_qspi_dev_intr_unreg(qspi_dev_ctx->qspi_priv->opriv,
						       qspi_dev_ctx->os_qspi_dev_ctx);

		nvlsi_rpu_osal_bus_qspi_dev_rem(qspi_dev_ctx->qspi_priv->opriv,
						qspi_dev_ctx->os_qspi_dev_ctx);

		nvlsi_rpu_osal_mem_free(qspi_priv->opriv,
					qspi_dev_ctx);

		qspi_dev_ctx = NULL;

		goto out;
	}

out:
	return qspi_dev_ctx;
}


void nvlsi_wlan_bus_qspi_dev_rem(void *bus_dev_ctx)
{
	struct nvlsi_wlan_bus_qspi_dev_ctx *qspi_dev_ctx = NULL;

	qspi_dev_ctx = bus_dev_ctx;

	nvlsi_rpu_osal_bus_qspi_dev_intr_unreg(qspi_dev_ctx->qspi_priv->opriv,
					       qspi_dev_ctx->os_qspi_dev_ctx);

	nvlsi_rpu_osal_mem_free(qspi_dev_ctx->qspi_priv->opriv,
				qspi_dev_ctx);
}


enum nvlsi_rpu_status nvlsi_wlan_bus_qspi_dev_init(void *bus_dev_ctx)
{
	enum nvlsi_rpu_status status = NVLSI_RPU_STATUS_FAIL;
	struct nvlsi_wlan_bus_qspi_dev_ctx *qspi_dev_ctx = NULL;

	qspi_dev_ctx = bus_dev_ctx;

	status = nvlsi_rpu_osal_bus_qspi_dev_init(qspi_dev_ctx->qspi_priv->opriv,
						  qspi_dev_ctx->os_qspi_dev_ctx);

	if (status != NVLSI_RPU_STATUS_SUCCESS) {
		nvlsi_rpu_osal_log_err(qspi_dev_ctx->qspi_priv->opriv,
				       "%s: nvlsi_rpu_osal_qspi_dev_init failed\n", __func__);

		goto out;
	}
out:
	return status;
}


void nvlsi_wlan_bus_qspi_dev_deinit(void *bus_dev_ctx)
{
	struct nvlsi_wlan_bus_qspi_dev_ctx *qspi_dev_ctx = NULL;

	qspi_dev_ctx = bus_dev_ctx;

	nvlsi_rpu_osal_bus_qspi_dev_deinit(qspi_dev_ctx->qspi_priv->opriv,
					   qspi_dev_ctx->os_qspi_dev_ctx);
}


void *nvlsi_wlan_bus_qspi_init(struct nvlsi_rpu_osal_priv *opriv,
			       void *params,
			       enum nvlsi_rpu_status (*intr_callbk_fn)(void *bal_dev_ctx))
{
	struct nvlsi_wlan_bus_qspi_priv *qspi_priv = NULL;

	qspi_priv = nvlsi_rpu_osal_mem_zalloc(opriv,
					      sizeof(*qspi_priv));

	if (!qspi_priv) {
		nvlsi_rpu_osal_log_err(opriv,
				       "%s: Unable to allocate memory for qspi_priv\n",
				       __func__);
		goto out;
	}

	qspi_priv->opriv = opriv;

	nvlsi_rpu_osal_mem_cpy(opriv,
			       &qspi_priv->cfg_params,
			       params,
			       sizeof(qspi_priv->cfg_params));

	qspi_priv->intr_callbk_fn = intr_callbk_fn;

	qspi_priv->os_qspi_priv = nvlsi_rpu_osal_bus_qspi_init(opriv);

	if (!qspi_priv->os_qspi_priv) {
		nvlsi_rpu_osal_log_err(opriv,
				       "%s: Unable to register QSPI driver\n",
				       __func__);

		nvlsi_rpu_osal_mem_free(opriv,
					qspi_priv);

		qspi_priv = NULL;

		goto out;
	}
out:
	return qspi_priv;
}


void nvlsi_wlan_bus_qspi_deinit(void *bus_priv)
{
	struct nvlsi_wlan_bus_qspi_priv *qspi_priv = NULL;

	qspi_priv = bus_priv;

	nvlsi_rpu_osal_bus_qspi_deinit(qspi_priv->opriv,
				       qspi_priv->os_qspi_priv);

	nvlsi_rpu_osal_mem_free(qspi_priv->opriv,
				qspi_priv);
}


unsigned int nvlsi_wlan_bus_qspi_read_word(void *dev_ctx,
					   unsigned long addr_offset)
{
	struct nvlsi_wlan_bus_qspi_dev_ctx *qspi_dev_ctx = NULL;
	unsigned int val = 0;

	qspi_dev_ctx = (struct nvlsi_wlan_bus_qspi_dev_ctx *)dev_ctx;

	val = nvlsi_rpu_osal_qspi_read_reg32(qspi_dev_ctx->qspi_priv->opriv,
					     qspi_dev_ctx->os_qspi_dev_ctx,
					     qspi_dev_ctx->host_addr_base + addr_offset);

	return val;
}


void nvlsi_wlan_bus_qspi_write_word(void *dev_ctx,
				    unsigned long addr_offset,
				    unsigned int val)
{
	struct nvlsi_wlan_bus_qspi_dev_ctx *qspi_dev_ctx = NULL;

	qspi_dev_ctx = (struct nvlsi_wlan_bus_qspi_dev_ctx *)dev_ctx;

	nvlsi_rpu_osal_qspi_write_reg32(qspi_dev_ctx->qspi_priv->opriv,
					qspi_dev_ctx->os_qspi_dev_ctx,
					qspi_dev_ctx->host_addr_base + addr_offset,
					val);
}


void nvlsi_wlan_bus_qspi_read_block(void *dev_ctx,
				    void *dest_addr,
				    unsigned long src_addr_offset,
				    size_t len)
{
	struct nvlsi_wlan_bus_qspi_dev_ctx *qspi_dev_ctx = NULL;

	qspi_dev_ctx = (struct nvlsi_wlan_bus_qspi_dev_ctx *)dev_ctx;

	nvlsi_rpu_osal_qspi_cpy_from(qspi_dev_ctx->qspi_priv->opriv,
				     qspi_dev_ctx->os_qspi_dev_ctx,
				     dest_addr,
				     qspi_dev_ctx->host_addr_base + src_addr_offset,
				     len);
}


void nvlsi_wlan_bus_qspi_write_block(void *dev_ctx,
				     unsigned long dest_addr_offset,
				     const void *src_addr,
				     size_t len)
{
	struct nvlsi_wlan_bus_qspi_dev_ctx *qspi_dev_ctx = NULL;

	qspi_dev_ctx = (struct nvlsi_wlan_bus_qspi_dev_ctx *)dev_ctx;

	nvlsi_rpu_osal_qspi_cpy_to(qspi_dev_ctx->qspi_priv->opriv,
				   qspi_dev_ctx->os_qspi_dev_ctx,
				   qspi_dev_ctx->host_addr_base + dest_addr_offset,
				   src_addr,
				   len);
}


unsigned long nvlsi_wlan_bus_qspi_dma_map(void *dev_ctx,
					  unsigned long virt_addr,
					  size_t len,
					  enum nvlsi_rpu_osal_dma_dir dma_dir)
{
	struct nvlsi_wlan_bus_qspi_dev_ctx *qspi_dev_ctx = NULL;
	unsigned long phy_addr = 0;

	qspi_dev_ctx = (struct nvlsi_wlan_bus_qspi_dev_ctx *)dev_ctx;

	phy_addr = qspi_dev_ctx->host_addr_base + (virt_addr - qspi_dev_ctx->addr_pktram_base);

	return phy_addr;
}


unsigned long nvlsi_wlan_bus_qspi_dma_unmap(void *dev_ctx,
					    unsigned long phy_addr,
					    size_t len,
					    enum nvlsi_rpu_osal_dma_dir dma_dir)
{
	struct nvlsi_wlan_bus_qspi_dev_ctx *qspi_dev_ctx = NULL;
	unsigned long virt_addr = 0;

	qspi_dev_ctx = (struct nvlsi_wlan_bus_qspi_dev_ctx *)dev_ctx;

	virt_addr = qspi_dev_ctx->addr_pktram_base + (phy_addr - qspi_dev_ctx->host_addr_base);

	return virt_addr;
}


struct nvlsi_rpu_bal_ops nvlsi_wlan_bus_qspi_ops = {
	.init = &nvlsi_wlan_bus_qspi_init,
	.deinit = &nvlsi_wlan_bus_qspi_deinit,
	.dev_add = &nvlsi_wlan_bus_qspi_dev_add,
	.dev_rem = &nvlsi_wlan_bus_qspi_dev_rem,
	.dev_init = &nvlsi_wlan_bus_qspi_dev_init,
	.dev_deinit = &nvlsi_wlan_bus_qspi_dev_deinit,
	.read_word = &nvlsi_wlan_bus_qspi_read_word,
	.write_word = &nvlsi_wlan_bus_qspi_write_word,
	.read_block = &nvlsi_wlan_bus_qspi_read_block,
	.write_block = &nvlsi_wlan_bus_qspi_write_block,
	.dma_map = &nvlsi_wlan_bus_qspi_dma_map,
	.dma_unmap = &nvlsi_wlan_bus_qspi_dma_unmap,
};


struct nvlsi_rpu_bal_ops *get_bus_ops(void)
{
	return &nvlsi_wlan_bus_qspi_ops;
}
