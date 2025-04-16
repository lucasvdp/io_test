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
	lp_printf("Received %d bytes %02x%02x%02x%02x %02x%02x%02x%02x ...\n", received,
			rx_buffer[0], rx_buffer[1], rx_buffer[2], rx_buffer[3],
			rx_buffer[4], rx_buffer[5], rx_buffer[6], rx_buffer[7]);

	if (received != (rx_buffer[0] << 8) + rx_buffer[1]) {
		lp_printf("Size mismatch, first bytes indicate %d but received %d\n",
			  (rx_buffer[0] << 8) + rx_buffer[1], received);
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

#define SPI_SLAVE NRF_SPIS1_NS
#define GPIO      NRF_P0_NS
#define UART      NRF_UARTE0_NS

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


int no_send(int size)
{
	return 0;
}

void gpiote_isr(const void *arg)
{
	GPIO->OUTCLR = GPIO_OUTSET_PIN1_Msk;
	NRF_GPIOTE1_NS->EVENTS_IN[0] = 0;
}

int no_recv(int size)
{

	// NRF_POWER_NS->TASKS_CONSTLAT = 1;
	GPIO->OUTSET = GPIO_OUTSET_PIN1_Msk;
	NRF_P0_NS->PIN_CNF[1] = GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos;
	NRF_P0_NS->PIN_CNF[2] = GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos;
	NRF_GPIOTE1_NS->CONFIG[0] = GPIOTE_CONFIG_MODE_Event << GPIOTE_CONFIG_MODE_Pos |
				    2 << GPIOTE_CONFIG_PSEL_Pos |
				    GPIOTE_CONFIG_POLARITY_HiToLo << GPIOTE_CONFIG_POLARITY_Pos;
	NRF_GPIOTE1_NS->INTENSET = GPIOTE_INTENSET_IN0_Msk;
	irq_connect_dynamic(GPIOTE1_IRQn, 0, gpiote_isr, NULL, 0);
	irq_enable(GPIOTE1_IRQn);

	sleep(10);

	NRF_GPIOTE1_NS->INTENCLR = GPIOTE_INTENCLR_IN0_Msk;
	GPIO->PIN_CNF[1] = 0;
	GPIO->PIN_CNF[2] = 0;

	return 0;//87.8 µs 11.5 µs 76.3
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
		{"None", NULL, 0, no_send, no_recv, no_deinit},
		{"SPI master @ 125 kbps with delay", spim_init, SPIM_FREQUENCY_FREQUENCY_K125,
				spim_send_delayed, spim_recv_delayed, spim_deinit},
		{"SPI master @ 1 Mbps with delay", spim_init, SPIM_FREQUENCY_FREQUENCY_M1,
				spim_send_delayed, spim_recv_delayed, spim_deinit},
		{"SPI master @ 1 Mbps", spim_init, SPIM_FREQUENCY_FREQUENCY_M1,
				spim_send, spim_recv, spim_deinit},
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
		{"TWI master @ 400 kbps", twim_init, TWIM_FREQUENCY_FREQUENCY_K400,
				twim_send, twim_recv, twim_deinit},
		{"TWI slave", twis_init, 0, twis_send, twis_recv, twis_deinit},
	};

	lp_printf("\nSelect device:\n");
	for (index = 0; index < ARRAY_SIZE(device_menu); index++) {
		lp_printf("  %c. %s\n", 'a' + index, device_menu[index].label);
	}

	lp_printf("  [. Constant latency (keep clock on)\n");
	lp_printf("  ]. Low power mode (disable clock while idle)\n");

	input = lp_get();

	if (input == '[') {
		lp_printf("Switching to Constant latency mode\n", input);
		NRF_POWER_NS->TASKS_CONSTLAT = 1;
	}
	if (input == ']') {
		lp_printf("Switching to low power mode\n", input);
		NRF_POWER_NS->TASKS_LOWPWR = 1;
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
	deinit = device_menu->deinit;

	return device_menu[index].label;
}

#endif


typedef struct {
	const char *label;
	int (*func)(void);
} test_option_t;

int sleep_10(void)
{
	return sleep(10);
}

int send_16(void)
{
	tx_buffer[0] = 0;
	tx_buffer[1] = 16;
	return send(16);
}

int send_1024(void)
{
	tx_buffer[0] = 1024 >> 8;
	tx_buffer[1] = 1024 & 0xff;
	return send(1024);
}

int recv_16(void)
{
	return recv(16);
}

int recv_1024(void)
{
	return recv(1024);
}

bool run_test(void)
{
	int ret;
	int input;
	test_option_t test_menu[] = {
		{"Sleep 10 s", sleep_10},
		{"Send 16 bytes", send_16},
		{"Send 1024 bytes", send_1024},
		{"Receive 16 bytes", recv_16},
		{"Receive 1024 bytes", recv_1024},
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

	ret = test_menu[input].func();

	sleep(1);

	if (ret == 0) {
		lp_printf("Test done\n", ret);
	} else if (ret < 0) {
		lp_printf("Test returned %d\n", ret);
	} else {
		if (test_menu[input].label[0] == 'R') {
			lp_print_rx(ret);
		} else {
			lp_printf("Send %d bytes\n", ret);
		}
	}

	return true;
}


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
