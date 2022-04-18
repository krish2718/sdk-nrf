#
# Copyright (c) 2022 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

zephyr_library_compile_definitions(
	KERNEL_VERSION_5_4
	SOC_CALDER
	RPU_SUPPORT_WAPI
	RPU_VHT_SUPPORT
	RPU_HE_SUPPORT
	RPU_BSS_DB_SUPPORT
	RPU_32_BIT_DMA_SUPPORT
	RPU_CONFIG_FMAC
	REG_MODE_SUPPORT
	REG_DEBUG_MODE_SUPPORT
	OFFLINE_MODE
	#FW_PATCH_INBUILT
)

zephyr_library_include_directories(
	../../../os_if/os/zephyr/inc
	../../../utils/inc
	../../../os_if/osal/inc
	../../../bus_if/bus/qspi/inc
	../../../bus_if/bal/inc
	../../../fw_if/umac_if/inc
	../../../fw_if/umac_if/inc/fw
	../../../fw_load/mips/fw/inc
	../../../hw_if/hal/inc
	../../../hw_if/hal/inc/fw
	./src/qspi/src/
	../../../../supplicant/wpa_supplicant_zephyr/src/wpa_supplicant/src/drivers/
)

file(GLOB OSAL_IF_SRC "../../../os_if/osal/src/*.c")
file(GLOB UTILS_IF_SRC "../../../utils/src/*.c")
file(GLOB HW_IF_SRC "../../../hw_if/hal/src/*.c")
file(GLOB BUS_IF_BAL "../../../bus_if/bal/src/*.c")
file(GLOB BUS_IF_SRC "../../../bus_if/bus/qspi/src/*.c")
file(GLOB FW_IF_SRC "../../../fw_if/umac_if/src/*.c")
file(GLOB OS_IF_SRC "src/*.c")
file(GLOB QSPI_IF_SRC "src/qspi/src/*.c")

list(FILTER QSPI_IF_SRC EXCLUDE REGEX ".*main.c$")

zephyr_library_sources(
	${OSAL_IF_SRC}
	${UTILS_IF_SRC}
	${HW_IF_SRC}
	${BUS_IF_BAL}
	${BUS_IF_SRC}
	${FW_IF_SRC}
	${OS_IF_SRC}
	${QSPI_IF_SRC}
)
