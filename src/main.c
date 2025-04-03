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

/* Keeping UARTE0 on drains a bit of power */
int lp_printf(const char *fmt, ...)
{
	va_list args;
	int ret;

	NRF_UARTE0_NS->ENABLE = UARTE_ENABLE_ENABLE_Enabled;

	va_start(args, fmt);
	ret = vprintf(fmt, args);  // Standard printf (or your custom version)
	va_end(args);

	while (!NRF_UARTE0_NS->EVENTS_ENDTX) {
		usleep(1000);
	}
	NRF_UARTE0_NS->ENABLE = 0;

	return ret;
}

void lp_print_rx(int received)
{
	if (received == 0) {
		lp_printf("Nothing received\n");
		return;
	}

	if (received < 0) {
		lp_printf("Receive error %x\n", -received);
	}

	lp_printf("Received %d bytes %02x%02x%02x%02x %02x%02x%02x%02x ...\n", received,
			rx_buffer[0], rx_buffer[1], rx_buffer[2], rx_buffer[3],
			rx_buffer[4], rx_buffer[5], rx_buffer[6], rx_buffer[7]);

	if (received != (rx_buffer[0] << 8) + rx_buffer[1]) {
		lp_printf("Size mismatch\n");
	}

	for (int i = 2; i < received; i++) {
		if (rx_buffer[i] != (i & 0xff)) {
			lp_printf("Mismatch in byte %d expected %02x got %02x\n", i, i & 0xff,
				  rx_buffer[i]);
			break;
		}
	}
}

uint8_t lp_get(void)
{
	static uint8_t input;

	NRF_UARTE0_NS->ENABLE = UARTE_ENABLE_ENABLE_Enabled;

	NRF_UARTE0_NS->RXD.PTR = (int)&input;
	NRF_UARTE0_NS->RXD.MAXCNT = 1;
	NRF_UARTE0_NS->EVENTS_ENDRX = 0;
	NRF_UARTE0_NS->TASKS_STARTRX = 1;

	while (!NRF_UARTE0_NS->EVENTS_ENDRX) {
		usleep(100000);
	}

	NRF_UARTE0_NS->TASKS_STOPRX = 1;

	NRF_UARTE0_NS->ENABLE = 0;

	return input;
}

#if DT_NODE_EXISTS(DT_NODELABEL(spi_master))

/* west build -b nrf9151dk/nrf9151/ns --pristine -- -DDTC_OVERLAY_FILE=boards/spi_master.overlay */
#include "spi_master_dt.c"

#elif DT_NODE_EXISTS(DT_NODELABEL(spi_slave))

/* west build -b nrf9151dk/nrf9151/ns --pristine -- -DDTC_OVERLAY_FILE=boards/spi_slave.overlay */
#include "spi_slave_dt.c"

#elif DT_NODE_EXISTS(DT_NODELABEL(uart))

/* west build -b nrf9151dk/nrf9151/ns --pristine -- -DDTC_OVERLAY_FILE=boards/uart.overlay */
#include "uart_dt.c"

#else

/* Not using device tree, use runtime test selection using UART0. */

#define RAW_TEST

#define SPI_SLAVE NRF_SPIS1_NS
#define GPIO      NRF_P0_NS
#define UART      NRF_UARTE0_NS

void spi_slave_init(void);
void spi_slave_send(int size);
int spi_slave_recv(int size, int timeout);
void spi_slave_deinit(void);

void uart_basic_init(void);
void uart_basic_send(int size);
int uart_basic_recv(int size, int timeout);
void uart_basic_deinit(void);

void no_send(int size)
{

}

int no_recv(int size, int timeout)
{
	return 0;
}

void no_deinit(void)
{

}

void (*send)(int size) = no_send;
int (*recv)(int size, int timeout) = no_recv;
void (*deinit)(void) = no_deinit;

#endif

int main(void)
{


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

	/* Disable UART0 to reduce power. */
	NRF_UARTE0_NS->TASKS_STOPRX = 1;
	NRF_UARTE0_NS->INTEN = 0;
	NRF_UARTE0_NS->ENABLE = 0;

	lp_printf("Sample has started\n");

	for (int i = 0; i < sizeof(tx_buffer); i++) {
		tx_buffer[i] = i;
	}

	while (1) {
		size_t received;
#ifdef RAW_TEST
		uint8_t input;
		char *test_name;

		lp_printf("\nSelect test:\n");
		lp_printf("  a. None\n");
		lp_printf("  b. SPI slave\n");
		lp_printf("  c. UART basic\n");

		input = lp_get();

		switch (input) {
		case 'a':
			test_name = "None";
			send = no_send;
			recv = no_recv;
			deinit = no_deinit;
			break;
		case 'b':
			test_name = "SPI slave";
			spi_slave_init();
			send = spi_slave_send;
			recv = spi_slave_recv;
			deinit = spi_slave_deinit;
			break;
		case 'c':
			test_name = "UART basic";
			uart_basic_init();
			send = uart_basic_send;
			recv = uart_basic_recv;
			deinit = uart_basic_deinit;
			break;
		default:
			test_name = "";
			break;
		}

		if (strlen(test_name)) {
			lp_printf("\nselected test '%s'\n", test_name);
		} else {
			lp_printf("invalid selection '%c'\n", input);
			continue;
		}
#else
		init();
#endif
		lp_printf("test send 16 bytes\n");
		sleep(1);
		tx_buffer[0] = 0;
		tx_buffer[1] = 16;
		send(16);

		sleep(5);

		lp_printf("test send 1024 bytes\n");
		sleep(1);
		tx_buffer[0] = 1024 >> 8;
		tx_buffer[1] = 1024 & 0xff;
		send(1024);

		sleep(5);

		lp_printf("test receive\n");
		sleep(1);
		received = recv(16, 60);
		sleep(1);
		lp_print_rx(received);

		sleep(4);

		received = recv(1024, 60);
		sleep(1);
		lp_print_rx(received);

		sleep(4);

		lp_printf("deinit\n");
		deinit();
		sleep(1);

#ifdef RAW_TEST
		lp_printf("Done with test '%s'\n", test_name);
#endif
	}
}
