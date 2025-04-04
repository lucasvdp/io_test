/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <zephyr/kernel.h>

#define SPI_SLAVE NRF_SPIS1_NS
#define GPIO      NRF_P0_NS

extern uint8_t tx_buffer[1024];
extern uint8_t rx_buffer[2048];

int lp_printf(const char *fmt, ...);

static bool transfer_done = false;
static bool timeout = false;

static void spi_slave_isr(const void *arg)
{
	transfer_done = true;
	SPI_SLAVE->EVENTS_END = 0;
}

static void timeout_isr(const void *arg)
{
	timeout = true;
	NRF_TIMER1_NS->EVENTS_COMPARE[0] = 0;
}

void spi_slave_init(void)
{
	/* SCK: Dir input, input connect, pull disabled, drive s0s1, sense disabled. */
	GPIO->PIN_CNF[6] = 0;
	/* MOSI: Dir input, input connect, pull disabled, drive s0s1, sense disabled. */
	GPIO->PIN_CNF[2] = 0;
	/* CSN: Dir input, input connect, pull disabled, drive s0s1, sense disabled. */
	GPIO->PIN_CNF[7] = 0;

	/* Configure pins. */
	SPI_SLAVE->PSEL.SCK = 6;
	SPI_SLAVE->PSEL.MISO = 3;
	SPI_SLAVE->PSEL.MOSI = 2;
	SPI_SLAVE->PSEL.CSN = 7;

	/* Configure buffers. */
	SPI_SLAVE->RXD.PTR = (int)rx_buffer;
	SPI_SLAVE->TXD.PTR = (int)tx_buffer;

	/* Most significant bit first, Sample on trailing edge, Active low.  */
	SPI_SLAVE->CONFIG = SPIS_CONFIG_CPOL_Msk | SPIS_CONFIG_CPHA_Msk;

	/* Equire semaphore on end of transmission. */
	SPI_SLAVE->SHORTS = SPIS_SHORTS_END_ACQUIRE_Msk;

	/* Enable interrupt to wake up at the end of a message. */
	SPI_SLAVE->INTENSET = SPIS_INTENSET_END_Msk;
	irq_connect_dynamic(SPIM1_SPIS1_TWIM1_TWIS1_UARTE1_IRQn, 0, spi_slave_isr, NULL, 0);
	irq_enable(SPIM1_SPIS1_TWIM1_TWIS1_UARTE1_IRQn);

	/* Enable. */
	SPI_SLAVE->ENABLE = SPIS_ENABLE_ENABLE_Enabled;

	/* MISO: Dir output, input disconnect, pull disabled, drive h0h1, sense disabled. */
	GPIO->PIN_CNF[3] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
			   (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
			   (GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos);

	/* Enable a timer with interupt. */
	irq_connect_dynamic(TIMER1_IRQn, 0, timeout_isr, NULL, 0);
	irq_enable(TIMER1_IRQn);
	NRF_TIMER1_NS->BITMODE = TIMER_BITMODE_BITMODE_32Bit;
	NRF_TIMER1_NS->PRESCALER = 4;
	NRF_TIMER1_NS->INTENSET = TIMER_INTENSET_COMPARE0_Msk;
}

void spi_slave_send(int size)
{
	SPI_SLAVE->TXD.MAXCNT = size;
	SPI_SLAVE->RXD.MAXCNT = 0;
	SPI_SLAVE->TASKS_RELEASE = 1;

	NRF_TIMER1_NS->CC[0] = 60 * 1000000;
	NRF_TIMER1_NS->TASKS_CLEAR = 1;
	NRF_TIMER1_NS->TASKS_START = 1;

	while (!transfer_done) {
		if (timeout) {
			SPI_SLAVE->TASKS_ACQUIRE;
			break;
		}
		__WFI();
	}

	NRF_TIMER1_NS->TASKS_STOP = 1;

	transfer_done = false;
	timeout = false;

	if (SPI_SLAVE->TXD.AMOUNT != size) {
		lp_printf("Send %d instead of %d bytes\n", SPI_SLAVE->TXD.AMOUNT, size);
	}
}

int spi_slave_recv(int size, int timeout_sec)
{
	SPI_SLAVE->TXD.MAXCNT = 0;
	SPI_SLAVE->RXD.MAXCNT = size;
	SPI_SLAVE->TASKS_RELEASE = 1;

	NRF_TIMER1_NS->CC[0] = timeout_sec * 1000000;
	NRF_TIMER1_NS->TASKS_CLEAR = 1;
	NRF_TIMER1_NS->TASKS_START = 1;

	while (!transfer_done) {
		if (timeout) {
			SPI_SLAVE->TASKS_ACQUIRE;
			break;
		}
		__WFI();
	}

	NRF_TIMER1_NS->TASKS_STOP = 1;

	transfer_done = false;
	timeout = false;

	return SPI_SLAVE->RXD.AMOUNT;
}

void spi_slave_deinit(void)
{
	NRF_TIMER1_NS->INTENCLR = TIMER_INTENCLR_COMPARE0_Msk;
	SPI_SLAVE->INTENCLR = SPIS_INTENCLR_END_Msk;
	GPIO->PIN_CNF[3] = 0;
	SPI_SLAVE->ENABLE = 0;
}
