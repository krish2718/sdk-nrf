#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(wifi_ble_radio_test)

set(perip_rt_dir ${ZEPHYR_NRF_MODULE_DIR}/samples/peripheral/radio_test)

file(GLOB perip_rt_sources ${perip_rt_dir}/src/*.c)

# NORDIC SDK APP START
target_sources(app PRIVATE
  src/early_shell_start.c
)

# Include peripheral BLE radio test only on nRF5x boards
# nRF7x (Wi-Fi) boards don't support BLE radio test
if(NOT BOARD MATCHES "nrf712.*")
  target_sources(app PRIVATE
    ${perip_rt_dir}/src/radio_test.c
    ${perip_rt_dir}/src/radio_cmd.c
    ${perip_rt_dir}/src/radio_power_set.c
  )
endif()
# NORDIC SDK APP END
