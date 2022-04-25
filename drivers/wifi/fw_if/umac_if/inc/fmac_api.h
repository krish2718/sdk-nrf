/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @brief Header containing API declarations for the
 * FMAC IF Layer of the Wi-Fi driver.
 */

#ifndef __FMAC_API_H__
#define __FMAC_API_H__

#include <stdbool.h>

#include "osal_api.h"
#include "host_rpu_umac_if.h"
#include "host_rpu_data_if.h"
#include "host_rpu_sys_if.h"

#include "fmac_structs.h"
#include "fmac_cmd.h"
#include "fmac_event.h"
#include "fmac_vif.h"
#include "fmac_bb.h"

#define NVLSI_WLAN_FMAC_DRV_VER "1.3.5.1"


/**
 * wifi_nrf_fmac_init() - Initializes the UMAC IF layer of the RPU WLAN FullMAC
 *                        driver.
 *
 * @rx_buf_pools : Pointer to configuration of Rx queue buffers.
 *		   See &struct rx_buf_pool_params
 *
 * This function initializes the UMAC IF layer of the RPU WLAN FullMAC driver.
 * It does the following:
 *
 *     - Creates and initializes the context for the UMAC IF layer.
 *     - Initializes the HAL layer.
 *     - Registers the driver to the underlying OS.
 *
 * Returns: Pointer to the context of the UMAC IF layer.
 */
struct wifi_nrf_fmac_priv *wifi_nrf_fmac_init(struct img_data_config_params *data_config,
						  struct rx_buf_pool_params *rx_buf_pools,
						  struct wifi_nrf_fmac_callbk_fns *callbk_fns);


/**
 * wifi_nrf_fmac_deinit() - De-initializes the UMAC IF layer of the RPU WLAN
 *                          FullMAC driver.
 * @wifi_nrf_fmac_priv: Pointer to the context of the UMAC IF layer.
 *
 * This function de-initializes the UMAC IF layer of the RPU WLAN FullMAC
 * driver. It does the following:
 *
 *     - De-initializes the HAL layer.
 *     - Frees the context for the UMAC IF layer.
 *
 * Returns: None
 */
void wifi_nrf_fmac_deinit(struct wifi_nrf_fmac_priv *fpriv);


/**
 * wifi_nrf_fmac_dev_add() - Adds a RPU instance.
 * @fpriv: Pointer to the context of the UMAC IF layer.
 *
 * This function adds an RPU instance. This function will return the
 * pointer to the context of the RPU instance. This pointer will need to be
 * supplied while invoking further device specific API's,
 * e.g. @wifi_nrf_fmac_scan etc.
 *
 * Returns: Pointer to the context of the RPU instance.
 */
struct wifi_nrf_fmac_dev_ctx *wifi_nrf_fmac_dev_add(struct wifi_nrf_fmac_priv *fpriv,
							void *os_dev_ctx);


/**
 * wifi_nrf_fmac_dev_rem() - Removes a RPU instance.
 * @fmac_dev_ctx: Pointer to the context of the RPU instance to be removed.
 *
 * This function handles the removal of an RPU instance at the UMAC IF layer.
 *
 * Returns: None.
 */
void wifi_nrf_fmac_dev_rem(struct wifi_nrf_fmac_dev_ctx *fmac_dev_ctx);


/**
 * wifi_nrf_fmac_dev_init() - Initializes a RPU instance.
 * @fmac_dev_ctx: Pointer to the context of the RPU instance to be removed.
 * @params: Parameters needed for initialization of RPU.
 *
 * This function initializes the firmware of an RPU instance.
 *
 * Returns:
 *		Pass: NVLSI_RPU_STATUS_SUCCESS.
 *		Fail: NVLSI_RPU_STATUS_FAIL.
 */
enum wifi_nrf_status wifi_nrf_fmac_dev_init(struct wifi_nrf_fmac_dev_ctx *fmac_dev_ctx,
					       struct wifi_nrf_fmac_init_dev_params *params);


/**
 * wifi_nrf_fmac_dev_deinit() - De-initializes a RPU instance.
 * @fmac_dev_ctx: Pointer to the context of the RPU instance to be removed.
 *
 * This function de-initializes the firmware of an RPU instance.
 *
 * Returns: None.
 */
void wifi_nrf_fmac_dev_deinit(struct wifi_nrf_fmac_dev_ctx *fmac_dev_ctx);


/**
 * wifi_nrf_fmac_fw_load() - Loads the Firmware(s) to the RPU WLAN device.
 * @fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device, which was
 *            passed as @wifi_nrf_fmac_dev_ctx parameter via the @add_dev_callbk_fn
 *            callback function.
 * @fmac_fw: Information about the FullMAC firmwares to be loaded.
 *
 * This function loads the FullMAC firmwares to the RPU WLAN device.
 *
 * Returns: Status
 *		Pass: %NVLSI_RPU_STATUS_SUCCESS
 *		Fail: %NVLSI_RPU_STATUS_FAIL
 */
enum wifi_nrf_status wifi_nrf_fmac_fw_load(struct wifi_nrf_fmac_dev_ctx *fmac_dev_ctx,
					      struct wifi_nrf_fmac_fw_info *fmac_fw);

/**
 * wifi_nrf_fmac_stats_get() - Issue a request to get stats from the RPU.
 * @wifi_nrf_fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 * @rpu_stats_type: The type of RPU statistics to get.
 * @op_mode: Production/FCM mode.
 * @stats: Pointer to memory where the stats are to be copied.
 *
 * This function is used to send a command (%NVLSI_CMD_GET_STATS) to
 * instruct the firmware to return the current RPU statistics. The RPU will
 * send the event (%NVLSI_EVENT_STATS) with the current statistics.
 *
 * Returns: Status
 *		Pass: %NVLSI_RPU_STATUS_SUCCESS
 *		Fail: %NVLSI_RPU_STATUS_FAIL
 */
enum wifi_nrf_status wifi_nrf_fmac_stats_get(struct wifi_nrf_fmac_dev_ctx *wifi_nrf_fmac_dev_ctx,
						struct rpu_op_stats *stats);


/**
 * wifi_nrf_fmac_ver_get() - Get FW versions from the RPU.
 * @wifi_nrf_fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 *
 * This function is used to get Firmware versions from the RPU and they
 * are then stored in the @wifi_nrf_fmac_dev_ctx.
 *
 * Returns: Status
 *		Pass: %NVLSI_RPU_STATUS_SUCCESS
 *		Fail: %NVLSI_RPU_STATUS_FAIL
 */
enum wifi_nrf_status wifi_nrf_fmac_ver_get(struct wifi_nrf_fmac_dev_ctx *wifi_nrf_fmac_dev_ctx);

/**
 * wifi_nrf_fmac_scan() - Issue a scan request to the RPU.
 * @wifi_nrf_fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 * @wifi_nrf_if_idx: Index of the interface on which the scan is to be performed.
 * @scan_info: The parameters needed by the RPU for scan operation.
 *
 * This function is used to send a command (%NVLSI_UMAC_CMD_TRIGGER_SCAN) to
 * instruct the firmware to trigger a scan. The scan can be performed in two
 * modes:
 *
 * Auto Mode (%AUTO_SCAN):
 *     In this mode the host does not need to specify any scan specific
 *     parameters. The RPU will perform the scan on all the channels permitted
 *     by and abiding by the regulations imposed by the
 *     WORLD (common denominator of all regulatory domains) regulatory domain.
 *     The scan will be performed using the wildcard SSID.
 *
 * Channel Map Mode (%CHANNEL_MAPPING_SCAN):
 *      In this mode the host can have fine grained control over the scan
 *      specific parameters to be used (e.g. Passive/Active scan selection,
 *      Number of probe requests per active scan, Channel list to scan,
 *      Permanence on each channel, SSIDs to scan etc.). This mode expects
 *      the regulatory restrictions to be taken care by the invoker of the
 *      API.
 *
 * Returns: Status
 *		Pass: %NVLSI_RPU_STATUS_SUCCESS
 *		Fail: %NVLSI_RPU_STATUS_FAIL
 */
enum wifi_nrf_status wifi_nrf_fmac_scan(void *wifi_nrf_fmac_dev_ctx,
					   unsigned char wifi_nrf_if_idx,
					   struct img_umac_scan_info *scan_info);


/**
 * wifi_nrf_fmac_scan_res_get() - Issue a scan results request to the RPU.
 * @wifi_nrf_fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 * @if_idx: Index of the interface on which the scan results are to be fetched.
 * @scan_type: The scan type (i.e. DISPLAY or CONNECT scan).
 *
 * This function is used to send a command (%NVLSI_UMAC_CMD_GET_SCAN_RESULTS) to
 * instruct the firmware to return the results of a scan.
 *
 * Returns: Status
 *		Pass: %NVLSI_RPU_STATUS_SUCCESS
 *		Fail: %NVLSI_RPU_STATUS_FAIL
 */
enum wifi_nrf_status wifi_nrf_fmac_scan_res_get(void *wifi_nrf_fmac_dev_ctx,
						   unsigned char vif_idx,
						   enum scan_reason scan_type);


/**
 * wifi_nrf_fmac_auth() - Issue a authentication request to the RPU.
 * @wifi_nrf_fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 * @wifi_nrf_if_idx: Index of the interface on which the authentication is to be
 *		performed.
 * @auth_info: The parameters needed by the RPU to generate the authentication
 *             request.
 *
 * This function is used to send a command (%NVLSI_UMAC_CMD_AUTHENTICATE) to
 * instruct the firmware to initiate a authentication request to an AP on the
 * interface identified with @wifi_nrf_if_idx.
 *
 * Returns: Status
 *		Pass: %NVLSI_RPU_STATUS_SUCCESS
 *		Fail: %NVLSI_RPU_STATUS_FAIL
 */
enum wifi_nrf_status wifi_nrf_fmac_auth(void *wifi_nrf_fmac_dev_ctx,
					   unsigned char wifi_nrf_if_idx,
					   struct img_umac_auth_info *auth_info);


/**
 * wifi_nrf_fmac_deauth() - Issue a deauthentication request to the RPU.
 * @wifi_nrf_fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 * @wifi_nrf_if_idx: Index of the interface on which the deauthentication is to be
 *              performed.
 * @deauth_info: Deauthentication specific information required by the RPU.
 *
 * This function is used to send a command (%NVLSI_UMAC_CMD_DEAUTHENTICATE) to
 * instruct the firmware to initiate a deauthentication notification to an AP
 * on the interface identified with @wifi_nrf_if_idx.
 *
 * Returns: Status
 *		Pass: %NVLSI_RPU_STATUS_SUCCESS
 *		Fail: %NVLSI_RPU_STATUS_FAIL
 */
enum wifi_nrf_status wifi_nrf_fmac_deauth(void *wifi_nrf_fmac_dev_ctx,
					     unsigned char wifi_nrf_if_idx,
					     struct img_umac_disconn_info *deauth_info);


/**
 * wifi_nrf_fmac_assoc() - Issue a association request to the RPU.
 * @wifi_nrf_fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 * @wifi_nrf_if_idx: Index of the interface on which the association is to be
 *              performed.
 * @assoc_info: The parameters needed by the RPU to generate the association
 *              request.
 *
 * This function is used to send a command (%NVLSI_UMAC_CMD_ASSOCIATE) to
 * instruct the firmware to initiate a association request to an AP on the
 * interface identified with @wifi_nrf_if_idx.
 *
 * Returns: Status
 *		Pass: %NVLSI_RPU_STATUS_SUCCESS
 *		Fail: %NVLSI_RPU_STATUS_FAIL
 */
enum wifi_nrf_status wifi_nrf_fmac_assoc(void *wifi_nrf_fmac_dev_ctx,
					    unsigned char wifi_nrf_if_idx,
					    struct img_umac_assoc_info *assoc_info);


/**
 * wifi_nrf_fmac_disassoc() - Issue a disassociation request to the RPU.
 * @wifi_nrf_fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 * @wifi_nrf_if_idx: Index of the interface on which the disassociation is to be
 *	        performed.
 * @disassoc_info: Disassociation specific information required by the RPU.
 *
 * This function is used to send a command (%NVLSI_UMAC_CMD_DISASSOCIATION) to
 * instruct the firmware to initiate a disassociation notification to an AP
 * on the interface identified with @wifi_nrf_if_idx.
 *
 * Returns: Status
 *		Pass: %NVLSI_RPU_STATUS_SUCCESS
 *		Fail: %NVLSI_RPU_STATUS_FAIL
 */
enum wifi_nrf_status wifi_nrf_fmac_disassoc(void *wifi_nrf_fmac_dev_ctx,
					       unsigned char wifi_nrf_if_idx,
					       struct img_umac_disconn_info *disassoc_info);


/**
 * wifi_nrf_fmac_add_key() - Add a key into the RPU security database.
 * @wifi_nrf_fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 * @wifi_nrf_if_idx: Index of the interface on which the key is to be added.
 * @key_info: Key specific information which needs to be passed to the RPU.
 * @mac_addr: MAC address of the peer with which the key is associated.
 *
 * This function is used to send a command (%NVLSI_UMAC_CMD_NEW_KEY) to
 * instruct the firmware to add a key to its security database.
 * The key is for the peer identified by @mac_addr on the
 * interface identified with @wifi_nrf_if_idx.
 *
 * Returns: Status
 *		Pass: %NVLSI_RPU_STATUS_SUCCESS
 *		Fail: %NVLSI_RPU_STATUS_FAIL
 */
enum wifi_nrf_status wifi_nrf_fmac_add_key(void *wifi_nrf_fmac_dev_ctx,
					      unsigned char wifi_nrf_if_idx,
					      struct img_umac_key_info *key_info,
					      const char *mac_addr);


/**
 * wifi_nrf_fmac_del_key() - Delete a key from the RPU security database.
 * @wifi_nrf_fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 * @wifi_nrf_if_idx: Index of the interface from which the key is to be deleted.
 * @key_info: Key specific information which needs to be passed to the RPU.
 * @mac_addr: MAC address of the peer with which the key is associated.
 *
 * This function is used to send a command (%NVLSI_UMAC_CMD_DEL_KEY) to
 * instruct the firmware to delete a key from its security database.
 * The key is for the peer identified by @mac_addr  on the
 * interface identified with @wifi_nrf_if_idx.
 *
 * Returns: Status
 *		Pass: %NVLSI_RPU_STATUS_SUCCESS
 *		Fail: %NVLSI_RPU_STATUS_FAIL
 */
enum wifi_nrf_status wifi_nrf_fmac_del_key(void *wifi_nrf_fmac_dev_ctx,
					      unsigned char wifi_nrf_if_idx,
					      struct img_umac_key_info *key_info,
					      const char *mac_addr);


/**
 * wifi_nrf_fmac_set_key() - Set a key as a default for data or management
 *                           traffic in the RPU security database.
 * @wifi_nrf_fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 * @wifi_nrf_if_idx: Index of the interface on which the key is to be set.
 * @key_info: Key specific information which needs to be passed to the RPU.
 *
 * This function is used to send a command (%NVLSI_UMAC_CMD_SET_KEY) to
 * instruct the firmware to set a key as default in its security database.
 * The key is either for data or management traffic and is classified based on
 * the flags element set in @key_info parameter.
 *
 * Returns: Status
 *		Pass: %NVLSI_RPU_STATUS_SUCCESS
 *		Fail: %NVLSI_RPU_STATUS_FAIL
 */
enum wifi_nrf_status wifi_nrf_fmac_set_key(void *wifi_nrf_fmac_dev_ctx,
					      unsigned char wifi_nrf_if_idx,
					      struct img_umac_key_info *key_info);


/**
 * wifi_nrf_fmac_set_bss() - Set BSS parameters for an AP mode interface to the
 *                           RPU.
 * @wifi_nrf_fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 * @wifi_nrf_if_idx: Index of the interface on which the BSS parameters are to be set.
 * @bss_info: BSS specific parameters which need to be passed to the RPU.
 *
 * This function is used to send a command (%NVLSI_UMAC_CMD_SET_BSS) to
 * instruct the firmware to set the BSS parameter for an AP mode interface.
 *
 * Returns: Status
 *		Pass: %NVLSI_RPU_STATUS_SUCCESS
 *		Fail: %NVLSI_RPU_STATUS_FAIL
 */
enum wifi_nrf_status wifi_nrf_fmac_set_bss(void *wifi_nrf_fmac_dev_ctx,
					      unsigned char wifi_nrf_if_idx,
					      struct img_umac_bss_info *bss_info);


/**
 * wifi_nrf_fmac_chg_bcn() - Update the Beacon Template.
 * @wifi_nrf_fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 * @wifi_nrf_if_idx: Index of the interface on which the Beacon Template is to be updated.
 * @data: Beacon Template which need to be passed to the RPU.
 *
 * This function is used to send a command (%NVLSI_UMAC_CMD_SET_BEACON) to
 * instruct the firmware to update beacon template for an AP mode interface.
 *
 * Returns: Status
 *              Pass: %NVLSI_RPU_STATUS_SUCCESS
 *              Fail: %NVLSI_RPU_STATUS_FAIL
 */
enum wifi_nrf_status wifi_nrf_fmac_chg_bcn(void *wifi_nrf_fmac_dev_ctx,
					      unsigned char wifi_nrf_if_idx,
					      struct img_umac_set_beacon_info *data);

/**
 * wifi_nrf_fmac_start_ap() - Start a SoftAP.
 * @wifi_nrf_fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 * @wifi_nrf_if_idx: Index of the interface on which the SoftAP is to be started.
 * @ap_info: AP operation specific parameters which need to be passed to the
 *           RPU.
 *
 * This function is used to send a command (%NVLSI_UMAC_CMD_START_AP) to
 * instruct the firmware to start a SoftAP on an interface identified with
 * @wifi_nrf_if_idx.
 *
 * Returns: Status
 *		Pass: %NVLSI_RPU_STATUS_SUCCESS
 *		Fail: %NVLSI_RPU_STATUS_FAIL
 */
enum wifi_nrf_status wifi_nrf_fmac_start_ap(void *wifi_nrf_fmac_dev_ctx,
					       unsigned char wifi_nrf_if_idx,
					       struct img_umac_start_ap_info *ap_info);


/**
 * wifi_nrf_fmac_stop_ap() - Stop a SoftAP.
 * @wifi_nrf_fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 * @wifi_nrf_if_idx: Index of the interface on which the SoftAP is to be stopped.
 *
 * This function is used to send a command (%NVLSI_UMAC_CMD_STOP_AP) to
 * instruct the firmware to stop a SoftAP on an interface identified with
 * @wifi_nrf_if_idx.
 *
 * Returns: Status
 *		Pass: %NVLSI_RPU_STATUS_SUCCESS
 *		Fail: %NVLSI_RPU_STATUS_FAIL
 */
enum wifi_nrf_status wifi_nrf_fmac_stop_ap(void *wifi_nrf_fmac_dev_ctx,
					      unsigned char wifi_nrf_if_idx);


/**
 * wifi_nrf_fmac_start_p2p_dev() - Start P2P mode on an interface.
 * @wifi_nrf_fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 * @wifi_nrf_if_idx: Index of the interface on which the P2P mode is to be started.
 *
 * This function is used to send a command (%NVLSI_UMAC_CMD_START_P2P_DEVICE) to
 * instruct the firmware to start P2P mode on an interface identified with
 * @wifi_nrf_if_idx.
 *
 * Returns: Status
 *		Pass: %NVLSI_RPU_STATUS_SUCCESS
 *		Fail: %NVLSI_RPU_STATUS_FAIL
 */
enum wifi_nrf_status wifi_nrf_fmac_p2p_dev_start(void *wifi_nrf_fmac_dev_ctx,
						    unsigned char wifi_nrf_if_idx);


/**
 * wifi_nrf_fmac_stop_p2p_dev() - Start P2P mode on an interface.
 * @wifi_nrf_fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 * @wifi_nrf_if_idx: Index of the interface on which the P2P mode is to be started.
 *
 * This function is used to send a command (%NVLSI_UMAC_CMD_START_P2P_DEVICE) to
 * instruct the firmware to start P2P mode on an interface identified with
 * @wifi_nrf_if_idx.
 *
 * Returns: Status
 *		Pass: %NVLSI_RPU_STATUS_SUCCESS
 *		Fail: %NVLSI_RPU_STATUS_FAIL
 */
enum wifi_nrf_status wifi_nrf_fmac_p2p_dev_stop(void *wifi_nrf_fmac_dev_ctx,
						   unsigned char wifi_nrf_if_idx);

/**
 * wifi_nrf_fmac_p2p_roc_start()
 * @wifi_nrf_fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 * @wifi_nrf_if_idx: Index of the interface to be kept on channel and stay there.
 * @roc_info: Contans channel and time in ms to stay on.
 *
 * This function is used to send a command (%NVLSI_UMAC_CMD_REMAIN_ON_CHANNEL)
 * to RPU to put p2p device in listen state for a duration.
 *
 * Returns: Status
 *		Pass: %NVLSI_RPU_FMAC_STATUS_SUCCESS
 *		Fail: %NVLSI_RPU_FMAC_STATUS_FAIL
 */
enum wifi_nrf_status wifi_nrf_fmac_p2p_roc_start(void *wifi_nrf_fmac_dev_ctx,
						    unsigned char wifi_nrf_if_idx,
						    struct remain_on_channel_info *roc_info);

/**
 * wifi_nrf_fmac_p2p_roc_stop()
 * @wifi_nrf_fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 * @wifi_nrf_if_idx Index of the interface to be kept on channel and stay there.
 * @cookie: cancel p2p listen state of the matching cookie.
 *
 * This function is used to send a command
 * (%NVLSI_UMAC_CMD_CANCEL_REMAIN_ON_CHANNEL) to RPU to put p2p device out
 * of listen state.
 *
 * Returns: Status
 *		 Pass: %NVLSI_RPU_FMAC_STATUS_SUCCESS
 *		 Fail: %NVLSI_RPU_FMAC_STATUS_FAIL
 */
enum wifi_nrf_status wifi_nrf_fmac_p2p_roc_stop(void *wifi_nrf_fmac_dev_ctx,
						   unsigned char wifi_nrf_if_idx,
						   unsigned long long cookie);

/**
 * wifi_nrf_fmac_mgmt_tx() - Transmit a management frame.
 * @wifi_nrf_fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 * @wifi_nrf_if_idx: Index of the interface on which the frame is to be
 *              transmitted.
 * @mgmt_tx_info: Information regarding the management frame to be
 *                transmitted.
 *
 * This function is used to instruct the RPU to transmit a management frame.
 * This is done using the command %NVLSI_UMAC_CMD_FRAME.
 *
 * Returns: Status
 *		Pass: %NVLSI_RPU_STATUS_SUCCESS
 *		Fail: %NVLSI_RPU_STATUS_FAIL
 */
enum wifi_nrf_status wifi_nrf_fmac_mgmt_tx(void *wifi_nrf_fmac_dev_ctx,
					      unsigned char wifi_nrf_if_idx,
					      struct img_umac_mgmt_tx_info *mgmt_tx_info);


/**
 * wifi_nrf_fmac_del_sta() - Remove a station.
 * @wifi_nrf_fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 * @wifi_nrf_if_idx: Index of the interface on which the STA is connected.
 * @del_sta_info: Information regarding the station to be removed.
 *
 * This function is used to instruct the RPU to remove a station and send a
 * deauthentication/disassociation frame to the same. This is done using the
 * command %NVLSI_UMAC_CMD_DEL_STATION.
 *
 * Returns: Status
 *		Pass: %NVLSI_RPU_STATUS_SUCCESS
 *		Fail: %NVLSI_RPU_STATUS_FAIL
 */
enum wifi_nrf_status wifi_nrf_fmac_del_sta(void *wifi_nrf_fmac_dev_ctx,
					      unsigned char wifi_nrf_if_idx,
					      struct img_umac_del_sta_info *del_sta_info);


/**
 * wifi_nrf_fmac_add_sta() - Indicate a new STA connection to the RPU.
 * @wifi_nrf_fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 * @wifi_nrf_if_idx: Index of the interface on which the STA is connected.
 * @add_sta_info: Information regarding the new station.
 *
 * This function is used to indicate to the RPU that a new STA has
 * successfully connected to the AP. This information is sent to the RPU
 * using the command %NVLSI_UMAC_CMD_NEW_STATION.
 *
 * Returns: Status
 *		Pass: %NVLSI_RPU_STATUS_SUCCESS
 *		Fail: %NVLSI_RPU_STATUS_FAIL
 */
enum wifi_nrf_status wifi_nrf_fmac_add_sta(void *wifi_nrf_fmac_dev_ctx,
					      unsigned char wifi_nrf_if_idx,
					      struct img_umac_add_sta_info *add_sta_info);

/**
 * wifi_nrf_fmac_chg_sta() - Indicate changes to STA connection parameters to
 *                           the RPU.
 * @wifi_nrf_fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 * @wifi_nrf_if_idx: Index of the interface on which the STA is connected.
 * @chg_sta_info: Information regarding updates to the station parameters.
 *
 * This function is used to indicate changes in the connected STA parameters
 * to the RPU. This information is sent to the RPU using the command
 * %NVLSI_UMAC_CMD_SET_STATION.
 *
 * Returns: Status
 *		Pass: %NVLSI_RPU_STATUS_SUCCESS
 *		Fail: %NVLSI_RPU_STATUS_FAIL
 */
enum wifi_nrf_status wifi_nrf_fmac_chg_sta(void *wifi_nrf_fmac_dev_ctx,
					      unsigned char wifi_nrf_if_idx,
					      struct img_umac_chg_sta_info *chg_sta_info);



/**
 * wifi_nrf_fmac_mgmt_frame_reg() - Register the management frame type which
 *                                  needs to be sent up to the host by the RPU.
 * @wifi_nrf_fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 * @wifi_nrf_if_idx: Index of the interface from which the received frame is to be
 *              sent up.
 * @frame_info: Information regarding the management frame to be sent up.
 *
 * This function is used to send a command (%NVLSI_UMAC_CMD_REGISTER_FRAME) to
 * instruct the firmware to pass frames matching that type/subtype to be
 * passed upto the host driver.
 *
 * Returns: Status
 *		Pass: %NVLSI_RPU_STATUS_SUCCESS
 *		Fail: %NVLSI_RPU_STATUS_FAIL
 */
enum wifi_nrf_status wifi_nrf_fmac_mgmt_frame_reg(void *wifi_nrf_fmac_dev_ctx,
						     unsigned char wifi_nrf_if_idx,
						     struct img_umac_mgmt_frame_info *frame_info);


/**
 * wifi_nrf_fmac_mac_addr() - Get unused MAC address from base mac address.
 * @wifi_nrf_fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 * @addr: Memory to copy the mac address to.
 *
 * Returns: Status
 *		Pass: %NVLSI_RPU_STATUS_SUCCESS
 *		Fail: %NVLSI_RPU_STATUS_FAIL
 */
enum wifi_nrf_status wifi_nrf_fmac_mac_addr(struct wifi_nrf_fmac_dev_ctx *fmac_dev_ctx,
					       unsigned char *addr);

/**
 * wifi_nrf_fmac_add_vif() - Add a new virtual interface.
 * @wifi_nrf_fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 * @os_vif_ctx: Pointer to VIF context that the user of the UMAC IF would like
 *              to be passed during invocation of callback functions like
 *              @rx_frm_callbk_fn etc.
 * @vif_info: Information regarding the interface to be added.
 *
 * This function is used to send a command (%NVLSI_UMAC_CMD_NEW_INTERFACE) to
 * instruct the firmware to add a new interface with parameters specified by
 * @vif_info.
 *
 * Returns: Index (maintained by the UMAC IF layer) of the VIF that was added.
 *          In case of error @MAX_VIFS will be returned.
 */
unsigned char wifi_nrf_fmac_add_vif(void *wifi_nrf_fmac_dev_ctx,
				      void *os_vif_ctx,
				      struct img_umac_add_vif_info *vif_info);


/**
 * wifi_nrf_fmac_del_vif() - Deletes a virtual interface.
 * @wifi_nrf_fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 * @wifi_nrf_if_idx: Index of the interface to be deleted.
 *
 * This function is used to send a command (%NVLSI_UMAC_CMD_DEL_INTERFACE) to
 * instruct the firmware to delete an interface which was added using
 * @wifi_nrf_fmac_add_vif.
 *
 * Returns: Status
 *		Pass: %NVLSI_RPU_STATUS_SUCCESS
 *		Fail: %NVLSI_RPU_STATUS_FAIL
 */
enum wifi_nrf_status wifi_nrf_fmac_del_vif(void *wifi_nrf_fmac_dev_ctx,
					      unsigned char wifi_nrf_if_idx);

/**
 * wifi_nrf_fmac_chg_vif() - Change the attributes of an interface.
 * @wifi_nrf_fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 * @wifi_nrf_if_idx: Index of the interface on which the functionality is to be
 *              bound.
 * @vif_info: Interface specific information which needs to be passed to the
 *            RPU.
 *
 * This function is used to change the attributes of an interface identified
 * with @wifi_nrf_if_idx. This information is sent to the RPU using the command
 * %NVLSI_UMAC_CMD_SET_INTERFACE.
 *
 * Returns: Status
 *		Pass: %NVLSI_RPU_STATUS_SUCCESS
 *		Fail: %NVLSI_RPU_STATUS_FAIL
 */
enum wifi_nrf_status wifi_nrf_fmac_chg_vif(void *wifi_nrf_fmac_dev_ctx,
					      unsigned char wifi_nrf_if_idx,
					      struct img_umac_chg_vif_attr_info *vif_info);


/**
 * wifi_nrf_fmac_chg_vif_state() - Change the state of a virtual interface.
 * @wifi_nrf_fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 * @wifi_nrf_if_idx: Index of the interface whose state needs to be changed.
 * @vif_info: State information to be changed for the interface.
 *
 * This function is used to send a command (%NVLSI_UMAC_CMD_SET_IFFLAGS) to
 * instruct the firmware to change the state of an interface with an index of
 * @wifi_nrf_if_idx and parameters specified by @vif_info.
 *
 * Returns: Status
 *		Pass: %NVLSI_RPU_STATUS_SUCCESS
 *		Fail: %NVLSI_RPU_STATUS_FAIL
 */
enum wifi_nrf_status wifi_nrf_fmac_chg_vif_state(void *wifi_nrf_fmac_dev_ctx,
						    unsigned char wifi_nrf_if_idx,
						    struct img_umac_chg_vif_state_info *vif_info);


/**
 * wifi_nrf_fmac_start_xmit() - Trasmit a frame to the RPU.
 * @wifi_nrf_fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 * @wifi_nrf_if_idx: Index of the interface on which the frame is to be
 *              transmitted.
 * @netbuf: Pointer to the OS specific network buffer.
 *
 * This function takes care of transmitting a frame to the RPU. It does the
 * following:
 *
 *     - Queues the frames to a transmit queue.
 *     - Based on token availability sends one or more frames to the RPU using
 *       the command NVLSI_CMD_TX_BUFF for transmission.
 *     - The firmware sends an NVLSI_CMD_TX_BUFF_DONE event once the command has
 *       been processed send to indicate whether the frame(s) have been
 *       transmited/aborted.
 *     - The diver can cleanup the frame buffers after receiving this event.
 *
 * Returns: Status
 *		Pass: %NVLSI_RPU_STATUS_SUCCESS
 *		Fail: %NVLSI_RPU_STATUS_FAIL
 */
enum wifi_nrf_status wifi_nrf_fmac_start_xmit(void *wifi_nrf_fmac_dev_ctx,
						 unsigned char wifi_nrf_if_idx,
						 void *netbuf);

/**
 * wifi_nrf_fmac_suspend() - Inform the RPU that host is going to suspend
 *                           state.
 * @wifi_nrf_fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 *
 * This function is used to send a command (%NVLSI_UMAC_CMD_SUSPEND) to
 * inform the RPU that host is going to suspend state.
 *
 * Returns: Status
 *		Pass: %NVLSI_RPU_STATUS_SUCCESS
 *		Fail: %NVLSI_RPU_STATUS_FAIL
 */
enum wifi_nrf_status wifi_nrf_fmac_suspend(void *wifi_nrf_fmac_dev_ctx);


/**
 * wifi_nrf_fmac_resume() - Nofity RPU that host has resumed from a suspended
 *                          state.
 * @wifi_nrf_fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 *
 * This function is used to send a command (%NVLSI_UMAC_CMD_RESUME)
 * to inform the RPU that host has resumed from a suspended state.
 *
 * Returns: Status
 *		Pass: %NVLSI_RPU_STATUS_SUCCESS
 *		Fail: %NVLSI_RPU_STATUS_FAIL
 */
enum wifi_nrf_status wifi_nrf_fmac_resume(void *wifi_nrf_fmac_dev_ctx);


/**
 * wifi_nrf_fmac_get_tx_power() - Get tx power
 *
 * @wifi_nrf_fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 * @wifi_nrf_vif_idx: VIF index.
 *
 * This function is used to send a command (%NVLSI_UMAC_CMD_GET_TX_POWER)
 * to get the transmit power on a particular interface.
 *
 * Returns: Status
 *		Pass: %NVLSI_RPU_STATUS_SUCCESS
 *		Fail: %NVLSI_RPU_STATUS_FAIL
 */
enum wifi_nrf_status wifi_nrf_fmac_get_tx_power(void *wifi_nrf_fmac_dev_ctx,
						   unsigned int wifi_nrf_vif_idx);

/**
 * wifi_nrf_fmac_get_channel() - Get chandef
 *
 * @wifi_nrf_fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 * @wifi_nrf_vif_idx: VIF index.
 *
 * This function is used to send a command (%NVLSI_UMAC_CMD_GET_CHANNEL)
 * to get the channel configured on a particular interface.
 *
 * Returns: Status
 *		Pass: %NVLSI_RPU_STATUS_SUCCESS
 *		Fail: %NVLSI_RPU_STATUS_FAIL
 */
enum wifi_nrf_status wifi_nrf_fmac_get_channel(void *wifi_nrf_fmac_dev_ctx,
						  unsigned int wifi_nrf_vif_idx);

/**
 * wifi_nrf_fmac_get_station() - Get station statistics
 *
 * @wifi_nrf_fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 * @wifi_nrf_vif_idx: VIF index.
 * @mac : MAC address of the station
 *
 * This function is used to send a command (%NVLSI_UMAC_CMD_GET_STATION)
 * to get station statistics using a mac address.
 *
 * Returns: Status
 *		Pass: %NVLSI_RPU_STATUS_SUCCESS
 *		Fail: %NVLSI_RPU_STATUS_FAIL
 */
enum wifi_nrf_status wifi_nrf_fmac_get_station(void *wifi_nrf_fmac_dev_ctx,
						  unsigned int wifi_nrf_vif_idx,
						  unsigned char *mac);


/**
 * wifi_nrf_fmac_set_power_save() - Configure WLAN power management.
 * @wifi_nrf_fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 * @wifi_nrf_if_idx: Index of the interface on which power management is to be set.
 * @state: State (Enable/Disable) of WLAN power management.
 *
 * This function is used to send a command (%NVLSI_UMAC_CMD_SET_POWER_SAVE)
 * to RPU to Enable/Disable WLAN Power management.
 *
 * Returns: Status
 *		Pass: %NVLSI_RPU_STATUS_SUCCESS
 *		Fail: %NVLSI_RPU_STATUS_FAIL
 */
enum wifi_nrf_status wifi_nrf_fmac_set_power_save(void *wifi_nrf_fmac_dev_ctx,
						     unsigned char wifi_nrf_if_idx,
						     bool state);

/**
 * wifi_nrf_fmac_set_qos_map() - Configure qos_map of for data
 * @wifi_nrf_fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 * @wifi_nrf_if_idx: Index of the interface on which the qos map be set.
 * @qos_info: qos_map information
 *
 * This function is used to send a command (%NVLSI_UMAC_CMD_SET_QOS_MAP)
 * to RPU to set qos map information.
 *
 * Returns: Status
 *              Pass: %NVLSI_RPU_STATUS_SUCCESS
 *              Fail: %NVLSI_RPU_STATUS_FAIL
 */
enum wifi_nrf_status wifi_nrf_fmac_set_qos_map(void *wifi_nrf_fmac_dev_ctx,
						  unsigned char wifi_nrf_if_idx,
						  struct img_umac_qos_map_info *qos_info);


/**
 * wifi_nrf_fmac_set_wowlan() - Configure WOWLAN.
 * @wifi_nrf_fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 * @var: Wakeup trigger condition.
 *
 * This function is used to send a command (%NVLSI_UMAC_CMD_SET_WOWLAN)
 * to RPU to configure Wakeup trigger condition in RPU.
 *
 * Returns: Status
 *		Pass: %NVLSI_RPU_STATUS_SUCCESS
 *		Fail: %NVLSI_RPU_STATUS_FAIL
 */
enum wifi_nrf_status wifi_nrf_fmac_set_wowlan(void *wifi_nrf_fmac_dev_ctx,
						 unsigned int var);

/**
 * wifi_nrf_fmac_set_wiphy_params() - set wiphy parameters
 * @wifi_nrf_fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 * @wiphy_info: wiphy parameters
 *
 * This function is used to send a command (%NVLSI_UMAC_CMD_SET_WIPHY)
 * to RPU to configure parameters relted to interface.
 *
 * Returns: Status
 *              Pass: %NVLSI_RPU_STATUS_SUCCESS
 *              Fail: %NVLSI_RPU_STATUS_FAIL
 */

enum wifi_nrf_status wifi_nrf_fmac_set_wiphy_params(void *wifi_nrf_fmac_dev_ctx,
						       unsigned char wifi_nrf_vif_idx,
						       struct img_umac_set_wiphy_info *wiphy_info);


/**
 * wifi_nrf_fmac_conf_btcoex() - Configure BT-Coex parameters in RPU.
 * @fmac_dev_ctx: Pointer to the UMAC IF context for a RPU WLAN device.
 * @params: BT-coex parameters which will be configured in RPU.
 *
 * This function is used to send a command (%NVLSI_CMD_BTCOEX) to RPU to configure
 * BT-Coex parameters in RPU.
 *
 * Returns: Status
 *              Pass: %NVLSI_RPU_STATUS_SUCCESS
 *              Fail: %NVLSI_RPU_STATUS_FAIL
 */
enum wifi_nrf_status wifi_nrf_fmac_conf_btcoex(struct wifi_nrf_fmac_dev_ctx *fmac_dev_ctx,
						  struct rpu_btcoex *params);
#endif /* __FMAC_API_H__ */
