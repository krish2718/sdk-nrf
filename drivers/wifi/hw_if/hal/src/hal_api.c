/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @brief File containing API definitions for the
 * HAL Layer of the Wi-Fi driver.
 */

#include "queue.h"
#include "hal_structs.h"
#include "hal_common.h"
#include "hal_reg.h"
#include "hal_mem.h"
#include "hal_interrupt.h"
#include "pal.h"


static enum nvlsi_rpu_status nvlsi_rpu_hal_rpu_pktram_buf_map_init(struct nvlsi_rpu_hal_dev_ctx *hal_dev_ctx)
{
	enum nvlsi_rpu_status status = NVLSI_RPU_STATUS_FAIL;
	unsigned int pool_idx = 0;

	status = pal_rpu_addr_offset_get(hal_dev_ctx->hpriv->opriv,
					 RPU_MEM_PKT_BASE,
					 &hal_dev_ctx->addr_rpu_pktram_base);

	if (status != NVLSI_RPU_STATUS_SUCCESS) {
		nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
				       "%s: pal_rpu_addr_offset_get failed\n",
				       __func__);
		goto out;
	}

	hal_dev_ctx->addr_rpu_pktram_base_tx = hal_dev_ctx->addr_rpu_pktram_base;

	hal_dev_ctx->addr_rpu_pktram_base_rx = hal_dev_ctx->addr_rpu_pktram_base_tx +
		(hal_dev_ctx->hpriv->cfg_params.max_tx_frms *
		 hal_dev_ctx->hpriv->cfg_params.max_tx_frm_sz);

	hal_dev_ctx->addr_rpu_pktram_base_rx_pool[0] = hal_dev_ctx->addr_rpu_pktram_base_rx;

	for (pool_idx = 1; pool_idx < MAX_NUM_OF_RX_QUEUES; pool_idx++)
		hal_dev_ctx->addr_rpu_pktram_base_rx_pool[pool_idx] = (hal_dev_ctx->addr_rpu_pktram_base_rx_pool[pool_idx - 1] +
								       (hal_dev_ctx->hpriv->cfg_params.rx_buf_pool[pool_idx - 1].num_bufs *
									hal_dev_ctx->hpriv->cfg_params.rx_buf_pool[pool_idx - 1].buf_sz));

	status = NVLSI_RPU_STATUS_SUCCESS;

out:
	return status;
}


unsigned long nvlsi_rpu_hal_buf_map_rx(struct nvlsi_rpu_hal_dev_ctx *hal_dev_ctx,
				       unsigned long buf,
				       unsigned int buf_len,
				       unsigned int pool_id,
				       unsigned int buf_id)
{
	struct nvlsi_rpu_hal_buf_map_info *rx_buf_info = NULL;
	unsigned long addr_to_map = 0;
	unsigned long bounce_buf_addr = 0;

	rx_buf_info = &hal_dev_ctx->rx_buf_info[pool_id][buf_id];

	if (rx_buf_info->mapped) {
		nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
				       "%s: Called for already mapped RX buffer\n",
				       __func__);
		goto out;
	}

	rx_buf_info->virt_addr = buf;
	rx_buf_info->buf_len = buf_len;

	if (buf_len != hal_dev_ctx->hpriv->cfg_params.rx_buf_pool[pool_id].buf_sz) {
		nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
				       "%s: Invalid buf_len (%d) for pool_id (%d)\n",
				       __func__,
				       buf_len,
				       pool_id);
		goto out;
	}

	bounce_buf_addr = hal_dev_ctx->addr_rpu_pktram_base_rx_pool[pool_id] +
		(buf_id * buf_len);

	nvlsi_rpu_bal_write_block(hal_dev_ctx->bal_dev_ctx,
				  (unsigned int)bounce_buf_addr,
				  (void *)buf,
				  hal_dev_ctx->hpriv->cfg_params.rx_buf_headroom_sz);

	addr_to_map = bounce_buf_addr + hal_dev_ctx->hpriv->cfg_params.rx_buf_headroom_sz;

	rx_buf_info->phy_addr = nvlsi_rpu_bal_dma_map(hal_dev_ctx->bal_dev_ctx,
						      addr_to_map,
						      buf_len,
						      NVLSI_RPU_OSAL_DMA_DIR_FROM_DEV);

	if (!rx_buf_info->phy_addr) {
		nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
				       "%s: DMA map failed\n",
				       __func__);
		goto out;
	}

out:
	if (rx_buf_info->phy_addr)
		rx_buf_info->mapped = true;

	return rx_buf_info->phy_addr;
}


unsigned long nvlsi_rpu_hal_buf_unmap_rx(struct nvlsi_rpu_hal_dev_ctx *hal_dev_ctx,
					 unsigned int data_len,
					 unsigned int pool_id,
					 unsigned int buf_id)
{
	struct nvlsi_rpu_hal_buf_map_info *rx_buf_info = NULL;
	unsigned long unmapped_addr = 0;
	unsigned long virt_addr = 0;

	rx_buf_info = &hal_dev_ctx->rx_buf_info[pool_id][buf_id];

	if (!rx_buf_info->mapped) {
		nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
				       "%s: Called for unmapped RX buffer\n",
				       __func__);
		goto out;
	}

	unmapped_addr = nvlsi_rpu_bal_dma_unmap(hal_dev_ctx->bal_dev_ctx,
						rx_buf_info->phy_addr,
						rx_buf_info->buf_len,
						NVLSI_RPU_OSAL_DMA_DIR_FROM_DEV);

	if (data_len) {
		if (!unmapped_addr) {
			nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
					       "%s: DMA unmap failed\n",
					       __func__);
			goto out;
		}

		nvlsi_rpu_bal_read_block(hal_dev_ctx->bal_dev_ctx,
					 (void *)(rx_buf_info->virt_addr +
						  hal_dev_ctx->hpriv->cfg_params.rx_buf_headroom_sz),
					 unmapped_addr,
					 data_len);
	}

	virt_addr = rx_buf_info->virt_addr;

	nvlsi_rpu_osal_mem_set(hal_dev_ctx->hpriv->opriv,
			       rx_buf_info,
			       0,
			       sizeof(*rx_buf_info));
out:
	return virt_addr;
}


unsigned long nvlsi_rpu_hal_buf_map_tx(struct nvlsi_rpu_hal_dev_ctx *hal_dev_ctx,
				       unsigned long buf,
				       unsigned int buf_len,
				       unsigned int desc_id)
{
	struct nvlsi_rpu_hal_buf_map_info *tx_buf_info = NULL;
	unsigned long addr_to_map = 0;
	unsigned long bounce_buf_addr = 0;

	tx_buf_info = &hal_dev_ctx->tx_buf_info[desc_id];

	if (tx_buf_info->mapped) {
		nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
				       "%s: Called for already mapped TX buffer\n",
				       __func__);
		goto out;
	}

	tx_buf_info->virt_addr = buf;
	tx_buf_info->buf_len = buf_len;

	if (buf_len > (hal_dev_ctx->hpriv->cfg_params.max_tx_frm_sz -
		       hal_dev_ctx->hpriv->cfg_params.tx_buf_headroom_sz)) {
		nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
				       "%s: Invalid TX buf_len (%d) for (%d)\n",
				       __func__,
				       buf_len,
				       desc_id);
		goto out;
	}

	bounce_buf_addr = hal_dev_ctx->addr_rpu_pktram_base_tx +
		(desc_id * hal_dev_ctx->hpriv->cfg_params.max_tx_frm_sz) +
		hal_dev_ctx->hpriv->cfg_params.tx_buf_headroom_sz;

	nvlsi_rpu_bal_write_block(hal_dev_ctx->bal_dev_ctx,
				  (unsigned int)bounce_buf_addr,
				  (void *)buf,
				  buf_len);

	addr_to_map = bounce_buf_addr;

	tx_buf_info->phy_addr = nvlsi_rpu_bal_dma_map(hal_dev_ctx->bal_dev_ctx,
						      addr_to_map,
						      buf_len,
						      NVLSI_RPU_OSAL_DMA_DIR_TO_DEV);

	if (!tx_buf_info->phy_addr) {
		nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
				       "%s: DMA map failed\n",
				       __func__);
		goto out;
	}

out:
	if (tx_buf_info->phy_addr)
		tx_buf_info->mapped = true;

	return tx_buf_info->phy_addr;
}


unsigned long nvlsi_rpu_hal_buf_unmap_tx(struct nvlsi_rpu_hal_dev_ctx *hal_dev_ctx,
					 unsigned int desc_id)
{
	struct nvlsi_rpu_hal_buf_map_info *tx_buf_info = NULL;
	unsigned long unmapped_addr = 0;
	unsigned long virt_addr = 0;

	tx_buf_info = &hal_dev_ctx->tx_buf_info[desc_id];

	if (!tx_buf_info->mapped) {
		nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
				       "%s: Called for unmapped TX buffer\n",
				       __func__);
		goto out;
	}

	unmapped_addr = nvlsi_rpu_bal_dma_unmap(hal_dev_ctx->bal_dev_ctx,
						tx_buf_info->phy_addr,
						tx_buf_info->buf_len,
						NVLSI_RPU_OSAL_DMA_DIR_TO_DEV);

	if (!unmapped_addr) {
		nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
				       "%s: DMA unmap failed\n",
				       __func__);
		goto out;
	}

	virt_addr = tx_buf_info->virt_addr;

	nvlsi_rpu_osal_mem_set(hal_dev_ctx->hpriv->opriv,
			       tx_buf_info,
			       0,
			       sizeof(*tx_buf_info));
out:
	return virt_addr;
}


static bool hal_rpu_hpq_is_empty(struct nvlsi_rpu_hal_dev_ctx *hal_dev_ctx,
				 struct host_rpu_hpq *hpq)
{
	enum nvlsi_rpu_status status = NVLSI_RPU_STATUS_FAIL;
	unsigned int val = 0;

	status = hal_rpu_reg_read(hal_dev_ctx,
				  &val,
				  hpq->dequeue_addr);

	if (status != NVLSI_RPU_STATUS_SUCCESS) {
		nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
				       "%s: Read from dequeue address failed, val (0x%X)\n",
				       __func__,
				       val);
		return true;
	}

	if (val)
		return false;

	return true;
}


static enum nvlsi_rpu_status hal_rpu_ready(struct nvlsi_rpu_hal_dev_ctx *hal_dev_ctx,
					   enum NVLSI_RPU_HAL_MSG_TYPE msg_type)
{
	bool is_empty = false;
	struct host_rpu_hpq *avl_buf_q = NULL;

	if (msg_type == NVLSI_RPU_HAL_MSG_TYPE_CMD_CTRL)
		avl_buf_q = &hal_dev_ctx->rpu_info.hpqm_info.cmd_avl_queue;
	else {
		nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
				       "%s: Invalid msg type %d\n",
				       __func__,
				       msg_type);

		return NVLSI_RPU_STATUS_FAIL;
	}

	/* Check if any command pointers are available to post a message */
	is_empty = hal_rpu_hpq_is_empty(hal_dev_ctx,
					avl_buf_q);

	if (is_empty == true)
		return NVLSI_RPU_STATUS_FAIL;

	return NVLSI_RPU_STATUS_SUCCESS;
}


static enum nvlsi_rpu_status hal_rpu_ready_wait(struct nvlsi_rpu_hal_dev_ctx *hal_dev_ctx,
						enum NVLSI_RPU_HAL_MSG_TYPE msg_type)
{
	unsigned long start_time_us = 0;
	enum nvlsi_rpu_status status = NVLSI_RPU_STATUS_FAIL;

	start_time_us = nvlsi_rpu_osal_time_get_curr_us(hal_dev_ctx->hpriv->opriv);

	while (hal_rpu_ready(hal_dev_ctx, msg_type) != NVLSI_RPU_STATUS_SUCCESS) {
		if (nvlsi_rpu_osal_time_elapsed_us(hal_dev_ctx->hpriv->opriv,
						   start_time_us) >= MAX_HAL_RPU_READY_WAIT) {
			nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
					       "%s: Timed out waiting (msg_type = %d)\n",
					       __func__,
					       msg_type);
			goto out;
		}
	}

	status = NVLSI_RPU_STATUS_SUCCESS;

out:
	return status;
}


static enum nvlsi_rpu_status hal_rpu_msg_trigger(struct nvlsi_rpu_hal_dev_ctx *hal_dev_ctx)
{
	enum nvlsi_rpu_status status = NVLSI_RPU_STATUS_FAIL;

	status = hal_rpu_reg_write(hal_dev_ctx,
				   RPU_REG_INT_TO_MCU_CTRL,
				   (hal_dev_ctx->num_cmds | 0x7fff0000));

	if (status != NVLSI_RPU_STATUS_SUCCESS) {
		nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
				       "%s: Writing to MCU cmd register failed\n",
				       __func__);
		goto out;
	}

	hal_dev_ctx->num_cmds++;
out:
	return status;
}


static enum nvlsi_rpu_status hal_rpu_msg_post(struct nvlsi_rpu_hal_dev_ctx *hal_dev_ctx,
					      enum NVLSI_RPU_HAL_MSG_TYPE msg_type,
					      unsigned int queue_id,
					      unsigned int msg_addr)
{
	enum nvlsi_rpu_status status = NVLSI_RPU_STATUS_SUCCESS;
	struct host_rpu_hpq *busy_queue = NULL;

	if ((msg_type == NVLSI_RPU_HAL_MSG_TYPE_CMD_CTRL) ||
	    (msg_type == NVLSI_RPU_HAL_MSG_TYPE_CMD_DATA_TX))
		busy_queue = &hal_dev_ctx->rpu_info.hpqm_info.cmd_busy_queue;
	else if (msg_type == NVLSI_RPU_HAL_MSG_TYPE_CMD_DATA_RX) {
		if (queue_id == 0)
			busy_queue = &hal_dev_ctx->rpu_info.hpqm_info.rx1_buf_busy_queue;
		else if (queue_id == 1)
			busy_queue = &hal_dev_ctx->rpu_info.hpqm_info.rx2_buf_busy_queue;
		else if (queue_id == 2)
			busy_queue = &hal_dev_ctx->rpu_info.hpqm_info.rx3_buf_busy_queue;
		else {
			nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
					       "%s: Invalid queue_id (%d)\n",
					       __func__,
					       queue_id);
			goto out;
		}
	} else {
		nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
				       "%s: Invalid msg_type (%d)\n",
				       __func__,
				       msg_type);
		goto out;
	}

	/* Copy the address, to which information was posted,
	 * to the busy queue.
	 */
	status = hal_rpu_hpq_enqueue(hal_dev_ctx,
				     busy_queue,
				     msg_addr);

	if (status != NVLSI_RPU_STATUS_SUCCESS) {
		nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
				       "%s: Queueing of message to RPU failed\n",
				       __func__);
		goto out;
	}

	if (msg_type != NVLSI_RPU_HAL_MSG_TYPE_CMD_DATA_RX) {
		/* Indicate to the RPU that the information has been posted */
		status = hal_rpu_msg_trigger(hal_dev_ctx);

		if (status != NVLSI_RPU_STATUS_SUCCESS) {
			nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
					       "%s: Posting command to RPU failed\n",
					       __func__);
			goto out;
		}
	}
out:
	return status;
}


static enum nvlsi_rpu_status hal_rpu_msg_get_addr(struct nvlsi_rpu_hal_dev_ctx *hal_dev_ctx,
						  enum NVLSI_RPU_HAL_MSG_TYPE msg_type,
						  unsigned int *msg_addr)
{
	enum nvlsi_rpu_status status = NVLSI_RPU_STATUS_FAIL;
	struct host_rpu_hpq *avl_queue = NULL;

	if (msg_type == NVLSI_RPU_HAL_MSG_TYPE_CMD_CTRL)
		avl_queue = &hal_dev_ctx->rpu_info.hpqm_info.cmd_avl_queue;
	else {
		nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
				       "%s: Invalid msg_type (%d)\n",
				       __func__,
				       msg_type);
		goto out;
	}

	status = hal_rpu_hpq_dequeue(hal_dev_ctx,
				     avl_queue,
				     msg_addr);

	if (status != NVLSI_RPU_STATUS_SUCCESS) {
		nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
				       "%s: Dequeue of address failed msg_addr 0x%X\n",
				       __func__,
				       *msg_addr);
		*msg_addr = 0;
		goto out;
	}


out:
	return status;
}


static enum nvlsi_rpu_status hal_rpu_msg_write(struct nvlsi_rpu_hal_dev_ctx *hal_dev_ctx,
					       enum NVLSI_RPU_HAL_MSG_TYPE msg_type,
					       void *msg,
					       unsigned int len)
{
	unsigned int msg_addr = 0;
	enum nvlsi_rpu_status status = NVLSI_RPU_STATUS_FAIL;

	/* Get the address from the RPU to which
	 * the command needs to be copied to
	 */
	status = hal_rpu_msg_get_addr(hal_dev_ctx,
				      msg_type,
				      &msg_addr);

	if (status != NVLSI_RPU_STATUS_SUCCESS) {
		nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
				       "%s: Getting address (0x%X) to post message failed\n",
				       __func__,
				       msg_addr);
		goto out;
	}

	/* Copy the information to the suggested address */
	status = hal_rpu_mem_write(hal_dev_ctx,
				   msg_addr,
				   msg,
				   len);

	if (status != NVLSI_RPU_STATUS_SUCCESS) {
		nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
				       "%s: Copying information to RPU failed\n",
				       __func__);
		goto out;
	}

	/* Post the updated information to the RPU */
	status = hal_rpu_msg_post(hal_dev_ctx,
				  msg_type,
				  0,
				  msg_addr);

	if (status != NVLSI_RPU_STATUS_SUCCESS) {
		nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
				       "%s: Posting command to RPU failed\n",
				       __func__);
		goto out;
	}

out:
	return status;
}


static enum nvlsi_rpu_status hal_rpu_cmd_process_queue(struct nvlsi_rpu_hal_dev_ctx *hal_dev_ctx)
{
	enum nvlsi_rpu_status status = NVLSI_RPU_STATUS_FAIL;
	struct nvlsi_rpu_hal_msg *cmd = NULL;

	while ((cmd = nvlsi_wlan_utils_q_dequeue(hal_dev_ctx->hpriv->opriv,
						 hal_dev_ctx->cmd_q))) {
		status = hal_rpu_ready_wait(hal_dev_ctx,
					    NVLSI_RPU_HAL_MSG_TYPE_CMD_CTRL);

		if (status != NVLSI_RPU_STATUS_SUCCESS) {
			nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
					       "%s: Timed out waiting for RPU to give a free command buffer\n",
					       __func__);
			nvlsi_rpu_osal_mem_free(hal_dev_ctx->hpriv->opriv,
						cmd);
			cmd = NULL;
			continue;
		}

		status = hal_rpu_msg_write(hal_dev_ctx,
					   NVLSI_RPU_HAL_MSG_TYPE_CMD_CTRL,
					   cmd->data,
					   cmd->len);

		if (status != NVLSI_RPU_STATUS_SUCCESS) {
			nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
					       "%s: Writing command to RPU failed\n",
					       __func__);
			nvlsi_rpu_osal_mem_free(hal_dev_ctx->hpriv->opriv,
						cmd);
			cmd = NULL;
			continue;
		}

		/* Free the command data and command */
		nvlsi_rpu_osal_mem_free(hal_dev_ctx->hpriv->opriv,
					cmd);
		cmd = NULL;
	}

	return status;
}


static enum nvlsi_rpu_status hal_rpu_cmd_queue(struct nvlsi_rpu_hal_dev_ctx *hal_dev_ctx,
					       void *cmd,
					       unsigned int cmd_size)
{
	int len = 0;
	int size = 0;
	char *data = NULL;
	struct nvlsi_rpu_hal_msg *hal_msg = NULL;
	enum nvlsi_rpu_status status = NVLSI_RPU_STATUS_FAIL;

	len = cmd_size;
	data = cmd;

	if (len > hal_dev_ctx->hpriv->cfg_params.max_cmd_size) {
		while (len > 0) {
			if (len > hal_dev_ctx->hpriv->cfg_params.max_cmd_size)
				size = hal_dev_ctx->hpriv->cfg_params.max_cmd_size;
			else
				size = len;

			hal_msg = nvlsi_rpu_osal_mem_zalloc(hal_dev_ctx->hpriv->opriv,
							    sizeof(*hal_msg) + size);

			if (!hal_msg) {
				nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
						       "%s: Unable to allocate buffer for fragmented HAL command\n",
						       __func__);
				status = NVLSI_RPU_STATUS_FAIL;
				goto out;
			}

			nvlsi_rpu_osal_mem_cpy(hal_dev_ctx->hpriv->opriv,
					       hal_msg->data,
					       data,
					       size);

			hal_msg->len = size;

			status = nvlsi_wlan_utils_q_enqueue(hal_dev_ctx->hpriv->opriv,
							    hal_dev_ctx->cmd_q,
							    hal_msg);

			if (status != NVLSI_RPU_STATUS_SUCCESS) {
				nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
						       "%s: Unable to queue fragmented HAL command\n",
						       __func__);
				goto out;
			}

			len -= size;
			data += size;
		}

	} else {
		hal_msg = nvlsi_rpu_osal_mem_zalloc(hal_dev_ctx->hpriv->opriv,
						    sizeof(*hal_msg) + len);

		if (!hal_msg) {
			nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
					       "%s: Unable to allocate buffer for HAL command\n",
					       __func__);
			status = NVLSI_RPU_STATUS_FAIL;
			goto out;
		}

		nvlsi_rpu_osal_mem_cpy(hal_dev_ctx->hpriv->opriv,
				       hal_msg->data,
				       cmd,
				       len);

		hal_msg->len = len;

		status = nvlsi_wlan_utils_q_enqueue(hal_dev_ctx->hpriv->opriv,
						    hal_dev_ctx->cmd_q,
						    hal_msg);

		if (status != NVLSI_RPU_STATUS_SUCCESS) {
			nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
					       "%s: Unable to queue fragmented command\n",
					       __func__);
			goto out;
		}
	}

	/* Free the original command data */
	nvlsi_rpu_osal_mem_free(hal_dev_ctx->hpriv->opriv,
				cmd);

out:
	return status;
}


enum nvlsi_rpu_status nvlsi_rpu_hal_ctrl_cmd_send(struct nvlsi_rpu_hal_dev_ctx *hal_dev_ctx,
						  void *cmd,
						  unsigned int cmd_size)
{
	enum nvlsi_rpu_status status = NVLSI_RPU_STATUS_FAIL;

	nvlsi_rpu_osal_spinlock_take(hal_dev_ctx->hpriv->opriv,
				     hal_dev_ctx->lock_hal);

	status = hal_rpu_cmd_queue(hal_dev_ctx,
				   cmd,
				   cmd_size);

	if (status != NVLSI_RPU_STATUS_SUCCESS) {
		nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
				       "%s: Queueing of command failed\n",
				       __func__);
		goto out;
	}

	status = hal_rpu_cmd_process_queue(hal_dev_ctx);

out:
	nvlsi_rpu_osal_spinlock_rel(hal_dev_ctx->hpriv->opriv,
				    hal_dev_ctx->lock_hal);

	return status;
}


enum nvlsi_rpu_status nvlsi_rpu_hal_data_cmd_send(struct nvlsi_rpu_hal_dev_ctx *hal_dev_ctx,
						  enum NVLSI_RPU_HAL_MSG_TYPE cmd_type,
						  void *cmd,
						  unsigned int cmd_size,
						  unsigned int desc_id,
						  unsigned int pool_id)
{
	enum nvlsi_rpu_status status = NVLSI_RPU_STATUS_FAIL;
	unsigned int addr_base = 0;
	unsigned int max_cmd_size = 0;
	unsigned int addr = 0;

	nvlsi_rpu_osal_spinlock_take(hal_dev_ctx->hpriv->opriv,
				     hal_dev_ctx->lock_hal);

	if (cmd_type == NVLSI_RPU_HAL_MSG_TYPE_CMD_DATA_RX) {
		addr_base = hal_dev_ctx->rpu_info.rx_cmd_base;
		max_cmd_size = RPU_DATA_CMD_SIZE_MAX_RX;
	} else if (cmd_type == NVLSI_RPU_HAL_MSG_TYPE_CMD_DATA_TX) {
		addr_base = hal_dev_ctx->rpu_info.tx_cmd_base;
		max_cmd_size = RPU_DATA_CMD_SIZE_MAX_TX;
	} else
		nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
				       "%s: Invalid data command type %d\n",
				       __func__,
				       cmd_type);

	addr = addr_base + (max_cmd_size * desc_id);

	/* Copy the information to the suggested address */
	status = hal_rpu_mem_write(hal_dev_ctx,
				   addr,
				   cmd,
				   cmd_size);

	if (status != NVLSI_RPU_STATUS_SUCCESS) {
		nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
				       "%s: Copying data cmd(%d) to RPU failed\n",
				       __func__,
				       cmd_type);
		goto out;
	}

	/* Post the updated information to the RPU */
	status = hal_rpu_msg_post(hal_dev_ctx,
				  cmd_type,
				  pool_id,
				  addr);

	if (status != NVLSI_RPU_STATUS_SUCCESS) {
		nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
				       "%s: Posting RX buf info to RPU failed\n",
				       __func__);
		goto out;
	}
out:
	nvlsi_rpu_osal_spinlock_rel(hal_dev_ctx->hpriv->opriv,
				    hal_dev_ctx->lock_hal);

	return status;
}


static void rx_tasklet_fn(unsigned long data)
{
	enum nvlsi_rpu_status status = NVLSI_RPU_STATUS_FAIL;
	struct nvlsi_rpu_hal_dev_ctx *hal_dev_ctx = NULL;

	hal_dev_ctx = (struct nvlsi_rpu_hal_dev_ctx *)data;

	status = hal_rpu_eventq_process(hal_dev_ctx);

	if (status != NVLSI_RPU_STATUS_SUCCESS)
		nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
				       "%s: Event queue processing failed\n",
				       __func__);
}


enum nvlsi_rpu_status hal_rpu_eventq_process(struct nvlsi_rpu_hal_dev_ctx *hal_dev_ctx)
{
	enum nvlsi_rpu_status status = NVLSI_RPU_STATUS_SUCCESS;
	struct nvlsi_rpu_hal_msg *event = NULL;
	unsigned long flags = 0;
	void *event_data = NULL;
	unsigned int event_len = 0;

	while (1) {
		nvlsi_rpu_osal_spinlock_irq_take(hal_dev_ctx->hpriv->opriv,
						 hal_dev_ctx->lock_rx,
						 &flags);

		event = nvlsi_wlan_utils_q_dequeue(hal_dev_ctx->hpriv->opriv,
						   hal_dev_ctx->event_q);

		nvlsi_rpu_osal_spinlock_irq_rel(hal_dev_ctx->hpriv->opriv,
						hal_dev_ctx->lock_rx,
						&flags);

		if (!event)
			goto out;

		event_data = event->data;
		event_len = event->len;

		/* Process the event further */
		status = hal_dev_ctx->hpriv->intr_callbk_fn(hal_dev_ctx->mac_dev_ctx,
							    event_data,
							    event_len);

		if (status != NVLSI_RPU_STATUS_SUCCESS)
			nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
					       "%s: Interrupt callback failed\n",
					       __func__);

		/* Free up the local buffer */
		nvlsi_rpu_osal_mem_free(hal_dev_ctx->hpriv->opriv,
					event);
		event = NULL;
	}

out:
	return status;
}


void nvlsi_rpu_hal_proc_ctx_set(struct nvlsi_rpu_hal_dev_ctx *hal_dev_ctx,
				enum RPU_PROC_TYPE proc)
{
	hal_dev_ctx->curr_proc = proc;
}


struct nvlsi_rpu_hal_dev_ctx *nvlsi_rpu_hal_dev_add(struct nvlsi_rpu_hal_priv *hpriv,
						    void *mac_dev_ctx)
{
	enum nvlsi_rpu_status status = NVLSI_RPU_STATUS_FAIL;
	struct nvlsi_rpu_hal_dev_ctx *hal_dev_ctx = NULL;
	unsigned int i = 0;
	unsigned int num_rx_bufs = 0;
	unsigned int size = 0;

	hal_dev_ctx = nvlsi_rpu_osal_mem_zalloc(hpriv->opriv,
						sizeof(*hal_dev_ctx));

	if (!hal_dev_ctx) {
		nvlsi_rpu_osal_log_err(hpriv->opriv,
				       "%s: Unable to allocate hal_dev_ctx\n",
				       __func__);
		goto out;
	}

	hal_dev_ctx->hpriv = hpriv;
	hal_dev_ctx->mac_dev_ctx = mac_dev_ctx;
	hal_dev_ctx->idx = hpriv->num_devs++;

	hal_dev_ctx->num_cmds = RPU_CMD_START_MAGIC;

	hal_dev_ctx->cmd_q = nvlsi_wlan_utils_q_alloc(hpriv->opriv);

	if (!hal_dev_ctx->cmd_q) {
		nvlsi_rpu_osal_log_err(hpriv->opriv,
				       "%s: Unable to allocate command queue\n",
				       __func__);
		nvlsi_rpu_osal_mem_free(hpriv->opriv,
					hal_dev_ctx);
		hal_dev_ctx = NULL;
		goto out;
	}

	hal_dev_ctx->event_q = nvlsi_wlan_utils_q_alloc(hpriv->opriv);

	if (!hal_dev_ctx->event_q) {
		nvlsi_rpu_osal_log_err(hpriv->opriv,
				       "%s: Unable to allocate event queue\n",
				       __func__);
		nvlsi_wlan_utils_q_free(hpriv->opriv,
					hal_dev_ctx->cmd_q);
		nvlsi_rpu_osal_mem_free(hpriv->opriv,
					hal_dev_ctx);
		hal_dev_ctx = NULL;
		goto out;
	}

	hal_dev_ctx->lock_hal = nvlsi_rpu_osal_spinlock_alloc(hpriv->opriv);

	if (!hal_dev_ctx->lock_hal) {
		nvlsi_rpu_osal_log_err(hpriv->opriv,
				       "%s: Unable to allocate HAL lock\n", __func__);
		nvlsi_wlan_utils_q_free(hpriv->opriv,
					hal_dev_ctx->cmd_q);
		nvlsi_wlan_utils_q_free(hpriv->opriv,
					hal_dev_ctx->event_q);
		nvlsi_rpu_osal_mem_free(hpriv->opriv,
					hal_dev_ctx);
		hal_dev_ctx = NULL;
		goto out;
	}

	nvlsi_rpu_osal_spinlock_init(hpriv->opriv,
				     hal_dev_ctx->lock_hal);

	hal_dev_ctx->lock_rx = nvlsi_rpu_osal_spinlock_alloc(hpriv->opriv);

	if (!hal_dev_ctx->lock_rx) {
		nvlsi_rpu_osal_log_err(hpriv->opriv,
				       "%s: Unable to allocate HAL lock\n",
				       __func__);
		nvlsi_rpu_osal_spinlock_free(hpriv->opriv,
					     hal_dev_ctx->lock_hal);
		nvlsi_wlan_utils_q_free(hpriv->opriv,
					hal_dev_ctx->cmd_q);
		nvlsi_wlan_utils_q_free(hpriv->opriv,
					hal_dev_ctx->event_q);
		nvlsi_rpu_osal_mem_free(hpriv->opriv,
					hal_dev_ctx);
		hal_dev_ctx = NULL;
		goto out;
	}

	nvlsi_rpu_osal_spinlock_init(hpriv->opriv,
				     hal_dev_ctx->lock_rx);

	hal_dev_ctx->rx_tasklet = nvlsi_rpu_osal_tasklet_alloc(hpriv->opriv);

	if (!hal_dev_ctx->rx_tasklet) {
		nvlsi_rpu_osal_log_err(hpriv->opriv,
				       "%s: Unable to allocate rx_tasklet\n",
				       __func__);
		nvlsi_rpu_osal_spinlock_free(hpriv->opriv,
					     hal_dev_ctx->lock_hal);
		nvlsi_rpu_osal_spinlock_free(hpriv->opriv,
					     hal_dev_ctx->lock_rx);
		nvlsi_wlan_utils_q_free(hpriv->opriv,
					hal_dev_ctx->cmd_q);
		nvlsi_wlan_utils_q_free(hpriv->opriv,
					hal_dev_ctx->event_q);
		nvlsi_rpu_osal_mem_free(hpriv->opriv,
					hal_dev_ctx);
		hal_dev_ctx = NULL;
		goto out;
	}

	nvlsi_rpu_osal_tasklet_init(hpriv->opriv,
				    hal_dev_ctx->rx_tasklet,
				    rx_tasklet_fn,
				    (unsigned long)hal_dev_ctx);

	hal_dev_ctx->bal_dev_ctx = nvlsi_rpu_bal_dev_add(hpriv->bpriv,
							 hal_dev_ctx);

	if (!hal_dev_ctx->bal_dev_ctx) {
		nvlsi_rpu_osal_log_err(hpriv->opriv,
				       "%s: nvlsi_rpu_bal_dev_add failed\n",
				       __func__);

		nvlsi_rpu_osal_tasklet_free(hpriv->opriv,
					    hal_dev_ctx->rx_tasklet);
		nvlsi_rpu_osal_spinlock_free(hpriv->opriv,
					     hal_dev_ctx->lock_hal);
		nvlsi_rpu_osal_spinlock_free(hpriv->opriv,
					     hal_dev_ctx->lock_rx);
		nvlsi_wlan_utils_q_free(hpriv->opriv,
					hal_dev_ctx->cmd_q);
		nvlsi_wlan_utils_q_free(hpriv->opriv,
					hal_dev_ctx->event_q);
		nvlsi_rpu_osal_mem_free(hpriv->opriv,
					hal_dev_ctx);
		hal_dev_ctx = NULL;
		goto out;
	}

	status = hal_rpu_irq_enable(hal_dev_ctx);

	if (status != NVLSI_RPU_STATUS_SUCCESS) {
		nvlsi_rpu_osal_log_err(hpriv->opriv,
				       "%s: hal_rpu_irq_enable failed\n",
				       __func__);

		nvlsi_rpu_bal_dev_rem(hal_dev_ctx->bal_dev_ctx);

		nvlsi_rpu_osal_tasklet_free(hpriv->opriv,
					    hal_dev_ctx->rx_tasklet);
		nvlsi_rpu_osal_spinlock_free(hpriv->opriv,
					     hal_dev_ctx->lock_hal);
		nvlsi_rpu_osal_spinlock_free(hpriv->opriv,
					     hal_dev_ctx->lock_rx);
		nvlsi_wlan_utils_q_free(hpriv->opriv,
					hal_dev_ctx->cmd_q);
		nvlsi_wlan_utils_q_free(hpriv->opriv,
					hal_dev_ctx->event_q);
		nvlsi_rpu_osal_mem_free(hpriv->opriv,
					hal_dev_ctx);
		hal_dev_ctx = NULL;
		goto out;
	}

	for (i = 0; i < MAX_NUM_OF_RX_QUEUES; i++) {
		num_rx_bufs = hal_dev_ctx->hpriv->cfg_params.rx_buf_pool[i].num_bufs;

		size = (num_rx_bufs * sizeof(struct nvlsi_rpu_hal_buf_map_info));
		hal_dev_ctx->rx_buf_info[i] = (struct nvlsi_rpu_hal_buf_map_info *)nvlsi_rpu_osal_mem_zalloc(hpriv->opriv,
													     size);

		if (!hal_dev_ctx->rx_buf_info[i]) {
			nvlsi_rpu_osal_log_err(hpriv->opriv,
					       "%s: No space for RX buf info[%d]\n",
					       __func__,
					       i);

			nvlsi_rpu_bal_dev_rem(hal_dev_ctx->bal_dev_ctx);

			nvlsi_rpu_osal_tasklet_free(hpriv->opriv,
						    hal_dev_ctx->rx_tasklet);
			nvlsi_rpu_osal_spinlock_free(hpriv->opriv,
						     hal_dev_ctx->lock_hal);
			nvlsi_rpu_osal_spinlock_free(hpriv->opriv,
						     hal_dev_ctx->lock_rx);
			nvlsi_wlan_utils_q_free(hpriv->opriv,
						hal_dev_ctx->cmd_q);
			nvlsi_wlan_utils_q_free(hpriv->opriv,
						hal_dev_ctx->event_q);
			nvlsi_rpu_osal_mem_free(hpriv->opriv,
						hal_dev_ctx);
			hal_dev_ctx = NULL;

			goto out;
		}
	}

	size = (hal_dev_ctx->hpriv->cfg_params.max_tx_frms * sizeof(struct nvlsi_rpu_hal_buf_map_info));

	hal_dev_ctx->tx_buf_info = (struct nvlsi_rpu_hal_buf_map_info *)nvlsi_rpu_osal_mem_zalloc(hpriv->opriv,
												  size);

	if (!hal_dev_ctx->tx_buf_info) {
		nvlsi_rpu_osal_log_err(hpriv->opriv,
				       "%s: No space for TX buf info\n",
				       __func__);

		for (i = 0; i < MAX_NUM_OF_RX_QUEUES; i++) {
			nvlsi_rpu_osal_mem_free(hpriv->opriv,
						hal_dev_ctx->rx_buf_info[i]);
			hal_dev_ctx->rx_buf_info[i] = NULL;
		}

		nvlsi_rpu_bal_dev_rem(hal_dev_ctx->bal_dev_ctx);

		nvlsi_rpu_osal_tasklet_free(hpriv->opriv,
					    hal_dev_ctx->rx_tasklet);
		nvlsi_rpu_osal_spinlock_free(hpriv->opriv,
					     hal_dev_ctx->lock_hal);
		nvlsi_rpu_osal_spinlock_free(hpriv->opriv,
					     hal_dev_ctx->lock_rx);
		nvlsi_wlan_utils_q_free(hpriv->opriv,
					hal_dev_ctx->cmd_q);
		nvlsi_wlan_utils_q_free(hpriv->opriv,
					hal_dev_ctx->event_q);
		nvlsi_rpu_osal_mem_free(hpriv->opriv,
					hal_dev_ctx);
		hal_dev_ctx = NULL;

		goto out;
	}

	status = nvlsi_rpu_hal_rpu_pktram_buf_map_init(hal_dev_ctx);

	if (status != NVLSI_RPU_STATUS_SUCCESS) {
		nvlsi_rpu_osal_log_err(hpriv->opriv,
				       "%s: Buffer map init failed\n",
				       __func__);
		nvlsi_rpu_osal_mem_free(hpriv->opriv,
					hal_dev_ctx->tx_buf_info);
		hal_dev_ctx->tx_buf_info = NULL;

		for (i = 0; i < MAX_NUM_OF_RX_QUEUES; i++) {
			nvlsi_rpu_osal_mem_free(hpriv->opriv,
						hal_dev_ctx->rx_buf_info[i]);
			hal_dev_ctx->rx_buf_info[i] = NULL;
		}

		nvlsi_rpu_bal_dev_rem(hal_dev_ctx->bal_dev_ctx);

		nvlsi_rpu_osal_tasklet_free(hpriv->opriv,
					    hal_dev_ctx->rx_tasklet);
		nvlsi_rpu_osal_spinlock_free(hpriv->opriv,
					     hal_dev_ctx->lock_hal);
		nvlsi_rpu_osal_spinlock_free(hpriv->opriv,
					     hal_dev_ctx->lock_rx);
		nvlsi_wlan_utils_q_free(hpriv->opriv,
					hal_dev_ctx->cmd_q);
		nvlsi_wlan_utils_q_free(hpriv->opriv,
					hal_dev_ctx->event_q);
		nvlsi_rpu_osal_mem_free(hpriv->opriv,
					hal_dev_ctx->rx_buf_info);
		nvlsi_rpu_osal_mem_free(hpriv->opriv,
					hal_dev_ctx);
		hal_dev_ctx = NULL;
		goto out;
	}

out:
	return hal_dev_ctx;
}


void nvlsi_rpu_hal_dev_rem(struct nvlsi_rpu_hal_dev_ctx *hal_dev_ctx)
{
	unsigned int i = 0;

	nvlsi_rpu_bal_dev_rem(hal_dev_ctx->bal_dev_ctx);

	nvlsi_rpu_osal_mem_free(hal_dev_ctx->hpriv->opriv,
				hal_dev_ctx->tx_buf_info);
	hal_dev_ctx->tx_buf_info = NULL;

	for (i = 0; i < MAX_NUM_OF_RX_QUEUES; i++) {
		nvlsi_rpu_osal_mem_free(hal_dev_ctx->hpriv->opriv,
					hal_dev_ctx->rx_buf_info[i]);
		hal_dev_ctx->rx_buf_info[i] = NULL;
	}

	nvlsi_rpu_osal_tasklet_kill(hal_dev_ctx->hpriv->opriv,
				    hal_dev_ctx->rx_tasklet);

	nvlsi_rpu_osal_tasklet_free(hal_dev_ctx->hpriv->opriv,
				    hal_dev_ctx->rx_tasklet);

	nvlsi_rpu_osal_spinlock_free(hal_dev_ctx->hpriv->opriv,
				     hal_dev_ctx->lock_hal);
	nvlsi_rpu_osal_spinlock_free(hal_dev_ctx->hpriv->opriv,
				     hal_dev_ctx->lock_rx);

	nvlsi_wlan_utils_q_free(hal_dev_ctx->hpriv->opriv,
				hal_dev_ctx->event_q);

	nvlsi_wlan_utils_q_free(hal_dev_ctx->hpriv->opriv,
				hal_dev_ctx->cmd_q);

	hal_dev_ctx->hpriv->num_devs--;

	nvlsi_rpu_osal_mem_free(hal_dev_ctx->hpriv->opriv,
				hal_dev_ctx);
}


enum nvlsi_rpu_status nvlsi_rpu_hal_dev_init(struct nvlsi_rpu_hal_dev_ctx *hal_dev_ctx)
{
	enum nvlsi_rpu_status status = NVLSI_RPU_STATUS_FAIL;

	status = nvlsi_rpu_bal_dev_init(hal_dev_ctx->bal_dev_ctx);

	if (status != NVLSI_RPU_STATUS_SUCCESS) {
		nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
				       "%s: nvlsi_rpu_bal_dev_init failed\n",
				       __func__);
		goto out;
	}

	/* Read the HPQM info for all the queues provided by the RPU
	 * (like command, event, RX buf queues etc)
	 */
	status = hal_rpu_mem_read(hal_dev_ctx,
				  &hal_dev_ctx->rpu_info.hpqm_info,
				  RPU_MEM_HPQ_INFO,
				  sizeof(hal_dev_ctx->rpu_info.hpqm_info));

	if (status != NVLSI_RPU_STATUS_SUCCESS) {
		nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
				       "%s: Failed to get the HPQ info\n",
				       __func__);
		goto out;
	}

	status = hal_rpu_mem_read(hal_dev_ctx,
				  &hal_dev_ctx->rpu_info.rx_cmd_base,
				  RPU_MEM_RX_CMD_BASE,
				  sizeof(hal_dev_ctx->rpu_info.rx_cmd_base));

	if (status != NVLSI_RPU_STATUS_SUCCESS) {
		nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
				       "%s: Reading the RX cmd base failed\n",
				       __func__);
		goto out;
	}

	hal_dev_ctx->rpu_info.tx_cmd_base = RPU_MEM_TX_CMD_BASE;
out:
	return status;
}


void nvlsi_rpu_hal_dev_deinit(struct nvlsi_rpu_hal_dev_ctx *hal_dev_ctx)
{
	nvlsi_rpu_bal_dev_deinit(hal_dev_ctx->bal_dev_ctx);
}


enum nvlsi_rpu_status nvlsi_rpu_hal_irq_handler(void *data)
{
	struct nvlsi_rpu_hal_dev_ctx *hal_dev_ctx = NULL;
	enum nvlsi_rpu_status status = NVLSI_RPU_STATUS_FAIL;
	unsigned long flags = 0;

	hal_dev_ctx = (struct nvlsi_rpu_hal_dev_ctx *)data;

	nvlsi_rpu_osal_spinlock_irq_take(hal_dev_ctx->hpriv->opriv,
					 hal_dev_ctx->lock_rx,
					 &flags);

	status = hal_rpu_irq_process(hal_dev_ctx);

	nvlsi_rpu_osal_spinlock_irq_rel(hal_dev_ctx->hpriv->opriv,
					hal_dev_ctx->lock_rx,
					&flags);

	if (status != NVLSI_RPU_STATUS_SUCCESS)
		goto out;

	nvlsi_rpu_osal_tasklet_schedule(hal_dev_ctx->hpriv->opriv,
					hal_dev_ctx->rx_tasklet);

out:
	return status;
}


static int nvlsi_rpu_hal_poll_reg(struct nvlsi_rpu_hal_dev_ctx *hal_dev_ctx,
				  unsigned int reg_addr,
				  unsigned int mask,
				  unsigned int req_value,
				  unsigned int poll_delay)
{
	enum nvlsi_rpu_status status = NVLSI_RPU_STATUS_FAIL;
	unsigned int val = 0;
	unsigned int count = 50;

	do {
		status = hal_rpu_reg_read(hal_dev_ctx,
					  &val,
					  reg_addr);

		if (status != NVLSI_RPU_STATUS_SUCCESS)
			nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
					       "%s: Read from address (0x%X) failed, val (0x%X)\n",
					       __func__,
					       reg_addr,
					       val);

		if ((val & mask) == req_value) {
			status = NVLSI_RPU_STATUS_SUCCESS;
			break;
		}

		nvlsi_rpu_osal_sleep_ms(hal_dev_ctx->hpriv->opriv,
					poll_delay);
	} while (count-- > 0);

	if (count == 0) {
		nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
				       "%s: Timed out polling on (0x%X)\n",
				       __func__,
				       reg_addr);

		status = NVLSI_RPU_STATUS_FAIL;
		goto out;
	}
out:
	return status;
}


/* Perform MIPS reset */
enum nvlsi_rpu_status nvlsi_rpu_hal_proc_reset(struct nvlsi_rpu_hal_dev_ctx *hal_dev_ctx,
					       enum RPU_PROC_TYPE rpu_proc)
{
	enum nvlsi_rpu_status status = NVLSI_RPU_STATUS_FAIL;

	if ((rpu_proc != RPU_PROC_TYPE_MCU_LMAC) &&
	    (rpu_proc != RPU_PROC_TYPE_MCU_UMAC)) {
		nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
				       "%s: Unsupported RPU processor(%d)\n",
				       __func__,
				       rpu_proc);
		goto out;
	}

	/* Perform pulsed soft reset of MIPS */
	if (rpu_proc == RPU_PROC_TYPE_MCU_LMAC)
		status = hal_rpu_reg_write(hal_dev_ctx,
					   RPU_REG_MIPS_MCU_CONTROL,
					   0x1);
	else
		status = hal_rpu_reg_write(hal_dev_ctx,
					   RPU_REG_MIPS_MCU2_CONTROL,
					   0x1);

	if (status != NVLSI_RPU_STATUS_SUCCESS) {
		nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
				       "%s: Pulsed soft reset of MCU failed for (%d) processor\n",
				       __func__,
				       rpu_proc);
		goto out;
	}


	/* Wait for it to come out of reset */
	if (rpu_proc == RPU_PROC_TYPE_MCU_LMAC)
		status = nvlsi_rpu_hal_poll_reg(hal_dev_ctx,
						RPU_REG_MIPS_MCU_CONTROL,
						0x1,
						0,
						10);
	else
		status = nvlsi_rpu_hal_poll_reg(hal_dev_ctx,
						RPU_REG_MIPS_MCU2_CONTROL,
						0x1,
						0,
						10);

	if (status != NVLSI_RPU_STATUS_SUCCESS) {
		nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
				       "%s: MCU (%d) failed to come out of reset\n",
				       __func__,
				       rpu_proc);
		goto out;
	}

	/* MIPS will restart from it's boot exception registers
	 * and hit its default wait instruction
	 */
	if (rpu_proc == RPU_PROC_TYPE_MCU_LMAC)
		status = nvlsi_rpu_hal_poll_reg(hal_dev_ctx,
						0xA4000018,
						0x1,
						0x1,
						10);
	else
		status = nvlsi_rpu_hal_poll_reg(hal_dev_ctx,
						0xA4000118,
						0x1,
						0x1,
						10);
out:
	return status;
}


enum nvlsi_rpu_status nvlsi_rpu_hal_fw_chk_boot(struct nvlsi_rpu_hal_dev_ctx *hal_dev_ctx,
						enum RPU_PROC_TYPE rpu_proc)
{
	enum nvlsi_rpu_status status = NVLSI_RPU_STATUS_FAIL;
	unsigned int addr = 0;
	unsigned int val = 0;
	unsigned int exp_val = 0;
	unsigned int i = 0;

	if (rpu_proc == RPU_PROC_TYPE_MCU_LMAC) {
		addr = RPU_MEM_LMAC_BOOT_SIG;
		exp_val = IMG_WLAN_LMAC_BOOT_SIG;
	} else if (rpu_proc == RPU_PROC_TYPE_MCU_UMAC) {
		addr = RPU_MEM_UMAC_BOOT_SIG;
		exp_val = IMG_WLAN_UMAC_BOOT_SIG;
	} else {
		nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
				       "%s: Invalid RPU processor (%d)\n",
				       __func__,
				       rpu_proc);
	}

	while (i < 1000) {
		status = hal_rpu_mem_read(hal_dev_ctx,
					  (unsigned char *)&val,
					  addr,
					  sizeof(val));

		if (status != NVLSI_RPU_STATUS_SUCCESS) {
			nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
					       "%s: Reading of boot signature failed for RPU(%d)\n",
					       __func__,
					       rpu_proc);
		}

		if (val == exp_val)
			break;

		/* Sleep for 10 ms */
		nvlsi_rpu_osal_sleep_ms(hal_dev_ctx->hpriv->opriv,
					10);

		i++;

	};

	if (i == 1000) {
		nvlsi_rpu_osal_log_err(hal_dev_ctx->hpriv->opriv,
				       "%s: Boot signature check failed for RPU(%d), Expected: 0x%X, Actual: 0x%X\n",
				       __func__,
				       rpu_proc,
				       exp_val,
				       val);
		status = NVLSI_RPU_STATUS_FAIL;
		goto out;
	}

	status = NVLSI_RPU_STATUS_SUCCESS;
out:
	return status;
}


struct nvlsi_rpu_hal_priv *nvlsi_rpu_hal_init(struct nvlsi_rpu_osal_priv *opriv,
					      struct nvlsi_rpu_hal_cfg_params *cfg_params,
					      enum nvlsi_rpu_status (*intr_callbk_fn)(void *mac_dev_ctx,
										      void *event_data,
										      unsigned int len))
{
	enum nvlsi_rpu_status status = NVLSI_RPU_STATUS_FAIL;
	struct nvlsi_rpu_hal_priv *hpriv = NULL;
	struct nvlsi_rpu_bal_cfg_params bal_cfg_params;

	hpriv = nvlsi_rpu_osal_mem_zalloc(opriv,
					  sizeof(*hpriv));

	if (!hpriv) {
		nvlsi_rpu_osal_log_err(opriv,
				       "%s: Unable to allocate memory for hpriv\n",
				       __func__);
		goto out;
	}

	hpriv->opriv = opriv;

	nvlsi_rpu_osal_mem_cpy(opriv,
			       &hpriv->cfg_params,
			       cfg_params,
			       sizeof(hpriv->cfg_params));

	hpriv->intr_callbk_fn = intr_callbk_fn;

	status = pal_rpu_addr_offset_get(opriv,
					 RPU_ADDR_PKTRAM_START,
					 &hpriv->addr_pktram_base);

	if (status != NVLSI_RPU_STATUS_SUCCESS) {
		nvlsi_rpu_osal_log_err(opriv,
				       "%s: pal_rpu_addr_offset_get failed\n",
				       __func__);
		goto out;
	}

	bal_cfg_params.addr_pktram_base = hpriv->addr_pktram_base;

	hpriv->bpriv = nvlsi_rpu_bal_init(opriv,
					  &bal_cfg_params,
					  &nvlsi_rpu_hal_irq_handler);

	if (!hpriv->bpriv) {
		nvlsi_rpu_osal_log_err(opriv,
				       "%s: Failed\n",
				       __func__);
		nvlsi_rpu_osal_mem_free(opriv,
					hpriv);
		hpriv = NULL;
	}
out:
	return hpriv;
}


void nvlsi_rpu_hal_deinit(struct nvlsi_rpu_hal_priv *hpriv)
{
	nvlsi_rpu_bal_deinit(hpriv->bpriv);

	nvlsi_rpu_osal_mem_free(hpriv->opriv,
				hpriv);
}
