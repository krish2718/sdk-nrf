/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @brief Header containing timer specific declarations for the
 * Zephyr OS layer of the Wi-Fi driver.
 */
#ifndef __TIMER_H__
#define __TIMER_H__

struct timer_list {
	struct k_timer k_timer;
	void (*function)(unsigned long data);
	unsigned long data;
};

void init_timer(struct timer_list *timer);

void mod_timer(struct timer_list *timer, int msec);

void del_timer_sync(struct timer_list *timer);

#endif /* __TIMER_H__ */

