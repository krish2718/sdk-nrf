/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @brief File containing SPI device interface specific definitions for the
 * Zephyr OS layer of the Wi-Fi driver.
 */

#include <drivers/spi.h>
#include <nrfx_spim.h>
#include <string.h>

#include "qspi_if.h"
#include "spi_if.h"

#include <logging/log.h>

LOG_MODULE_DECLARE(wifi_nrf, CONFIG_WIFI_NRF_LOG_LEVEL);

struct qspi_config *spim_config;

struct spi_cs_control cs_contrl = {
	.gpio = GPIO_DT_SPEC_GET(DT_NODELABEL(spi4), cs_gpios)
};

static const struct spi_config spi_cfg = {
	.operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB,
	.frequency = 8000000,
	.slave = 0,
	.cs = &cs_contrl
};

const struct device *spim_perip;

const struct device *spim_perip_get(void)
{
	const struct device *spi_perip;

#ifdef CONFIG_BOARD_NRF52840DK_NRF52840
	spi_perip = device_get_binding("SPI_3");
#elif defined(CONFIG_BOARD_NRF7002DK_NRF5340_CPUAPP) || defined(CONFIG_BOARD_NRF5340DK_NRF5340_CPUAPP)
	spi_perip = device_get_binding("SPI_4");
#endif

	if (spi_perip == NULL) {
		LOG_ERR("Could not get SPI device\n");
		return NULL;
	}

	LOG_INF("%p : got device\n", spi_perip);

	return spi_perip;
}

unsigned int spim_xfer_tx(const struct device *spi_perip, const struct spi_config *spi_cfg,
			  unsigned int addr, const void *data, unsigned int len)
{
	int err;
	static uint8_t *tx_buffer;

	tx_buffer = (uint8_t *)(k_malloc(4 + len));

	const struct spi_buf tx_buf = { .buf = tx_buffer, .len = 4 + len };
	const struct spi_buf_set tx = { .buffers = &tx_buf, .count = 1 };

	/* PP opcode : 0x02 */
	tx_buffer[0] = 0x02;

	/* addr */
	tx_buffer[1] = (((addr >> 16) & 0xFF) | 0x80);
	tx_buffer[2] = (addr >> 8) & 0xFF;
	tx_buffer[3] = addr & 0xFF;

	/* data */
	memcpy(tx_buffer + 4, data, len);

	err = spi_transceive(spi_perip, spi_cfg, &tx, NULL);

	k_free(tx_buffer);

	return err;
}

unsigned int spim_xfer_rx(const struct device *spi_perip, const struct spi_config *spi_cfg,
			  unsigned int addr, void *data, unsigned int len)
{
	int err;
	int dummy = 5;
	uint8_t *tx_buffer = NULL;
	uint8_t *rx_buffer = NULL;

	tx_buffer = (uint8_t *)(k_malloc(dummy + len));

	if (tx_buffer == NULL) {
		LOG_ERR("SPI error:NO MEMORY for tx_buffer\n");
		return -ENOMEM;
	}

	rx_buffer = (uint8_t *)(k_malloc(dummy + len));

	if (rx_buffer == NULL) {
		LOG_ERR("SPI error:NO MEMORY rx_buffer\n");
		k_free(tx_buffer);
		return -ENOMEM;
	}

	memset(tx_buffer, 0, len + dummy);
	memset(rx_buffer, 0, len + dummy);

	const struct spi_buf tx_buf = { .buf = tx_buffer, .len = (len + dummy) };
	const struct spi_buf_set tx = { .buffers = &tx_buf, .count = 1 };

	struct spi_buf rx_buf = {
		.buf = rx_buffer,
		.len = (len + dummy),
	};
	const struct spi_buf_set rx = { .buffers = &rx_buf, .count = 1 };

	/* FASTREAD opcode : 0x0b */

	tx_buffer[0] = 0x0b;
	tx_buffer[1] = (addr >> 16) & 0xFF;
	tx_buffer[2] = (addr >> 8) & 0xFF;
	tx_buffer[3] = addr & 0xFF;

	err = spi_transceive(spi_perip, spi_cfg, &tx, &rx);
	memcpy(data, rx_buffer + dummy, len);

	if (err)
		LOG_ERR("SPI error: %d\n", err);

	k_free(tx_buffer);
	k_free(rx_buffer);

	return err;
}

int spim_RDSR1(const struct device *spi_perip)
{
	int err, len;
	static uint8_t tx_buffer[6] = { 0x1f };
	const struct spi_config *spim_cfg = &spi_cfg;

	len = sizeof(tx_buffer);

	const struct spi_buf tx_buf = { .buf = tx_buffer, .len = len };
	const struct spi_buf_set tx = { .buffers = &tx_buf, .count = 1 };

	uint8_t sr[6];

	struct spi_buf rx_buf = {
		.buf = &sr,
		.len = sizeof(sr),
	};
	const struct spi_buf_set rx = { .buffers = &rx_buf, .count = 1 };

	err = spi_transceive(spi_perip, spim_cfg, &tx, &rx);

	if(err == 0)
		return sr[1];

	return err;
}

int spim_wait_while_rpu_awake(const struct device *spi_perip,
				       const struct spi_config *spi_cfg)
{
	int ret;

	for (int ii = 0; ii < 1; ii++) {

		ret = spim_RDSR1(spi_perip);

		if ((ret < 0) || ((ret & RPU_AWAKE_BIT) == 0)) {
			LOG_INF("RDSR1 = 0x%x\t\n", ret);
		} else {
			LOG_INF("RDSR1 = 0x%x\n", ret);
			break;
		}
		k_msleep(1);
	}

	return ret;
}

/* Wait until RDSR2 confirms RPU_WAKE write is successful */
int spim_validate_rpu_awake(const struct device *spi_perip,
		const struct spi_config *spi_cfg)
{
	int err, len;
	static uint8_t tx_buffer[6] = { 0x2f };

	len = sizeof(tx_buffer);

	const struct spi_buf tx_buf = { .buf = tx_buffer, .len = len };
	const struct spi_buf_set tx = { .buffers = &tx_buf, .count = 1 };

	uint8_t sr[6];

	struct spi_buf rx_buf = {
		.buf = &sr,
		.len = sizeof(sr),
	};
	const struct spi_buf_set rx = { .buffers = &rx_buf, .count = 1 };

	for (int ii = 0; ii < 1; ii++) {
		err = spi_transceive(spi_perip, spi_cfg, &tx, &rx);

		LOG_DBG("%x %x %x %x %x %x\n", sr[0], sr[1], sr[2], sr[3], sr[4], sr[5]);

		if ((err < 0) || ((sr[1] & RPU_AWAKE_BIT) == 0)) {
			LOG_INF("RDSR2 = 0x%x\n", sr[1]);
		} else {
			LOG_INF("RDSR2 = 0x%x\n", sr[1]);
			break;
		}
		/* k_msleep(1); */
	}

	return 0;
}

int spim_cmd_rpu_wakeup(const struct device *spi_perip, const struct spi_config *spi_cfg,
				 uint32_t data)
{
	int err, len;
	static uint8_t tx_buffer[] = { 0x3f, 0x1 };

	tx_buffer[1] = data & 0xff;

	/* printf("TODO : %s:\n", __func__); */

	len = sizeof(tx_buffer);

	const struct spi_buf tx_buf = { .buf = tx_buffer, .len = len };
	const struct spi_buf_set tx = { .buffers = &tx_buf, .count = 1 };

	err = spi_transceive(spi_perip, spi_cfg, &tx, NULL);

	if (err) {
		LOG_ERR("SPI error: %d\n", err);
	} else {
		/* Connect MISO to MOSI for loopback */
		/* LOG_ERR("TX sent: %x\n", tx_buffer[0]); */
	}

	return 0;
}

unsigned int spim_cmd_sleep_rpu(const struct device *spi_perip, const struct spi_config *spi_cfg)
{
	int err, len;
	static uint8_t tx_buffer[] = { 0x3f, 0x0 };

	/* printf("TODO : %s:\n", __func__); */

	len = sizeof(tx_buffer);

	const struct spi_buf tx_buf = { .buf = tx_buffer, .len = len };
	const struct spi_buf_set tx = { .buffers = &tx_buf, .count = 1 };

	err = spi_transceive(spi_perip, spi_cfg, &tx, NULL);

	if (err) {
		LOG_ERR("SPI error: %d\n", err);
	} else {
		/* Connect MISO to MOSI for loopback */
		/* LOG_ERR("TX sent: %x\n", tx_buffer[0]); */
	}

	return 0;
}

int spim_init(struct qspi_config *config)
{
	spim_perip = spim_perip_get();

	spim_config = config;

#if defined(CONFIG_BOARD_NRF7002DK_NRF5340_CPUAPP) || defined(CONFIG_BOARD_NRF5340DK_NRF5340_CPUAPP)
	/* SPIM4 (<32Mbps)  : 1Mbps -> 1Mhz */
	config->spimfreq = config->freq * 1000000;
#endif
#ifdef CONFIG_BOARD_NRF52840DK_NRF52840
	/* SPIM3 (<8Mbps): 1Mbps -> 1Mhz */
	config->spimfreq = config->freq * 1000000;
#endif

	k_sem_init(&spim_config->lock, 1, 1);

	return 0;
}

void spim_addr_check(unsigned int addr, const void *data, unsigned int len)
{
	if ((addr % 4 != 0) || (((unsigned int)data) % 4 != 0) || (len % 4 != 0)) {
		LOG_ERR("%s : Unaligned address %x %x %d %x %x\n", __func__, addr,
		       (unsigned int)data, (addr % 4 != 0), (((unsigned int)data) % 4 != 0),
		       (len % 4 != 0));
	}
}

int spim_write(unsigned int addr, const void *data, int len)
{
	int status;

	spim_addr_check(addr, data, len);

	addr |= spim_config->addrmask;

	k_sem_take(&spim_config->lock, K_FOREVER);

	status = spim_xfer_tx(spim_perip, &spi_cfg, addr, data, len);

	k_sem_give(&spim_config->lock);

	return status;
}

int spim_read(unsigned int addr, void *data, int len)
{
	int status;

	spim_addr_check(addr, data, len);

	addr |= spim_config->addrmask;

	k_sem_take(&spim_config->lock, K_FOREVER);

	status = spim_xfer_rx(spim_perip, &spi_cfg, addr, data, len);

	k_sem_give(&spim_config->lock);

	return status;
}

int spim_hl_readw(unsigned int addr, void *data)
{
	uint8_t *rxb = NULL;
	int status = -1;
	int len = 4;

	len = len + (4 * spim_config->qspi_slave_latency);

	rxb = k_malloc(len);

	if (rxb == NULL) {
		LOG_ERR("%s: ERROR ENOMEM line %d\n", __func__, __LINE__);
		return -ENOMEM;
	}
	memset(rxb, 0, len);

	k_sem_take(&spim_config->lock, K_FOREVER);

	status = spim_xfer_rx(spim_perip, &spi_cfg, addr, rxb, len);

	k_sem_give(&spim_config->lock);

	*(uint32_t *)data = *(uint32_t *)(rxb + (len - 4));

	k_free(rxb);

	return status;
}

int spim_hl_read(unsigned int addr, void *data, int len)
{
	int count = 0;

	spim_addr_check(addr, data, len);

	while (count < (len / 4)) {
		spim_hl_readw(addr + (4 * count), (char *)data + (4 * count));
		count++;
	}

	return 0;
}

/* ------------------------------added for wifi utils -------------------------------- */

int spim_cmd_rpu_wakeup_fn(const struct device *spi_perip, uint32_t data)
{
	return spim_cmd_rpu_wakeup(spim_perip, &spi_cfg, data);
}

int spim_cmd_sleep_rpu_fn(const struct device *spi_perip)
{
	return spim_cmd_sleep_rpu(spim_perip, &spi_cfg);
}

int spim_wait_while_rpu_awake_fn(const struct device *spi_perip)
{
	return spim_wait_while_rpu_awake(spim_perip, &spi_cfg);
}

int spim_validate_rpu_awake_fn(const struct device *spi_perip)
{
	return spim_validate_rpu_awake(spim_perip, &spi_cfg);
}
