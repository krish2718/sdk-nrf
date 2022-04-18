/*
 * File Name: osal.c
 *
 * Implements OSAL APIs to abstract OS primitives.
 *
 * Copyright (c) 2011-2020 Imagination Technologies Ltd.
 * All rights reserved.
 *
 */
#include "osal_api.h"
#include "osal_ops.h"

struct nvlsi_rpu_osal_priv *nvlsi_rpu_osal_init(void)
{
	struct nvlsi_rpu_osal_priv *opriv = NULL;
	struct nvlsi_rpu_osal_ops *ops = NULL;

	ops = get_os_ops();

	opriv = ops->mem_zalloc(sizeof(struct nvlsi_rpu_osal_priv));

	if (!opriv)
		goto out;

	opriv->ops = ops;

out:
	return opriv;
}


void nvlsi_rpu_osal_deinit(struct nvlsi_rpu_osal_priv *opriv)
{
	struct nvlsi_rpu_osal_ops *ops = NULL;

	ops = opriv->ops;

	ops->mem_free(opriv);
}


void *nvlsi_rpu_osal_mem_alloc(struct nvlsi_rpu_osal_priv *opriv,
			       size_t size)
{
	return opriv->ops->mem_alloc(size);
}


void *nvlsi_rpu_osal_mem_zalloc(struct nvlsi_rpu_osal_priv *opriv,
				size_t size)
{
	return opriv->ops->mem_zalloc(size);
}


void nvlsi_rpu_osal_mem_free(struct nvlsi_rpu_osal_priv *opriv,
			     void *buf)
{
	opriv->ops->mem_free(buf);
}


void *nvlsi_rpu_osal_mem_cpy(struct nvlsi_rpu_osal_priv *opriv,
			     void *dest,
			     const void *src,
			     size_t count)
{
	return opriv->ops->mem_cpy(dest,
				   src,
				   count);
}


void *nvlsi_rpu_osal_mem_set(struct nvlsi_rpu_osal_priv *opriv,
			     void *start,
			     int val,
			     size_t size)
{
	return opriv->ops->mem_set(start,
				   val,
				   size);
}


void *nvlsi_rpu_osal_iomem_mmap(struct nvlsi_rpu_osal_priv *opriv,
				unsigned long addr,
				unsigned long size)
{
	return opriv->ops->iomem_mmap(addr,
				      size);
}


void nvlsi_rpu_osal_iomem_unmap(struct nvlsi_rpu_osal_priv *opriv,
				volatile void *addr)
{
	opriv->ops->iomem_unmap(addr);
}


unsigned int nvlsi_rpu_osal_iomem_read_reg32(struct nvlsi_rpu_osal_priv *opriv,
					     const volatile void *addr)
{
	return opriv->ops->iomem_read_reg32(addr);
}


void nvlsi_rpu_osal_iomem_write_reg32(struct nvlsi_rpu_osal_priv *opriv,
				      volatile void *addr,
				      unsigned int val)
{
	opriv->ops->iomem_write_reg32(addr,
				      val);
}


void nvlsi_rpu_osal_iomem_cpy_from(struct nvlsi_rpu_osal_priv *opriv,
				   void *dest,
				   const volatile void *src,
				   size_t count)
{
	opriv->ops->iomem_cpy_from(dest,
				   src,
				   count);
}


void nvlsi_rpu_osal_iomem_cpy_to(struct nvlsi_rpu_osal_priv *opriv,
				 volatile void *dest,
				 const void *src,
				 size_t count)
{
	opriv->ops->iomem_cpy_to(dest,
				 src,
				 count);
}


void *nvlsi_rpu_osal_spinlock_alloc(struct nvlsi_rpu_osal_priv *opriv)
{
	return opriv->ops->spinlock_alloc();
}


void nvlsi_rpu_osal_spinlock_free(struct nvlsi_rpu_osal_priv *opriv,
				  void *lock)
{
	opriv->ops->spinlock_free(lock);
}


void nvlsi_rpu_osal_spinlock_init(struct nvlsi_rpu_osal_priv *opriv,
				  void *lock)
{
	opriv->ops->spinlock_init(lock);
}


void nvlsi_rpu_osal_spinlock_take(struct nvlsi_rpu_osal_priv *opriv,
				  void *lock)
{
	opriv->ops->spinlock_take(lock);
}


void nvlsi_rpu_osal_spinlock_rel(struct nvlsi_rpu_osal_priv *opriv,
				 void *lock)
{
	opriv->ops->spinlock_rel(lock);
}

void nvlsi_rpu_osal_spinlock_irq_take(struct nvlsi_rpu_osal_priv *opriv,
				      void *lock,
				      unsigned long *flags)
{
	opriv->ops->spinlock_irq_take(lock,
				      flags);
}


void nvlsi_rpu_osal_spinlock_irq_rel(struct nvlsi_rpu_osal_priv *opriv,
				     void *lock,
				     unsigned long *flags)
{
	opriv->ops->spinlock_irq_rel(lock,
				     flags);
}


int nvlsi_rpu_osal_log_dbg(struct nvlsi_rpu_osal_priv *opriv,
			   const char *fmt,
			   ...)
{
	va_list args;
	int ret = -1;

	va_start(args, fmt);

	ret = opriv->ops->log_dbg(fmt, args);

	va_end(args);

	return ret;
}


int nvlsi_rpu_osal_log_info(struct nvlsi_rpu_osal_priv *opriv,
			    const char *fmt,
			    ...)
{
	va_list args;
	int ret = -1;

	va_start(args, fmt);

	ret = opriv->ops->log_info(fmt, args);

	va_end(args);


	return ret;
}


int nvlsi_rpu_osal_log_err(struct nvlsi_rpu_osal_priv *opriv,
			   const char *fmt,
			   ...)
{
	va_list args;
	int ret = -1;

	va_start(args, fmt);

	ret = opriv->ops->log_err(fmt, args);

	va_end(args);

	return ret;
}


void *nvlsi_rpu_osal_llist_node_alloc(struct nvlsi_rpu_osal_priv *opriv)
{
	return opriv->ops->llist_node_alloc();
}


void nvlsi_rpu_osal_llist_node_free(struct nvlsi_rpu_osal_priv *opriv,
				    void *node)
{
	opriv->ops->llist_node_free(node);
}


void *nvlsi_rpu_osal_llist_node_data_get(struct nvlsi_rpu_osal_priv *opriv,
					 void *node)
{
	return opriv->ops->llist_node_data_get(node);
}


void nvlsi_rpu_osal_llist_node_data_set(struct nvlsi_rpu_osal_priv *opriv,
					void *node,
					void *data)
{
	opriv->ops->llist_node_data_set(node,
					data);
}


void *nvlsi_rpu_osal_llist_alloc(struct nvlsi_rpu_osal_priv *opriv)
{
	return opriv->ops->llist_alloc();
}


void nvlsi_rpu_osal_llist_free(struct nvlsi_rpu_osal_priv *opriv,
			       void *llist)
{
	return opriv->ops->llist_free(llist);
}


void nvlsi_rpu_osal_llist_init(struct nvlsi_rpu_osal_priv *opriv,
			       void *llist)
{
	return opriv->ops->llist_init(llist);
}


void nvlsi_rpu_osal_llist_add_node_tail(struct nvlsi_rpu_osal_priv *opriv,
					void *llist,
					void *llist_node)
{
	return opriv->ops->llist_add_node_tail(llist,
					       llist_node);
}


void *nvlsi_rpu_osal_llist_get_node_head(struct nvlsi_rpu_osal_priv *opriv,
					 void *llist)
{
	return opriv->ops->llist_get_node_head(llist);
}


void *nvlsi_rpu_osal_llist_get_node_nxt(struct nvlsi_rpu_osal_priv *opriv,
					void *llist,
					void *llist_node)
{
	return opriv->ops->llist_get_node_nxt(llist,
					      llist_node);
}


void nvlsi_rpu_osal_llist_del_node(struct nvlsi_rpu_osal_priv *opriv,
				   void *llist,
				   void *llist_node)
{
	opriv->ops->llist_del_node(llist,
				   llist_node);
}


unsigned int nvlsi_rpu_osal_llist_len(struct nvlsi_rpu_osal_priv *opriv,
				      void *llist)
{
	return opriv->ops->llist_len(llist);
}


void *nvlsi_rpu_osal_nbuf_alloc(struct nvlsi_rpu_osal_priv *opriv,
				unsigned int size)
{
	return opriv->ops->nbuf_alloc(size);
}


void nvlsi_rpu_osal_nbuf_free(struct nvlsi_rpu_osal_priv *opriv,
			      void *nbuf)
{
	opriv->ops->nbuf_free(nbuf);
}


void nvlsi_rpu_osal_nbuf_headroom_res(struct nvlsi_rpu_osal_priv *opriv,
				      void *nbuf,
				      unsigned int size)
{
	opriv->ops->nbuf_headroom_res(nbuf,
				      size);
}


unsigned int nvlsi_rpu_osal_nbuf_headroom_get(struct nvlsi_rpu_osal_priv *opriv,
					      void *nbuf)
{
	return opriv->ops->nbuf_headroom_get(nbuf);
}


unsigned int nvlsi_rpu_osal_nbuf_data_size(struct nvlsi_rpu_osal_priv *opriv,
					   void *nbuf)
{
	return opriv->ops->nbuf_data_size(nbuf);
}


void *nvlsi_rpu_osal_nbuf_data_get(struct nvlsi_rpu_osal_priv *opriv,
				   void *nbuf)
{
	return opriv->ops->nbuf_data_get(nbuf);
}


void *nvlsi_rpu_osal_nbuf_data_put(struct nvlsi_rpu_osal_priv *opriv,
				   void *nbuf,
				   unsigned int size)
{
	return opriv->ops->nbuf_data_put(nbuf,
					 size);
}


void *nvlsi_rpu_osal_nbuf_data_push(struct nvlsi_rpu_osal_priv *opriv,
				    void *nbuf,
				    unsigned int size)
{
	return opriv->ops->nbuf_data_push(nbuf,
					  size);
}


void *nvlsi_rpu_osal_nbuf_data_pull(struct nvlsi_rpu_osal_priv *opriv,
				    void *nbuf,
				    unsigned int size)
{
	return opriv->ops->nbuf_data_pull(nbuf,
					  size);
}


void *nvlsi_rpu_osal_tasklet_alloc(struct nvlsi_rpu_osal_priv *opriv)
{
	return opriv->ops->tasklet_alloc();
}


void nvlsi_rpu_osal_tasklet_free(struct nvlsi_rpu_osal_priv *opriv,
				 void *tasklet)
{
	opriv->ops->tasklet_free(tasklet);
}


void nvlsi_rpu_osal_tasklet_init(struct nvlsi_rpu_osal_priv *opriv,
				 void *tasklet,
				 void (*callbk_fn)(unsigned long),
				 unsigned long data)
{
	opriv->ops->tasklet_init(tasklet,
				 callbk_fn,
				 data);
}


void nvlsi_rpu_osal_tasklet_schedule(struct nvlsi_rpu_osal_priv *opriv,
				     void *tasklet)
{
	opriv->ops->tasklet_schedule(tasklet);
}


void nvlsi_rpu_osal_tasklet_kill(struct nvlsi_rpu_osal_priv *opriv,
				 void *tasklet)
{
	opriv->ops->tasklet_kill(tasklet);
}


void nvlsi_rpu_osal_sleep_ms(struct nvlsi_rpu_osal_priv *opriv,
			     unsigned int msecs)
{
	opriv->ops->sleep_ms(msecs);
}


void nvlsi_rpu_osal_delay_us(struct nvlsi_rpu_osal_priv *opriv,
			     unsigned long usecs)
{
	opriv->ops->delay_us(usecs);
}


unsigned long nvlsi_rpu_osal_time_get_curr_us(struct nvlsi_rpu_osal_priv *opriv)
{
	return opriv->ops->time_get_curr_us();
}


unsigned int nvlsi_rpu_osal_time_elapsed_us(struct nvlsi_rpu_osal_priv *opriv,
					    unsigned long start_time_us)
{
	return opriv->ops->time_elapsed_us(start_time_us);
}


void *nvlsi_rpu_osal_bus_pcie_init(struct nvlsi_rpu_osal_priv *opriv,
				   const char *dev_name,
				   unsigned int vendor_id,
				   unsigned int sub_vendor_id,
				   unsigned int device_id,
				   unsigned int sub_device_id)
{
	return opriv->ops->bus_pcie_init(dev_name,
					 vendor_id,
					 sub_vendor_id,
					 device_id,
					 sub_device_id);
}


void nvlsi_rpu_osal_bus_pcie_deinit(struct nvlsi_rpu_osal_priv *opriv,
				    void *os_pcie_priv)
{
	opriv->ops->bus_pcie_deinit(os_pcie_priv);
}


void *nvlsi_rpu_osal_bus_pcie_dev_add(struct nvlsi_rpu_osal_priv *opriv,
				      void *os_pcie_priv,
				      void *osal_pcie_dev_ctx)
{
	return opriv->ops->bus_pcie_dev_add(os_pcie_priv,
					    osal_pcie_dev_ctx);

}


void nvlsi_rpu_osal_bus_pcie_dev_rem(struct nvlsi_rpu_osal_priv *opriv,
				     void *os_pcie_dev_ctx)
{
	return opriv->ops->bus_pcie_dev_rem(os_pcie_dev_ctx);
}


enum nvlsi_rpu_status nvlsi_rpu_osal_bus_pcie_dev_init(struct nvlsi_rpu_osal_priv *opriv,
						       void *os_pcie_dev_ctx)
{
	return opriv->ops->bus_pcie_dev_init(os_pcie_dev_ctx);

}


void nvlsi_rpu_osal_bus_pcie_dev_deinit(struct nvlsi_rpu_osal_priv *opriv,
					void *os_pcie_dev_ctx)
{
	return opriv->ops->bus_pcie_dev_deinit(os_pcie_dev_ctx);
}


enum nvlsi_rpu_status nvlsi_rpu_osal_bus_pcie_dev_intr_reg(struct nvlsi_rpu_osal_priv *opriv,
							   void *os_pcie_dev_ctx,
							   void *callbk_data,
							   int (*callbk_fn)(void *callbk_data))
{
	return opriv->ops->bus_pcie_dev_intr_reg(os_pcie_dev_ctx,
						 callbk_data,
						 callbk_fn);
}


void nvlsi_rpu_osal_bus_pcie_dev_intr_unreg(struct nvlsi_rpu_osal_priv *opriv,
					    void *os_pcie_dev_ctx)
{
	opriv->ops->bus_pcie_dev_intr_unreg(os_pcie_dev_ctx);
}


void *nvlsi_rpu_osal_bus_pcie_dev_dma_map(struct nvlsi_rpu_osal_priv *opriv,
					  void *os_pcie_dev_ctx,
					  void *virt_addr,
					  size_t size,
					  enum nvlsi_rpu_osal_dma_dir dir)
{
	return opriv->ops->bus_pcie_dev_dma_map(os_pcie_dev_ctx,
						virt_addr,
						size,
						dir);
}


void nvlsi_rpu_osal_bus_pcie_dev_dma_unmap(struct nvlsi_rpu_osal_priv *opriv,
					   void *os_pcie_dev_ctx,
					   void *dma_addr,
					   size_t size,
					   enum nvlsi_rpu_osal_dma_dir dir)
{
	opriv->ops->bus_pcie_dev_dma_unmap(os_pcie_dev_ctx,
					   dma_addr,
					   size,
					   dir);
}


void nvlsi_rpu_osal_bus_pcie_dev_host_map_get(struct nvlsi_rpu_osal_priv *opriv,
					      void *os_pcie_dev_ctx,
					      struct nvlsi_rpu_osal_host_map *host_map)
{
	opriv->ops->bus_pcie_dev_host_map_get(os_pcie_dev_ctx,
					      host_map);
}


void *nvlsi_rpu_osal_bus_qspi_init(struct nvlsi_rpu_osal_priv *opriv)
{
	return opriv->ops->bus_qspi_init();
}


void nvlsi_rpu_osal_bus_qspi_deinit(struct nvlsi_rpu_osal_priv *opriv,
				    void *os_qspi_priv)
{
	opriv->ops->bus_qspi_deinit(os_qspi_priv);
}


void *nvlsi_rpu_osal_bus_qspi_dev_add(struct nvlsi_rpu_osal_priv *opriv,
				      void *os_qspi_priv,
				      void *osal_qspi_dev_ctx)
{
	return opriv->ops->bus_qspi_dev_add(os_qspi_priv,
					    osal_qspi_dev_ctx);
}


void nvlsi_rpu_osal_bus_qspi_dev_rem(struct nvlsi_rpu_osal_priv *opriv,
				     void *os_qspi_dev_ctx)
{
	opriv->ops->bus_qspi_dev_rem(os_qspi_dev_ctx);
}


enum nvlsi_rpu_status nvlsi_rpu_osal_bus_qspi_dev_init(struct nvlsi_rpu_osal_priv *opriv,
						       void *os_qspi_dev_ctx)
{
	return opriv->ops->bus_qspi_dev_init(os_qspi_dev_ctx);
}


void nvlsi_rpu_osal_bus_qspi_dev_deinit(struct nvlsi_rpu_osal_priv *opriv,
					void *os_qspi_dev_ctx)
{
	opriv->ops->bus_qspi_dev_deinit(os_qspi_dev_ctx);
}


enum nvlsi_rpu_status nvlsi_rpu_osal_bus_qspi_dev_intr_reg(struct nvlsi_rpu_osal_priv *opriv,
							   void *os_qspi_dev_ctx,
							   void *callbk_data,
							   int (*callbk_fn)(void *callbk_data))
{
	return opriv->ops->bus_qspi_dev_intr_reg(os_qspi_dev_ctx,
						 callbk_data,
						 callbk_fn);
}


void nvlsi_rpu_osal_bus_qspi_dev_intr_unreg(struct nvlsi_rpu_osal_priv *opriv,
					    void *os_qspi_dev_ctx)
{
	opriv->ops->bus_qspi_dev_intr_unreg(os_qspi_dev_ctx);
}


void nvlsi_rpu_osal_bus_qspi_dev_host_map_get(struct nvlsi_rpu_osal_priv *opriv,
					      void *os_qspi_dev_ctx,
					      struct nvlsi_rpu_osal_host_map *host_map)
{
	opriv->ops->bus_qspi_dev_host_map_get(os_qspi_dev_ctx,
					      host_map);
}


unsigned int nvlsi_rpu_osal_qspi_read_reg32(struct nvlsi_rpu_osal_priv *opriv,
					    void *priv,
					    unsigned long addr)
{
	return opriv->ops->qspi_read_reg32(priv,
					   addr);
}


void nvlsi_rpu_osal_qspi_write_reg32(struct nvlsi_rpu_osal_priv *opriv,
				     void *priv,
				     unsigned long addr,
				     unsigned int val)
{
	opriv->ops->qspi_write_reg32(priv,
				     addr,
				     val);
}


void nvlsi_rpu_osal_qspi_cpy_from(struct nvlsi_rpu_osal_priv *opriv,
				  void *priv,
				  void *dest,
				  unsigned long addr,
				  size_t count)
{
	opriv->ops->qspi_cpy_from(priv,
				  dest,
				  addr,
				  count);
}


void nvlsi_rpu_osal_qspi_cpy_to(struct nvlsi_rpu_osal_priv *opriv,
				void *priv,
				unsigned long addr,
				const void *src,
				size_t count)
{
	opriv->ops->qspi_cpy_to(priv,
				addr,
				src,
				count);
}
