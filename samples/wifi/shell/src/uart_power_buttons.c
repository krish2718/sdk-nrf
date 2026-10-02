/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <dk_buttons_and_leds.h>
#include <nrf71_idle_power.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	if ((has_changed & DK_BTN1_MSK) && (button_state & DK_BTN1_MSK)) {
		nrf71_idle_power_suspend_console();
	} else if ((has_changed & DK_BTN2_MSK) && (button_state & DK_BTN2_MSK)) {
		nrf71_idle_power_resume_console();
		printk("Console UART resumed (Button 2)\n");
	}
}

int uart_power_buttons_init(void)
{
	return dk_buttons_init(button_handler);
}
