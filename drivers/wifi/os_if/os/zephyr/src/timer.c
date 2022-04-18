/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @brief File containing timer specific definitons for the
 * Zephyr OS layer of the Wi-Fi driver.
 */

#include <zephyr.h>
#include <sys/printk.h>
#include <drivers/gpio.h>
#include <stdio.h>
#include <string.h>

#include "timer.h"

void my_expiry_function(struct k_timer *timer_id)
{
	struct timer_list *timer;

	timer = k_timer_user_data_get(timer_id);

	timer->function(timer->data);
}

void init_timer(struct timer_list *timer)
{
	k_timer_init(&timer->k_timer, my_expiry_function, NULL);

	k_timer_user_data_set(&timer->k_timer, timer);
}

void mod_timer(struct timer_list *timer, int msec)
{
	k_timer_start(&timer->k_timer, K_MSEC(msec), K_FOREVER);
}

void del_timer_sync(struct timer_list *timer)
{
	k_timer_status_sync(&timer->k_timer);

	k_timer_stop(&timer->k_timer);
}
