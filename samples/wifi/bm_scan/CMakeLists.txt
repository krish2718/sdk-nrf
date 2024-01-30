#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nrf_wifi_bm_scan)

set(OS_AGNOSTIC_BASE ${ZEPHYR_NRFXLIB_MODULE_DIR}/nrf_wifi)

target_include_directories(app PRIVATE
  inc
  ${OS_AGNOSTIC_BASE}/utils/inc
  ${OS_AGNOSTIC_BASE}/os_if/inc
  ${OS_AGNOSTIC_BASE}/bus_if/bus/qspi/inc
  ${OS_AGNOSTIC_BASE}/bus_if/bal/inc
  ${OS_AGNOSTIC_BASE}/fw_if/umac_if/inc
  ${OS_AGNOSTIC_BASE}/fw_load/mips/fw/inc
  ${OS_AGNOSTIC_BASE}/hw_if/hal/inc
  ${OS_AGNOSTIC_BASE}/hw_if/hal/inc/fw
  ${OS_AGNOSTIC_BASE}/fw_if/umac_if/inc/fw
  ${OS_AGNOSTIC_BASE}/fw_if/umac_if/inc/default
)

target_sources(app PRIVATE
  ${OS_AGNOSTIC_BASE}/os_if/src/osal.c
  ${OS_AGNOSTIC_BASE}/utils/src/list.c
  ${OS_AGNOSTIC_BASE}/utils/src/queue.c
  ${OS_AGNOSTIC_BASE}/utils/src/util.c
  ${OS_AGNOSTIC_BASE}/hw_if/hal/src/hal_api.c
  ${OS_AGNOSTIC_BASE}/hw_if/hal/src/hal_fw_patch_loader.c
  ${OS_AGNOSTIC_BASE}/hw_if/hal/src/hal_interrupt.c
  ${OS_AGNOSTIC_BASE}/hw_if/hal/src/hal_mem.c
  ${OS_AGNOSTIC_BASE}/hw_if/hal/src/hal_reg.c
  ${OS_AGNOSTIC_BASE}/hw_if/hal/src/hpqm.c
  ${OS_AGNOSTIC_BASE}/hw_if/hal/src/pal.c
  ${OS_AGNOSTIC_BASE}/bus_if/bal/src/bal.c
  ${OS_AGNOSTIC_BASE}/bus_if/bus/qspi/src/qspi.c
  ${OS_AGNOSTIC_BASE}/fw_if/umac_if/src/cmd.c
  ${OS_AGNOSTIC_BASE}/fw_if/umac_if/src/event.c
  ${OS_AGNOSTIC_BASE}/fw_if/umac_if/src/fmac_api_common.c
  ${OS_AGNOSTIC_BASE}/fw_if/umac_if/src/default/fmac_api.c
    src/scan.c
)
