#
# Copyright (c) 2022 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(hello_world)

# TODO: Remove this hack
target_include_directories(mbedcrypto_common
        INTERFACE
        ${CMAKE_SOURCE_DIR}
        ${ZEPHYR_BASE}/subsys/net/ip
)

target_sources(app PRIVATE
	src/main.c
)
