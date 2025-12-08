#
# Copyright (c) 2025 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nrf91_com_test)

# NORDIC SDK APP START
target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/spi_master_bare.c)
target_sources(app PRIVATE src/spi_slave_bare.c)
target_sources(app PRIVATE src/uart_bare.c)
target_sources(app PRIVATE src/twi_master_bare.c)
target_sources(app PRIVATE src/twi_slave_bare.c)
target_sources(app PRIVATE src/gpio.c)
# NORDIC SDK APP END

zephyr_include_directories(src)

