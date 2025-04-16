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
#define PIN_SCK    6
#define PIN_MISO   3
#define PIN_MOSI   2
#define PIN_CS     7

extern uint8_t tx_buffer[1024];
extern uint8_t rx_buffer[2048];

int lp_printf(const char *fmt, ...);

static bool transfer_done = false;

static void spis_isr(const void *arg)
{
	transfer_done = true;
	SPI_SLAVE->EVENTS_END = 0;
}

void spis_init(uint32_t bitrate)
{
	/* SCK: Dir input, input connect, pull disabled, drive s0s1, sense disabled. */
	GPIO->PIN_CNF[PIN_SCK] = 0;
	/* MOSI: Dir input, input connect, pull disabled, drive s0s1, sense disabled. */
	GPIO->PIN_CNF[PIN_MOSI] = 0;
	/* CSN: Dir input, input connect, pull disabled, drive s0s1, sense disabled. */
	GPIO->PIN_CNF[PIN_CS] = 0;

	/* Configure pins. */
	SPI_SLAVE->PSEL.SCK = PIN_SCK;
	SPI_SLAVE->PSEL.MISO = PIN_MISO;
	SPI_SLAVE->PSEL.MOSI = PIN_MOSI;
	SPI_SLAVE->PSEL.CSN = PIN_CS;

	/* Configure buffers. */
	SPI_SLAVE->RXD.PTR = (int)rx_buffer;
	SPI_SLAVE->TXD.PTR = (int)tx_buffer;

	/* Most significant bit first, Sample on trailing edge, Active low.  */
	SPI_SLAVE->CONFIG = SPIS_CONFIG_CPOL_Msk | SPIS_CONFIG_CPHA_Msk;

	/* Equire semaphore on end of transmission. */
	SPI_SLAVE->SHORTS = SPIS_SHORTS_END_ACQUIRE_Msk;

	/* Enable interrupt to wake up at the end of a message. */
	SPI_SLAVE->INTENSET = SPIS_INTENSET_END_Msk;
	irq_connect_dynamic(SPIM1_SPIS1_TWIM1_TWIS1_UARTE1_IRQn, 0, spis_isr, NULL, 0);
	irq_enable(SPIM1_SPIS1_TWIM1_TWIS1_UARTE1_IRQn);

	/* Enable. */
	SPI_SLAVE->ENABLE = SPIS_ENABLE_ENABLE_Enabled;

	/* MISO: Dir output, input disconnect, pull disabled, drive h0h1, sense disabled. */
	GPIO->PIN_CNF[PIN_MISO] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
				  (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
				  (GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos);

	lp_printf("    SCK     P0.%02d\n", PIN_SCK);
	lp_printf("    MOSI    P0.%02d\n", PIN_MOSI);
	lp_printf("    MISO    P0.%02d\n", PIN_MISO);
	lp_printf("    CS      P0.%02d\n", PIN_CS);
}

int spis_send(int size)
{
	SPI_SLAVE->TXD.MAXCNT = size;
	SPI_SLAVE->RXD.MAXCNT = 0;
	SPI_SLAVE->TASKS_RELEASE = 1;

	while (!transfer_done) {
		__WFI();
	}

	transfer_done = false;

	return SPI_SLAVE->TXD.AMOUNT;
}

int spis_recv(int size)
{
	SPI_SLAVE->TXD.MAXCNT = 0;
	SPI_SLAVE->RXD.MAXCNT = size;
	SPI_SLAVE->TASKS_RELEASE = 1;

	while (!transfer_done) {
		__WFI();
	}

	transfer_done = false;

	return SPI_SLAVE->RXD.AMOUNT;
}

void spis_deinit(void)
{
	SPI_SLAVE->INTENCLR = SPIS_INTENCLR_END_Msk;
	GPIO->PIN_CNF[PIN_MISO] = 0;
	SPI_SLAVE->ENABLE = 0;
}
