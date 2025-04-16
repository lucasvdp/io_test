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
#define PIN_TXD 6
#define PIN_RXD 7
#define PIN_REQ 2
#define PIN_RDY 3


extern uint8_t tx_buffer[1024];
extern uint8_t rx_buffer[2048];

int lp_printf(const char *fmt, ...);
bool rx_done = false;
bool tx_done = false;

void uart_isr(const void *arg)
{
	if (UART->EVENTS_ENDRX) {
		rx_done = true;
		UART->EVENTS_ENDRX = 0;
	}
	if (UART->EVENTS_ENDTX) {
		tx_done = true;
		UART->EVENTS_ENDTX = 0;
	}
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

	/* Output low, pull disabled,standard 0, standard 1. */
	GPIO->OUTCLR = 1 << PIN_REQ;
	GPIO->PIN_CNF[PIN_REQ] = GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos;

	/* Output disconnect, pull disabled, standard 0, disconnect 1. */
	GPIO->OUTSET = 1 << PIN_RDY;
	GPIO->PIN_CNF[PIN_RDY] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
				 (GPIO_PIN_CNF_DRIVE_S0D1 << GPIO_PIN_CNF_DRIVE_Pos);

	lp_printf("    REQ     P0.%02d\n", PIN_REQ);
	lp_printf("    RDY     P0.%02d\n", PIN_RDY);
}

int uart_send(size_t size)
{
	UART->TXD.MAXCNT = size;
	UART->TASKS_STARTTX = 1;

	while (!tx_done) {
		__WFI();
	}

	tx_done = false;

	UART->TASKS_STOPTX = 1;
	return UART->TXD.AMOUNT;
}

int uart_lp_send(size_t size)
{
	UART->TXD.MAXCNT = size;

	/* Start send ON REQ pin high to low using channel 1. */
	GPIOTE->CONFIG[0] = GPIOTE_CONFIG_MODE_Event |
			    (PIN_REQ << GPIOTE_CONFIG_PSEL_Pos) |
			    (GPIOTE_CONFIG_POLARITY_HiToLo << GPIOTE_CONFIG_POLARITY_Pos);
	GPIOTE->PUBLISH_IN[0] = GPIOTE_PUBLISH_IN_EN_Msk | 1;
	UART->SUBSCRIBE_STARTTX = UARTE_SUBSCRIBE_STARTTX_EN_Msk | 1;
	NRF_DPPIC->CHENSET = (1 << 1);

	/* Set REQ pin to input with pullup this will signal RDY. */
	GPIO->PIN_CNF[PIN_REQ] = GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos;

	while (!tx_done) {
		__WFI();
	}

	tx_done = false;

	UART->TASKS_STOPTX = 1;

	/* Disable start on REQ pin high to low. */
	GPIOTE->CONFIG[0] = GPIOTE_CONFIG_MODE_Disabled | (PIN_REQ << GPIOTE_CONFIG_PSEL_Pos);
	GPIOTE->CONFIG[0] = 0;
	GPIOTE->PUBLISH_IN[0] = 0;
	UART->SUBSCRIBE_STARTTX = 0;
	NRF_DPPIC->CHENCLR = (1 << 1);

	/* Dir output low, input disconnect, pull disabled, drive s0s1, sense disabled. */
	GPIO->OUTCLR = 1 << PIN_REQ;
	GPIO->PIN_CNF[PIN_REQ] = GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos;

	return UART->TXD.AMOUNT;
}

int uart_recv(int size)
{
	UART->RXD.MAXCNT = size;
	UART->TASKS_STARTRX = 1;

	while (!rx_done) {
		__WFI();
	}

	rx_done = false;

	return UART->RXD.AMOUNT;
}

void pin_isr(const void *arg)
{
	__NOP();
	UART->TASKS_STARTRX = 1;

	/* Dir output low, input disconnect, pull disabled, drive s0s1, sense disabled. */
	GPIOTE->CONFIG[1] = 0;
	GPIO->OUTCLR = 1 << PIN_RDY;

	__NOP();
	__NOP();
	__NOP();
	__NOP();
	__NOP();
	__NOP();
	__NOP();
	__NOP();

	/* Set RDY pin to input with pullup this will signal UART is Ready to REQ. */
	GPIO->OUTSET = 1 << PIN_RDY;

	/* Link RDY pin high to low to STOPRX using Channel 2. */
	GPIOTE->CONFIG[1] = GPIOTE_CONFIG_MODE_Event |
			    (PIN_RDY << GPIOTE_CONFIG_PSEL_Pos) |
			    (GPIOTE_CONFIG_POLARITY_HiToLo << GPIOTE_CONFIG_POLARITY_Pos);
	GPIOTE->PUBLISH_IN[1] = GPIOTE_PUBLISH_IN_EN_Msk | 2;
	UART->SUBSCRIBE_STOPRX = UARTE_SUBSCRIBE_STOPRX_EN_Msk | 2;
	NRF_DPPIC->CHENSET = (1 << 2);

	/* Disable this interupt. */
	GPIOTE->INTENCLR = GPIOTE_INTENCLR_IN1_Msk;
	GPIOTE->EVENTS_IN[1] = 0;
}

int uart_lp_recv(int size)
{
	/* We have to use an interrupt instead of DPPI links as we cannot use GPIOTE as an input
	 * and an output on the same pin at the same time.
	 * We could use two more pins instead of the interrupt.
	 */
	UART->RXD.MAXCNT = size;

	/* Enable interrupt on pin RDY low to high. */
	GPIOTE->CONFIG[1] = GPIOTE_CONFIG_MODE_Event |
			    (PIN_RDY << GPIOTE_CONFIG_PSEL_Pos) |
			    (GPIOTE_CONFIG_POLARITY_LoToHi << GPIOTE_CONFIG_POLARITY_Pos);
	GPIOTE->EVENTS_IN[1] = 0;
	GPIOTE->INTENSET = GPIOTE_INTENSET_IN1_Msk;
	irq_connect_dynamic(GPIOTE1_IRQn, 0, pin_isr, NULL, 0);
	irq_enable(GPIOTE1_IRQn);

	while (!rx_done) {
		__WFI();
	}

	rx_done = false;

	/* Stop RDY pin STOPRX link. */
	GPIOTE->PUBLISH_IN[1] = 0;
	/* Need to disable GPIOTE in two steps to prevent 23 µA current leak. */
	GPIOTE->CONFIG[1] = GPIOTE_CONFIG_MODE_Disabled | (PIN_RDY << GPIOTE_CONFIG_PSEL_Pos);
	GPIOTE->CONFIG[1] = 0;
	UART->SUBSCRIBE_STOPRX = 0;
	NRF_DPPIC->CHENCLR = (1 << 2);

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
