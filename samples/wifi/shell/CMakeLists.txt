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
	${ZEPHYR_BASE}/samples/net/zperf/src/shell_utils.c
	${ZEPHYR_BASE}/samples/net/zperf/src/zperf_session.c
	${ZEPHYR_BASE}/samples/net/zperf/src/zperf_shell.c
	${ZEPHYR_BASE}/samples/net/zperf/src/zperf_udp_receiver.c
	${ZEPHYR_BASE}/samples/net/zperf/src/zperf_udp_uploader.c
	${ZEPHYR_BASE}/samples/net/zperf/src/zperf_tcp_receiver.c
	${ZEPHYR_BASE}/samples/net/zperf/src/zperf_tcp_uploader.c
)
