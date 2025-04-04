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

extern uint8_t tx_buffer[1024];
extern uint8_t rx_buffer[2048];

int lp_printf(const char *fmt, ...);

static bool transfer_done = false;

void spi_master_isr(const void *arg)
{
	transfer_done = true;
	SPI_MASTER->EVENTS_END = 0;
}

void spi_master_init(void)
{
	/* MISO: Dir input, input connect, pull disabled, drive s0s1, sense disabled. */
	GPIO->PIN_CNF[3] = 0;

	/* Configure pins. */
	SPI_MASTER->PSEL.SCK = 6;
	SPI_MASTER->PSEL.MISO = 3;
	SPI_MASTER->PSEL.MOSI = 2;

	/* Configure buffers. */
	SPI_MASTER->RXD.PTR = (int)rx_buffer;
	SPI_MASTER->TXD.PTR = (int)tx_buffer;

	/* Most significant bit first, Sample on trailing edge, Active low.  */
	SPI_MASTER->CONFIG = SPIS_CONFIG_CPOL_Msk | SPIS_CONFIG_CPHA_Msk;

	/* Frequency */
	SPI_MASTER->FREQUENCY = SPIM_FREQUENCY_FREQUENCY_M1;

	/* Enable interrupt to wake up at the end of a message. */
	SPI_MASTER->INTENSET = SPIM_INTENSET_END_Msk;
	irq_connect_dynamic(SPIM1_SPIS1_TWIM1_TWIS1_UARTE1_IRQn, 0, spi_master_isr, NULL, 0);
	irq_enable(SPIM1_SPIS1_TWIM1_TWIS1_UARTE1_IRQn);

	/* Enable. */
	SPI_MASTER->ENABLE = SPIM_ENABLE_ENABLE_Enabled;

	/* SCK: Dir output, input disconnect, pull disabled, drive h0h1, sense disabled. */
	GPIO->PIN_CNF[6] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
			   (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
			   (GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos);
	/* MOSI: Dir output, input disconnect, pull disabled, drive h0h1, sense disabled. */
	GPIO->PIN_CNF[2] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
			   (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
			   (GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos);
	/* CS: high. */
	GPIO->OUTSET = GPIO_OUTSET_PIN7_Msk;
	/* CS: Dir output, input disconnect, pull disabled, drive h0h1, sense disabled. */
	GPIO->PIN_CNF[7] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
			   (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
			   (GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos);
}

void spi_master_send(int size)
{
	/* Delay so the Slave is always ready first. */
	sleep(2);

	SPI_MASTER->TXD.MAXCNT = size;
	SPI_MASTER->RXD.MAXCNT = 0;

	/* CS: low. */
	GPIO->OUTCLR = GPIO_OUTCLR_PIN7_Msk;

	usleep(1);

	SPI_MASTER->TASKS_START = 1;

	while (!transfer_done) {
		__WFI();
	}

	transfer_done = false;

	usleep(1);

	/* CS: high. */
	GPIO->OUTSET = GPIO_OUTSET_PIN7_Msk;
}

int spi_master_recv(int size, int timeout)
{
	/* Delay so the Slave is always ready first. */
	sleep(2);

	SPI_MASTER->TXD.MAXCNT = 0;
	SPI_MASTER->RXD.MAXCNT = size;

	/* CS: low. */
	GPIO->OUTCLR = GPIO_OUTCLR_PIN7_Msk;

	usleep(1);

	SPI_MASTER->TASKS_START = 1;

	while (!transfer_done) {
		__WFI();
	}

	transfer_done = false;

	usleep(1);

	/* CS: high. */
	GPIO->OUTSET = GPIO_OUTSET_PIN7_Msk;

	return SPI_MASTER->RXD.AMOUNT;
}

void spi_master_deinit(void)
{
	SPI_MASTER->INTENCLR = SPIM_INTENCLR_END_Msk;
	GPIO->PIN_CNF[6] = 0;
	GPIO->PIN_CNF[2] = 0;
	GPIO->PIN_CNF[7] = 0;
	SPI_MASTER->ENABLE = 0;
}
