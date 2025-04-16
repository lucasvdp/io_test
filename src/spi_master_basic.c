/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <zephyr/kernel.h>

#define SPI_MASTER NRF_SPIM1_NS
#define GPIO       NRF_P0_NS
#define PIN_SCK    6
#define PIN_MISO   3
#define PIN_MOSI   2
#define PIN_CS     7

extern uint8_t tx_buffer[1024];
extern uint8_t rx_buffer[2048];

int lp_printf(const char *fmt, ...);

static bool transfer_done = false;

void spim_isr(const void *arg)
{
	transfer_done = true;
	SPI_MASTER->EVENTS_END = 0;
}

void spim_init(uint32_t bitrate)
{
	/* MISO: Dir input, input connect, pull disabled, drive s0s1, sense disabled. */
	GPIO->PIN_CNF[PIN_MISO] = 0;

	/* Configure pins. */
	SPI_MASTER->PSEL.SCK = PIN_SCK;
	SPI_MASTER->PSEL.MOSI = PIN_MOSI;
	SPI_MASTER->PSEL.MISO = PIN_MISO;

	/* Configure buffers. */
	SPI_MASTER->RXD.PTR = (int)rx_buffer;
	SPI_MASTER->TXD.PTR = (int)tx_buffer;

	/* Most significant bit first, Sample on trailing edge, Active low.  */
	SPI_MASTER->CONFIG = SPIS_CONFIG_CPOL_Msk | SPIS_CONFIG_CPHA_Msk;

	/* Frequency */
	SPI_MASTER->FREQUENCY = bitrate;

	/* Enable interrupt to wake up at the end of a message. */
	SPI_MASTER->INTENSET = SPIM_INTENSET_END_Msk;
	irq_connect_dynamic(SPIM1_SPIS1_TWIM1_TWIS1_UARTE1_IRQn, 0, spim_isr, NULL, 0);
	irq_enable(SPIM1_SPIS1_TWIM1_TWIS1_UARTE1_IRQn);

	/* Enable. */
	SPI_MASTER->ENABLE = SPIM_ENABLE_ENABLE_Enabled;

	/* SCK: Dir output, input disconnect, pull disabled, drive h0h1, sense disabled. */
	GPIO->PIN_CNF[PIN_SCK] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
				 (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
				 (GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos);
	/* MOSI: Dir output, input disconnect, pull disabled, drive h0h1, sense disabled. */
	GPIO->PIN_CNF[PIN_MOSI] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
				  (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
				  (GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos);
	/* CS: high. */
	GPIO->OUTSET = 1 << PIN_CS;
	/* CS: Dir output, input disconnect, pull disabled, drive h0h1, sense disabled. */
	GPIO->PIN_CNF[PIN_CS] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
				(GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
				(GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos);

	lp_printf("    SCK     P0.%02d\n", PIN_SCK);
	lp_printf("    MOSI    P0.%02d\n", PIN_MOSI);
	lp_printf("    MISO    P0.%02d\n", PIN_MISO);
	lp_printf("    CS      P0.%02d\n", PIN_CS);
}

int spim_send(int size)
{
	SPI_MASTER->TXD.MAXCNT = size;
	SPI_MASTER->RXD.MAXCNT = 0;

	/* CS: low. */
	GPIO->OUTCLR = 1 << PIN_CS;

	SPI_MASTER->TASKS_START = 1;

	while (!transfer_done) {
		__WFI();
	}

	transfer_done = false;

	/* CS: high. */
	GPIO->OUTSET = 1 << PIN_CS;

	return SPI_MASTER->TXD.AMOUNT;
}

int spim_send_delayed(int size)
{
	SPI_MASTER->TXD.MAXCNT = size;
	SPI_MASTER->RXD.MAXCNT = 0;

	/* CS: low. */
	GPIO->OUTCLR = 1 << PIN_CS;

	usleep(80);

	SPI_MASTER->TASKS_START = 1;

	while (!transfer_done) {
		__WFI();
	}

	transfer_done = false;

	usleep(1);

	/* CS: high. */
	GPIO->OUTSET = 1 << PIN_CS;

	return SPI_MASTER->TXD.AMOUNT;
}

int spim_recv(int size)
{
	SPI_MASTER->TXD.MAXCNT = 0;
	SPI_MASTER->RXD.MAXCNT = size;

	/* CS: low. */
	GPIO->OUTCLR = 1 << PIN_CS;

	SPI_MASTER->TASKS_START = 1;

	while (!transfer_done) {
		__WFI();
	}

	transfer_done = false;

	/* CS: high. */
	GPIO->OUTSET = 1 << PIN_CS;

	return SPI_MASTER->RXD.AMOUNT;
}

int spim_recv_delayed(int size)
{
	SPI_MASTER->TXD.MAXCNT = 0;
	SPI_MASTER->RXD.MAXCNT = size;

	/* CS: low. */
	GPIO->OUTCLR = 1 << PIN_CS;

	usleep(80);

	SPI_MASTER->TASKS_START = 1;

	while (!transfer_done) {
		__WFI();
	}

	transfer_done = false;

	usleep(1);

	/* CS: high. */
	GPIO->OUTSET = 1 << PIN_CS;

	return SPI_MASTER->RXD.AMOUNT;
}

void spim_deinit(void)
{
	SPI_MASTER->INTENCLR = SPIM_INTENCLR_END_Msk;
	GPIO->PIN_CNF[PIN_SCK] = 0;
	GPIO->PIN_CNF[2] = 0;
	GPIO->PIN_CNF[PIN_CS] = 0;
	SPI_MASTER->ENABLE = 0;
}
