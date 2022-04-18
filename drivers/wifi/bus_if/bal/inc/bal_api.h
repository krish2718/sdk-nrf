/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @brief Header containing the API declarations for the Bus Abstraction Layer
 * (BAL) of the Wi-Fi driver.
 */

#ifndef __BAL_API_H__
#define __BAL_API_H__

#include "osal_api.h"
#include "bal_structs.h"

/**
 * nvlsi_rpu_bal_init() - Initialize the BAL layer.
 *
 * @intr_callbk_fn: Pointer to the callback function which the user of this
 *                  layer needs to implement to handle interrupts from the
 *                  RPU.
 *
 * This API is used to initialize the BAL layer and is expected to be called
 * before using the BAL layer. This API returns a pointer to the BAL context
 * which might need to be passed to further API calls.
 *
 * Returns: Pointer to instance of BAL layer context.
 */
struct nvlsi_rpu_bal_priv *nvlsi_rpu_bal_init(struct nvlsi_rpu_osal_priv *opriv,
					      struct nvlsi_rpu_bal_cfg_params *cfg_params,
					      enum nvlsi_rpu_status (*intr_callbk_fn)(void *hal_ctx));


/**
 * nvlsi_rpu_bal_deinit() - Deinitialize the BAL layer.
 * @bpriv: Pointer to the BAL layer context returned by the
 *         @nvlsi_rpu_bal_init API.
 *
 * This API is used to deinitialize the BAL layer and is expected to be called
 * after done using the BAL layer.
 *
 * Returns: None.
 */
void nvlsi_rpu_bal_deinit(struct nvlsi_rpu_bal_priv *bpriv);


struct nvlsi_rpu_bal_dev_ctx *nvlsi_rpu_bal_dev_add(struct nvlsi_rpu_bal_priv *bpriv,
						    void *hal_dev_ctx);

void nvlsi_rpu_bal_dev_rem(struct nvlsi_rpu_bal_dev_ctx *bal_dev_ctx);

enum nvlsi_rpu_status nvlsi_rpu_bal_dev_init(struct nvlsi_rpu_bal_dev_ctx *bal_dev_ctx);

void nvlsi_rpu_bal_dev_deinit(struct nvlsi_rpu_bal_dev_ctx *bal_dev_ctx);

unsigned int nvlsi_rpu_bal_read_word(void *ctx,
				     unsigned long addr_offset);

void nvlsi_rpu_bal_write_word(void *ctx,
			      unsigned long addr_offset,
			      unsigned int val);

void nvlsi_rpu_bal_read_block(void *ctx,
			      void *dest_addr,
			      unsigned long src_addr_offset,
			      size_t len);

void nvlsi_rpu_bal_write_block(void *ctx,
			       unsigned long dest_addr_offset,
			       const void *src_addr,
			       size_t len);

unsigned long nvlsi_rpu_bal_dma_map(void *ctx,
				    unsigned long virt_addr,
				    size_t len,
				    enum nvlsi_rpu_osal_dma_dir dma_dir);

unsigned long nvlsi_rpu_bal_dma_unmap(void *ctx,
				      unsigned long phy_addr,
				      size_t len,
				      enum nvlsi_rpu_osal_dma_dir dma_dir);

void nvlsi_rpu_bal_bus_access_rec_enab(void *ctx);

void nvlsi_rpu_bal_bus_access_rec_disab(void *ctx);

void nvlsi_pru_bal_bus_access_cnt_print(void *ctx);
#endif /* __BAL_API_H__ */
