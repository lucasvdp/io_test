/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 */

#include <zephyr/drivers/uart.h>
#include <zephyr/pm/device.h>

#define USED_DEV DT_NODELABEL(lpuart)

const struct device *p_dev;

#define BUF_SIZE 4
K_MEM_SLAB_DEFINE_STATIC(uart_slab, BUF_SIZE, 3, 4);

K_SEM_DEFINE(uart_rx_done, 0, 1);
K_SEM_DEFINE(uart_tx_done, 0, 1);

int rx_bytes = 0;

static void uart_callback(const struct device *dev,
			  struct uart_event *evt,
			  void *user_data)
{
	uint8_t *buf;

	switch (evt->type) {
	case UART_TX_DONE:
		k_sem_give(&uart_tx_done);
		break;

	case UART_TX_ABORTED:
		break;

	case UART_RX_RDY:
		k_sem_give(&uart_rx_done);
		if (evt->data.rx_buf.buf != rx_buffer) {
			lp_printf("buffer overrun\n");
			break;
		}
		rx_bytes = evt->data.rx.len;
		break;

	case UART_RX_BUF_REQUEST:
		k_mem_slab_alloc(&uart_slab, (void **)&buf, K_NO_WAIT);
		uart_rx_buf_rsp(p_dev, buf, BUF_SIZE);
		break;

	case UART_RX_BUF_RELEASED:
		if (evt->data.rx_buf.buf != rx_buffer) {
			k_mem_slab_free(&uart_slab, (void *)evt->data.rx_buf.buf);
		}
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
	const struct uart_config uart_cfg = {
		.baudrate = 1000000,
		.data_bits = UART_CFG_DATA_BITS_8,
		.parity = UART_CFG_PARITY_NONE,
		.stop_bits = UART_CFG_STOP_BITS_1,
		.flow_ctrl = UART_CFG_FLOW_CTRL_NONE
	};

	p_dev = DEVICE_DT_GET(USED_DEV);

	if (!device_is_ready(p_dev)) {
		lp_printf("Device not ready\n");
		return;
	}

	err = uart_configure(p_dev, &uart_cfg);
	if (err) {
		lp_printf("Failed to configure runtime %d", err);
	}

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

	k_sem_take(&uart_tx_done, K_FOREVER);

	return size;
}

int recv(int size)
{
	int err;

	rx_bytes = 0;

	err = uart_rx_enable(p_dev, rx_buffer, size, SYS_FOREVER_US);
	if (err) {
		lp_printf("Error enable rx: %d\n", err);
		return err;
	}

	k_sem_take(&uart_rx_done, K_FOREVER);

	uart_rx_disable(p_dev);

	return rx_bytes;
}

void deinit(void)
{

}
