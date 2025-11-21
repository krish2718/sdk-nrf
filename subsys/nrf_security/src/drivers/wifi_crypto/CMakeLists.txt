#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

# Clear out wifi_crypto_driver_sources before source additions
set(wifi_crypto_driver_sources)

# Add Wi-Fu Crypto sources and include dirs
include(wifi_crypto/wifi_crypto.cmake)

# Add include-directories
target_include_directories(psa_crypto_library_config
  INTERFACE
    ${wifi_crypto_driver_include_dirs}
)

add_library(wifi_crypto_driver STATIC
  ${wifi_crypto_driver_sources}
)

# Link psa_core with this PSA crypto driver
target_link_libraries(psa_core
  PRIVATE
    wifi_crypto_driver
)

nrf_security_add_zephyr_options_library(wifi_crypto_driver)
