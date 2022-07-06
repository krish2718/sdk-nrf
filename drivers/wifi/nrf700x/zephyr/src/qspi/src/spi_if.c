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
#include <string.h>

#include "qspi_if.h"
#include "spi_if.h"

#include <logging/log.h>

LOG_MODULE_DECLARE(wifi_nrf, CONFIG_WIFI_NRF_LOG_LEVEL);

#define NRF7002_NODE DT_NODELABEL(nrf7002)

static struct qspi_config *spim_config;

static const struct spi_dt_spec spi_spec =
SPI_DT_SPEC_GET(NRF7002_NODE, SPI_WORD_SET(8) | SPI_TRANSFER_MSB, 0);

static int spim_xfer_tx(unsigned int addr, void *data, unsigned int len)
{
	int err;
	uint8_t hdr[4] = {
		0x02, /* PP opcode */
		(((addr >> 16) & 0xFF) | 0x80),
		(addr >> 8) & 0xFF,
		(addr & 0xFF)
	};

	const struct spi_buf tx_buf[] = {
		{.buf = hdr,  .len = sizeof(hdr) },
		{.buf = data, .len = len },
	};
	const struct spi_buf_set tx = { .buffers = tx_buf, .count = 2 };


	err = spi_transceive_dt(&spi_spec, &tx, NULL);

	return err;
}


static int spim_xfer_rx(unsigned int addr, void *data, unsigned int len, unsigned int discard_bytes)
{
	uint8_t hdr[] = {
		0x0b, /* FASTREAD opcode */
		(addr >> 16) & 0xFF,
		(addr >> 8) & 0xFF,
		addr & 0xFF,
		0 /* dummy byte */
	};

	const struct spi_buf tx_buf[] = {
		{.buf = hdr,  .len = sizeof(hdr) },
		{.buf = NULL, .len = len },
	};

	const struct spi_buf_set tx = { .buffers = tx_buf, .count = 2 };

	const struct spi_buf rx_buf[] = {
		{.buf = NULL,  .len = sizeof(hdr) + discard_bytes},
		{.buf = data, .len = len },
	};

	const struct spi_buf_set rx = { .buffers = rx_buf, .count = 2 };

	return spi_transceive_dt(&spi_spec, &tx, &rx);
}

int spim_RDSR1(void)
{
	int err;
	uint8_t tx_buffer[6] = { 0x1f };

	const struct spi_buf tx_buf = {
		.buf = tx_buffer,
		.len = sizeof(tx_buffer)
	};

	const struct spi_buf_set tx = {
		.buffers = &tx_buf,
		.count = 1
	};

	uint8_t sr[6];

	struct spi_buf rx_buf = {
		.buf = &sr,
		.len = sizeof(sr),
	};
	const struct spi_buf_set rx = { .buffers = &rx_buf, .count = 1 };

	err = spi_transceive_dt(&spi_spec, &tx, &rx);

	if (err == 0)
		return sr[1];

	return err;
}

int spim_wait_while_rpu_awake(void)
{
	int ret;

	for (int ii = 0; ii < 10; ii++) {

		ret = spim_RDSR1();

		if ((ret < 0) || ((ret & RPU_AWAKE_BIT) == 0)) {
			LOG_DBG("RDSR1 = 0x%x\t\n", ret);
		} else {
			LOG_DBG("RDSR1 = 0x%x\n", ret);
			break;
		}
		k_msleep(1);
	}

	return ret;
}

/* Wait until RDSR2 confirms RPU_WAKE write is successful */
int spim_validate_rpu_awake(void)
{
	int err;
	uint8_t tx_buffer[6] = { 0x2f };

	const struct spi_buf tx_buf = {
		.buf = tx_buffer,
		.len = sizeof(tx_buffer)
	};
	const struct spi_buf_set tx = {
		.buffers = &tx_buf,
		.count = 1
	};

	uint8_t sr[6];

	struct spi_buf rx_buf = {
		.buf = &sr,
		.len = sizeof(sr),
	};
	const struct spi_buf_set rx = {
		.buffers = &rx_buf,
		.count = 1
	};

	for (int ii = 0; ii < 1; ii++) {
		err = spi_transceive_dt(&spi_spec, &tx, &rx);

		LOG_DBG("%x %x %x %x %x %x\n", sr[0], sr[1], sr[2], sr[3], sr[4], sr[5]);

		if ((err < 0) || ((sr[1] & RPU_AWAKE_BIT) == 0)) {
			LOG_DBG("RDSR2 = 0x%x\n", sr[1]);
		} else {
			LOG_DBG("RDSR2 = 0x%x\n", sr[1]);
			break;
		}
		/* k_msleep(1); */
	}

	return 0;
}

int spim_cmd_rpu_wakeup(uint32_t data)
{
	int err;
	uint8_t tx_buffer[] = { 0x3f, data & 0xff };

	const struct spi_buf tx_buf = { .buf = tx_buffer, .len = sizeof(tx_buffer) };
	const struct spi_buf_set tx = { .buffers = &tx_buf, .count = 1 };

	err = spi_transceive_dt(&spi_spec, &tx, NULL);

	if (err) {
		LOG_ERR("SPI error: %d\n", err);
	}

	return 0;
}

unsigned int spim_cmd_sleep_rpu(void)
{
	int err;
	uint8_t tx_buffer[] = { 0x3f, 0x0 };

	const struct spi_buf tx_buf = { .buf = tx_buffer, .len = sizeof(tx_buffer) };
	const struct spi_buf_set tx = { .buffers = &tx_buf, .count = 1 };

	err = spi_transceive_dt(&spi_spec, &tx, NULL);

	if (err) {
		LOG_ERR("SPI error: %d\n", err);
	}

	return 0;
}

int spim_init(struct qspi_config *config)
{
	if (!spi_is_ready(&spi_spec)) {
		LOG_ERR("Device %s is not ready\n", spi_spec.bus->name);
		return -ENODEV;
	}

	spim_config = config;

	k_sem_init(&spim_config->lock, 1, 1);

	return 0;
}

static void spim_addr_check(unsigned int addr, const void *data, unsigned int len)
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

	status = spim_xfer_tx(addr, (void *)data, len);

	k_sem_give(&spim_config->lock);

	return status;
}

int spim_read(unsigned int addr, void *data, int len)
{
	int status;

	spim_addr_check(addr, data, len);

	addr |= spim_config->addrmask;

	k_sem_take(&spim_config->lock, K_FOREVER);

	status = spim_xfer_rx(addr, data, len, 0);

	k_sem_give(&spim_config->lock);

	return status;
}

static int spim_hl_readw(unsigned int addr, void *data)
{
	int status = -1;
	int len = 4;

	len = len + (4 * spim_config->qspi_slave_latency);

	k_sem_take(&spim_config->lock, K_FOREVER);

	status = spim_xfer_rx(addr, data, len, len-4);

	k_sem_give(&spim_config->lock);

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

int spim_cmd_rpu_wakeup_fn(uint32_t data)
{
	return spim_cmd_rpu_wakeup(data);
}

int spim_cmd_sleep_rpu_fn(void)
{
	return spim_cmd_sleep_rpu();
}

int spim_wait_while_rpu_awake_fn(void)
{
	return spim_wait_while_rpu_awake();
}

int spim_validate_rpu_awake_fn(void)
{
	return spim_validate_rpu_awake();
}
