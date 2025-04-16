/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 */

#include <zephyr/drivers/spi.h>
#include <zephyr/pm/device.h>

#define USED_DEV DT_NODELABEL(spi_master)
#define DT_DRV_COMPAT nordic_nrf_spim
#define SPI_MASTER NRF_SPIM1_NS

const struct device *p_dev;
struct spi_config spi_cfg = {
	.operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB | SPI_MODE_CPOL | SPI_MODE_CPHA,
	.frequency = 1000000,
	.slave = 0,
	.cs.gpio = GPIO_DT_SPEC_INST_GET(0, cs_gpios),
	.cs.delay = 100,
};

void init(void)
{
	p_dev = DEVICE_DT_GET(USED_DEV);

	if (p_dev == NULL) {
		lp_printf("Could not get device\n");
		return;
	}
	lp_printf("\nUsing SPI Master device: %s\n", p_dev->name);
	lp_printf("    SCK     P0.%02d\n", SPI_MASTER->PSEL.SCK);
	lp_printf("    MOSI    P0.%02d\n", SPI_MASTER->PSEL.MOSI);
	lp_printf("    MISO    P0.%02d\n", SPI_MASTER->PSEL.MISO);
	lp_printf("    CS      P0.%02d\n", spi_cfg.cs.gpio.pin);

	pm_device_action_run(p_dev, PM_DEVICE_ACTION_RESUME);
}

int send(int size)
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

	return err == 0 ? size : err;
}

int recv(int size)
{
	int err;

	struct spi_buf rx_buf = {
		.buf = rx_buffer,
		.len = size,
	};
	const struct spi_buf_set rx = {
		.buffers = &rx_buf,
		.count = 1
	};

	err = spi_read(p_dev, &spi_cfg, &rx);

	return err == 0 ? size : err;
}

void deinit(void)
{
	spi_release(p_dev, &spi_cfg);
	/* Suspend is needed to re-initialise SPIM so we can change the frequency. */
	pm_device_action_run(p_dev, PM_DEVICE_ACTION_SUSPEND);
}
