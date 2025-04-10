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

extern uint8_t tx_buffer[1024];
extern uint8_t rx_buffer[2048];

int lp_printf(const char *fmt, ...);

static bool transfer_done = false;
static bool error = false;

void twi_master_isr(const void *arg)
{
	if (TWI_MASTER->EVENTS_STOPPED) {
		transfer_done = true;
		TWI_MASTER->EVENTS_STOPPED = 0;
	}
	if (TWI_MASTER->EVENTS_ERROR) {
		error = true;
		TWI_MASTER->EVENTS_ERROR = 0;
	}
}

void twi_master_init(void)
{
	/* SCK: Dir input, input disconnect, pull up, drive s0d1, sense disabled. */
	GPIO->PIN_CNF[2] = (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
			   (GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos) |
			   (GPIO_PIN_CNF_DRIVE_S0D1 << GPIO_PIN_CNF_DRIVE_Pos);
	/* MOSI: Dir input, input disconnect, pull up, drive s0d1, sense disabled. */
	GPIO->PIN_CNF[3] = (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
			   (GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos) |
			   (GPIO_PIN_CNF_DRIVE_S0D1 << GPIO_PIN_CNF_DRIVE_Pos);

	/* Configure pins. */
	TWI_MASTER->PSEL.SCL = 2;
	TWI_MASTER->PSEL.SDA = 3;

	/* Configure buffers. */
	TWI_MASTER->RXD.PTR = (int)rx_buffer;
	TWI_MASTER->TXD.PTR = (int)tx_buffer;

	/* Configure. */
	TWI_MASTER->FREQUENCY = TWIM_FREQUENCY_FREQUENCY_K400;
	TWI_MASTER->ADDRESS = 42;

	/* Stop after transfer. */
	TWI_MASTER->SHORTS = TWIM_SHORTS_LASTTX_STOP_Msk | TWIM_SHORTS_LASTRX_STOP_Msk;

	/* Enable interrupt to wake up at the end of a message. */
	TWI_MASTER->INTENSET = TWIM_INTEN_STOPPED_Msk | TWIM_INTEN_ERROR_Msk;
	irq_connect_dynamic(SPIM1_SPIS1_TWIM1_TWIS1_UARTE1_IRQn, 0, twi_master_isr, NULL, 0);
	irq_enable(SPIM1_SPIS1_TWIM1_TWIS1_UARTE1_IRQn);

	/* Enable. */
	TWI_MASTER->ENABLE = TWIM_ENABLE_ENABLE_Enabled;
}

void twi_master_send(int size)
{
	transfer_done = false;
	error = false;

	TWI_MASTER->TXD.MAXCNT = size;
	TWI_MASTER->TASKS_STARTTX = 1;


	while (!transfer_done) {
		if (error) {
			lp_printf("  Send error %d\n", TWI_MASTER->ERRORSRC);
			TWI_MASTER->ERRORSRC = TWI_MASTER->ERRORSRC;
			TWI_MASTER->TASKS_STOP = 1;
			break;
		}

		__WFI();
	}
}

int twi_master_recv(int size, int timeout_sec)
{
	transfer_done = false;
	error = false;

	TWI_MASTER->RXD.MAXCNT = size;
	TWI_MASTER->TASKS_STARTRX = 1;

	while (!transfer_done) {
		if (error) {
			lp_printf("  Recv error %d\n", TWI_MASTER->ERRORSRC);
			TWI_MASTER->ERRORSRC = TWI_MASTER->ERRORSRC;
			TWI_MASTER->TASKS_STOP = 1;
			break;
		}
		__WFI();
	}

	return TWI_MASTER->RXD.AMOUNT;
}

void twi_master_deinit(void)
{
	TWI_MASTER->INTENCLR = TWIM_INTENCLR_STOPPED_Msk;
	GPIO->PIN_CNF[2] = 0;
	GPIO->PIN_CNF[3] = 0;
	TWI_MASTER->ENABLE = 0;
}
