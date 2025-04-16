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
	lp_printf("\nUsing SPI Slave device: %s\n", p_dev->name);
	lp_printf("    SCK     P0.%02d\n", SPI_SLAVE->PSEL.SCK);
	lp_printf("    MOSI    P0.%02d\n", SPI_SLAVE->PSEL.MOSI);
	lp_printf("    MISO    P0.%02d\n", SPI_SLAVE->PSEL.MISO);
	lp_printf("    CS      P0.%02d\n", SPI_SLAVE->PSEL.CSN);
}

static const struct spi_config spi_cfg = {
	.operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB | SPI_OP_MODE_SLAVE |
		     SPI_MODE_CPOL | SPI_MODE_CPHA,
	.frequency = 1000000,
	.slave = 1,
};

int send(int size)
{
	const struct spi_buf tx_buf = {
		.buf = tx_buffer,
		.len = size
	};
	const struct spi_buf_set tx = {
		.buffers = &tx_buf,
		.count = 1
	};

	/* Always returns 0, how do we tell how many bytes were send? */
	return spi_write(p_dev, &spi_cfg, &tx);
}

int recv(int size)
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
