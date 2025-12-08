/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 */

#include <zephyr/drivers/i2c.h>
#include <zephyr/pm/device.h>

#define USED_DEV DT_NODELABEL(twi_slave)
// #define DT_DRV_COMPAT nordic_nrf_twis
#define TWI_SLAVE NRF_TWIS1_NS

K_SEM_DEFINE(i2c_done, 0, 1);

const struct device *p_dev;
size_t transfer_len;

void buf_write_received_cb(struct i2c_target_config *config, uint8_t *ptr, uint32_t len)
{
	memcpy(rx_buffer, ptr, len);
	transfer_len = len;
	k_sem_give(&i2c_done);
}

int buf_read_requested_cb(struct i2c_target_config *config, uint8_t **ptr, uint32_t *len)
{
	*ptr = tx_buffer;
	*len = transfer_len;

	k_sem_give(&i2c_done);

	return 0;
}

static struct i2c_target_callbacks callbacks = {
	.buf_write_received = buf_write_received_cb,
	.buf_read_requested = buf_read_requested_cb
};

static struct i2c_target_config config = {
	.address = 42,
	.callbacks = &callbacks
};

void init(void)
{
	p_dev = DEVICE_DT_GET(USED_DEV);

	if (p_dev == NULL) {
		lp_printf("Could not get device\n");
		return;
	}

	int err = i2c_target_register(p_dev, &config);

	if (err) {
		lp_printf("Failed to register slave, err %d\n", err);
	}

	lp_printf("\nUsing TWI Slave device: %s\n", p_dev->name);
	lp_printf("    SCL     P0.%02d\n", TWI_SLAVE->PSEL.SCL);
	lp_printf("    SDA     P0.%02d\n", TWI_SLAVE->PSEL.SDA);
}

int send(int size)
{
	transfer_len = size;

	k_sem_take(&i2c_done, K_FOREVER);

	return transfer_len;
}

int recv(int size)
{
	transfer_len = 0;

	k_sem_take(&i2c_done, K_FOREVER);

	return transfer_len;
}

void deinit(void)
{

}
