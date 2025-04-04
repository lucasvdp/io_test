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

int received;

void uart_callback(const struct device *dev, struct uart_event *evt, void *user_data)
{
	if (evt->type == UART_RX_RDY) {
		received = evt->data.rx.len;
	} else if (evt->type == UART_RX_STOPPED) {
		received = -evt->data.rx_stop.reason;
	}
}

void init(void)
{
	int err;

	p_dev = DEVICE_DT_GET(USED_DEV);

	if (p_dev == NULL) {
		lp_printf("Could not get device\n");
		return;
	}

	err = uart_callback_set(p_dev, uart_callback, NULL);
	if (err) {
		lp_printf("Could not set uart callback: %d\n", err);
		return;
	}

	lp_printf("\nUsing %s %p %p %p\n", p_dev->name, p_dev->api, p_dev->config, p_dev->data);
}


void send(size_t size)
{
	int err;

	err = uart_tx(p_dev, tx_buffer, size, 1000);
	if (err) {
		lp_printf("Could not send: %d\n", err);
		return;
	}
}

int recv(int size, int timeout)
{
	received = 0;
	uart_rx_enable(p_dev, rx_buffer, size, 1000);

	for (int i = 0; i < timeout; i++) {
		if (received) {
			return received;
		}
		sleep(1);
	}

	uart_rx_disable(p_dev);
	return 0;
}

void deinit(void)
{

}
