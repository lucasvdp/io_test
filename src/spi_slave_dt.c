/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 */

#include <zephyr/drivers/spi.h>
#include <../../modules/hal/nordic/nrfx/hal/nrf_spis.h>

#define USED_DEV DT_NODELABEL(spi_slave)
#define SPI_SLAVE NRF_SPIS1_NS

const struct device *p_dev;

void init(void)
{
	p_dev = DEVICE_DT_GET(USED_DEV);

	if (p_dev == NULL) {
		lp_printf("Could not get device\n");
		return;
	}
	lp_printf("\nUsing %s %p %p %p\n", p_dev->name, p_dev->api, p_dev->config, p_dev->data);
}

static const struct spi_config spi_cfg = {
	.operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB | SPI_OP_MODE_SLAVE |
		     SPI_MODE_CPOL | SPI_MODE_CPHA,
	.frequency = 1000000,
	.slave = 1,
};

void send(int size)
{
	int err;

	const struct spi_buf tx_buf = {
		.buf = tx_buffer,
		.len = size
	};
	const struct spi_buf_set tx = {
		.buffers = &tx_buf,
		.count = 1
	};

	err = spi_write(p_dev, &spi_cfg, &tx);

	if (err < 0) {
		lp_printf("SPI Send error: %d\n", err);
	}
}

int recv(int size, int timeout)
{
	struct spi_buf rx_buf = {
		.buf = rx_buffer,
		.len = size
	};
	const struct spi_buf_set rx = {
		.buffers = &rx_buf,
		.count = 1
	};

	return spi_read(p_dev, &spi_cfg, &rx);
}

void deinit(void)
{

}
