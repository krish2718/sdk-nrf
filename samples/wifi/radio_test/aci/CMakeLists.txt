#
# Copyright (c) 2022 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
if (CONFIG_INCLUDE_NET_CORE_IMAGE)
add_child_image(
   NAME ble_radio_test
   SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../../samples/peripheral/radio_test
   DOMAIN CPUNET
   BOARD ${CONFIG_DOMAIN_CPUNET_BOARD}
)
endif()
