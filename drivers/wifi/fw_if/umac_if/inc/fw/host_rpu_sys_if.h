/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */


#ifndef __HOST_RPU_SYS_IF_H__
#define __HOST_RPU_SYS_IF_H__

#include "host_rpu_common_if.h"
#include "lmac_if_common.h"

#ifndef PACK
#define PACK(__Declaration__) __Declaration__ __attribute__((__packed__))
#endif

#define USE_PROTECTION_NONE 0
#define USE_PROTECTION_RTS 1
#define USE_PROTECTION_CTS2SELF 2

#define USE_SHORT_PREAMBLE 0
#define DONT_USE_SHORT_PREAMBLE 1

#define MARK_RATE_AS_MCS_INDEX 0x80
#define MARK_RATE_AS_RATE 0x00

#define ENABLE_GREEN_FIELD 0x01
#define ENABLE_CHNL_WIDTH_40MHZ 0x02
#define ENABLE_SGI 0x04
#define ENABLE_11N_FORMAT 0x08
#define ENABLE_VHT_FORMAT 0x10
#define ENABLE_CHNL_WIDTH_80MHZ 0x20

#define MAX_TX_TOKENS 12
#define MAX_TX_AGG_SIZE 16
#define MAX_RX_BUFS_PER_EVNT 64
#define MAX_MGMT_BUFS 16

/* #define ETH_ADDR_LEN		6 */
#define MAX_RF_CALIB_DATA	900

#define IMG_ETH_ADDR_LEN	6

#define PHY_THRESHOLD_NORMAL    (-65)
#define PHY_THRESHOLD_PROD_MODE (-93)

#define MAX_TX_STREAMS 1
#define MAX_RX_STREAMS 1

#define MAX_NUM_VIFS 2
#define MAX_NUM_STAS 2
#define MAX_NUM_APS 1

/* #define IMG_WLAN_PHY_CALIB_FLAG_RXDC 1 */
/* #define IMG_WLAN_PHY_CALIB_FLAG_TXDC 2 */
/* #define IMG_WLAN_PHY_CALIB_FLAG_TXPOW 4 */
/* #define IMG_WLAN_PHY_CALIB_FLAG_TXIQ 8 */
/* #define IMG_WLAN_PHY_CALIB_FLAG_RXIQ 16 */
/* #define IMG_WLAN_PHY_CALIB_FLAG_DPD 32 */

/**
 * enum img_sys_iftype - Interface types based on functionality.
 *
 * @IMG_UMAC_IFTYPE_UNSPECIFIED: Unspecified type, driver decides.
 * @IMG_UMAC_IFTYPE_ADHOC: Independent BSS member.
 * @IMG_UMAC_IFTYPE_STATION: Managed BSS member.
 * @IMG_UMAC_IFTYPE_AP: Access point.
 * @IMG_UMAC_IFTYPE_AP_VLAN: VLAN interface for access points; VLAN interfaces
 *      are a bit special in that they must always be tied to a pre-existing
 *      AP type interface.
 * @IMG_UMAC_IFTYPE_WDS: Wireless Distribution System.
 * @IMG_UMAC_IFTYPE_MONITOR: Monitor interface receiving all frames.
 * @IMG_UMAC_IFTYPE_MESH_POINT: Mesh point.
 * @IMG_UMAC_IFTYPE_P2P_CLIENT: P2P client.
 * @IMG_UMAC_IFTYPE_P2P_GO: P2P group owner.
 * @IMG_UMAC_IFTYPE_P2P_DEVICE: P2P device interface type, this is not a netdev
 *      and therefore can't be created in the normal ways, use the
 *      %IMG_UMAC_CMD_START_P2P_DEVICE and %IMG_UMAC_CMD_STOP_P2P_DEVICE
 *      commands (Refer &enum img_umac_commands) to create and destroy one.
 * @IMG_UMAC_IFTYPE_OCB: Outside Context of a BSS.
 *      This mode corresponds to the MIB variable dot11OCBActivated=true.
 * @IMG_UMAC_IFTYPE_MAX: Highest interface type number currently defined.
 * @IMG_UMAC_IFTYPES: Number of defined interface types.
 *
 * Lists the different interface types based on how they are configured
 * functionally.
 */
enum img_sys_iftype {
	IMG_UMAC_IFTYPE_UNSPECIFIED,
	IMG_UMAC_IFTYPE_ADHOC,
	IMG_UMAC_IFTYPE_STATION,
	IMG_UMAC_IFTYPE_AP,
	IMG_UMAC_IFTYPE_AP_VLAN,
	IMG_UMAC_IFTYPE_WDS,
	IMG_UMAC_IFTYPE_MONITOR,
	IMG_UMAC_IFTYPE_MESH_POINT,
	IMG_UMAC_IFTYPE_P2P_CLIENT,
	IMG_UMAC_IFTYPE_P2P_GO,
	IMG_UMAC_IFTYPE_P2P_DEVICE,
	IMG_UMAC_IFTYPE_OCB,

	/* keep last */
	IMG_UMAC_IFTYPES,
	IMG_UMAC_IFTYPE_MAX = IMG_UMAC_IFTYPES - 1
};

/**
 * enum rpu_op_mode - operating modes.
 *
 * @RPU_OP_MODE_NORMAL: Normal mode is the regular mode of operation
 * @RPU_OP_MODE_DBG: Debug mode can be used to control certain parameters
 *	like TX rate etc in order to debug functional issues
 * @RPU_OP_MODE_PROD: Production mode is used for performing production
 *	tests using continuous Tx/Rx on a configured channel at a particular
 *	rate, power etc
 * @RPU_OP_MODE_FCM: In the FCM mode different type of calibration like RF
 *	calibration can be performed
 *
 * Lists the different types of operating modes.
 */
enum rpu_op_mode {
	RPU_OP_MODE_PROD,
	RPU_OP_MODE_FCM,
	RPU_OP_MODE_REG,
	RPU_OP_MODE_DBG,
	RPU_OP_MODE_MAX
};

/**
 * enum rpu_stats_type - statistics type.
 *
 * To obtain statistics relevant to the operation mode set via op_mode
 * parameter.
 * Statistics will be updated in: /sys/kernel/debug/img/wlan/stats
 */

enum rpu_stats_type {
	RPU_STATS_TYPE_ALL,
	RPU_STATS_TYPE_HOST,
	RPU_STATS_TYPE_UMAC,
	RPU_STATS_TYPE_LMAC,
	RPU_STATS_TYPE_PHY,
	RPU_STATS_TYPE_MAX
};


/**
 * enum rpu_tput_mode - Throughput mode
 *
 * Throughput mode to be used for transmitting the packet.
 */
enum rpu_tput_mode {
	RPU_TPUT_MODE_LEGACY,
	RPU_TPUT_MODE_HT,
	RPU_TPUT_MODE_MAX
};

/**
 * enum img_sys_commands - system commands
 * @IMG_CMD_INIT: After host driver bringup host sends the IMG_CMD_INIT
 *	to the RPU. then RPU initializes and responds with
 *	IMG_EVENT_BUFF_CONFIG.
 * @IMG_CMD_BUFF_CONFIG_COMPLETE: Host sends this command to RPU after
 *	completion of all buffers configuration
 * @IMG_CMD_TX: command to send a Tx packet
 * @IMG_CMD_MODE: command to specify mode of operation
 * @IMG_CMD_GET_STATS: command to get statistics
 * @IMG_CMD_CLEAR_STATS: command to clear statistics
 * @IMG_CMD_RX: command to ENABLE/DISABLE receiving packets in production mode
 * @IMG_CMD_DEINIT: RPU De-initialization
 *
 */
enum img_sys_commands {
	IMG_CMD_INIT,
	IMG_CMD_TX,
	IMG_CMD_IF_TYPE,
	IMG_CMD_MODE,
	IMG_CMD_GET_STATS,
	IMG_CMD_CLEAR_STATS,
	IMG_CMD_RX,
	IMG_CMD_PWR,
	IMG_CMD_DEINIT,
	IMG_CMD_BTCOEX,
	IMG_CMD_RF_TEST,
};

/**
 * enum img_sys_events -
 * @IMG_EVENT_BUFF_CONFIG: Response to IMG_CMD_INIT
 *	see &struct img_event_buffs_config
 * @IMG_EVENT_BUFF_CONFIG_DONE: Response to IMG_CMD_BUFF_CONFIG_COMPLETE
 * @IMG_EVENT_STATS: Response to IMG_CMD_GET_STATS
 * @IMG_EVENT_DEINIT_DONE: Response to IMG_CMD_DEINIT
 *
 * Events from the RPU for different commands.
 */
enum img_sys_events {
	IMG_EVENT_PWR_DATA,
	IMG_EVENT_INIT_DONE,
	IMG_EVENT_STATS,
	IMG_EVENT_DEINIT_DONE,
	IMG_EVENT_RF_TEST,
};

enum rpu_ch_bw {
	RPU_CH_BW_20,
	RPU_CH_BW_40,
	RPU_CH_BW_MAX
};

PACK(
struct chan_params {
	unsigned int primary_num;
	unsigned char bw;
	int sec_20_offset;
	int sec_40_offset;
});

PACK(
struct rpu_conf_rx_prod_mode_params {
	unsigned char nss;
	/* Input to the RF for operation */
	unsigned char rf_params[RF_PARAMS_SIZE];
	struct chan_params chan;
	char phy_threshold;
	unsigned int phy_calib;
	unsigned char rx;
});


PACK(
struct umac_tx_dbg_params {
	unsigned int tx_cmd;
	unsigned int tx_non_coalescing_pkts_rcvd_from_host;
	unsigned int tx_coalescing_pkts_rcvd_from_host;
	unsigned int tx_max_coalescing_pkts_rcvd_from_host;
	unsigned int tx_cmds_max_used;
	unsigned int tx_cmds_currently_in_use;
	unsigned int tx_done_events_send_to_host;
	unsigned int tx_done_success_pkts_to_host;
	unsigned int tx_done_failure_pkts_to_host;
	unsigned int tx_cmds_with_crypto_pkts_rcvd_from_host;
	unsigned int tx_cmds_with_non_cryptot_pkts_rcvd_from_host;
	unsigned int tx_cmds_with_broadcast_pkts_rcvd_from_host;
	unsigned int tx_cmds_with_multicast_pkts_rcvd_from_host;
	unsigned int tx_cmds_with_unicast_pkts_rcvd_from_host;

	unsigned int xmit;
	unsigned int send_addba_req;
	unsigned int addba_resp;
	unsigned int softmac_tx;
	unsigned int internal_pkts;
	unsigned int external_pkts;
	unsigned int tx_cmds_to_lmac;
	unsigned int tx_dones_from_lmac;
	unsigned int total_cmds_to_lmac;

	unsigned int tx_packet_data_count;
	unsigned int tx_packet_mgmt_count;
	unsigned int tx_packet_beacon_count;
	unsigned int tx_packet_probe_req_count;
	unsigned int tx_packet_auth_count;
	unsigned int tx_packet_deauth_count;
	unsigned int tx_packet_assoc_req_count;
	unsigned int tx_packet_disassoc_count;
	unsigned int tx_packet_action_count;
	unsigned int tx_packet_other_mgmt_count;
	unsigned int tx_packet_non_mgmt_data_count;

});

PACK(
struct umac_rx_dbg_params {
	unsigned int lmac_events;
	unsigned int rx_events;
	unsigned int rx_coalised_events;
	unsigned int total_rx_pkts_from_lmac;

	unsigned int max_refill_gap;
	unsigned int current_refill_gap;

	unsigned int out_oforedr_mpdus;
	unsigned int reoredr_free_mpdus;

	unsigned int umac_consumed_pkts;
	unsigned int host_consumed_pkts;

	unsigned int rx_mbox_post;
	unsigned int rx_mbox_receive;

	unsigned int reordering_ampdu;
	unsigned int timer_mbox_post;
	unsigned int timer_mbox_rcv;
	unsigned int work_mbox_post;
	unsigned int work_mbox_rcv;
	unsigned int tasklet_mbox_post;
	unsigned int tasklet_mbox_rcv;
	unsigned int userspace_offload_frames;
	unsigned int alloc_buf_fail;

	unsigned int rx_packet_total_count;
	unsigned int rx_packet_data_count;
	unsigned int rx_packet_qos_data_count;
	unsigned int rx_packet_protected_data_count;
	unsigned int rx_packet_mgmt_count;
	unsigned int rx_packet_beacon_count;
	unsigned int rx_packet_probe_resp_count;
	unsigned int rx_packet_auth_count;
	unsigned int rx_packet_deauth_count;
	unsigned int rx_packet_assoc_resp_count;
	unsigned int rx_packet_disassoc_count;
	unsigned int rx_packet_action_count;
	unsigned int rx_packet_probe_req_count;
	unsigned int rx_packet_other_mgmt_count;
	char max_coalised_pkts;
	unsigned int null_skb_pointer_from_lmac;
	unsigned int unexpected_mgmt_pkt;

});

PACK(
struct umac_cmd_evnt_dbg_params {
	unsigned char cmd_init;
	unsigned char event_init_done;
	unsigned char cmd_get_stats;
	unsigned char event_ps_state;
	unsigned char cmd_set_reg;
	unsigned char cmd_get_reg;
	unsigned char cmd_req_set_reg;
	unsigned char cmd_trigger_scan;
	unsigned char event_scan_done;
	unsigned char cmd_get_scan;
	unsigned char cmd_auth;
	unsigned char cmd_assoc;
	unsigned char cmd_connect;
	unsigned char cmd_deauth;
	unsigned char cmd_register_frame;
	unsigned char cmd_frame;
	unsigned char cmd_del_key;
	unsigned char cmd_new_key;
	unsigned char cmd_set_key;
	unsigned char cmd_get_key;
	unsigned char event_beacon_hint;
	unsigned char event_reg_change;
	unsigned char event_wiphy_reg_change;
	unsigned char cmd_set_station;
	unsigned char cmd_new_station;
	unsigned char cmd_del_station;
	unsigned char cmd_new_interface;
	unsigned char cmd_set_interface;
	unsigned char cmd_get_interface;
	unsigned char cmd_set_ifflags;
	unsigned char cmd_set_ifflags_done;
	unsigned char cmd_set_bss;
	unsigned char cmd_set_wiphy;
	unsigned char cmd_start_ap;
	unsigned char cmd_rf_test;
	unsigned int LMAC_CMD_PS;
	unsigned int CURR_STATE;
});


#ifdef REG_DEBUG_MODE_SUPPORT
PACK(
struct rpu_umac_stats {
	struct umac_tx_dbg_params  tx_dbg_params;
	struct umac_rx_dbg_params  rx_dbg_params;
	struct umac_cmd_evnt_dbg_params   cmd_evnt_dbg_params;
});


PACK(
struct rpu_lmac_stats {
	/*LMAC DEBUG Stats*/
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
	/*END:LMAC DEBUG Stats*/
});

#endif /* REG_DEBUG_MODE_SUPPORT */

PACK(
struct rpu_phy_stats {
	char rssi_avg;
	unsigned char pdout_val;
	unsigned int ofdm_crc32_pass_cnt;
	unsigned int ofdm_crc32_fail_cnt;
	unsigned int dsss_crc32_pass_cnt;
	unsigned int dsss_crc32_fail_cnt;
});


/**
 * struct img_sys_head - Command/Event header.
 * @cmd: Command/Event id.
 * @len: Payload length.
 *
 * This header needs to be initialized in every command and has the event
 * id info in case of events.
 */
PACK(
struct img_sys_head {

	unsigned int                cmd_event;
	unsigned int                len;

});



/**
 * struct bgscan_params - Background Scan parameters.
 * @enabled: Enable/Disable background scan.
 * @channel_list: List of channels to scan.
 * @channel_flags: Channel flags for each of the channels which are to be
 *                 scanned.
 * @scan_intval: Back ground scan is done at regular intervals. This
 *               value is set to the interval value (in ms).
 * @channel_dur: Time to be spent on each channel (in ms).
 * @serv_channel_dur: In "Connected State" scanning, we need to share the time
 *                    between operating channel and non-operating channels.
 *                    After scanning each channel, the firmware spends
 *                    "serv_channel_dur" (in ms) on the operating channel.
 * @num_channels: Number of channels to be scanned.
 *
 * This structure specifies the parameters which will be used during a
 * Background Scan.
 */
PACK(
struct img_bgscan_params {
	unsigned int enabled;
	unsigned char channel_list[50];
	unsigned char channel_flags[50];
	unsigned int scan_intval;
	unsigned int channel_dur;
	unsigned int serv_channel_dur;
	unsigned int num_channels;
});



/**
 * @IMG_FEATURE_DISABLE: Feature Disable.
 * @IMG_FEATURE_ENABLE: Feature Enable.
 *
 */
#define IMG_FEATURE_DISABLE 0
#define IMG_FEATURE_ENABLE 1

/**
 * enum max_rx_ampdu_size - Max Rx AMPDU size in KB
 *
 * Max Rx AMPDU Size
 */
enum max_rx_ampdu_size {
	MAX_RX_AMPDU_SIZE_8KB,
	MAX_RX_AMPDU_SIZE_16KB,
	MAX_RX_AMPDU_SIZE_32KB,
	MAX_RX_AMPDU_SIZE_64KB,
	MAX_ALIGN_INT = 0xffffffff
};

/**
 * struct img_data_config_params - Data config parameters
 * @rate_protection_type:0->NONE, 1->RTS/CTS, 2->CTS2SELF
 * @aggregation: Agreegation is enabled(IMG_FEATURE_ENABLE) or disabled(IMG_FEATURE_DISABLE)
 * @wmm: WMM is enabled(IMG_FEATURE_ENABLE) or disabled(IMG_FEATURE_DISABLE)
 * @max_num_tx_agg_sessions: Max number of aggregated TX sessions
 * @max_num_rx_agg_sessions: Max number of aggregated RX sessions
 * @reorder_buf_size: Reorder buffer size (1 to 64)
 * @max_rxampdu_size: Max RX AMPDU size (8/16/32/64 KB), see enum max_rx_ampdu_size
 *
 * Data configuration parameters provided in command IMG_CMD_INIT
 */
PACK(
struct img_data_config_params {
	unsigned char rate_protection_type;
	unsigned char aggregation;
	unsigned char wmm;
	unsigned char max_num_tx_agg_sessions;
	unsigned char max_num_rx_agg_sessions;
	unsigned char max_tx_aggregation;
	unsigned char reorder_buf_size;
	unsigned int max_rxampdu_size;
});


/**
 * struct img_sys_params - Init parameters during IMG_CMD_INIT
 * @mac_addr: MAC address of the interface
 * @sleepEnable: enable rpu sleep
 * @hw_bringup_time:
 * @sw_bringup_time:
 * @bcn_time_out:
 * @calibSleepClk:
 * @rf_params: RF parameters
 * @rf_params_valid: Indicates whether the @rf_params has a valid value.
 * @phy_calib: PHY calibration parameters
 *
 * System parameters provided for command IMG_CMD_INIT
 */
PACK(
struct img_sys_params {
	unsigned int sleepEnable;
	unsigned int hw_bringup_time;
	unsigned int sw_bringup_time;
	unsigned int bcn_time_out;
	unsigned int calibSleepClk;
	unsigned int phy_calib;
#ifdef REG_DEBUG_MODE_SUPPORT
	unsigned char mac_addr[IMG_ETH_ADDR_LEN];
	unsigned char rf_params[RF_PARAMS_SIZE];
	unsigned char rf_params_valid;
#endif /* REG_DEBUG_MODE_SUPPORT */
});




/**
 * struct img_cmd_sys_init - Initialize UMAC
 * @sys_head: umac header, see &img_sys_head
 * @wdev_id : id of the interface.
 * @sys_params: iftype, mac address, see img_sys_params
 * @rx_buf_pools: LMAC Rx buffs pool params, see struct rx_buf_pool_params
 * @data_config_params: Data configuration params, see struct img_data_config_params
 * After host driver bringup host sends the IMG_CMD_INIT to the RPU.
 * then RPU initializes and responds with IMG_EVENT_BUFF_CONFIG.
 */
PACK(
struct img_cmd_sys_init {
	struct img_sys_head sys_head;
	unsigned int wdev_id;
	struct img_sys_params sys_params;
	struct rx_buf_pool_params rx_buf_pools[MAX_NUM_OF_RX_QUEUES];
	struct img_data_config_params data_config_params;
	struct temp_vbat_config  temp_vbat_config_params;
});



/**
 * struct img_cmd_sys_deinit - De-initialize UMAC
 * @sys_head: umac header, see &img_sys_head
 *
 * De-initializes the RPU.
 */
PACK(
struct img_cmd_sys_deinit {
	struct img_sys_head sys_head;
});



/* host has to use img_data_buff_config_complete structure to inform rpu about
 * completin of buffers configuraton. fill IMG_CMD_BUFF_CONFIG_COMPLETE in cmd
 * field.
 */
PACK(
struct img_cmd_buff_config_complete {
	struct img_sys_head        sys_head;
});


enum opt {
	DISABLE,
	ENABLE
};

enum rpu_pkt_preamble {
	RPU_PKT_PREAMBLE_SHORT,
	RPU_PKT_PREAMBLE_LONG,
	RPU_PKT_PREAMBLE_MIXED,
	RPU_PKT_PREAMBLE_MAX,
};

PACK(
struct img_cmd_mode {
	struct img_sys_head	sys_head;
	enum rpu_op_mode mode;

});

#ifdef CONF_SUPPORT
PACK(
struct rpu_conf_params {
	unsigned char nss;
	unsigned char antenna_sel;
	unsigned char rf_params[RF_PARAMS_SIZE];
	unsigned char tx_pkt_chnl_bw;
	unsigned char tx_pkt_tput_mode;
	unsigned char tx_pkt_sgi;
	unsigned char tx_pkt_nss;
	unsigned char tx_pkt_preamble;
	unsigned char tx_pkt_stbc;
	unsigned char tx_pkt_fec_coding;
	char tx_pkt_mcs;
	char tx_pkt_rate;
	char phy_threshold;
	unsigned int phy_calib;
#ifdef DEBUG_MODE_SUPPORT
	enum rpu_stats_type stats_type;
	unsigned char max_agg_limit;
	unsigned char max_agg;
	unsigned char mimo_ps;
	unsigned char rate_protection_type;
	unsigned char ch_scan_mode;
	unsigned char ch_probe_cnt;
	unsigned short active_scan_dur;
	unsigned short passive_scan_dur;
#endif /* DEBUG_MODE_SUPPORT */
	unsigned int op_mode;
	struct chan_params chan;
	unsigned char tx_mode;
	int tx_pkt_num;
	unsigned int tx_pkt_len;
	unsigned int tx_power;
	unsigned char tx;
	unsigned char rx;
	unsigned char aux_adc_input_chain_id;
});



/**
 * struct img_cmd_mode_params
 * @sys_head: UMAC header, See &struct img_sys_head
 * @conf: configuration parameters of different modes see &union rpu_conf_params
 *
 * configures the RPU with config parameters provided in this command
 */
PACK(
struct img_cmd_mode_params {
	struct img_sys_head        sys_head;
	rpu_conf_params_t     conf;
	unsigned int pkt_length[MAX_TX_AGG_SIZE];
	unsigned int ddr_ptrs[MAX_TX_AGG_SIZE];
});

#endif /* CONF_SUPPORT */

/**
 * struct img_cmd_rx - command rx
 * @sys_head: UMAC header, See &struct img_sys_head
 * @conf: rx configuration parameters see &struct rpu_conf_rx_prod_mode_params
 * @:rx_enable: 1-Enable Rx to receive packets contineously on specified channel
 *              0-Disable Rx stop receiving packets and clear statistics
 *
 * Command RPU to Enable/Disable Rx
 */
PACK(
struct img_cmd_rx {
	struct img_sys_head	sys_head;
	struct rpu_conf_rx_prod_mode_params conf;
});


/**
 * struct img_cmd_get_stats - Get statistics
 * @sys_head: UMAC header, See &struct img_sys_head
 * @stats_type: Statistics type see &enum rpu_stats_type
 * @op_mode: Production mode or FCM mode
 *
 * This command is to Request the statistics corresponding to stats_type
 * selected
 *
 */
PACK(
struct img_cmd_get_stats {
	struct img_sys_head        sys_head;
	enum rpu_stats_type	   stats_type;
	enum rpu_op_mode	   op_mode;
});

/**
 * struct img_cmd_clear_stats - clear statistics
 * @sys_head: UMAC header, See &struct img_sys_head.
 * @stats_type: Type of statistics to clear see &enum rpu_stats_type
 *
 * This command is to clear the statistics corresponding to stats_type selected
 */
PACK(
struct img_cmd_clear_stats {
	struct img_sys_head        sys_head;
	enum rpu_stats_type	   stats_type;
});


PACK(
struct img_cmd_pwr {
	struct img_sys_head	sys_head;
	struct img_rpu_pwr_data data_type;
});

PACK(
struct rpu_btcoex {
	int coex_cmd_ctrl;
	enum ext_bt_mode bt_mode;
	enum bt_coex_module_en_dis bt_ctrl;
	struct ptaExtParams pta_params;
});

PACK(
struct img_cmd_btcoex {
	struct img_sys_head sys_head;
	struct rpu_btcoex conf;
});


PACK(
struct rpu_cmd_rftest_info {
	unsigned char ptr[128];
});


PACK(
struct img_cmd_rftest {
	struct img_sys_head sys_head;
	struct rpu_cmd_rftest_info rf_test_info;
});


PACK(
struct rpu_evnt_rftest_info {
	unsigned char ptr[16];
});


PACK(
struct img_event_rftest {
	struct img_sys_head sys_head;
	struct rpu_evnt_rftest_info rf_test_info;
});



PACK(
struct img_event_pwr_data {
	struct img_sys_head  sys_head;
	enum img_rpu_pwr_status mon_status;
	struct img_rpu_pwr_data data_type;
	struct img_rpu_pwr_data data;
	});

/**
 * struct rpu_fw_stats - FW statistics
 * @phy:  PHY statistics  see &struct rpu_phy_stats
 * @lmac: LMAC statistics see &struct rpu_lmac_stats
 * @umac: UMAC statistics see &struct rpu_umac_stats
 *
 * This structure is a combination of all the statistics that the RPU firmware
 * can provide
 *
 */
PACK(
struct rpu_fw_stats {
	struct rpu_phy_stats	phy;
#ifdef REG_DEBUG_MODE_SUPPORT
	struct rpu_lmac_stats lmac;
	struct rpu_umac_stats umac;
#endif /* REG_DEBUG_MODE_SUPPORT */
});


/**
 * struct img_umac_event_stats - statistics event
 * @sys_head: UMAC header, See &struct img_sys_head.
 * @fw: All the statistics that the firmware can provide.
 *
 * This event is the response to command IMG_CMD_GET_STATS.
 *
 */
PACK(
struct img_umac_event_stats {
	struct img_sys_head sys_head;
	struct rpu_fw_stats fw;
});



#ifdef REG_DEBUG_MODE_SUPPORT
/**
 * struct img_event_buff_config_done - Buffers configuration done
 * @sys_head: UMAC header, See &struct img_sys_head.
 * @mac_addr: Mac address of the RPU
 *
 * RPU sends this event in response to IMG_CMD_BUFF_CONFIG_COMPLETE informing
 * RPU is initialized
 */
PACK(
struct img_event_buff_config_done {
	struct img_sys_head        sys_head;
	unsigned char              mac_addr[IMG_ETH_ADDR_LEN];

});



/**
 * struct img_txrx_buffs_config - TX/RX buffers config event.
 * @sys_head: UMAC header, See &struct img_sys_head.
 * @max_tx_descs: Max number of tx descriptors.
 * @max_2k_rx_descs: Max number of 2k rx descriptors.
 * @num_8k_rx_descs: Max number of 2k rx descriptors.
 * @num_mgmt_descs: Max number of mgmt buffers.
 *
 * After initialization RPU sends IMG_EVENT_BUFF_CONFIG
 * to inform host regarding descriptors.
 * 8K buffer are for internal purpose. At initialization time host
 * submits the 8K buffer and UMAC uses buffers to configure LMAC
 * for receiving AMSDU packets.
 */
PACK(
struct img_event_buffs_config {
	struct img_sys_head         sys_head;
	unsigned int                max_tx_descs;
	unsigned int                max_2k_rx_descs;
	unsigned int                num_8k_rx_descs;
	unsigned int                num_mgmt_descs;

});

#endif /* REG_DEBUG_MODE_SUPPORT */


/**
 * struct img_event_init_done - UMAC initialization done
 * @sys_head: UMAC header, See &struct img_sys_head.
 *
 * RPU sends this event in response to IMG_CMD_INIT indicating that the RPU is
 * initialized
 */
PACK(
struct img_event_init_done {
	struct img_sys_head        sys_head;
});


/**
 * struct img_event_deinit_done - UMAC deinitialization done
 * @sys_head: UMAC header, See &struct img_sys_head.
 *
 * RPU sends this event in response to IMG_CMD_DEINIT indicating that the RPU is
 * deinitialized
 */
PACK(
struct img_event_deinit_done {
	struct img_sys_head        sys_head;
});

#endif /* __HOST_RPU_SYS_IF_H__ */
