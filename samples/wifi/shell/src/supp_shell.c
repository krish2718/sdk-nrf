/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/* @file
 * @brief WPA supplicant shell module
 */

#include <logging/log.h>
LOG_MODULE_REGISTER(net_supp_shell, LOG_LEVEL_INF);

#include <zephyr.h>
#include <stdio.h>
#include <stdlib.h>
#include <shell/shell.h>
#include <init.h>
#include <ctype.h>

#include <net/net_if.h>
#include <net/wifi_mgmt.h>
#include <net/net_event.h>
#include "common.h"
#include "wpa_supplicant/config.h"
#include "wpa_supplicant_i.h"
#include "supp_mgmt.h"
#include "driver_zephyr.h"

#define SUPPLICANT_SHELL_MODULE "wpa_cli"

#define SUPPLICANT_SHELL_MGMT_EVENTS                                           \
	(NET_EVENT_SUPP_SCAN_RESULT | NET_EVENT_SUPP_SCAN_DONE |               \
	 NET_EVENT_SUPP_CONNECT_RESULT)

static struct {
	const struct shell *shell;

	union {
		struct {
			uint8_t connecting : 1;
			uint8_t disconnecting : 1;
			uint8_t _unused : 6;
		};
		uint8_t all;
	};
} context;

static uint32_t scan_result;

static struct net_mgmt_event_callback supplicant_shell_mgmt_cb;
int cli_main (int, const char **);
extern struct wpa_supplicant *wpa_s_0;
struct wpa_ssid *ssid_0 = NULL;

#define MAX_SSID_LEN 32

#define print(shell, level, fmt, ...)                                          \
	do {                                                                   \
		if (shell) {                                                   \
			shell_fprintf(shell, level, fmt, ##__VA_ARGS__);       \
		} else {                                                       \
			wpa_printf(MSG_INFO, fmt, ##__VA_ARGS__);                            \
		}                                                              \
	} while (false)

#ifdef notyet
static void handle_supplicant_connect_result(struct net_mgmt_event_callback *cb)
{
	const struct wifi_status *status = (const struct wifi_status *)cb->info;

	if (status->status) {
		print(context.shell, SHELL_WARNING,
		      "Connection request failed (%d)\n", status->status);
	} else {
		print(context.shell, SHELL_NORMAL, "Connected\n");
	}

	context.connecting = false;
}

static void
handle_supplicant_disconnect_result(struct net_mgmt_event_callback *cb)
{
	const struct wifi_status *status = (const struct wifi_status *)cb->info;

	if (context.disconnecting) {
		print(context.shell,
		      status->status ? SHELL_WARNING : SHELL_NORMAL,
		      "Disconnection request %s (%d)\n",
		      status->status ? "failed" : "done", status->status);
		context.disconnecting = false;
	} else {
		print(context.shell, SHELL_NORMAL, "Disconnected\n");
	}
}
#endif /* notyet */

static void supplicant_mgmt_event_handler(struct net_mgmt_event_callback *cb,
					  uint32_t mgmt_event,
					  struct net_if *iface)
{
	switch (mgmt_event) {
#ifdef notyet
	case NET_EVENT_SUPP_SCAN_RESULT:
		handle_supplicant_scan_result(cb);
		break;
	case NET_EVENT_SUPP_SCAN_DONE:
		handle_supplicant_scan_done(cb);
		break;
	case NET_EVENT_SUPP_CONNECT_RESULT:
		handle_supplicant_connect_result(cb);
		break;
	case NET_EVENT_SUPP_DISCONNECT_RESULT:
		handle_supplicant_disconnect_result(cb);
		break;
#endif /* notyet */
	default:
		break;
	}
}

static int __wifi_args_to_params(size_t argc, const char *argv[],
				 struct wifi_connect_req_params *params)
{
	char *endptr;
	int idx = 1;

	if (argc < 1) {
		return -EINVAL;
	}

	/* SSID */
	params->ssid = (char *) argv[0];
	params->ssid_length = strlen(params->ssid);

#ifdef notyet
	/* Channel (optional) */
	if ((idx < argc) && (strlen(argv[idx]) <= 2)) {
		params->channel = strtol(argv[idx], &endptr, 10);
		if (*endptr != '\0') {
			return -EINVAL;
		}

		if (params->channel == 0U) {
			params->channel = WIFI_CHANNEL_ANY;
		}

		idx++;
	} else {
		params->channel = WIFI_CHANNEL_ANY;
	}
#endif

	/* PSK (optional) */
	if (idx < argc) {
		params->psk = (char*) argv[idx];
		params->psk_length = strlen(argv[idx]);
		params->security = WIFI_SECURITY_TYPE_PSK;
		idx++;
		if ((idx < argc) && (strlen(argv[idx]) <= 2)) {
				params->security= strtol(argv[idx], &endptr, 10);
		}
	} else {
		params->security = WIFI_SECURITY_TYPE_NONE;
	}

	return 0;
}

static int cmd_supplicant_connect(const struct shell *shell, size_t argc,
				  const char *argv[])
{
	static struct wifi_connect_req_params cnx_params;
	struct wifi_connect_req_params *params;
	struct wpa_ssid *ssid = NULL;
	bool pmf = true;

	if (__wifi_args_to_params(argc - 1, &argv[1], &cnx_params)) {
		shell_help(shell);
		return -ENOEXEC;
	}

	params = &cnx_params;

	if (!wpa_s_0) {
		wpa_printf(MSG_ERROR, "%s: wpa_supplicant is not initialized, dropping connect\n",
			   __func__);
		return -1;
	}
	ssid = wpa_supplicant_add_network(wpa_s_0);
	ssid->ssid = os_zalloc(sizeof(u8) * MAX_SSID_LEN);

	memcpy(ssid->ssid, params->ssid, params->ssid_length);
	ssid->ssid_len = params->ssid_length;
	ssid->disabled = 1;
	ssid->key_mgmt = WPA_KEY_MGMT_NONE;

	wpa_s_0->conf->filter_ssids = 1;
	wpa_s_0->conf->ap_scan= 1;

	if (params->psk) {
		// TODO: Extend enum wifi_security_type
		if (params->security == 3) {
			ssid->key_mgmt = WPA_KEY_MGMT_SAE;
			str_clear_free(ssid->sae_password);
			ssid->sae_password = dup_binstr(params->psk, params->psk_length);
			if (ssid->sae_password == NULL) {
				wpa_printf(MSG_ERROR, "%s:Failed to copy sae_password\n", __func__);
				return -1;
			}
		} else {
			if (params->security == 2)
				ssid->key_mgmt = WPA_KEY_MGMT_PSK_SHA256;
			else
				ssid->key_mgmt = WPA_KEY_MGMT_PSK;
			str_clear_free(ssid->passphrase);
			ssid->passphrase = dup_binstr(params->psk, params->psk_length);
			if (ssid->passphrase == NULL) {
				wpa_printf(MSG_ERROR, "%s:Failed to copy passphrase\n", __func__);
				return -1;
			}
		}

		wpa_config_update_psk(ssid);

		if (pmf)
			ssid->ieee80211w = 1;

	}

	wpa_supplicant_enable_network(wpa_s_0, ssid);
	wpa_supplicant_select_network(wpa_s_0, ssid);
	return 0;
}

static int cmd_supplicant(const struct shell *shell, size_t argc,
			    const char *argv[])
{
	return cli_main(argc, argv);
}

#ifdef notyet
static int cmd_supplicant_disconnect(const struct shell *shell, size_t argc,
				     const char *argv[])
{
	struct net_if *iface = net_if_get_default();
	int status;

	context.disconnecting = true;
	context.shell = shell;

	status = net_mgmt(NET_REQUEST_SUPP_DISCONNECT, iface, NULL, 0);

	if (status) {
		context.disconnecting = false;

		if (status == -EALREADY) {
			shell_fprintf(shell, SHELL_INFO,
				      "Already disconnected\n");
		} else {
			shell_fprintf(shell, SHELL_WARNING,
				      "Disconnect request failed\n");
			return -ENOEXEC;
		}
	} else {
		shell_fprintf(shell, SHELL_NORMAL, "Disconnect requested\n");
	}

	return 0;
}
#endif /* notyet */

static void scan_result_cb(struct net_if *iface, int status,
			   struct wifi_scan_result *entry)
{
	if (!iface) {
		return;
	}

	if (!entry){
		if (status) {
			print(context.shell, SHELL_WARNING,
			"Scan request failed (%d)\n", status);
		} else {
				print(context.shell, SHELL_NORMAL, "Scan request done\n");
		}
		return;
	}
	scan_result++;

	if (scan_result == 1U) {
		print(context.shell, SHELL_NORMAL,
		      "\n%-4s | %-32s %-5s | %-4s | %-4s | %-5s\n", "Num", "SSID",
		      "(len)", "Chan", "RSSI", "Sec");
	}

	print(context.shell, SHELL_NORMAL,
	      "%-4d | %-32s %-5u | %-4u | %-4d | %-5s\n", scan_result,
	      entry->ssid, entry->ssid_length, entry->channel, entry->rssi,
	      (entry->security == WIFI_SECURITY_TYPE_PSK ? "WPA/WPA2" :
							   "Open"));
}

static int cmd_supplicant_scan(const struct shell *shell, size_t argc,
			       const char *argv[])
{
	struct net_if *iface = net_if_get_default();
	const struct device *dev = net_if_get_device(iface);
	const struct zep_wpa_supp_dev_ops *nvlsi_dev_ops = dev->api;

	return nvlsi_dev_ops->off_api.disp_scan(dev, scan_result_cb);
}

#ifdef notyet
static int cmd_supplicant_ap_enable(const struct shell *shell, size_t argc,
				    const char *argv[])
{
	struct net_if *iface = net_if_get_default();
	static struct wifi_connect_req_params cnx_params;

	if (__wifi_args_to_params(argc - 1, &argv[1], &cnx_params)) {
		shell_help(shell);
		return -ENOEXEC;
	}

	context.shell = shell;

	if (net_mgmt(NET_REQUEST_SUPP_AP_ENABLE, iface, &cnx_params,
		     sizeof(struct wifi_connect_req_params))) {
		shell_fprintf(shell, SHELL_WARNING, "AP mode failed\n");
		return -ENOEXEC;
	} else {
		shell_fprintf(shell, SHELL_NORMAL, "AP mode enabled\n");
	}

	return 0;
}

static int cmd_supplicant_ap_disable(const struct shell *shell, size_t argc,
				     const char *argv[])
{
	struct net_if *iface = net_if_get_default();

	if (net_mgmt(NET_REQUEST_SUPP_AP_DISABLE, iface, NULL, 0)) {
		shell_fprintf(shell, SHELL_WARNING, "AP mode disable failed\n");

		return -ENOEXEC;
	} else {
		shell_fprintf(shell, SHELL_NORMAL, "AP mode disabled\n");
	}

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(supplicant_cmd_ap,
			       SHELL_CMD(enable, NULL,
					 "<SSID> <SSID length> [channel] [PSK]",
					 cmd_supplicant_ap_enable),
			       SHELL_CMD(disable, NULL,
					 "Disable Access Point mode",
					 cmd_supplicant_ap_disable),
			       SHELL_SUBCMD_SET_END);
#endif /* notyet */

SHELL_STATIC_SUBCMD_SET_CREATE(
	supplicant_commands,
	SHELL_CMD(scan, NULL, "Scan AP", cmd_supplicant_scan),
#ifdef notyet
	SHELL_CMD(scan_results, NULL, "Display Scan results", cmd_supplicant_scan_results),
#endif /* notyet */
	SHELL_CMD(connect, NULL,
		  "\"<SSID>\"\n<channel number (optional), "
		  "0 means all>\n"
		  "<PSK (optional: valid only for secured SSIDs)>",
		  cmd_supplicant_connect),
    SHELL_CMD(add_network, NULL, "\"Add Network network id/number\"", cmd_supplicant),
	SHELL_CMD(set_network, NULL, "\"Set Network params\"", cmd_supplicant),
	SHELL_CMD(set , NULL, "\"Set Global params\"", cmd_supplicant),
	SHELL_CMD(get , NULL, "\"Set Global params\"", cmd_supplicant),
	SHELL_CMD(enable_network, NULL, "\"Enable Network\"", cmd_supplicant),
	SHELL_CMD(remove_network, NULL, "\"Remove Network\"", cmd_supplicant),
	SHELL_CMD(get_network, NULL, "\"get Network\"", cmd_supplicant),
	SHELL_CMD(select_network, NULL, "\"Select Network which will be enabled; rest of the networks will be disabled\"", cmd_supplicant),
	SHELL_CMD(disable_network, NULL, "\"\"", cmd_supplicant),
	SHELL_CMD(disconnect, NULL, "\"\"", cmd_supplicant),
	SHELL_CMD(reassociate, NULL, "\"\"", cmd_supplicant),
	SHELL_CMD(status, NULL, "\"get client status\"", cmd_supplicant),
	SHELL_CMD(bssid, NULL, "\"Associate with thid BSSID\"", cmd_supplicant),
	SHELL_CMD(sta_autoconnect, NULL, "\"\"", cmd_supplicant),
	SHELL_CMD(signal_poll, NULL, "\"\"", cmd_supplicant),
#ifdef notyet
	SHELL_CMD(disconnect, NULL, "Disconnect from AP",
		  cmd_supplicant_disconnect),
	SHELL_CMD(ap, &supplicant_cmd_ap, "Access Point mode commands", NULL),
#endif /* notyet */
	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(wpa_cli, &supplicant_commands, "WPA supplicant commands",
		   NULL);

static int supplicant_shell_init(const struct device *unused)
{
	ARG_UNUSED(unused);

	context.shell = NULL;
	context.all = 0U;
	scan_result = 0U;

#ifdef notyet
	net_mgmt_init_event_callback(&supplicant_shell_mgmt_cb,
				     supplicant_mgmt_event_handler,
				     SUPPLICANT_SHELL_MGMT_EVENTS);

	net_mgmt_add_event_callback(&supplicant_shell_mgmt_cb);
#endif /* notyet */
	return 0;
}

SYS_INIT(supplicant_shell_init, APPLICATION,
	 CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
