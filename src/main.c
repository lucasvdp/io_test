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
#include <modem/nrf_modem_lib.h>

uint8_t tx_buffer[1024];
uint8_t rx_buffer[2048];

#if DT_NODE_EXISTS(DT_NODELABEL(spi_master))
#include "spi_master_dt.c"
#elif DT_NODE_EXISTS(DT_NODELABEL(spi_slave))
#include "spi_slave_dt.c"
#elif DT_NODE_EXISTS(DT_NODELABEL(uart))
#include "uart_dt.c"
#else
#define RAW_TEST
static uint8_t input;

#define SPI_SLAVE NRF_SPIS1_NS
#define GPIO      NRF_P0_NS
#define UART      NRF_UARTE0_NS

void spi_slave_init(void);
void spi_slave_test(size_t size);
void spi_slave_deinit(void);

void init(void)
{
	NRF_UARTE0_NS->RXD.PTR = (int)&input;
	NRF_UARTE0_NS->RXD.MAXCNT = 1;
}

void (*test)(size_t) = NULL;

#endif

int main(void)
{

	/* Disable UART0 receive as we are not using it. */
	// NRF_UARTE0_NS->INTENCLR = UARTE_INTENCLR_ENDRX_Msk;
	// NRF_UARTE0_NS->TASKS_STOPRX = 1;

	// NRF_GPIOTE1_NS->CONFIG[0] = GPIOTE_CONFIG_MODE_Event |
	// 			    16 << GPIOTE_CONFIG_PSEL_Pos |
	// 			    GPIOTE_CONFIG_POLARITY_LoToHi << GPIOTE_CONFIG_POLARITY_Pos;
	// NRF_GPIOTE1_NS->PUBLISH_IN[0] = GPIOTE_PUBLISH_IN_EN_Msk |
	// 				0 << GPIOTE_PUBLISH_IN_CHIDX_Pos;
	// NRF_UARTE0_NS->SUBSCRIBE_STARTRX = UARTE_SUBSCRIBE_STARTRX_EN_Msk |
	// 				   0 << UARTE_SUBSCRIBE_STARTRX_CHIDX_Pos;
	// NRF_DPPIC->CHENSET = (1 << 0);

	// NRF_GPIOTE1_NS->CONFIG[1] = GPIOTE_CONFIG_MODE_Event |
	// 			    16 << GPIOTE_CONFIG_PSEL_Pos |
	// 			    GPIOTE_CONFIG_POLARITY_HiToLo << GPIOTE_CONFIG_POLARITY_Pos;
	// NRF_GPIOTE1_NS->PUBLISH_IN[1] = GPIOTE_PUBLISH_IN_EN_Enabled << GPIOTE_PUBLISH_IN_EN_Pos |
	// 				1 << GPIOTE_PUBLISH_IN_CHIDX_Pos;
	// NRF_UARTE0_NS->SUBSCRIBE_STOPRX = UARTE_SUBSCRIBE_STARTRX_EN_Msk |
	// 				  1 << UARTE_SUBSCRIBE_STARTRX_CHIDX_Pos;
	// NRF_DPPIC->CHENSET = (1 << 1);


	/* Modem is not used but always needs to be initialised to prevent 3 mA power drain. */
	nrf_modem_lib_init();

	printf("Sample has started\n");

	for (int i = 0; i < sizeof(tx_buffer); i++) {
		tx_buffer[i] = i;
	}

	sleep(1);
	init();

	while (1) {
#ifdef RAW_TEST
		char *test_name;

		printf("\nSelect test:\n");
		printf("  a. SPI slave\n");

		NRF_UARTE0_NS->EVENTS_ENDRX = 0;
		NRF_UARTE0_NS->TASKS_STARTRX = 1;

		while (!NRF_UARTE0_NS->EVENTS_ENDRX) {
			usleep(100000);
		}

		NRF_UARTE0_NS->TASKS_STOPRX = 1;

		switch (input) {
		case 'a':
			test_name = "SPI slave";
			spi_slave_init();
			test = spi_slave_test;
			break;
		default:
			test_name = "";
			break;
		}

		if (strlen(test_name)) {
			printf("selected test '%s'\n", test_name);
		} else {
			printf("invalid selection '%c'\n", input);
			continue;
		}

		sleep(1);
#endif
		tx_buffer[0] = 0;
		tx_buffer[1] = 16;
		test(16);
		sleep(5);

		tx_buffer[0] = 1024 >> 8;
		tx_buffer[1] = 1024 & 0xff;
		test(1024);
		sleep(10);

#ifdef RAW_TEST
		NRF_UARTE1_NS->ENABLE = 0;
		printf("Done with test '%s'\n", test_name);
#endif
	}
}
