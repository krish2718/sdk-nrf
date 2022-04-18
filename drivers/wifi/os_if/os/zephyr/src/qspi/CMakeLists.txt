#
# Copyright (c) 2022 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(hello_world)

target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/qspi_if.c)
target_sources(app PRIVATE src/spi_if.c)
target_sources(app PRIVATE src/interrupts.c)
target_sources(app PRIVATE src/device.c)
