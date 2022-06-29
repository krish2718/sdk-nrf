#
# Copyright (c) 2022 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(hello_world)

set(HOSTAP_BASE ${WEST_TOPDIR}/modules/lib/hostap)
set(WPA_SUPPLICANT_BASE ${HOSTAP_BASE}/wpa_supplicant)
set(COMMON_SRC_BASE ${HOSTAP_BASE}/src)
# TODO: Remove this hack
target_include_directories(mbedcrypto_common
        INTERFACE
        ${CMAKE_SOURCE_DIR}
        ${ZEPHYR_BASE}/subsys/net/ip
        #${HOSTAP_BASE}/
        ${WPA_SUPPLICANT_BASE}/
	${COMMON_SRC_BASE}/
)

target_sources(app PRIVATE
	src/main.c
	src/supp_shell.c
)
