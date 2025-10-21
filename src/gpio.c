/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <zephyr/kernel.h>

#define GPIO        NRF_P0_NS
#define GPIOTE      NRF_GPIOTE1_NS

#define PIN_INPUT   8		/* BUTTON1 */
#define PIN_OUTPUT  0		/* LED1 */

int lp_printf(const char *fmt, ...);

static bool test_done;

 void gpio_init(uint32_t bitrate)
{
	/* Enable output pin to low. */
	GPIO->OUTCLR = 1 << PIN_OUTPUT;
	GPIO->PIN_CNF[PIN_OUTPUT] = GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos;

	/* Enable input pin with pullup. */
	GPIO->PIN_CNF[PIN_INPUT] = GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos;

	lp_printf("    Input   P0.%02d\n", PIN_INPUT);
	lp_printf("    Output  P0.%02d\n", PIN_OUTPUT);
	lp_printf("Select 'Receive' and pull input low to measure response time on output\n");
}

int gpio_send(int size)
{
	/* Output pin high. */
	GPIO->OUTSET = 1 << PIN_OUTPUT;

	sleep(1);

	/* Output pin low. */
	GPIO->OUTCLR = 1 << PIN_OUTPUT;

	return 0;
}

void gpiote_isr(const void *arg)
{
	/* Output pin low. */
	GPIO->OUTCLR = 1 << PIN_OUTPUT;

	/* Clear interrupt. */
	GPIOTE->INTENCLR = GPIOTE_INTENCLR_IN0_Msk;

	test_done = true;
}

int gpio_recv(int size)
{
	test_done = false;

	/* Output pin high. */
	GPIO->OUTSET = 1 << PIN_OUTPUT;

	/* Enable interrupt. */
	GPIOTE->CONFIG[0] = GPIOTE_CONFIG_MODE_Event << GPIOTE_CONFIG_MODE_Pos |
			    PIN_INPUT << GPIOTE_CONFIG_PSEL_Pos |
			    GPIOTE_CONFIG_POLARITY_HiToLo << GPIOTE_CONFIG_POLARITY_Pos;
	GPIOTE->EVENTS_IN[0] = 0;
	GPIOTE->INTENSET = GPIOTE_INTENSET_IN0_Msk;
	irq_connect_dynamic(GPIOTE1_IRQn, 0, gpiote_isr, NULL, 0);
	irq_enable(GPIOTE1_IRQn);

	while (!test_done) {
		__WFI();
	}

	/* Disable interrupt. */
	irq_disable(GPIOTE1_IRQn);
	GPIOTE->INTENCLR = GPIOTE_INTENCLR_IN0_Msk;

	/* Output pin low. */
	GPIO->OUTCLR = 1 << PIN_OUTPUT;

	return 0;//87.8 µs 11.5 µs 76.3
}

void gpio_deinit(void)
{
	/* Disable output pin. */
	GPIO->OUTCLR = 1 << PIN_OUTPUT;
	GPIO->PIN_CNF[PIN_OUTPUT] = 0;

	/* Disable input pin. */
	GPIO->PIN_CNF[PIN_INPUT] = 0;
}
