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

	/* Enable. */
	SPI_SLAVE->ENABLE = SPIS_ENABLE_ENABLE_Enabled;

	/* MISO: Dir output, input disconnect, pull disabled, drive h0h1, sense disabled. */
	GPIO->PIN_CNF[3] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
			   (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
			   (GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos);
}

void spi_slave_send(int size)
{
	SPI_SLAVE->TXD.MAXCNT = size;
	SPI_SLAVE->RXD.MAXCNT = 0;
	SPI_SLAVE->EVENTS_END = 0;
	SPI_SLAVE->TASKS_RELEASE = 1;

	while (!SPI_SLAVE->EVENTS_END) {
		sleep(1);
	}

	if (SPI_SLAVE->TXD.AMOUNT != size) {
		lp_printf("Send %d instead of %d bytes\n", SPI_SLAVE->TXD.AMOUNT, size);
	}
}

int spi_slave_recv(int size, int timeout)
{
	SPI_SLAVE->TXD.MAXCNT = 0;
	SPI_SLAVE->RXD.MAXCNT = size;
	SPI_SLAVE->EVENTS_END = 0;
	SPI_SLAVE->TASKS_RELEASE = 1;

	while (!SPI_SLAVE->EVENTS_END) {
		sleep(1);
	}

	return SPI_SLAVE->RXD.AMOUNT;
}

void spi_slave_deinit(void)
{
	GPIO->PIN_CNF[3] = 0;
	SPI_SLAVE->ENABLE = 0;
}
