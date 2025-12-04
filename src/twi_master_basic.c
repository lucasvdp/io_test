/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <zephyr/kernel.h>

#define TWI_MASTER NRF_TWIM1_NS
#define GPIO       NRF_P0_NS
#define PIN_SCL    2
#define PIN_SDA    3

extern uint8_t tx_buffer[1024];
extern uint8_t rx_buffer[2048];

int lp_printf(const char *fmt, ...);

K_SEM_DEFINE(twim_done, 0, 1);
static bool error = false;

void twim_isr(const void *arg)
{
	if (TWI_MASTER->EVENTS_STOPPED) {
		TWI_MASTER->EVENTS_STOPPED = 0;
	}
	if (TWI_MASTER->EVENTS_ERROR) {
		error = true;
		TWI_MASTER->EVENTS_ERROR = 0;
	}
	k_sem_give(&twim_done);
}

void twim_init(uint32_t bitrate)
{
	/* SCK: Dir input, input disconnect, pull up, drive s0d1, sense disabled. */
	GPIO->PIN_CNF[PIN_SCL] = (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
				 (GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos) |
				 (GPIO_PIN_CNF_DRIVE_S0D1 << GPIO_PIN_CNF_DRIVE_Pos);
	/* MOSI: Dir input, input disconnect, pull up, drive s0d1, sense disabled. */
	GPIO->PIN_CNF[PIN_SDA] = (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
				 (GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos) |
				 (GPIO_PIN_CNF_DRIVE_S0D1 << GPIO_PIN_CNF_DRIVE_Pos);

	/* Configure pins. */
	TWI_MASTER->PSEL.SCL = PIN_SCL;
	TWI_MASTER->PSEL.SDA = PIN_SDA;

	/* Configure buffers. */
	TWI_MASTER->RXD.PTR = (int)rx_buffer;
	TWI_MASTER->TXD.PTR = (int)tx_buffer;

	/* Configure. */
	TWI_MASTER->FREQUENCY = bitrate;
	TWI_MASTER->ADDRESS = 42;

	/* Stop after transfer. */
	TWI_MASTER->SHORTS = TWIM_SHORTS_LASTTX_STOP_Msk | TWIM_SHORTS_LASTRX_STOP_Msk;

	/* Enable interrupt to wake up at the end of a message. */
	TWI_MASTER->INTENSET = TWIM_INTENSET_STOPPED_Msk | TWIM_INTENSET_ERROR_Msk;
	irq_connect_dynamic(SPIM1_SPIS1_TWIM1_TWIS1_UARTE1_IRQn, 0, twim_isr, NULL, 0);
	irq_enable(SPIM1_SPIS1_TWIM1_TWIS1_UARTE1_IRQn);

	/* Enable. */
	TWI_MASTER->ENABLE = TWIM_ENABLE_ENABLE_Enabled;

	lp_printf("    SCL     P0.%02d\n", PIN_SCL);
	lp_printf("    SDA     P0.%02d\n", PIN_SDA);
}

int twim_send(int size)
{
	int err = 0;

	error = false;

	TWI_MASTER->TXD.MAXCNT = size;
	TWI_MASTER->TASKS_STARTTX = 1;

	k_sem_take(&twim_done, K_FOREVER);

	if (error) {
		err = TWI_MASTER->ERRORSRC;
		TWI_MASTER->ERRORSRC = err;
		TWI_MASTER->TASKS_STOP = 1;
		k_sem_take(&twim_done, K_FOREVER);
		return -err;
	}

	return TWI_MASTER->TXD.AMOUNT;
}

int twim_recv(int size)
{
	int err = 0;

	error = false;

	TWI_MASTER->RXD.MAXCNT = size;
	TWI_MASTER->TASKS_STARTRX = 1;

	k_sem_take(&twim_done, K_FOREVER);

	if (error) {
		err = TWI_MASTER->ERRORSRC;
		TWI_MASTER->ERRORSRC = err;
		TWI_MASTER->TASKS_STOP = 1;
		k_sem_take(&twim_done, K_FOREVER);
		return -err;
	}

	return TWI_MASTER->RXD.AMOUNT;
}

void twim_deinit(void)
{
	TWI_MASTER->INTENCLR = TWIM_INTENCLR_STOPPED_Msk| TWIM_INTENCLR_ERROR_Msk;
	TWI_MASTER->SHORTS = 0;
	GPIO->PIN_CNF[PIN_SCL] = 0;
	GPIO->PIN_CNF[PIN_SDA] = 0;
	TWI_MASTER->ENABLE = 0;
}
