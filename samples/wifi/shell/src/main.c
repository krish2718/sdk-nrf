/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <sys/printk.h>

void main(void)
{
#ifdef CONFIG_BOARD_NRF5340DK_NRF5340_CPUAPP
#define __SYSTEM_CLOCK_MAX      (128000000UL)
	/* Use CPU freqyency 128MHz i.e., 128MHz/Div1 */
	*((volatile uint32_t *)0x50039530ul) = 0xBEEF0044ul;
	NRF_CLOCK_S->HFCLKCTRL = CLOCK_HFCLKCTRL_HCLK_Div1 << CLOCK_HFCLKCTRL_HCLK_Pos;
	SystemCoreClockUpdate();
#endif
	printk("Starting %s with CPU frequency: %d\n", CONFIG_BOARD, SystemCoreClock);
}
