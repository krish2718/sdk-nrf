# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nrf_wifi_tx_rx)

set(NRF_WIFI_DIR ${ZEPHYR_NRF_WIFI_MODULE_DIR})

FILE(GLOB app_sources src/*.c)
target_include_directories(app PRIVATE
    ${NRF_WIFI_DIR}/fw_if/umac_if/inc
    ${NRF_WIFI_DIR}/hw_if/hal/inc
    ${NRF_WIFI_DIR}/fw_if/umac_if/inc/fw
)
target_compile_definitions(app PRIVATE
    NRF70_RAW_DATA_TX
)
target_sources(app PRIVATE ${app_sources})
