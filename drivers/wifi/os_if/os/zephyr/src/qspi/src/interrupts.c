/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @brief File containing QSPI device interrupt handler specific definitions
 * for the Zephyr OS layer of the Wi-Fi driver.
 */

#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include <sys/util.h>
#include <sys/printk.h>
#include <inttypes.h>

#define SLEEP_TIME_MS	1

/*
 * Get button configuration from the devicetree sw0 alias. This is mandatory.
 */
#define SW1_NODE	DT_ALIAS(sw1)
#define SW2_NODE	DT_ALIAS(sw2)

#if !DT_NODE_HAS_STATUS(SW1_NODE, okay)
#error "Unsupported board: sw1 devicetree alias is not defined"
#endif

void gpio_free_irq(int pin, struct gpio_callback *button_cb_data)
{
	printk("TODO : %s\n", __func__);
}

int gpio_request_irq(int pin, struct gpio_callback *button_cb_data,
	void (*irq_handler)())
{
	int ret;
	struct gpio_dt_spec button;

#ifdef CONFIG_BOARD_NRF52840DK_NRF52840
	struct gpio_dt_spec sw2 = GPIO_DT_SPEC_GET_OR(SW2_NODE, gpios, {0});

	/* GPIO0 P0.24 is mapped to SW2 */
	button = sw2;
#elif CONFIG_BOARD_NRF5340DK_NRF5340_CPUAPP
	struct gpio_dt_spec sw1 = GPIO_DT_SPEC_GET_OR(SW1_NODE, gpios, {0});

	/* GPIO0 P0.24 is mapped to SW1 */
	button = sw1;
#endif

	if (!device_is_ready(button.port)) {
		printk("Error: button device %s is not ready\n",
			button.port->name);
		return -1;
	}

	ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret != 0) {
		printk("Error %d: failed to configure %s pin %d\n",
			ret, button.port->name, button.pin);
		return ret;
	}

	ret = gpio_pin_interrupt_configure_dt(&button,
					      GPIO_INT_EDGE_RISING);
	if (ret != 0) {
		printk("Error %d: failed to configure interrupt on %s pin %d\n",
			ret, button.port->name, button.pin);
		return ret;
	}

	gpio_init_callback(button_cb_data, irq_handler, BIT(button.pin));
	gpio_add_callback(button.port, button_cb_data);

	return 0;
}

void hard_reset(void)
{
	const struct device *port;
	unsigned int pin = 12; /* P0.12 pin on nrf5340 */

	port = device_get_binding("GPIO_0");

	gpio_pin_configure(port, pin, GPIO_OUTPUT_INACTIVE);
	k_msleep(500);

	gpio_pin_configure(port, pin, GPIO_OUTPUT_ACTIVE);
	k_msleep(500);
}
