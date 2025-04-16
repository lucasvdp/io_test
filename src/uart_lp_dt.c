/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 */

#include <zephyr/drivers/uart.h>
#include <zephyr/pm/device.h>

#define USED_DEV DT_NODELABEL(uart_lp)

const struct device *p_dev;

static void uart_callback(const struct device *dev,
			  struct uart_event *evt,
			  void *user_data)
{
	int err;

	switch (evt->type) {
	case UART_TX_DONE:
		// lp_printf("Tx sent %d bytes\n", evt->data.tx.len);
		break;

	case UART_TX_ABORTED:
		// lp_printf("Tx aborted\n");
		break;

	case UART_RX_RDY:
		// lp_printf("Received data %d bytes\n", evt->data.rx.len);
		break;

	case UART_RX_BUF_REQUEST:
		err = uart_rx_buf_rsp(p_dev, rx_buffer, sizeof(rx_buffer));
		if (err) {
			// lp_printf("rx buf rsp err: %d\n", err);
		}
		break;

	case UART_RX_BUF_RELEASED:
		break;

	case UART_RX_DISABLED:
		break;

	case UART_RX_STOPPED:
		break;
	}
}

void init(void)
{
	int err;

	p_dev = DEVICE_DT_GET(USED_DEV);

	err = uart_callback_set(p_dev, uart_callback, NULL);
	if (err) {
		lp_printf("Error setting call back: %d\n", err);
	}
}


int send(size_t size)
{
	int err = uart_tx(p_dev, tx_buffer, size, 10000);

	if (err) {
		lp_printf("Error uart tx: %d\n", err);
		return err;
	}
	return size;
}

int recv(int size)
{
	int err = uart_rx_enable(p_dev, rx_buffer, size, 10000);
	if (err) {
		lp_printf("Error enable rx: %d\n", err);
		return err;
	}
	return size;
}

void deinit(void)
{

}
