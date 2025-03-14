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

const struct device *p_dev;

void init(void)
{
	p_dev = DEVICE_DT_GET(USED_DEV);

	if (p_dev == NULL) {
		printf("Could not get device\n");
		return;
	}
	printf("Using SPI Master device %s\n", p_dev->name);
}

void test_at_freq(size_t size, int freq)
{
	int err;

	struct spi_config spi_cfg = {
		.operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB | SPI_MODE_CPOL | SPI_MODE_CPHA,
		.frequency = freq * 1000,
		.slave = 0,
		.cs.gpio = GPIO_DT_SPEC_INST_GET(0, cs_gpios),
		.cs.delay = 100,
	};

	const struct spi_buf tx_buf = {
		.buf = tx_buffer,
		.len = size
	};
	const struct spi_buf_set tx = {
		.buffers = &tx_buf,
		.count = 1
	};

	struct spi_buf rx_buf = {
		.buf = rx_buffer,
		.len = size,
	};
	const struct spi_buf_set rx = {
		.buffers = &rx_buf,
		.count = 1
	};

	pm_device_action_run(p_dev, PM_DEVICE_ACTION_RESUME);

	printf("\nTransmitting %d bytes to spi @ %d kHz\n", size, freq);
	sleep(1);
	err = spi_write(p_dev, &spi_cfg, &tx);
	sleep(1);
	if (err) {
		printf("error: %d\n", err);
	} else {
		printf("success\n");
	}

	printf("\nReceiving %d bytes from spi @ %d kHz\n", size, freq);
	sleep(1);
	err = spi_read(p_dev, &spi_cfg, &rx);
	sleep(1);
	if (err) {
		printf("error: %d\n", err);
	} else {
		printf("success %d: %02x %02x %02x %02x\n", (rx_buffer[0] << 8) + rx_buffer[1],
		       rx_buffer[2], rx_buffer[3], rx_buffer[4], rx_buffer[5]);
	}

	printf("\nTransceiving %d bytes on spi @ %d kHz\n", size, freq);
	sleep(1);
	/* Connect MISO to MOSI for loopback */
	err = spi_transceive(p_dev, &spi_cfg, &tx, &rx);
	sleep(1);
	if (err) {
		printf("error: %d\n", err);
	} else {
		printf("success %d: %02x %02x %02x %02x\n", (rx_buffer[0] << 8) + rx_buffer[1],
		       rx_buffer[2], rx_buffer[3], rx_buffer[4], rx_buffer[5]);
	}

	spi_release(p_dev, &spi_cfg);
	/* Suspend is needed to re-initialise SPIM so we can change the frequency. */
	pm_device_action_run(p_dev, PM_DEVICE_ACTION_SUSPEND);
}

void test(size_t size)
{
	test_at_freq(size, 125);
	sleep(2);

	test_at_freq(size, 1000);
	sleep(2);

	test_at_freq(size, 8000);
}
