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
#define PIN_SCL    2
#define PIN_SDA    3

extern uint8_t tx_buffer[1024];
extern uint8_t rx_buffer[2048];

int lp_printf(const char *fmt, ...);

K_SEM_DEFINE(twis_done, 0, 1);

void twis_isr(const void *arg)
{
	if (TWI_SLAVE->EVENTS_STOPPED) {
		k_sem_give(&twis_done);
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

void twis_init(uint32_t bitrate)
{
	/* SCK: Dir output, input disconnect, pull up, drive h0d1, sense disabled. */
	GPIO->PIN_CNF[PIN_SCL] = (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
				 (GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos) |
				 (GPIO_PIN_CNF_DRIVE_S0D1 << GPIO_PIN_CNF_DRIVE_Pos);
	/* MOSI: Dir output, input disconnect, pull up, drive h0d1, sense disabled. */
	GPIO->PIN_CNF[PIN_SDA] = (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
				 (GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos) |
				 (GPIO_PIN_CNF_DRIVE_S0D1 << GPIO_PIN_CNF_DRIVE_Pos);

	/* Configure pins. */
	TWI_SLAVE->PSEL.SCL = PIN_SCL;
	TWI_SLAVE->PSEL.SDA = PIN_SDA;

	/* Configure buffers. */
	TWI_SLAVE->RXD.PTR = (int)rx_buffer;
	TWI_SLAVE->RXD.MAXCNT = 0;
	TWI_SLAVE->TXD.PTR = (int)tx_buffer;
	TWI_SLAVE->TXD.MAXCNT = 0;

	/* Configure. */
	TWI_SLAVE->ADDRESS[0] = 42;
	TWI_SLAVE->CONFIG = TWIS_CONFIG_ADDRESS0_Msk;
	/* Suspend after we have received a READ or WRITE flag so we can either start RX or TX. */
	TWI_SLAVE->SHORTS = TWIS_SHORTS_READ_SUSPEND_Msk | TWIS_SHORTS_WRITE_SUSPEND_Msk;

	/* Enable interrupt to wake up at the end of a message. */
	TWI_SLAVE->INTENSET = TWIS_INTENSET_STOPPED_Msk | TWIS_INTENSET_READ_Msk |
			      TWIS_INTENSET_WRITE_Msk;
	irq_connect_dynamic(SPIM1_SPIS1_TWIM1_TWIS1_UARTE1_IRQn, 0, twis_isr, NULL, 0);
	irq_enable(SPIM1_SPIS1_TWIM1_TWIS1_UARTE1_IRQn);

	/* Enable. */
	TWI_SLAVE->ENABLE = TWIS_ENABLE_ENABLE_Enabled;

	lp_printf("    SCL     P0.%02d\n", PIN_SCL);
	lp_printf("    SDA     P0.%02d\n", PIN_SDA);
}

int twis_send(int size)
{
	TWI_SLAVE->TXD.MAXCNT = size;

	if (k_sem_take(&twis_done,  K_SECONDS(60))) {
		return -ETIMEDOUT;
	}

	if (!TWI_SLAVE->EVENTS_TXSTARTED) {
		return -EBADR;	/* Got RX instead of TX. */
	}

	TWI_SLAVE->EVENTS_TXSTARTED = 0;

	return TWI_SLAVE->TXD.AMOUNT;
}

int twis_recv(int size)
{
	TWI_SLAVE->RXD.MAXCNT = size;

	if (k_sem_take(&twis_done,  K_SECONDS(60))) {
		return -ETIMEDOUT;
	}

	if (!TWI_SLAVE->EVENTS_RXSTARTED) {
		return -EBADR;	/* Got TX instead of RX. */
	}

	TWI_SLAVE->EVENTS_RXSTARTED = 0;

	return TWI_SLAVE->RXD.AMOUNT;
}

void twis_deinit(void)
{
	TWI_SLAVE->INTENCLR = TWIS_INTENCLR_STOPPED_Msk | TWIS_INTENCLR_READ_Msk |
			      TWIS_INTENCLR_WRITE_Msk;
	TWI_SLAVE->SHORTS = 0;
	GPIO->PIN_CNF[PIN_SCL] = 0;
	GPIO->PIN_CNF[PIN_SDA] = 0;
	TWI_SLAVE->ENABLE = 0;
}
