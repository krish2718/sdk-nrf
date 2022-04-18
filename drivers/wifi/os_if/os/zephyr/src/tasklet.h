/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @brief Header containing tasklet specific declarations for the
 * Zephyr OS layer of the Wi-Fi driver.
 */

#ifndef __TASKLET_H__
#define __TASKLET_H__

struct tasklet_struct {
	struct k_sem lock;
	void (*callback)(unsigned long data);
	unsigned long data;
	k_tid_t tid;
	struct k_thread tdata;
};

void tasklet_init(struct tasklet_struct *tasklet,
		  void (*callback)(unsigned long callbk_data),
		  unsigned long data);

void tasklet_schedule(struct tasklet_struct *tasklet);

void tasklet_kill(struct tasklet_struct *tasklet);

#endif /* __TASKLET_H__ */
