/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 */

#include <zephyr/drivers/uart.h>
#include <zephyr/pm/device.h>

#define USED_DEV DT_NODELABEL(uart1)

const struct device *p_dev;

bool received_something = false;


void uart_callback(const struct device *dev, struct uart_event *evt, void *user_data)
{
	printf("uart callback %d\n", evt->type);

	if (evt->type == UART_RX_RDY) {
		received_something = true;
	}
}

void init(void)
{
	int err;

	p_dev = DEVICE_DT_GET(USED_DEV);

	if (p_dev == NULL) {
		printf("Could not get device\n");
		return;
	}

	err = uart_callback_set(p_dev, uart_callback, NULL);
	if (err) {
		printf("Could not set uart callback: %d\n", err);
		return;
	}

	printf("Using %s %p %p %p\n", p_dev->name, p_dev->api, p_dev->config, p_dev->data);
}


void test(size_t size)
{
	int err;

	printf("\ntest %d\n", size);
	err = uart_tx(p_dev, tx_buffer, size, 1000);
	printf("result %d\n", err);

	sleep(1);

	received_something = false;
	uart_rx_enable(p_dev, rx_buffer, sizeof(rx_buffer), 1000);

	while (!received_something) {
		// printf("pending %d\n", uart_irq_is_pending(p_dev));
		sleep(1);
	}
}


