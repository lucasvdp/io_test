Peripheral tests for nRF9151
############################

Run peripheral tests on an nRF9151 to test functionality and power consumption.

Connect a serial terminal to UART0 (115200 8N1) for monitoring and controlling the tests. This UART
will be turned off during the test to not influence the power consumption.

Building
========

There are two ways of compiling this application:

* compile without device tree overlays, you can select the peripheral you want to test from the
  terminal. In this mode registers will be written directly from the application.
* compile with a specific device tree overlay. In this mode NCS drivers will be used to control the
  peripheral. Example:

``west build -p -b nrf9151dk/nrf9151/ns -- -DEXTRA_DTC_OVERLAY_FILE=boards/spi_slave.overlay``

Interrupt latency
=================

When compiling without device tree overlay you can select the ``GPIO interrupt response timing``
option to measure the latency from getting an interrupt (pushing BUTTON1) to performing an action
(turning off LED1).

Configuration
=============

You can change the device mode for all the tests from constant latency to low power using ``[`` and
``]`` in the main menu.
