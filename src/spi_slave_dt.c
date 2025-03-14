/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 */

#include <zephyr/drivers/spi.h>

#define USED_DEV DT_NODELABEL(spi_slave)
#define SPI_SLAVE NRF_SPIS1_NS

const struct device *p_dev;

void init(void)
{
	p_dev = DEVICE_DT_GET(USED_DEV);

	if (p_dev == NULL) {
		printf("Could not get device\n");
		return;
	}
	printf("Using %s %p %p %p\n", p_dev->name, p_dev->api, p_dev->config, p_dev->data);

	// for (uint32_t *i = 0x40009000; i <= 0x400095c0; i++) {
	// 	printf("%p = %08x %d\n", i, *i, *i);
	// }

}

static const struct spi_config spi_cfg = {
	.operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB | SPI_OP_MODE_SLAVE |
		     SPI_MODE_CPOL | SPI_MODE_CPHA,
	.frequency = 125000,
	.slave = 1,
};

#include <../../modules/hal/nordic/nrfx/hal/nrf_spis.h>

void test(size_t size)
{
	int err;

	const struct spi_buf tx_buf = {
		.buf = tx_buffer,
		.len = sizeof(tx_buffer)
	};
	const struct spi_buf_set tx = {
		.buffers = &tx_buf,
		.count = 1
	};

	struct spi_buf rx_buf = {
		.buf = rx_buffer,
		.len = sizeof(rx_buffer),
	};
	const struct spi_buf_set rx = {
		.buffers = &rx_buf,
		.count = 1
	};

	while (1) {

		printf("\ngoing to read from spi %p\n", rx_buffer);

		err = spi_transceive(p_dev, &spi_cfg, &tx, &rx);
		if (err) {
			printf("SPI Received: %d\n", err);
			for (int i = 2; i < err; i++) {
				if ((i & 0xff) != rx_buffer[i]) {
					printf("Expected %d got %d for %d\n", i & 0xff, rx_buffer[i], i);
					break;
				}
			}

			printf("success %d: %02x %02x %02x %02x\n", (rx_buffer[0] << 8) + rx_buffer[1],
				rx_buffer[2], rx_buffer[3], rx_buffer[4], rx_buffer[5]);
		} else {
			printf("SPI Received nothing ?!?\n");
			printf("read: %d %d %d %d %d %x\n", rx.count, rx.buffers->len, err, SPI_SLAVE->RXD.AMOUNT, SPI_SLAVE->RXD.MAXCNT, SPI_SLAVE->RXD.PTR);
		}
	}
}
