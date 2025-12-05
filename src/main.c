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

#define RED	"\e[0;31m"
#define GREEN	"\e[0;32m"
#define NORMAL	"\e[0m"

uint8_t tx_buffer[8*1024];
uint8_t rx_buffer[8*1024];

/* Keeping UARTE0 on drains a bit of power */
int lp_printf(const char *fmt, ...)
{
	va_list args;
	int ret;

	NRF_UARTE0_NS->ENABLE = UARTE_ENABLE_ENABLE_Enabled;

	va_start(args, fmt);
	ret = vprintf(fmt, args);
	va_end(args);

	while (!NRF_UARTE0_NS->EVENTS_ENDTX) {
		usleep(1000);
	}
	NRF_UARTE0_NS->ENABLE = 0;

	return ret;
}

void lp_print_rx(int received, int expected)
{
	lp_printf("Received %d bytes %02x%02x%02x%02x %02x%02x%02x%02x ... ", received,
			rx_buffer[0], rx_buffer[1], rx_buffer[2], rx_buffer[3],
			rx_buffer[4], rx_buffer[5], rx_buffer[6], rx_buffer[7]);

	if (received != expected) {
		lp_printf(RED "Instead of %d bytes" NORMAL, expected);
		return;

	}

	if (received != (rx_buffer[0] << 8) + rx_buffer[1]) {
		lp_printf(RED "Packet header indicates %d bytes" NORMAL,
			  (rx_buffer[0] << 8) + rx_buffer[1]);
		return;
	}

	for (int i = 2; i < received; i++) {
		if (rx_buffer[i] != (i & 0xff)) {
			lp_printf(RED "Mismatch in byte %d" NORMAL" expected %02x got %02x\n", i,
				  i & 0xff, rx_buffer[i]);
			return;
		}
	}

	lp_printf(GREEN "OK\n" NORMAL);
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

#elif DT_NODE_EXISTS(DT_NODELABEL(uart_lp))

/* west build -b nrf9151dk/nrf9151/ns --pristine -- -DDTC_OVERLAY_FILE=boards/uart_lp.overlay \
 * -DEXTRA_CONF_FILE=boards/uart_lp.conf
 */
#include "uart_lp_dt.c"

#elif DT_NODE_EXISTS(DT_NODELABEL(twi_master))

/* west build -b nrf9151dk/nrf9151/ns --pristine -- -DDTC_OVERLAY_FILE=boards/twi_master.overlay */
#include "twi_master_dt.c"

#elif DT_NODE_EXISTS(DT_NODELABEL(twi_slave))
/* Doesn't work, not supported? */
/* west build -b nrf9151dk/nrf9151/ns --pristine -- -DDTC_OVERLAY_FILE=boards/twi_slave.overlay */
#include "twi_slave_dt.c"
#else

/* Not using device tree, use runtime test selection using UART0. */

#define RAW_TEST

void spim_init(uint32_t bitrate);
int spim_send(int size);
int spim_send_delayed(int size);
int spim_recv(int size);
int spim_recv_delayed(int size);
void spim_deinit(void);

void spis_init(uint32_t bitrate);
int spis_send(int size);
int spis_recv(int size);
void spis_deinit(void);

void uart_init(uint32_t bitrate);
int uart_send(int size);
int uart_recv(int size);
void uart_deinit(void);

void uart_timeout_init(uint32_t bitrate);
void uart_timeout_deinit(void);

void uart_lp_init(uint32_t bitrate);
int uart_lp_send(int size);
int uart_lp_recv(int size);

void twim_init(uint32_t bitrate);
int twim_send(int size);
int twim_recv(int size);
void twim_deinit(void);

void twis_init(uint32_t bitrate);
int twis_send(int size);
int twis_recv(int size);
void twis_deinit(void);

void gpio_init(uint32_t bitrate);
int gpio_send(int size);
int gpio_recv(int size);
void gpio_deinit(void);

int no_send(int size)
{
	return 0;
}

int no_recv(int size)
{
	return 0;
}

void no_deinit(void)
{

}

int (*send)(int size) = no_send;
int (*recv)(int size) = no_recv;
void (*deinit)(void) = no_deinit;

typedef struct {
	char *label;
	void (*init)(uint32_t);
	uint32_t bitrate;
	int (*send)(int);
	int (*recv)(int);
	void (*deinit)(void);
} device_option_t;

char *select_device(void)
{
	char input;
	int index;
	device_option_t device_menu[] = {
		{"None (measure idle power)", NULL, 0, no_send, no_recv, no_deinit},
		{"SPI master @ 125 kbps with increased CSN to CLK delay", spim_init,
				SPIM_FREQUENCY_FREQUENCY_K125,
				spim_send_delayed, spim_recv_delayed, spim_deinit},
		{"SPI master @ 1 Mbps with increased CSN to CLK delay", spim_init,
				SPIM_FREQUENCY_FREQUENCY_M1,
				spim_send_delayed, spim_recv_delayed, spim_deinit},
		{"SPI master @ 1 Mbps", spim_init, SPIM_FREQUENCY_FREQUENCY_M1,
				spim_send, spim_recv, spim_deinit},
		{"SPI master @ 8 Mbps with increased CSN to CLK delay", spim_init,
				SPIM_FREQUENCY_FREQUENCY_M8,
				spim_send_delayed, spim_recv_delayed, spim_deinit},
		{"SPI slave", spis_init, 0, spis_send, spis_recv, spis_deinit},
		{"UART @ 115.2 kbps", uart_init, UARTE_BAUDRATE_BAUDRATE_Baud115200,
				uart_send, uart_recv, uart_deinit},
		{"UART @ 1 Mbps", uart_init, UARTE_BAUDRATE_BAUDRATE_Baud1M,
				uart_send, uart_recv, uart_deinit},
		{"UART @ 2 Mbps", uart_init, 2 * UARTE_BAUDRATE_BAUDRATE_Baud1M,
				uart_send, uart_recv, uart_deinit},
		{"UART with RX timeout @ 1 Mbps", uart_timeout_init, UARTE_BAUDRATE_BAUDRATE_Baud1M,
				uart_send, uart_recv, uart_timeout_deinit},
		{"UART with enable pins @ 1 Mbps", uart_lp_init, UARTE_BAUDRATE_BAUDRATE_Baud1M,
				uart_lp_send, uart_lp_recv, uart_deinit},
		{"TWI master @ 100 kbps", twim_init, TWIM_FREQUENCY_FREQUENCY_K100,
				twim_send, twim_recv, twim_deinit},
		{"TWI master @ 250 kbps", twim_init, TWIM_FREQUENCY_FREQUENCY_K250,
				twim_send, twim_recv, twim_deinit},
		{"TWI master @ 400 kbps", twim_init, TWIM_FREQUENCY_FREQUENCY_K400,
				twim_send, twim_recv, twim_deinit},
		{"TWI slave", twis_init, 0, twis_send, twis_recv, twis_deinit},
		{"GPIO interrupt response timing", gpio_init, 0, gpio_send, gpio_recv, gpio_deinit},
	};

	lp_printf("\nSelect peripheral:\n");
	for (index = 0; index < ARRAY_SIZE(device_menu); index++) {
		lp_printf("  %c. %s\n", 'a' + index, device_menu[index].label);
	}

	lp_printf("Configuration:\n");
	lp_printf("  [. Constant latency (keep clock on)\n");
	lp_printf("  ]. Low power mode (disable clock while idle)\n");

	input = lp_get();

	if (input == '[') {
		lp_printf("Switching to Constant latency mode\n", input);
		NRF_POWER_NS->TASKS_CONSTLAT = 1;
		return "";
	}
	if (input == ']') {
		lp_printf("Switching to low power mode\n", input);
		NRF_POWER_NS->TASKS_LOWPWR = 1;
		return "";
	}
	if (input < 'a' || input >= 'a' + ARRAY_SIZE(device_menu)) {
		lp_printf("Invalid selection '%c'\n", input);
		return "";
	}

	index = input - 'a';

	lp_printf("Selected device '%s'\n", device_menu[index].label);

	if (device_menu[index].init) {
		device_menu[index].init(device_menu[index].bitrate);
	}
	send = device_menu[index].send;
	recv = device_menu[index].recv;
	deinit = device_menu[index].deinit;

	return device_menu[index].label;
}

#endif


typedef struct {
	const char *label;
	int size;
	int (*func)(int);
} test_option_t;

int sleep_10(int size)
{
	return sleep(10);
}

int set_size_and_send(int size)
{
	tx_buffer[0] = size >> 8;
	tx_buffer[1] = size & 0xff;
	return send(size);
}

bool run_test(void)
{
	int ret;
	int input;
	test_option_t test_menu[] = {
		{"Sleep 10 s", 0, sleep_10},
		{"Send 16 bytes", 16, set_size_and_send},
		{"Send 1024 bytes", 1024, set_size_and_send},
		{"Send 8 kbytes", 8 * 1024 - 2, set_size_and_send},
		{"Receive 16 bytes", 16, recv},
		{"Receive 1024 bytes", 1024, recv},
		{"Receive 8 kbytes", 8 * 1024 - 2, recv},
	};

	lp_printf("\nSelect test:\n");
	for (int i = 0; i < ARRAY_SIZE(test_menu); i++) {
		lp_printf("  %c. %s\n", '1' + i, test_menu[i].label);
	}
	lp_printf("  Esc. Disable device\n");

	input = lp_get();

	if (input == '\e') {
		lp_printf("Disable device\n");
		return false;
	}

	input -= '1';

	if (input < 0 || input >= ARRAY_SIZE(test_menu)) {
		/* Invalid selection. */
		lp_printf("Invalid selection '%c'\n", input + '1');
		return true;
	}

	lp_printf("Selected test '%s'\n", test_menu[input].label);

	sleep(1);

	ret = test_menu[input].func(test_menu[input].size);

	sleep(1);

	if (ret == 0) {
		lp_printf("Test done\n", ret);
	} else if (ret < 0) {
		lp_printf(RED "Test returned %d\n" NORMAL, ret);
	} else {
		if (test_menu[input].label[0] == 'R') {
			lp_print_rx(ret, test_menu[input].size);
		} else if (test_menu[input].size != ret) {
			lp_printf("Send %d bytes " RED "instead of %d bytes" NORMAL "\n", ret,
				  test_menu[input].size);
		} else {
			lp_printf("Send %d bytes " GREEN "OK" NORMAL "\n", ret);
		}
	}

	return true;
}


int main(void)
{
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
#ifdef RAW_TEST
		char *test_name = select_device();

		if (strlen(test_name) == 0) {
			continue;
		}
#else
		init();
#endif

		while (run_test());

		deinit();
		sleep(1);

#ifdef RAW_TEST
		lp_printf("'%s' disabled\n", test_name);
#endif
	}
}
