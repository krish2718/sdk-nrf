#
# Copyright (c) 2022 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

# BLE transport layer for WiFi provisioning
zephyr_library_named(wifi_prov_ble)

# Link against the core WiFi provisioning library
target_link_libraries(wifi_prov_ble PRIVATE wifi_prov_core)

# Add include path to generated .pb.h header files
zephyr_library_include_directories(${CMAKE_CURRENT_BINARY_DIR})

zephyr_library_sources(
    wifi_prov_ble.c)
