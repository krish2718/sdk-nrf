/* main.c - Application main entry point */

/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <zephyr/logging/log.h>

#include <zephyr/types.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/printk.h>
#include <zephyr/linker/sections.h>
#include <zephyr/random/random.h>

#include <zephyr/ztest.h>


#if defined(CONFIG_WIFI_LOG_LEVEL_DBG)
#define DBG(fmt, ...) printk(fmt, ##__VA_ARGS__)
#else
#define DBG(fmt, ...)
#endif


ZTEST(nrf_wifi, test_basic_memtest)
{

	printk("Running basic memory test\n");
}


ZTEST_SUITE(nrf_wifi, NULL, NULL, NULL, NULL, NULL);
