/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 */

#include <zephyr/drivers/i2c.h>
#include <zephyr/pm/device.h>

#define USED_DEV DT_NODELABEL(twi_master)
#define DT_DRV_COMPAT nordic_nrf_twim
#define TWI_MASTER NRF_TWIM1_NS

const struct device *p_dev;

void init(void)
{
	p_dev = DEVICE_DT_GET(USED_DEV);

	if (p_dev == NULL) {
		lp_printf("Could not get device\n");
		return;
	}
	lp_printf("\nUsing TWI Master device: %s\n", p_dev->name);
	lp_printf("    SCL     P0.%02d\n", TWI_MASTER->PSEL.SCL);
	lp_printf("    SDA     P0.%02d\n", TWI_MASTER->PSEL.SDA);
}

int send(int size)
{
	int err = i2c_write(p_dev, tx_buffer, size, 42);

	return err ? err : size;
}

int recv(int size)
{
	int err = i2c_read(p_dev, rx_buffer, size, 42);

	return err ? err : size;
}

void deinit(void)
{

}
