/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <zephyr/kernel.h>

#define UART    NRF_UARTE1_NS
#define TIMER   NRF_TIMER0_NS
#define GPIO    NRF_P0_NS
#define GPIOTE  NRF_GPIOTE1_NS
#define REQ_DPPI_CHANNEL 1
#define REQ_GPIOTE_NR 0
#define RDY_DPPI_CHANNEL 2
#define RDY_GPIOTE_NR 1
#define PIN_TXD 6
#define PIN_RXD 7
#define PIN_REQ 2
#define PIN_RDY 3

extern uint8_t tx_buffer[1024];
extern uint8_t rx_buffer[2048];

int lp_printf(const char *fmt, ...);

K_SEM_DEFINE(uart_done, 0, 1);

void uart_isr(const void *arg)
{
	if (UART->EVENTS_ENDRX) {
		UART->EVENTS_ENDRX = 0;
	}
	if (UART->EVENTS_ENDTX) {
		UART->EVENTS_ENDTX = 0;
	}
	k_sem_give(&uart_done);
}

void uart_init(uint32_t bitrate)
{
	/* Dir input, input connect, pull disabled, drive s0s1, sense disabled. */
	GPIO->PIN_CNF[PIN_RXD] = 0;

	/* Select pins. */
	UART->PSEL.TXD = PIN_TXD;
	UART->PSEL.RXD = PIN_RXD;

	/* Configure buffers. */
	UART->RXD.PTR = (int)rx_buffer;
	UART->RXD.MAXCNT = sizeof(rx_buffer);
	UART->TXD.PTR = (int)tx_buffer;
	UART->TXD.MAXCNT = sizeof(tx_buffer);

	/* Baudrate 1M. */
	UART->BAUDRATE = bitrate;

	/* HW flow control disabled, Parity N, Stopbits 1. */
	UART->CONFIG = 0;


	/* Enable interrupt to wake up at the end of a message. */
	UART->INTENSET = UARTE_INTENSET_ENDRX_Msk | UARTE_INTENSET_ENDTX_Msk;
	irq_connect_dynamic(SPIM1_SPIS1_TWIM1_TWIS1_UARTE1_IRQn, 0, uart_isr, NULL, 0);
	irq_enable(SPIM1_SPIS1_TWIM1_TWIS1_UARTE1_IRQn);

	/* Enable. */
	UART->ENABLE = UARTE_ENABLE_ENABLE_Enabled;

	/* Dir output, input disconnect, pull disabled, drive s0s1, sense disabled. */
	GPIO->PIN_CNF[PIN_TXD] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
				 (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos);

	lp_printf("    TXD     P0.%02d\n", PIN_TXD);
	lp_printf("    RXD     P0.%02d\n", PIN_RXD);
}

void uart_timeout_init(uint32_t bitrate)
{
	uart_init(bitrate);

	/* Set a timer to stop receive before the buffer is full if no more bytes are received. */
	TIMER->CC[0] = 1000;	/* 1 ms */
	TIMER->SHORTS = TIMER_SHORTS_COMPARE0_STOP_Msk;

	/* Start and clear a timer when a byte is received using channel 1.  */
	UART->PUBLISH_RXDRDY = UARTE_PUBLISH_RXDRDY_EN_Msk | 1;
	TIMER->SUBSCRIBE_START = TIMER_SUBSCRIBE_START_EN_Msk | 1;
	TIMER->SUBSCRIBE_CLEAR = TIMER_SUBSCRIBE_CLEAR_EN_Msk | 1;
	NRF_DPPIC->CHENSET = (1 << 1);

	/* Stop UART receive on timeout using channel 2. */
	TIMER->PUBLISH_COMPARE[0] = TIMER_PUBLISH_COMPARE_EN_Msk | 2;
	UART->SUBSCRIBE_STOPRX = UARTE_SUBSCRIBE_STOPRX_EN_Msk | 2;
	NRF_DPPIC->CHENSET = (1 << 2);
}

void uart_lp_init(uint32_t bitrate)
{
	uart_init(bitrate);

	/* Disable UART completely while not using it to save power. */
	UART->ENABLE = 0;

	/* Output low, connect input, pull disabled, standard 0, standard 1, no sense. */
	GPIO->OUTCLR = 1 << PIN_REQ;
	GPIO->PIN_CNF[PIN_REQ] = GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos;

	/* Output high, connect input, pull disabled, standard 0, disconnect 1, Sense high. */
	GPIO->OUTSET = 1 << PIN_RDY;
	GPIO->PIN_CNF[PIN_RDY] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
				 (GPIO_PIN_CNF_DRIVE_S0D1 << GPIO_PIN_CNF_DRIVE_Pos) |
				 (GPIO_PIN_CNF_SENSE_High << GPIO_PIN_CNF_SENSE_Pos);

	lp_printf("    REQ     P0.%02d\n", PIN_REQ);
	lp_printf("    RDY     P0.%02d\n", PIN_RDY);
}

int uart_send(size_t size)
{
	UART->TXD.MAXCNT = size;
	UART->TASKS_STARTTX = 1;

	k_sem_take(&uart_done, K_FOREVER);

	UART->TASKS_STOPTX = 1;
	return UART->TXD.AMOUNT;
}

int uart_lp_send(size_t size)
{
	UART->TXD.MAXCNT = size;

	/* Enable UART */
	UART->ENABLE = UARTE_ENABLE_ENABLE_Enabled;

	/* Enable TX ON REQ pin high to low using channel 1. */
	GPIOTE->CONFIG[REQ_GPIOTE_NR] = GPIOTE_CONFIG_MODE_Event |
				      (PIN_REQ << GPIOTE_CONFIG_PSEL_Pos) |
				      (GPIOTE_CONFIG_POLARITY_HiToLo << GPIOTE_CONFIG_POLARITY_Pos);
	GPIOTE->PUBLISH_IN[REQ_GPIOTE_NR] = GPIOTE_PUBLISH_IN_EN_Msk | REQ_DPPI_CHANNEL;
	UART->SUBSCRIBE_STARTTX = UARTE_SUBSCRIBE_STARTTX_EN_Msk | REQ_DPPI_CHANNEL;
	NRF_DPPIC->CHENSET = (1 << REQ_DPPI_CHANNEL);

	/* Set REQ pin to input with pullup this will signal RDY. */
	GPIO->PIN_CNF[PIN_REQ] = GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos;

	/* Wait for TX done. */
	k_sem_take(&uart_done, K_FOREVER);

	UART->TASKS_STOPTX = 1;

	/* Disable TX on REQ pin in two steps to prevent 23 µA current leak. */
	GPIOTE->CONFIG[REQ_GPIOTE_NR] = GPIOTE_CONFIG_MODE_Disabled |
					(PIN_REQ << GPIOTE_CONFIG_PSEL_Pos);
	GPIOTE->CONFIG[REQ_GPIOTE_NR] = 0;
	GPIOTE->PUBLISH_IN[REQ_GPIOTE_NR] = 0;
	UART->SUBSCRIBE_STARTTX = 0;
	NRF_DPPIC->CHENCLR = (1 << REQ_DPPI_CHANNEL);

	/* Dir output low, input disconnect, pull disabled, drive s0s1, sense disabled. */
	GPIO->OUTCLR = 1 << PIN_REQ;
	GPIO->PIN_CNF[PIN_REQ] = GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos;

	/* Disable UART completely to save power. */
	UART->ENABLE = 0;

	return UART->TXD.AMOUNT;
}

int uart_recv(int size)
{
	UART->RXD.MAXCNT = size;
	UART->TASKS_STARTRX = 1;

	k_sem_take(&uart_done, K_FOREVER);

	return UART->RXD.AMOUNT;
}

void pin_isr(const void *arg)
{
	__NOP();

	/* Enable UART andd RX. */
	UART->ENABLE = UARTE_ENABLE_ENABLE_Enabled;
	UART->TASKS_STARTRX = 1;

	/* Toggle RDY low to signal we have enabled RX. */
	GPIO->OUTCLR = 1 << PIN_RDY;
	__NOP();
	__NOP();
	__NOP();
	__NOP();
	__NOP();
	__NOP();
	__NOP();
	__NOP();
	GPIO->OUTSET = 1 << PIN_RDY;

	/* Link RDY pin high to low to STOPRX using Channel 2. */
	GPIOTE->CONFIG[RDY_GPIOTE_NR] = GPIOTE_CONFIG_MODE_Event |
				      (PIN_RDY << GPIOTE_CONFIG_PSEL_Pos) |
				      (GPIOTE_CONFIG_POLARITY_HiToLo << GPIOTE_CONFIG_POLARITY_Pos);
	GPIOTE->PUBLISH_IN[RDY_GPIOTE_NR] = GPIOTE_PUBLISH_IN_EN_Msk | RDY_DPPI_CHANNEL;
	UART->SUBSCRIBE_STOPRX = UARTE_SUBSCRIBE_STOPRX_EN_Msk | RDY_DPPI_CHANNEL;
	NRF_DPPIC->CHENSET = (1 << RDY_DPPI_CHANNEL);

	/* Disable this interrupt. */
	GPIOTE->INTENCLR = GPIOTE_INTENCLR_PORT_Msk;
	GPIOTE->EVENTS_PORT = 0;
}

int uart_lp_recv(int size)
{
	/* We can't enable UART using DPPI so we will use an interrupt instead
	 * We use PORT instead of IN[n] to safe 20 µA.
	 */

	UART->RXD.MAXCNT = size + 1; /* Shortcut, we want pin RDY to stop RX not buffer overrun. */

	/* Enable interrupt on pin RDY high. */
	GPIOTE->EVENTS_PORT = 0;
	GPIOTE->INTENSET = GPIOTE_INTENSET_PORT_Msk;
	irq_connect_dynamic(GPIOTE1_IRQn, 0, pin_isr, NULL, 0);
	irq_enable(GPIOTE1_IRQn);

	/* Wait for RX to finish. */
	k_sem_take(&uart_done, K_FOREVER);

	/* Disable interrupt on RDY pin in two steps to prevent 23 µA current leak. */
	GPIOTE->CONFIG[RDY_GPIOTE_NR] = GPIOTE_CONFIG_MODE_Disabled |
					(PIN_RDY << GPIOTE_CONFIG_PSEL_Pos);
	GPIOTE->CONFIG[RDY_GPIOTE_NR] = 0;
	GPIOTE->PUBLISH_IN[RDY_GPIOTE_NR] = 0;
	UART->SUBSCRIBE_STOPRX = 0;
	NRF_DPPIC->CHENCLR = (1 << RDY_DPPI_CHANNEL);

	/* Disable UART completely to save power. */
	UART->ENABLE = 0;

	return UART->RXD.AMOUNT;
}

void uart_deinit(void)
{
	GPIO->PIN_CNF[PIN_TXD] = 0;
	UART->INTENCLR = UARTE_INTENCLR_ENDRX_Msk | UARTE_INTENCLR_ENDTX_Msk;

	/* Disable. */
	UART->ENABLE = 0;
}

void uart_timeout_deinit(void)
{
	uart_deinit();

	TIMER->SHORTS = 0;

	/* Start and clear a timer when a byte is received using channel 1.  */
	UART->PUBLISH_RXDRDY = 0;
	TIMER->SUBSCRIBE_START = 0;
	TIMER->SUBSCRIBE_CLEAR = 0;
	NRF_DPPIC->CHENCLR = (1 << 1);

	/* Stop UART receive on timeout using channel 2. */
	TIMER->PUBLISH_COMPARE[0] = 0;
	UART->SUBSCRIBE_STOPRX = 0;
	NRF_DPPIC->CHENCLR = (1 << 2);
}
