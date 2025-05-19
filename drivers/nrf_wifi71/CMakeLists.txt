#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0
#

zephyr_library_named(nrf_wifi)

zephyr_include_directories(
  inc
  fw
  # for net_sprint_ll_addr
  ${ZEPHYR_BASE}/subsys/net/ip
)

zephyr_library_sources(
  src/main.c
)

if(NOT CONFIG_NRF71_RADIO_TEST)
  zephyr_library_sources(
    src/net_if.c
    src/wifi_mgmt_scan.c
  )
endif()

if(NOT CONFIG_NRF71_SCAN_ONLY)
  zephyr_library_sources(
    src/wpa_supp_if.c
    src/wifi_mgmt.c
  )
endif()
