/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(spi, LOG_LEVEL_DBG);

#define SPI_SLAVE NRF_SPIS1_NS
#define GPIO      NRF_P0_NS

extern uint8_t tx_buffer[1024];
extern uint8_t rx_buffer[2048];

void spi_slave_init(void)
{
	GPIO->PIN_CNF[2] = 0x300;
	GPIO->PIN_CNF[3] = 0x302;
	GPIO->PIN_CNF[6] = 0x300;
	GPIO->PIN_CNF[7] = 0x300;

	SPI_SLAVE->PSEL.SCK = 6;
	SPI_SLAVE->PSEL.MISO = 3;
	SPI_SLAVE->PSEL.MOSI = 2;
	SPI_SLAVE->PSEL.CSN = 7;
	SPI_SLAVE->RXD.PTR = (int)rx_buffer;
	SPI_SLAVE->RXD.MAXCNT = sizeof(rx_buffer);
	SPI_SLAVE->TXD.PTR = (int)tx_buffer;
	SPI_SLAVE->TXD.MAXCNT = sizeof(tx_buffer);
	SPI_SLAVE->CONFIG = SPIS_CONFIG_CPOL_Msk | SPIS_CONFIG_CPHA_Msk;
	SPI_SLAVE->SHORTS = 4;
	SPI_SLAVE->ENABLE = 2;

	SPI_SLAVE->TASKS_RELEASE = 1;
}

void spi_slave_test(size_t size)
{
	while (1) {
		while (!SPI_SLAVE->EVENTS_END) {
			sleep(1);
		}

		printf("\nReceived %d\n", SPI_SLAVE->RXD.AMOUNT);
		printf("success %d: %02x %02x %02x %02x\n", (rx_buffer[0] << 8) + rx_buffer[1],
		       rx_buffer[2], rx_buffer[3], rx_buffer[4], rx_buffer[5]);
		printf("status %x\n", SPI_SLAVE->STATUS);
		printf("send %d\n", SPI_SLAVE->TXD.AMOUNT);
		SPI_SLAVE->EVENTS_END = 0;
		SPI_SLAVE->TASKS_RELEASE = 1;
	}
}

void spi_slave_deinit(void)
{

}
