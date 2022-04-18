/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @brief Header containing SPI device interface specific declarations for the
 * Zephyr OS layer of the Wi-Fi driver.
 */

/* SPIM driver config */

#ifdef CONFIG_BOARD_NRF52840DK_NRF52840

#define CONFIG_SPI_3_NRF_SPIM           1
#define CONFIG_SPI_3_NRF_ORC            0xff
#define CONFIG_SPI_3_NRF_RX_DELAY       2

#elif CONFIG_BOARD_NRF5340DK_NRF5340_CPUAPP

#define CONFIG_SPI_4_NRF_SPIM           1
#define CONFIG_SPI_4_NRF_ORC            0xff
#define CONFIG_SPI_4_NRF_RX_DELAY       2
#define CONFIG_SPI_INIT_PRIORITY        70
#define CONFIG_SPI_NRFX_RAM_BUFFER_SIZE 8

#endif

#define CONFIG_SPI_INIT_PRIORITY        70
#define CONFIG_SPI_NRFX_RAM_BUFFER_SIZE 8

#define QSPI_NODE DT_NODELABEL(qspi)
#define QSPI_PROP_AT(prop, idx) DT_PROP_BY_IDX(QSPI_NODE, prop, idx)

int spim_init(struct qspi_config *config);

int spim_write(unsigned int addr, const void *data, int len);

int spim_read(unsigned int addr, void *data, int len);

int spim_hl_read(unsigned int addr, void *data, int len);
