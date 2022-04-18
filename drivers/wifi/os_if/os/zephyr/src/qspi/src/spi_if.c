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

#define LOG_DOMAIN "spi_nrfx_spim"
#define LOG_LEVEL CONFIG_SPI_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(spi_nrfx_spim);

#include "spi_context.h"
#include "qspi_if.h"
#include "spi_if.h"

struct qspi_config *spim_config;
struct spi_nrfx_data {
	struct spi_context ctx;
	const struct device *dev;
	size_t chunk_len;
	bool   busy;
#ifdef CONFIG_PM_DEVICE
	uint32_t pm_state;
#endif
#if (CONFIG_SPI_NRFX_RAM_BUFFER_SIZE > 0)
	uint8_t   buffer[CONFIG_SPI_NRFX_RAM_BUFFER_SIZE];
#endif
};

struct spi_nrfx_config {
	nrfx_spim_t	   spim;
	size_t		   max_chunk_len;
	uint32_t	   max_freq;
	nrfx_spim_config_t config;
};

static inline struct spi_nrfx_data *get_dev_data(const struct device *dev)
{
	return dev->data;
}

static inline const struct spi_nrfx_config *get_dev_config(const struct device *dev)
{
	return dev->config;
}

static inline nrf_spim_frequency_t get_nrf_spim_frequency(uint32_t frequency)
{
	/* Get the highest supported frequency not exceeding the requested one.
	 */
	if (frequency < 250000) {
		return NRF_SPIM_FREQ_125K;
	} else if (frequency < 500000) {
		return NRF_SPIM_FREQ_250K;
	} else if (frequency < 1000000) {
		return NRF_SPIM_FREQ_500K;
	} else if (frequency < 2000000) {
		return NRF_SPIM_FREQ_1M;
	} else if (frequency < 4000000) {
		return NRF_SPIM_FREQ_2M;
	} else if (frequency < 8000000) {
		return NRF_SPIM_FREQ_4M;
/* Only the devices with HS-SPI can use SPI clock higher than 8 MHz and
 * have SPIM_FREQUENCY_FREQUENCY_M32 defined in their own bitfields.h
 */
#if defined(SPIM_FREQUENCY_FREQUENCY_M32)
	} else if (frequency < 16000000) {
		return NRF_SPIM_FREQ_8M;
	} else if (frequency < 32000000) {
		return NRF_SPIM_FREQ_16M;
	} else {
		return NRF_SPIM_FREQ_32M;
#else
	} else {
		return NRF_SPIM_FREQ_8M;
#endif
	}
}

static inline nrf_spim_mode_t get_nrf_spim_mode(uint16_t operation)
{
	if (SPI_MODE_GET(operation) & SPI_MODE_CPOL) {
		if (SPI_MODE_GET(operation) & SPI_MODE_CPHA)
			return NRF_SPIM_MODE_3;
		else
			return NRF_SPIM_MODE_2;
	} else {
		if (SPI_MODE_GET(operation) & SPI_MODE_CPHA)
			return NRF_SPIM_MODE_1;
		else
			return NRF_SPIM_MODE_0;
	}
}

static inline nrf_spim_bit_order_t get_nrf_spim_bit_order(uint16_t operation)
{
	if (operation & SPI_TRANSFER_LSB)
		return NRF_SPIM_BIT_ORDER_LSB_FIRST;
	else
		return NRF_SPIM_BIT_ORDER_MSB_FIRST;
}

static int configure(const struct device *dev,
		     const struct spi_config *spi_cfg)
{
	struct spi_context *ctx = &get_dev_data(dev)->ctx;
	const nrfx_spim_t *spim = &get_dev_config(dev)->spim;
	nrf_spim_frequency_t freq;

	if (spi_context_configured(ctx, spi_cfg))
		/* Already configured. No need to do it again. */
		return 0;

	if (SPI_OP_MODE_GET(spi_cfg->operation) != SPI_OP_MODE_MASTER) {
		LOG_ERR("Slave mode is not supported on %s",
			    dev->name);
		return -EINVAL;
	}

	if (spi_cfg->operation & SPI_MODE_LOOP) {
		LOG_ERR("Loopback mode is not supported");
		return -EINVAL;
	}

	if ((spi_cfg->operation & SPI_LINES_MASK) != SPI_LINES_SINGLE) {
		LOG_ERR("Only single line mode is supported");
		return -EINVAL;
	}

	if (SPI_WORD_SIZE_GET(spi_cfg->operation) != 8) {
		LOG_ERR("Word sizes other than 8 bits are not supported");
		return -EINVAL;
	}

	if (spi_cfg->frequency < 125000) {
		LOG_ERR("Frequencies lower than 125 kHz are not supported");
		return -EINVAL;
	}

	ctx->config = spi_cfg;
	spi_context_cs_configure(ctx);

	/* Limit the frequency to that supported by the SPIM instance */
	freq = get_nrf_spim_frequency(MIN(spi_cfg->frequency,
					  get_dev_config(dev)->max_freq));

	nrf_spim_configure(spim->p_reg,
			   get_nrf_spim_mode(spi_cfg->operation),
			   get_nrf_spim_bit_order(spi_cfg->operation));

	nrf_spim_frequency_set(spim->p_reg, get_nrf_spim_frequency(spim_config->spimfreq));

	return 0;
}

static void transfer_next_chunk(const struct device *dev)
{
	struct spi_nrfx_data *dev_data = get_dev_data(dev);
	const struct spi_nrfx_config *dev_config = get_dev_config(dev);
	struct spi_context *ctx = &dev_data->ctx;
	int error = 0;

	size_t chunk_len = spi_context_max_continuous_chunk(ctx);

	if (chunk_len > 0) {
		nrfx_spim_xfer_desc_t xfer;
		nrfx_err_t result;
		const uint8_t *tx_buf = ctx->tx_buf;
#if (CONFIG_SPI_NRFX_RAM_BUFFER_SIZE > 0)
		if (spi_context_tx_buf_on(ctx) && !nrfx_is_in_ram(tx_buf)) {
			if (chunk_len > sizeof(dev_data->buffer))
				chunk_len = sizeof(dev_data->buffer);

			memcpy(dev_data->buffer, tx_buf, chunk_len);
			tx_buf = dev_data->buffer;
		}
#endif
		if (chunk_len > dev_config->max_chunk_len)
			chunk_len = dev_config->max_chunk_len;

		dev_data->chunk_len = chunk_len;

		xfer.p_tx_buffer = tx_buf;
		xfer.tx_length   = spi_context_tx_buf_on(ctx) ? chunk_len : 0;
		xfer.p_rx_buffer = ctx->rx_buf;
		xfer.rx_length   = spi_context_rx_buf_on(ctx) ? chunk_len : 0;

		/* This SPIM driver is only used by the NRF52832 if
		 * SOC_NRF52832_ALLOW_SPIM_DESPITE_PAN_58 is enabled
		 */
		if (IS_ENABLED(CONFIG_SOC_NRF52832) &&
		   (xfer.rx_length == 1 && xfer.tx_length <= 1)) {
			LOG_WRN("Transaction aborted since it would trigger nRF52832 PAN 58");
			error = -EIO;
		}

		if (!error) {
			result = nrfx_spim_xfer(&dev_config->spim, &xfer, 0);

			if (result == NRFX_SUCCESS)
				return;

			error = -EIO;
		}
	}

	spi_context_cs_control(ctx, false);

	LOG_DBG("Transaction finished with status %d", error);

	spi_context_complete(ctx, error);
	dev_data->busy = false;
}

static int transceive(const struct device *dev,
		      const struct spi_config *spi_cfg,
		      const struct spi_buf_set *tx_bufs,
		      const struct spi_buf_set *rx_bufs,
		      bool asynchronous,
		      struct k_poll_signal *signal)
{
	struct spi_nrfx_data *dev_data = get_dev_data(dev);
	int error;

	spi_context_lock(&dev_data->ctx, asynchronous, signal, spi_cfg);

	error = configure(dev, spi_cfg);

	if (error == 0) {
		dev_data->busy = true;

		spi_context_buffers_setup(&dev_data->ctx, tx_bufs, rx_bufs, 1);
		spi_context_cs_control(&dev_data->ctx, true);

		transfer_next_chunk(dev);

		error = spi_context_wait_for_completion(&dev_data->ctx);
	}

	spi_context_release(&dev_data->ctx, error);

	return error;
}

static int spi_nrfx_transceive(const struct device *dev,
			       const struct spi_config *spi_cfg,
			       const struct spi_buf_set *tx_bufs,
			       const struct spi_buf_set *rx_bufs)
{
	return transceive(dev, spi_cfg, tx_bufs, rx_bufs, false, NULL);
}

#ifdef CONFIG_SPI_ASYNC
static int spi_nrfx_transceive_async(const struct device *dev,
				     const struct spi_config *spi_cfg,
				     const struct spi_buf_set *tx_bufs,
				     const struct spi_buf_set *rx_bufs,
				     struct k_poll_signal *async)
{
	return transceive(dev, spi_cfg, tx_bufs, rx_bufs, true, async);
}
#endif /* CONFIG_SPI_ASYNC */

static int spi_nrfx_release(const struct device *dev,
			    const struct spi_config *spi_cfg)
{
	struct spi_nrfx_data *dev_data = get_dev_data(dev);

	if (!spi_context_configured(&dev_data->ctx, spi_cfg))
		return -EINVAL;

	if (dev_data->busy)
		return -EBUSY;

	spi_context_unlock_unconditionally(&dev_data->ctx);

	return 0;
}

static const struct spi_driver_api spi_nrfx_driver_api = {
	.transceive = spi_nrfx_transceive,
#ifdef CONFIG_SPI_ASYNC
	.transceive_async = spi_nrfx_transceive_async,
#endif
	.release = spi_nrfx_release,
};


static void event_handler(const nrfx_spim_evt_t *p_event, void *p_context)
{
	struct spi_nrfx_data *dev_data = p_context;

	if (p_event->type == NRFX_SPIM_EVENT_DONE) {
		spi_context_update_tx(&dev_data->ctx, 1, dev_data->chunk_len);
		spi_context_update_rx(&dev_data->ctx, 1, dev_data->chunk_len);

		transfer_next_chunk(dev_data->dev);
	}
}

static int init_spim(const struct device *dev)
{
	struct spi_nrfx_data *data = get_dev_data(dev);
	nrfx_err_t result;

	data->dev = dev;

	/* This sets only default values of frequency, mode and bit order.
	 * The proper ones are set in configure() when a transfer is started.
	 */
	result = nrfx_spim_init(&get_dev_config(dev)->spim,
				&get_dev_config(dev)->config,
				event_handler,
				data);
	if (result != NRFX_SUCCESS) {
		LOG_ERR("Failed to initialize device: %s", dev->name);
		return -EBUSY;
	}

#ifdef CONFIG_PM_DEVICE
	data->pm_state = PM_DEVICE_STATE_ACTIVE;
	get_dev_data(dev)->pm_state = PM_DEVICE_STATE_ACTIVE;
#endif

	return 0;
}

#ifdef CONFIG_PM_DEVICE
static int spim_nrfx_pm_control(const struct device *dev,
				uint32_t ctrl_command,
				uint32_t *state, pm_device_cb cb, void *arg)
{
	int ret = 0;
	struct spi_nrfx_data *data = get_dev_data(dev);
	const struct spi_nrfx_config *config = get_dev_config(dev);

	if (ctrl_command == PM_DEVICE_STATE_SET) {
		uint32_t new_state = *state;

		if (new_state != data->pm_state) {
			switch (new_state) {
			case PM_DEVICE_STATE_ACTIVE:
				ret = init_spim(dev);
				/* Force reconfiguration before next transfer */
				data->ctx.config = NULL;
				break;

			case PM_DEVICE_STATE_LOW_POWER:
			case PM_DEVICE_STATE_SUSPEND:
			case PM_DEVICE_STATE_OFF:
				if (data->pm_state == PM_DEVICE_STATE_ACTIVE)
					nrfx_spim_uninit(&config->spim);

				break;

			default:
				ret = -ENOTSUP;
			}

			if (!ret)
				data->pm_state = new_state;
		}
	} else {
		__ASSERT_NO_MSG(ctrl_command == PM_DEVICE_STATE_GET);
		*state = data->pm_state;
	}

	if (cb)
		cb(dev, ret, state, arg);

	return ret;
}
#endif /* CONFIG_PM_DEVICE */

/*
 * We use NODELABEL here because the nrfx API requires us to call
 * functions which are named according to SoC peripheral instance
 * being operated on. Since DT_INST() makes no guarantees about that,
 * it won't work.
 */
#define SPIM(idx) DT_NODELABEL(spi##idx)
#define SPIM_PROP(idx, prop) DT_PROP(SPIM(idx), prop)

#define SPIM_NRFX_MISO_PULL_DOWN(idx) DT_PROP(SPIM(idx), miso_pull_down)
#define SPIM_NRFX_MISO_PULL_UP(idx) DT_PROP(SPIM(idx), miso_pull_up)

#define SPIM_NRFX_MISO_PULL(idx)			\
	(SPIM_NRFX_MISO_PULL_UP(idx)			\
		? SPIM_NRFX_MISO_PULL_DOWN(idx)		\
			? -1 /* invalid configuration */\
			: NRF_GPIO_PIN_PULLUP		\
		: SPIM_NRFX_MISO_PULL_DOWN(idx)		\
			? NRF_GPIO_PIN_PULLDOWN		\
			: NRF_GPIO_PIN_NOPULL)

#define SPI_NRFX_SPIM_EXTENDED_CONFIG(idx)				\
	IF_ENABLED(NRFX_SPIM_EXTENDED_ENABLED,				\
		(.dcx_pin = NRFX_SPIM_PIN_NOT_USED,			\
		 IF_ENABLED(SPIM##idx##_FEATURE_RXDELAY_PRESENT,	\
			(.rx_delay = CONFIG_SPI_##idx##_NRF_RX_DELAY,))	\
		))

#define SPI_NRFX_SPIM_DEVICE(idx)					       \
	BUILD_ASSERT(						       \
		!SPIM_NRFX_MISO_PULL_UP(idx) || !SPIM_NRFX_MISO_PULL_DOWN(idx),\
		"SPIM"#idx						       \
		": cannot enable both pull-up and pull-down on MISO line");    \
	static int spi_##idx##_init(const struct device *dev)		       \
	{								       \
		IRQ_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_SPIM##idx),		       \
			    DT_IRQ(SPIM(idx), priority),		       \
			    nrfx_isr, nrfx_spim_##idx##_irq_handler, 0);       \
		int err = init_spim(dev);				       \
		spi_context_unlock_unconditionally(&get_dev_data(dev)->ctx);   \
		return err;						       \
	}								       \
	static struct spi_nrfx_data spi_##idx##_data = {		       \
		SPI_CONTEXT_INIT_LOCK(spi_##idx##_data, ctx),		       \
		SPI_CONTEXT_INIT_SYNC(spi_##idx##_data, ctx),		       \
		.busy = false,						       \
	};								       \
	static const struct spi_nrfx_config spi_##idx##z_config = {	       \
		.spim = NRFX_SPIM_INSTANCE(idx),			       \
		.max_chunk_len = (1 << SPIM##idx##_EASYDMA_MAXCNT_SIZE) - 1,   \
		.max_freq = SPIM##idx##_MAX_DATARATE * 1000000,		       \
		.config = {						       \
			.sck_pin   = DT_PROP(QSPI_NODE, sck_pin),              \
			.mosi_pin  = QSPI_PROP_AT(io_pins, 0),		       \
			.miso_pin  = QSPI_PROP_AT(io_pins, 1),		       \
			.ss_pin    = QSPI_PROP_AT(csn_pins, 0),		       \
			.orc       = CONFIG_SPI_##idx##_NRF_ORC,	       \
			.frequency = NRF_SPIM_FREQ_4M,			       \
			.mode      = NRF_SPIM_MODE_0,			       \
			.bit_order = NRF_SPIM_BIT_ORDER_MSB_FIRST,	       \
			.miso_pull = SPIM_NRFX_MISO_PULL(idx),		       \
			SPI_NRFX_SPIM_EXTENDED_CONFIG(idx)		       \
		}							       \
	};								       \
	DEVICE_DT_DEFINE(SPIM(idx),					       \
		      spi_##idx##_init,					       \
		      spim_nrfx_pm_control,				       \
		      &spi_##idx##_data,				       \
		      &spi_##idx##z_config,				       \
		      POST_KERNEL, CONFIG_SPI_INIT_PRIORITY,		       \
		      &spi_nrfx_driver_api)

#ifdef CONFIG_SPI_0_NRF_SPIM
SPI_NRFX_SPIM_DEVICE(0);
#endif

#ifdef CONFIG_SPI_1_NRF_SPIM
SPI_NRFX_SPIM_DEVICE(1);
#endif

#ifdef CONFIG_SPI_2_NRF_SPIM
SPI_NRFX_SPIM_DEVICE(2);
#endif

#ifdef CONFIG_SPI_3_NRF_SPIM
SPI_NRFX_SPIM_DEVICE(3);
#endif

#ifdef CONFIG_SPI_4_NRF_SPIM
SPI_NRFX_SPIM_DEVICE(4);
#endif

const struct device *spim_perip_get(void)
{
	const struct device *spi_perip;

#ifdef CONFIG_BOARD_NRF52840DK_NRF52840
	spi_perip = device_get_binding("SPI_3");
#elif CONFIG_BOARD_NRF5340DK_NRF5340_CPUAPP
	spi_perip = device_get_binding("SPI_4");
#endif

	if (spi_perip == NULL) {
		printk("Could not get SPI device\n");
		return NULL;
	}

	/* printf("%p : got device\n", spi_perip); */

	return spi_perip;
}

unsigned int spim_xfer_tx(const struct device *spi_perip, const struct spi_config *spi_cfg,
		unsigned int addr, const void *data, unsigned int len)
{
	int err;
	static uint8_t *tx_buffer;

	tx_buffer = (uint8_t *)(k_malloc(4+len));

	const struct spi_buf tx_buf = {
		.buf = tx_buffer,
		.len = 4 + len
	};
	const struct spi_buf_set tx = {
		.buffers = &tx_buf,
		.count = 1
	};

	/* PP opcode : 0x02 */
	tx_buffer[0] = 0x02;

	/* addr */
	tx_buffer[1] = (((addr >> 16) & 0xFF) | 0x80);
	tx_buffer[2] = (addr >> 8) & 0xFF;
	tx_buffer[3] = addr & 0xFF;

	/* data */
	memcpy(tx_buffer+4, data, len);

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

	tx_buffer = (uint8_t *)(k_malloc(dummy+len));

	if (tx_buffer == NULL) {
		printk("SPI error:NO MEMORY for tx_buffer\n");
		return -ENOMEM;
	}

	rx_buffer = (uint8_t *)(k_malloc(dummy+len));

	if (rx_buffer == NULL) {
		printk("SPI error:NO MEMORY rx_buffer\n");
		k_free(tx_buffer);
		return -ENOMEM;
	}

	memset(tx_buffer, 0, len+dummy);
	memset(rx_buffer, 0, len+dummy);

	const struct spi_buf tx_buf = {
		.buf = tx_buffer,
		.len = (len+dummy)
	};
	const struct spi_buf_set tx = {
		.buffers = &tx_buf,
		.count = 1
	};

	struct spi_buf rx_buf = {
		.buf = rx_buffer,
		.len =  (len+dummy),
	};
	const struct spi_buf_set rx = {
		.buffers = &rx_buf,
		.count = 1
	};

	/* FASTREAD opcode : 0x0b */

	tx_buffer[0] = 0x0b;
	tx_buffer[1] = (addr >> 16) & 0xFF;
	tx_buffer[2] = (addr >> 8) & 0xFF;
	tx_buffer[3] = addr & 0xFF;

	err = spi_transceive(spi_perip, spi_cfg, &tx, &rx);
	memcpy(data, rx_buffer+dummy, len);

	if (err)
		printk("SPI error: %d\n", err);

	k_free(tx_buffer);
	k_free(rx_buffer);

	return err;
}

unsigned int spim_wait_while_rpu_awake(const struct device *spi_perip, const struct spi_config *spi_cfg)
{
	int err, len;
	static uint8_t tx_buffer[6] = {0x1f};

	len = sizeof(tx_buffer);

	const struct spi_buf tx_buf = {
		.buf = tx_buffer,
		.len = len
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

	do {

		err = spi_transceive(spi_perip, spi_cfg, &tx, &rx);

		/* printk("%x %x %x %x %x %x\n", sr[0], sr[1], sr[2], sr[3], sr[4], sr[5]); */

	} while ((err < 0) || ((sr[1] & RPU_AWAKE_BIT) == 0));

	if (err) {
		printk("SPI error: %d\n", err);
	} else {
		/* Connect MISO to MOSI for loopback */
		/* printk("TX sent: %x\n", tx_buffer[0]); */
	}

	return 0;
}

unsigned int spim_cmd_rpu_wakeup(const struct device *spi_perip, const struct spi_config *spi_cfg)
{
	int err, len;
	static uint8_t tx_buffer[] = {0x3f, 0x1};

	/* printf("TODO : %s:\n", __func__); */

	len = sizeof(tx_buffer);

	const struct spi_buf tx_buf = {
		.buf = tx_buffer,
		.len = len
	};
	const struct spi_buf_set tx = {
		.buffers = &tx_buf,
		.count = 1
	};

	err = spi_transceive(spi_perip, spi_cfg, &tx, NULL);

	if (err) {
		printk("SPI error: %d\n", err);
	} else {
		/* Connect MISO to MOSI for loopback */
		/* printk("TX sent: %x\n", tx_buffer[0]); */
	}

	return 0;
}

static const struct spi_config spi_cfg = {
	.operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB,
	.frequency = 8000000,
	.slave = 0,
};

const struct device *spim_perip;

int spim_init(struct qspi_config *config)
{
	spim_perip = spim_perip_get();

	spim_config = config;

#ifdef CONFIG_BOARD_NRF5340DK_NRF5340_CPUAPP
	/* SPIM4 (<32Mbps)  : 1Mbps -> 1Mhz */
	config->spimfreq = config->freq * 1000000;
#endif
#ifdef CONFIG_BOARD_NRF52840DK_NRF52840
	/* SPIM3 (<8Mbps): 1Mbps -> 1Mhz */
	config->spimfreq = config->freq * 1000000;
#endif

	k_sem_init(&spim_config->lock, 1, 1);

	spim_cmd_rpu_wakeup(spim_perip, &spi_cfg);

	spim_wait_while_rpu_awake(spim_perip, &spi_cfg);

	return 0;
}

void spim_addr_check(unsigned int addr, const void *data, unsigned int len)
{
	if ((addr%4 != 0) || (((unsigned int)data)%4 != 0) || (len%4 != 0)) {
		printk("%s : Unaligned address %x %x %d %x %x\n",
				__func__, addr, (unsigned int)data,
				(addr%4 != 0),
				(((unsigned int)data)%4 != 0),
				(len%4 != 0)
			   );
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
		printk("%s: ERROR ENOMEM line %d\n", __func__, __LINE__);
		return -ENOMEM;
	}
	memset(rxb, 0, len);

	k_sem_take(&spim_config->lock, K_FOREVER);

	status = spim_xfer_rx(spim_perip, &spi_cfg, addr, rxb, len);

	k_sem_give(&spim_config->lock);

	*(uint32_t *)data = *(uint32_t *)(rxb+(len-4));

	k_free(rxb);

	return status;
}

int spim_hl_read(unsigned int addr, void *data, int len)
{
	int count = 0;

	spim_addr_check(addr, data, len);

	while (count < (len/4)) {
		spim_hl_readw(addr + (4*count), (char *)data + (4*count));
		count++;
	}

	return 0;
}
