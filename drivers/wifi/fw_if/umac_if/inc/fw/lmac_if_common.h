/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @brief Common structures and definations.
 */

#ifndef __LMAC_IF_COMMON__
#define __LMAC_IF_COMMON__

#include "rpu_if.h"
#include "phy_rf_params.h"
#include "pack_def.h"

#define RPU_MEM_LMAC_BOOT_SIG 0xB7000D50
#define RPU_MEM_LMAC_VER 0xB7000D54

#define RPU_MEM_LMAC_PATCH_BIN 0x80046000
#define RPU_MEM_LMAC_PATCH_BIMG 0x80047000

#define IMG_WLAN_LMAC_VER(ver) ((ver & 0xFF000000) >> 24)
#define IMG_WLAN_LMAC_VER_MAJ(ver) ((ver & 0x00FF0000) >> 16)
#define IMG_WLAN_LMAC_VER_MIN(ver) ((ver & 0x0000FF00) >> 8)
#define IMG_WLAN_LMAC_VER_EXTRA(ver) (ver & 0x000000FF)

#define IMG_WLAN_LMAC_BOOT_SIG 0x5A5A5A5A
#define IMG_WLAN_LMAC_ROM_PATCH_OFFSET (RPU_MEM_LMAC_PATCH_BIMG -\
					RPU_ADDR_MCU1_CORE_RET_START)
#define IMG_WLAN_LMAC_BOOT_EXCP_VECT_0 0x3c1a8000
#define IMG_WLAN_LMAC_BOOT_EXCP_VECT_1 0x275a0000
#define IMG_WLAN_LMAC_BOOT_EXCP_VECT_2 0x03400008
#define IMG_WLAN_LMAC_BOOT_EXCP_VECT_3 0x00000000

#define IMG_WLAN_LMAC_MAX_RX_BUFS 256



#define HW_SLEEP_ENABLE 2
#define SW_SLEEP_ENABLE 1
#define SLEEP_DISABLE 0
#define HW_DELAY 20000
#define SW_DELAY 5000
#define BCN_TIMEOUT 20000
#define CALIB_SLEEP_CLOCK_ENABLE 1


#define ACTIVE_SCAN_DURATION 50
#define PASSIVE_SCAN_DURATION 130
#define WORKING_CH_SCAN_DURATION 50
#define CHNL_PROBE_CNT 2

#define		PKT_TYPE_MPDU				0
#define		PKT_TYPE_MSDU_WITH_MAC		1
#define		PKT_TYPE_MSDU				2


#define			IMG_RPU_PWR_STATUS_SUCCESS	0
#define			IMG_RPU_PWR_STATUS_FAIL		-1

/**
 * struct lmac_prod_stats : used to get the production mode stats
 **/

/* Events */
#define MAX_RSSI_SAMPLES 10
struct lmac_prod_stats {
	/*Structure that holds all the debug information in LMAC*/
	unsigned int  resetCmdCnt;
	unsigned int  resetCompleteEventCnt;
	unsigned int  unableGenEvent;
	unsigned int  chProgCmdCnt;
	unsigned int  channelProgDone;
	unsigned int  txPktCnt;
	unsigned int  txPktDoneCnt;
	unsigned int  scanPktCnt;
	unsigned int  internalPktCnt;
	unsigned int  internalPktDoneCnt;
	unsigned int  ackRespCnt;
	unsigned int  txTimeout;
	unsigned int  deaggIsr;
	unsigned int  deaggInptrDescEmpty;
	unsigned int  deaggCircularBufferFull;
	unsigned int  lmacRxisrCnt;
	unsigned int  rxDecryptcnt;
	unsigned int  processDecryptFail;
	unsigned int  prepaRxEventFail;
	unsigned int  rxCorePoolFullCnt;
	unsigned int  rxMpduCrcSuccessCnt;
	unsigned int  rxMpduCrcFailCnt;
	unsigned int  rxOfdmCrcSuccessCnt;
	unsigned int  rxOfdmCrcFailCnt;
	unsigned int  rxDSSSCrcSuccessCnt;
	unsigned int  rxDSSSCrcFailCnt;
	unsigned int  rxCryptoStartCnt;
	unsigned int  rxCryptoDoneCnt;
	unsigned int  rxEventBufFull;
	unsigned int  rxExtramBufFull;
	unsigned int  scanReq;
	unsigned int  scanComplete;
	unsigned int  scanAbortReq;
	unsigned int  scanAbortComplete;
	unsigned int  internalBufPoolNull;
};

/**
 * struct phy_prod_stats : used to get the production mode stats
 **/
struct phy_prod_stats {
	unsigned int ofdm_crc32_pass_cnt;
	unsigned int ofdm_crc32_fail_cnt;
	unsigned int dsss_crc32_pass_cnt;
	unsigned int dsss_crc32_fail_cnt;
	char averageRSSI;
};

union rpu_stats {
	struct lmac_prod_stats lmac_stats;
	struct phy_prod_stats  phy_stats;
};


struct hpqmQueue {
	unsigned int  pop_addr;
	unsigned int  push_addr;
	unsigned int  idNum;
	unsigned int  status_addr;
	unsigned int  status_mask;
} __IMG_PKD;


struct INT_HPQ {
	unsigned int  id;
       /* The head and tail values are relative
	* to the start of the
	* HWQM register block.
	*/
	unsigned int head;
	unsigned int tail;
} __IMG_PKD;



/**
 * struct lmac_fw_config_params:lmac firmware config params
 * @boot_status:		lmac firmware boot status. LMAC will set to
 *				0x5a5a5a5a after completing boot process.
 * @rpu_config_name:		rpu config name. this is a string and
 *				expected sting is explorer or whisper
 * @rpu_config_number:		rpu config number
 * @HP_lmac_to_host_isr_en:	lmac register addres to enable ISR to Host
 * @HP_lmac_to_host_isr_clear:	Address to Clear host ISR
 * @HP_set_lmac_isr:		Address to set ISR to lmac Clear host ISR
 * @FreeCmdPtrQ:		queue which contains Free GRAM pointers for
 *				commands.
 * @cmdPtrQ:			Command pointer queue. Host should pick gram
 *				pointer from FreeCmdPtrQ. Populate command in
 *				GRAM and submit back to this queue for RPU.
 * @eventPtrQ:			Event pointer queue. Host should pick gram
 *				event pointer in isr
 * @version:			lmac firmware version
 *
 *
 */
struct lmac_fw_config_params {
	unsigned int boot_status;
	unsigned int version;
	unsigned int lmacRxBufferAddr;
	unsigned int lmacRxMaxDescCnt;
	unsigned int lmacRxDescSize;
	unsigned char rpu_config_name[16];
	unsigned char rpu_config_number[8];
	unsigned int numRX;
	unsigned int numTX;
	#define FREQ_2_4_GHZ 1
	#define FREQ_5_GHZ  2
	unsigned int bands;
	unsigned int sysFrequencyInMhz;
	struct hpqmQueue FreeCmdPtrQ;
	struct hpqmQueue cmdPtrQ;
	struct hpqmQueue eventPtrQ;
	struct hpqmQueue freeEventPtrQ;
	struct hpqmQueue SKBGramPtrQ_1;
	struct hpqmQueue SKBGramPtrQ_2;
	struct hpqmQueue SKBGramPtrQ_3;
	unsigned int HP_lmac_to_host_isr_en;
	unsigned int HP_lmac_to_host_isr_clear;
	unsigned int HP_set_lmac_isr;

	#define NUM_32_QUEUES 4
	struct INT_HPQ hpq32[NUM_32_QUEUES];

} __IMG_PKD;

#define MAX_NUM_OF_RX_QUEUES 3



struct rx_buf_pool_params {
	unsigned short buf_sz;
	unsigned short num_bufs;
} __IMG_PKD;
struct temp_vbat_config {
	unsigned int temp_based_calib_en;
	unsigned int temp_calib_bitmap;
	unsigned int vbat_calibp_bitmap;
	unsigned int temp_vbat_mon_period;
	int VthVeryLow;
	int VthLow;
	int VthHi;
	int temp_threshold;
	int vbat_threshold;
} __IMG_PKD;
#endif
