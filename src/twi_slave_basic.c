/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <zephyr/kernel.h>

#define TWI_SLAVE  NRF_TWIS1_NS
#define GPIO       NRF_P0_NS

extern uint8_t tx_buffer[1024];
extern uint8_t rx_buffer[2048];

int lp_printf(const char *fmt, ...);

static bool transfer_done = false;

void twi_slave_isr(const void *arg)
{
	if (TWI_SLAVE->EVENTS_STOPPED) {
		transfer_done = true;
		TWI_SLAVE->EVENTS_STOPPED = 0;
	}
	if (TWI_SLAVE->EVENTS_READ) {
		TWI_SLAVE->TASKS_PREPARETX = 1;
		TWI_SLAVE->TASKS_RESUME = 1;
		TWI_SLAVE->EVENTS_READ = 0;
	}
	if (TWI_SLAVE->EVENTS_WRITE) {
		TWI_SLAVE->TASKS_PREPARERX = 1;
		TWI_SLAVE->TASKS_RESUME = 1;
		TWI_SLAVE->EVENTS_WRITE = 0;
	}
}

void twi_slave_init(void)
{
	/* SCK: Dir output, input disconnect, pull up, drive h0d1, sense disabled. */
	GPIO->PIN_CNF[2] = (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
			   (GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos) |
			   (GPIO_PIN_CNF_DRIVE_S0D1 << GPIO_PIN_CNF_DRIVE_Pos);
	/* MOSI: Dir output, input disconnect, pull up, drive h0d1, sense disabled. */
	GPIO->PIN_CNF[3] = (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
			   (GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos) |
			   (GPIO_PIN_CNF_DRIVE_S0D1 << GPIO_PIN_CNF_DRIVE_Pos);

	/* Configure pins. */
	TWI_SLAVE->PSEL.SCL = 2;
	TWI_SLAVE->PSEL.SDA = 3;

	/* Configure buffers. */
	TWI_SLAVE->RXD.PTR = (int)rx_buffer;
	TWI_SLAVE->RXD.MAXCNT = sizeof(rx_buffer);
	TWI_SLAVE->TXD.PTR = (int)tx_buffer;
	TWI_SLAVE->TXD.MAXCNT = sizeof(tx_buffer);

	/* Configure. */
	TWI_SLAVE->ADDRESS[0] = 42;
	TWI_SLAVE->CONFIG = TWIS_CONFIG_ADDRESS0_Msk;
	TWI_SLAVE->SHORTS = TWIS_SHORTS_READ_SUSPEND_Msk | TWIS_SHORTS_WRITE_SUSPEND_Msk;

	/* Enable interrupt to wake up at the end of a message. */
	TWI_SLAVE->INTENSET = TWIS_INTEN_STOPPED_Msk | TWIS_INTEN_READ_Msk | TWIS_INTEN_WRITE_Msk;
	irq_connect_dynamic(SPIM1_SPIS1_TWIM1_TWIS1_UARTE1_IRQn, 0, twi_slave_isr, NULL, 0);
	irq_enable(SPIM1_SPIS1_TWIM1_TWIS1_UARTE1_IRQn);

	/* Enable. */
	TWI_SLAVE->ENABLE = TWIS_ENABLE_ENABLE_Enabled;
}

void twi_slave_send(int size)
{
	while (!transfer_done) {
		__WFI();
	}

	transfer_done = false;

	if (TWI_SLAVE->EVENTS_RXSTARTED) {
		lp_printf("  Received %d bytes\n", TWI_SLAVE->RXD.AMOUNT);
		TWI_SLAVE->EVENTS_RXSTARTED = 0;
	}
	if (TWI_SLAVE->EVENTS_TXSTARTED) {
		lp_printf("  Send %d bytes\n", TWI_SLAVE->TXD.AMOUNT);
		TWI_SLAVE->EVENTS_TXSTARTED = 0;
	}
}

int twi_slave_recv(int size, int timeout)
{
	while (!transfer_done) {
		__WFI();
	}

	transfer_done = false;

	if (TWI_SLAVE->EVENTS_TXSTARTED) {
		lp_printf("  Send %d bytes\n", TWI_SLAVE->TXD.AMOUNT);
		TWI_SLAVE->EVENTS_TXSTARTED = 0;
	}
	if (TWI_SLAVE->EVENTS_RXSTARTED) {
		lp_printf("  Received %d bytes\n", TWI_SLAVE->RXD.AMOUNT);
		TWI_SLAVE->EVENTS_RXSTARTED = 0;
		return TWI_SLAVE->RXD.AMOUNT;
	}

	return 0;
}

void twi_slave_deinit(void)
{
	TWI_SLAVE->INTENCLR = TWIS_INTENCLR_STOPPED_Msk;
	GPIO->PIN_CNF[2] = 0;
	GPIO->PIN_CNF[3] = 0;
	TWI_SLAVE->ENABLE = 0;
}
