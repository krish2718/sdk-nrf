/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @brief File containing tasklet specific definitions for the
 * Zephyr OS layer of the Wi-Fi driver.
 */

#include <zephyr.h>
#include <sys/printk.h>

#include "tasklet.h"

#define STACKSIZE 4096
#define PRIORITY 0

K_THREAD_STACK_DEFINE(thread1_stack_area, STACKSIZE);

void tasklet_cb(void *dummy1, void *dummy2, void *dummy3)
{
	struct tasklet_struct *tasklet = dummy1;

	while (1) {

		k_sem_take(&tasklet->lock, K_FOREVER);

		tasklet->callback(tasklet->data);
	}
}

void tasklet_init(struct tasklet_struct *tasklet,
						 void (*callback)(unsigned long),
						 unsigned long data)
{
	tasklet->callback = callback;
	tasklet->data = data;

	printk("TODO : %s\n", __func__);
	printk("TODO : Can handle only 1 thread creation\n");
	printk("TODO : Use Meta-IRQ Priorities ??\n");

	k_sem_init(&tasklet->lock, 1, 1);

	tasklet->tid =
	k_thread_create(&tasklet->tdata, thread1_stack_area,
			K_THREAD_STACK_SIZEOF(thread1_stack_area),
			tasklet_cb, tasklet, NULL, NULL,
			PRIORITY, 0, K_NO_WAIT);
}

void tasklet_schedule(struct tasklet_struct *tasklet)
{
	k_sem_give(&tasklet->lock);
}

void tasklet_kill(struct tasklet_struct *tasklet)
{
	k_thread_abort(tasklet->tid);
}
