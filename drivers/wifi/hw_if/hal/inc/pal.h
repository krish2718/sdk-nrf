/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @brief Header containing SoC specific declarations for the
 * HAL Layer of the Wi-Fi driver.
 */

#ifndef __PAL_H__
#define __PAL_H__

#include "hal_api.h"

#define SOC_BOOT_SUCCESS  0
#define SOC_BOOT_FAIL     1
#define SOC_BOOT_ERRORS   2

#define DEFAULT_IMGPCI_VENDOR_ID 0x0700
#define DEFAULT_IMGPCI_DEVICE_ID PCI_ANY_ID
#define PCIE_BAR_OFFSET_WLAN_RPU 0x0
#define PCIE_DMA_MASK 0xFFFFFFFF

#define SOC_MMAP_ADDR_OFFSET_PKTRAM_HOST_VIEW 0x0C0000
#define SOC_MMAP_ADDR_OFFSET_PKTRAM_RPU_VIEW 0x380000

#ifdef RPU_CONFIG_72
#define SOC_MMAP_ADDR_OFFSET_GRAM_PKD 0xC00000
#define SOC_MMAP_ADDR_OFFSET_SYSBUS 0xE00000
#define SOC_MMAP_ADDR_OFFSET_PBUS 0xE40000
#else
#define SOC_MMAP_ADDR_OFFSET_GRAM_PKD 0x80000
#define SOC_MMAP_ADDR_OFFSET_SYSBUS 0x00000
#define SOC_MMAP_ADDR_OFFSET_PBUS 0x40000
#endif /* RPU_CONFIG_72 */

#define NVLSI_WLAN_FW_LMAC_PATCH_LOC_PRI "img/wlan/wifi_nrf_wlan_lmac_patch_pri.bimg"
#define NVLSI_WLAN_FW_LMAC_PATCH_LOC_SEC "img/wlan/wifi_nrf_wlan_lmac_patch_sec.bin"
#define NVLSI_WLAN_FW_UMAC_PATCH_LOC_PRI "img/wlan/wifi_nrf_wlan_umac_patch_pri.bimg"
#define NVLSI_WLAN_FW_UMAC_PATCH_LOC_SEC "img/wlan/wifi_nrf_wlan_umac_patch_sec.bin"


enum wifi_nrf_wlan_fw_type {
	NVLSI_WLAN_FW_TYPE_LMAC_PATCH,
	NVLSI_WLAN_FW_TYPE_UMAC_PATCH,
	NVLSI_WLAN_FW_TYPE_MAX
};


enum wifi_nrf_wlan_fw_subtype {
	NVLSI_WLAN_FW_SUBTYPE_PRI,
	NVLSI_WLAN_FW_SUBTYPE_SEC,
	NVLSI_WLAN_FW_SUBTYPE_MAX
};


enum wifi_nrf_status pal_rpu_addr_offset_get(struct wifi_nrf_osal_priv *opriv,
					      unsigned int rpu_addr,
					      unsigned long *addr_offset);

char *pal_ops_get_fw_loc(struct wifi_nrf_osal_priv *opriv,
			 enum wifi_nrf_wlan_fw_type fw_type,
			 enum wifi_nrf_wlan_fw_subtype fw_subtype);
#endif /* __PAL_H__ */
