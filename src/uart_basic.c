/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <zephyr/kernel.h>

#define UART NRF_UARTE1_NS
#define TIMER NRF_TIMER0_NS
#define GPIO NRF_P0_NS

extern uint8_t tx_buffer[1024];
extern uint8_t rx_buffer[2048];

int lp_printf(const char *fmt, ...);
bool rx_done = false;
bool timeout = false;

void uart_isr(const void *arg)
{
	rx_done = true;
	UART->EVENTS_ENDRX = 0;
}

void uart_basic_init(void)
{
	/* Dir input, input connect, pull disabled, drive s0s1, sense disabled. */
	GPIO->PIN_CNF[7] = 0;

	/* Select pins. */
	UART->PSEL.TXD = 6;
	UART->PSEL.RXD = 7;

	/* Configure buffers. */
	UART->RXD.PTR = (int)rx_buffer;
	UART->RXD.MAXCNT = sizeof(rx_buffer);
	UART->TXD.PTR = (int)tx_buffer;
	UART->TXD.MAXCNT = sizeof(tx_buffer);

	/* Baudrate 1M. */
	UART->BAUDRATE = UARTE_BAUDRATE_BAUDRATE_Baud1M;

	/* HW flow control disabled, Parity N, Stopbits 1. */
	UART->CONFIG = 0;

	/* Link ENDTX event to STOPTX task using channel 0. */
	UART->PUBLISH_ENDTX = SPIM_PUBLISH_ENDTX_EN_Msk | 0;
	UART->SUBSCRIBE_STOPTX = UARTE_SUBSCRIBE_STOPTX_EN_Msk | 0;
	NRF_DPPIC->CHENSET = (1 << 0);

	/* Enable. */
	UART->ENABLE = UARTE_ENABLE_ENABLE_Enabled;
	TIMER->SHORTS = TIMER_SHORTS_COMPARE0_STOP_Msk;

	/* Set a timer to stop receive before the buffer is full if no more bytes are received. */
	TIMER->CC[0] = 1000;	/* 1 ms */

	/* Start and clear a timer when a byte is received using channel 1.  */
	UART->PUBLISH_RXDRDY = UARTE_PUBLISH_RXDRDY_EN_Msk | 1;
	TIMER->SUBSCRIBE_START = TIMER_SUBSCRIBE_START_EN_Msk | 1;
	TIMER->SUBSCRIBE_CLEAR = TIMER_SUBSCRIBE_CLEAR_EN_Msk | 1;
	NRF_DPPIC->CHENSET = (1 << 1);

	/* Stop UART receive on timeout using channel 2. */
	TIMER->PUBLISH_COMPARE[0] = TIMER_PUBLISH_COMPARE_EN_Msk | 2;
	UART->SUBSCRIBE_STOPRX = UARTE_SUBSCRIBE_STOPRX_EN_Msk | 2;
	NRF_DPPIC->CHENSET = (1 << 2);

	/* Enable interrupt to wake up at the end of a received message. */
	irq_connect_dynamic(SPIM1_SPIS1_TWIM1_TWIS1_UARTE1_IRQn, 0, uart_isr, NULL, 0);
	irq_enable(SPIM1_SPIS1_TWIM1_TWIS1_UARTE1_IRQn);

	/* Dir output, input disconnect, pull disabled, drive s0s1, sense disabled. */
	GPIO->PIN_CNF[6] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
			   (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos);
}

void uart_basic_send(size_t size)
{
	UART->EVENTS_ENDTX = 0;
	UART->TXD.MAXCNT = size;
	UART->TASKS_STARTTX = 1;
}

void timeout_isr(const void *arg)
{
	timeout = true;
	NRF_TIMER1_NS->EVENTS_COMPARE[0] = 0;
}

int uart_basic_recv(int size, int timeout_sec)
{
	UART->RXD.MAXCNT = size;
	UART->INTENSET = UARTE_INTENSET_ENDRX_Msk;
	UART->TASKS_STARTRX = 1;

	irq_connect_dynamic(TIMER1_IRQn, 0, timeout_isr, NULL, 0);
	irq_enable(TIMER1_IRQn);
	NRF_TIMER1_NS->BITMODE = TIMER_BITMODE_BITMODE_32Bit;
	NRF_TIMER1_NS->PRESCALER = 4;
	NRF_TIMER1_NS->CC[0] = timeout_sec * 1000000;
	NRF_TIMER1_NS->INTENSET = TIMER_INTENSET_COMPARE0_Msk;
	NRF_TIMER1_NS->TASKS_CLEAR = 1;
	NRF_TIMER1_NS->TASKS_START = 1;

	while (!rx_done) {
		if (timeout) {
			UART->TASKS_STOPRX = 1;
		}
		__WFI();
	}

	NRF_TIMER1_NS->TASKS_STOP = 1;

	rx_done = false;
	timeout = false;

	return UART->RXD.AMOUNT;
}

void uart_basic_deinit(void)
{
	//GPIO->PIN_CNF[6] = 0;
	GPIO->PIN_CNF[6] = GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos;
	/* Disable. */
	UART->ENABLE = 0;
}
