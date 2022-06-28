/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @brief File containing work specific definitions for the
 * Zephyr OS layer of the Wi-Fi driver.
 */

#include <zephyr.h>
#include <sys/printk.h>
#include <logging/log.h>

#include "zephyr_work.h"

#define STACKSIZE 4096
#define PRIORITY 0
#define MAX_WORK_ITEMS 10

LOG_MODULE_DECLARE(wifi_nrf, CONFIG_WIFI_NRF_LOG_LEVEL);

K_THREAD_STACK_DEFINE(wq_stack_area, STACKSIZE);
struct k_work_q zep_wifi_drv_q;

struct zep_work_item zep_work_item[MAX_WORK_ITEMS];

int get_free_work_item_index(void)
{
	unsigned int i;

	for (i = 0; i < MAX_WORK_ITEMS; i++) {
		if (zep_work_item[i].in_use)
			continue;
		return i;
	}

	return -1;
}

void workqueue_callback(struct k_work *work)
{
	struct zep_work_item *item = CONTAINER_OF(work, struct zep_work_item, work);

	item->callback(item->data);
}

struct zep_work_item *work_alloc(void)
{
	unsigned int free_work_index = get_free_work_item_index();

	if (free_work_index < 0) {
		LOG_ERR("%s: Reached maximum work items", __func__);
		return NULL;
	}

	return &zep_work_item[free_work_index];
}

void work_init(struct zep_work_item *item, void (*callback)(unsigned long),
		  unsigned long data)
{
	if (zep_wifi_drv_q.flags != K_WORK_QUEUE_STARTED) {
		k_work_queue_init(&zep_wifi_drv_q);

		k_work_queue_start(&zep_wifi_drv_q,
						   wq_stack_area,
						   K_THREAD_STACK_SIZEOF(wq_stack_area),
						   PRIORITY,
						   NULL);
	}

	item->callback = callback;
	item->data = data;

	k_work_init(&item->work, workqueue_callback);
}

void work_schedule(struct zep_work_item *item)
{
	k_work_submit_to_queue(&zep_wifi_drv_q, &item->work);
}

void work_kill(struct zep_work_item *item)
{
	// TODO: Based on context, use _sync version
	k_work_cancel(&item->work);
}

void work_free(struct zep_work_item *item)
{
	item->in_use = 0;
}
