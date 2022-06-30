#
# Copyright (c) 2022 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

zephyr_library()

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
	SHELIAK_SOC	# SOC
	ASICBUILD	# SOC
	C0_CHIP
	#RPU_SLEEP_SUPPORT
)

zephyr_library_include_directories(
	os_if/os/zephyr/inc
	utils/inc
	os_if/osal/inc
	bus_if/bus/qspi/inc
	bus_if/bal/inc
	fw_if/umac_if/inc
	fw_if/umac_if/inc/fw
	fw_load/mips/fw/inc
	hw_if/hal/inc
	hw_if/hal/inc/fw
	os_if/os/zephyr/src/qspi/src/
)

zephyr_library_sources(
	os_if/osal/src/osal.c
	utils/src/list.c
	utils/src/queue.c
	hw_if/hal/src/hal_api.c
	hw_if/hal/src/hal_fw_patch_loader.c
	hw_if/hal/src/hal_interrupt.c
	hw_if/hal/src/hal_mem.c
	hw_if/hal/src/hal_reg.c
	hw_if/hal/src/hpqm.c
	hw_if/hal/src/pal.c
	bus_if/bal/src/bal.c
	bus_if/bus/qspi/src/qspi.c
	fw_if/umac_if/src/cmd.c
	fw_if/umac_if/src/event.c
	fw_if/umac_if/src/fmac_ap.c
	fw_if/umac_if/src/fmac_peer.c
	fw_if/umac_if/src/fmac_util.c
	fw_if/umac_if/src/fmac_vif.c
	fw_if/umac_if/src/fmac_api.c
	fw_if/umac_if/src/rx.c
	fw_if/umac_if/src/tx.c
	os_if/os/zephyr/src/shim.c
	os_if/os/zephyr/src/zephyr_work.c
	os_if/os/zephyr/src/timer.c
	os_if/os/zephyr/src/util.c
	os_if/os/zephyr/src/zephyr_disp_scan.c
	os_if/os/zephyr/src/zephyr_fmac_main.c
	os_if/os/zephyr/src/zephyr_net_if.c
	os_if/os/zephyr/src/zephyr_wpa_supp_if.c
	os_if/os/zephyr/src/qspi/src/device.c
	os_if/os/zephyr/src/qspi/src/qspi_if.c
	os_if/os/zephyr/src/qspi/src/qspi_main.c
	os_if/os/zephyr/src/qspi/src/spi_if.c
)
