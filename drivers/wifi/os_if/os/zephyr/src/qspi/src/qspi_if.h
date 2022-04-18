/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @brief Header containing QSPI device interface specific declarations for the
 * Zephyr OS layer of the Wi-Fi driver.
 */

#ifndef __QSPI_IF_H__
#define __QSPI_IF_H__

#include <drivers/gpio.h>
#include <nrfx_qspi.h>

/* QSPI driver config */
#define CONFIG_FLASH_HAS_DRIVER_ENABLED 1
#define CONFIG_FLASH_HAS_PAGE_LAYOUT 1
#define CONFIG_FLASH_JESD216 1
#define CONFIG_FLASH 1
#define CONFIG_FLASH_PAGE_LAYOUT 1
#define CONFIG_SOC_FLASH_NRF 1
#define CONFIG_SOC_FLASH_NRF_RADIO_SYNC_NONE 1
#define CONFIG_NORDIC_QSPI_NOR 1
#define CONFIG_NORDIC_QSPI_NOR_INIT_PRIORITY 80
#define CONFIG_NORDIC_QSPI_NOR_STACK_WRITE_BUFFER_SIZE 0

#define RPU_WAKEUP_NOW BIT(0) /* WAKEUP_NOW */
#define RPU_AWAKE_BIT BIT(1)  /* RPU AWAKE FROM SLEEP */
#define RPU_READY_BIT BIT(2)  /* RPI IS READY */

struct qspi_config {
	nrf_qspi_addrmode_t addrmode;
	nrf_qspi_readoc_t readoc;
	nrf_qspi_writeoc_t writeoc;
	nrf_qspi_frequency_t sckfreq;
	unsigned int freq;
	unsigned int spimfreq;
	unsigned char RDC4IO;
	bool easydma;
	bool single_op;
	bool quad_spi;
	bool encryption;
	bool CMD_CNONCE;
	bool enc_enabled;
	struct k_sem lock;
	unsigned int addrmask;
	unsigned char qspi_slave_latency;
#ifdef CONFIG_BOARD_NRF5340DK_NRF5340_CPUAPP
	nrf_qspi_encryption_t p_cfg;
#endif /*CONFIG_BOARD_NRF5340DK_NRF5340_CPUAPP*/
	int test_hlread;
	char *test_name;
	int test_start;
	int test_end;
	int test_iterations;
	int test_timediff_read;
	int test_timediff_write;
	int test_status;
	int test_iteration;
};
struct qspi_dev {
	int (*deinit)(void);
	void *config;
	int (*init)(struct qspi_config *config);
	int (*write)(unsigned int addr, const void *data, int len);
	int (*read)(unsigned int addr, void *data, int len);
	int (*hl_read)(unsigned int addr, void *data, int len);
	void (*hard_reset)(void);
};

int qspi_cmd_wakeup_rpu(const struct device *dev);

int qspi_init(struct qspi_config *config);

int qspi_write(unsigned int addr, const void *data, int len);

int qspi_read(unsigned int addr, void *data, int len);

int qspi_hl_read(unsigned int addr, void *data, int len);

int qspi_deinit(void);

void gpio_free_irq(int pin, struct gpio_callback *button_cb_data);

int gpio_request_irq(int pin, struct gpio_callback *button_cb_data,
					 void (*irq_handler)());

struct qspi_config *qspi_defconfig(void);

struct qspi_dev *qspi_dev(void);

void hard_reset(void);

#endif /* __QSPI_IF_H__ */
