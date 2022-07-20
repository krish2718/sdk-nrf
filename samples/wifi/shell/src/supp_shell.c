/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/* @file
 * @brief WPA supplicant shell module
 */

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
#include "zephyr_fmac_main.h"
#ifdef CONFIG_WPA_SUPP
#include "drivers/driver_zephyr.h"
#include "utils/common.h"
#include "config.h"
#include "wpa_supplicant_i.h"
#endif /* CONFIG_WPA_SUPP */

#define SUPPLICANT_SHELL_MODULE "wpa_cli"

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

int cli_main (int argc, const char **argv);
extern struct wpa_supplicant *wpa_s_0;
struct wpa_ssid *ssid_0;

#define MAX_SSID_LEN 32


static void scan_result_cb(struct net_if *iface,
			   int status,
			   struct wifi_scan_result *entry)
{
	if (!iface)
		return;

	if (!entry) {
		if (status) {
			shell_fprintf(context.shell,
				      SHELL_WARNING,
				      "Scan request failed (%d)\n",
				      status);
		} else {
			shell_fprintf(context.shell,
				      SHELL_NORMAL,
				      "Scan request done\n");
		}

		return;
	}

	scan_result++;

	if (scan_result == 1U) {
		shell_fprintf(context.shell,
			      SHELL_NORMAL,
			      "\n%-4s | %-32s %-5s | %-4s | %-4s | %-5s\n", "Num", "SSID",
			      "(len)", "Chan", "RSSI", "Sec");
	}

	shell_fprintf(context.shell,
		      SHELL_NORMAL,
		      "%-4d | %-32s %-5u | %-4u | %-4d | %-5s\n",
		      scan_result,
		      entry->ssid,
		      entry->ssid_length,
		      entry->channel,
		      entry->rssi,
		      (entry->security == WIFI_SECURITY_TYPE_PSK ? "WPA/WPA2" : "Open"));
}


static int cmd_supplicant_scan(const struct shell *shell,
			       size_t argc,
			       const char *argv[])
{
	struct net_if *iface = net_if_get_default();
	const struct device *dev = device_get_binding("wlan0");
	const struct wifi_nrf_dev_ops *dev_ops = dev->api;

	context.shell = shell;

	return dev_ops->off_api.disp_scan(dev,
					  scan_result_cb);
}


#ifdef CONFIG_WPA_SUPP
static int __wifi_args_to_params(size_t argc,
				 const char *argv[],
				 struct wifi_connect_req_params *params)
{
	char *endptr;
	int idx = 1;

	if (argc < 1)
		return -EINVAL;

	/* SSID */
	params->ssid = (char *)argv[0];
	params->ssid_length = strlen(params->ssid);

	/* PSK (optional) */
	if (idx < argc) {
		params->psk = (char *)argv[idx];
		params->psk_length = strlen(argv[idx]);
		params->security = WIFI_SECURITY_TYPE_PSK;
		idx++;

		if ((idx < argc) && (strlen(argv[idx]) <= 2)) {
			params->security = strtol(argv[idx], &endptr, 10);
		}
	} else
		params->security = WIFI_SECURITY_TYPE_NONE;

	return 0;
}


static int cmd_supplicant_connect(const struct shell *shell,
				  size_t argc,
				  const char *argv[])
{
	static struct wifi_connect_req_params cnx_params;
	struct wifi_connect_req_params *params;
	struct wpa_ssid *ssid = NULL;
	bool pmf = true;

	if (__wifi_args_to_params(argc - 1,
				  &argv[1],
				  &cnx_params)) {
		shell_help(shell);
		return -ENOEXEC;
	}

	params = &cnx_params;

	if (!wpa_s_0) {
		shell_fprintf(context.shell,
			      SHELL_ERROR,
			      "%s: wpa_supplicant is not initialized, dropping connect\n",
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
	wpa_s_0->conf->ap_scan = 1;

	if (params->psk) {
		// TODO: Extend enum wifi_security_type
		if (params->security == 3) {
			ssid->key_mgmt = WPA_KEY_MGMT_SAE;
			str_clear_free(ssid->sae_password);
			ssid->sae_password = dup_binstr(params->psk, params->psk_length);

			if (ssid->sae_password == NULL) {
				shell_fprintf(context.shell,
					      SHELL_ERROR,
					      "%s:Failed to copy sae_password\n",
					      __func__);
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
				shell_fprintf(context.shell,
					      SHELL_ERROR,
					      "%s:Failed to copy passphrase\n",
					      __func__);
				return -1;
			}
		}

		wpa_config_update_psk(ssid);

		if (pmf)
			ssid->ieee80211w = 1;

	}

	wpa_supplicant_enable_network(wpa_s_0,
				      ssid);

	wpa_supplicant_select_network(wpa_s_0,
				      ssid);

	return 0;
}


static int cmd_supplicant(const struct shell *shell,
			  size_t argc,
			  const char *argv[])
{
	return cli_main(argc,
			argv);
}

int parse_proto(const char *value)
{
	int val = 0, last, errors = 0;
	char *start, *end, *buf;

	buf = os_strdup(value);
	if (buf == NULL)
		return -1;
	start = buf;

	while (*start != '\0') {
		while (*start == ' ' || *start == '\t')
			start++;
		if (*start == '\0')
			break;
		end = start;
		while (*end != ' ' && *end != '\t' && *end != '\0')
			end++;
		last = *end == '\0';
		*end = '\0';
		if (os_strcmp(start, "WPA") == 0)
			val |= WPA_PROTO_WPA;
		else if (os_strcmp(start, "RSN") == 0 ||
			 os_strcmp(start, "WPA2") == 0)
			val |= WPA_PROTO_RSN;
		else if (os_strcmp(start, "OSEN") == 0)
			val |= WPA_PROTO_OSEN;
		else {
			shell_fprintf(context.shell,
			      SHELL_ERROR, "invalid proto '%s'",
				   start);
			errors++;
		}

		if (last)
			break;
		start = end + 1;
	}
	os_free(buf);

	return val;
}

int parse_auth_alg(const char *value)
{
	int val = 0, last, errors = 0;
	char *start, *end, *buf;

	buf = os_strdup(value);
	if (buf == NULL)
		return -1;
	start = buf;

	while (*start != '\0') {
		while (*start == ' ' || *start == '\t')
			start++;
		if (*start == '\0')
			break;
		end = start;
		while (*end != ' ' && *end != '\t' && *end != '\0')
			end++;
		last = *end == '\0';
		*end = '\0';
		if (os_strcmp(start, "OPEN") == 0)
			val |= WPA_AUTH_ALG_OPEN;
		else if (os_strcmp(start, "SHARED") == 0)
			val |= WPA_AUTH_ALG_SHARED;
		else if (os_strcmp(start, "LEAP") == 0)
			val |= WPA_AUTH_ALG_LEAP;
		else {
			shell_fprintf(context.shell,
			      SHELL_ERROR, "invalid auth_alg '%s'", start);
			errors++;
		}

		if (last)
			break;
		start = end + 1;
	}
	os_free(buf);

	return val;
}

const char *parse_bssid(const char *txt, u8 *addr)
{
	size_t i;

	for (i = 0; i < ETH_ALEN; i++) {
		int a;

		a = hex2byte(txt);
		if (a < 0)
			return NULL;
		txt += 2;
		addr[i] = a;
		if (i < ETH_ALEN - 1 && *txt++ != ':')
			return NULL;
	}
	return txt;
}

int *parse_int_array(const char *value)
{
	int *freqs;
	size_t used, len;
	const char *pos;

	used = 0;
	len = 10;
	freqs = os_calloc(len + 1, sizeof(int));
	if (freqs == NULL)
		return NULL;

	pos = value;
	while (pos) {
		while (*pos == ' ')
			pos++;
		if (used == len) {
			int *n;
			size_t i;

			n = os_realloc_array(freqs, len * 2 + 1, sizeof(int));
			if (n == NULL) {
				os_free(freqs);
				return NULL;
			}
			for (i = len; i <= len * 2; i++)
				n[i] = 0;
			freqs = n;
			len *= 2;
		}

		freqs[used] = atoi(pos);
		if (freqs[used] == 0)
			break;
		used++;
		pos = os_strchr(pos + 1, ' ');
	}

	return freqs;
}

int parse_key_mgmt(const char *value)
{
	int val = 0, last, errors = 0;
	char *start, *end, *buf;

	buf = os_strdup(value);
	if (buf == NULL)
		return -1;
	start = buf;

	while (*start != '\0') {
		while (*start == ' ' || *start == '\t')
			start++;
		if (*start == '\0')
			break;
		end = start;
		while (*end != ' ' && *end != '\t' && *end != '\0')
			end++;
		last = *end == '\0';
		*end = '\0';
		if (os_strcmp(start, "WPA-PSK") == 0)
			val |= WPA_KEY_MGMT_PSK;
		else if (os_strcmp(start, "WPA-EAP") == 0)
			val |= WPA_KEY_MGMT_IEEE8021X;
		else if (os_strcmp(start, "IEEE8021X") == 0)
			val |= WPA_KEY_MGMT_IEEE8021X_NO_WPA;
		else if (os_strcmp(start, "NONE") == 0)
			val |= WPA_KEY_MGMT_NONE;
		else if (os_strcmp(start, "WPA-NONE") == 0)
			val |= WPA_KEY_MGMT_WPA_NONE;
#ifdef CONFIG_IEEE80211R
		else if (os_strcmp(start, "FT-PSK") == 0)
			val |= WPA_KEY_MGMT_FT_PSK;
		else if (os_strcmp(start, "FT-EAP") == 0)
			val |= WPA_KEY_MGMT_FT_IEEE8021X;
#ifdef CONFIG_SHA384
		else if (os_strcmp(start, "FT-EAP-SHA384") == 0)
			val |= WPA_KEY_MGMT_FT_IEEE8021X_SHA384;
#endif /* CONFIG_SHA384 */
#endif /* CONFIG_IEEE80211R */
		else if (os_strcmp(start, "WPA-PSK-SHA256") == 0)
			val |= WPA_KEY_MGMT_PSK_SHA256;
		else if (os_strcmp(start, "WPA-EAP-SHA256") == 0)
			val |= WPA_KEY_MGMT_IEEE8021X_SHA256;
#ifdef CONFIG_WPS
		else if (os_strcmp(start, "WPS") == 0)
			val |= WPA_KEY_MGMT_WPS;
#endif /* CONFIG_WPS */
#ifdef CONFIG_SAE
		else if (os_strcmp(start, "SAE") == 0)
			val |= WPA_KEY_MGMT_SAE;
		else if (os_strcmp(start, "FT-SAE") == 0)
			val |= WPA_KEY_MGMT_FT_SAE;
#endif /* CONFIG_SAE */
#ifdef CONFIG_HS20
		else if (os_strcmp(start, "OSEN") == 0)
			val |= WPA_KEY_MGMT_OSEN;
#endif /* CONFIG_HS20 */
#ifdef CONFIG_SUITEB
		else if (os_strcmp(start, "WPA-EAP-SUITE-B") == 0)
			val |= WPA_KEY_MGMT_IEEE8021X_SUITE_B;
#endif /* CONFIG_SUITEB */
#ifdef CONFIG_SUITEB192
		else if (os_strcmp(start, "WPA-EAP-SUITE-B-192") == 0)
			val |= WPA_KEY_MGMT_IEEE8021X_SUITE_B_192;
#endif /* CONFIG_SUITEB192 */
#ifdef CONFIG_FILS
		else if (os_strcmp(start, "FILS-SHA256") == 0)
			val |= WPA_KEY_MGMT_FILS_SHA256;
		else if (os_strcmp(start, "FILS-SHA384") == 0)
			val |= WPA_KEY_MGMT_FILS_SHA384;
#ifdef CONFIG_IEEE80211R
		else if (os_strcmp(start, "FT-FILS-SHA256") == 0)
			val |= WPA_KEY_MGMT_FT_FILS_SHA256;
		else if (os_strcmp(start, "FT-FILS-SHA384") == 0)
			val |= WPA_KEY_MGMT_FT_FILS_SHA384;
#endif /* CONFIG_IEEE80211R */
#endif /* CONFIG_FILS */
#ifdef CONFIG_OWE
		else if (os_strcmp(start, "OWE") == 0)
			val |= WPA_KEY_MGMT_OWE;
#endif /* CONFIG_OWE */
#ifdef CONFIG_DPP
		else if (os_strcmp(start, "DPP") == 0)
			val |= WPA_KEY_MGMT_DPP;
#endif /* CONFIG_DPP */
		else {
			shell_fprintf(context.shell,
			      SHELL_ERROR, "invalid key_mgmt '%s'", start);
			errors++;
		}

		if (last)
			break;
		start = end + 1;
	}
	os_free(buf);

	return val;
}
static int cmd_supplicant_enable_network(const struct shell *shell, size_t argc,
			       const char *argv[])
{
	int id;

	if (argc > 2) {
		shell_fprintf(context.shell,
			      SHELL_ERROR, "%s:Incorrect usage.\n", __func__);
		return -1;
	}

	id = atoi(argv[1]);
	return zephyr_supp_enable_network(id);
}

static int cmd_supplicant_select_network(const struct shell *shell, size_t argc,
			       const char *argv[])
{
	int id;

	if (argc > 2) {
		shell_fprintf(context.shell,
			    SHELL_ERROR, "%s:Incorrect usage.\n", __func__);
		return -1;
	}

	id = atoi(argv[1]);
	return zephyr_supp_select_network(id);
}

static int cmd_supplicant_disable_network(const struct shell *shell, size_t argc,
			       const char *argv[])
{
	int id;

	if (argc > 2) {
		shell_fprintf(context.shell,
			      SHELL_ERROR, "%s:Incorrect usage.\n", __func__);
		return -1;
	}

	id = atoi(argv[1]);
	return zephyr_supp_disable_network(id);
}

static int cmd_supplicant_add_network(const struct shell *shell, size_t argc,
			       const char *argv[])
{
	if (argc > 1) {
		shell_fprintf(context.shell,
			      SHELL_ERROR, "%s:Incorrect usage.\n", __func__);
		return -1;
	}

	return zephyr_supp_add_network();
}

static int cmd_supplicant_remove_network(const struct shell *shell, size_t argc,
			       const char *argv[])
{
	const char *value;

	if (argc > 3) {
		shell_fprintf(context.shell,
			      SHELL_ERROR, "%s:Incorrect usage.\n", __func__);
		return -1;
	}

	value = argv[1];
	return zephyr_supp_remove_network(value);
}

static int cmd_supplicant_reassociate(const struct shell *shell, size_t argc,
			       const char *argv[])
{
	return zephyr_supp_reassociate();
}

static int cmd_supplicant_disconnect(const struct shell *shell, size_t argc,
			       const char *argv[])
{
	return zephyr_supp_disconnect();
}

static int cmd_supplicant_set_network(const struct shell *shell, size_t argc,
			       const char *argv[])
{
	int id;
	char *param, *value;
	char ssid[SSID_MAX_LEN] = {'\0'};
	int key_mgmt;
	int pairwise;
	int group;
	int proto;
	int auth_alg;
	uint8_t bssid[ETH_ALEN];
	int *freqs = NULL;

	printf("argc: %d\n", argc);
	if (argc < 3) {
		shell_fprintf(context.shell,
			      SHELL_ERROR, "%s:Incorrect usage.\n", __func__);
		return -1;
	}

	id = atoi(argv[1]);
	param = argv[2];
	value = argv[3];
	if (os_strcmp(param, "ssid") == 0) {
		return zephyr_supp_set_ssid(id, value);
	} else if (os_strcmp(param, "psk") == 0) {
		return zephyr_supp_set_psk(id, value);
	} else if (os_strcmp(param, "key_mgmt") == 0) {
		key_mgmt = parse_key_mgmt(value);
		return zephyr_supp_set_key_mgmt(id, value);
	} else if (os_strcmp(param, "pairwise") == 0) {
		pairwise = wpa_parse_cipher(value);
		if (pairwise == -1)
			return -1;
		return zephyr_supp_set_pairwise_cipher(id, pairwise);
	} else if (os_strcmp(param, "group") == 0) {
		group = wpa_parse_cipher(value);
		if (group == -1)
			return -1;
		return zephyr_supp_set_group_cipher(id, group);
	} else if (os_strcmp(param, "proto") == 0) {
		proto = parse_proto(value);
		return zephyr_supp_set_proto(id, proto);
	} else if (os_strcmp(param, "auth_alg") == 0) {
		auth_alg = parse_auth_alg(value);
		return zephyr_supp_set_auth_alg(id, auth_alg);
	} else if (os_strcmp(param, "scan_ssid") == 0) {
		return zephyr_supp_set_scan_ssid(id, atoi(value));
	} else if (os_strcmp(param, "bssid") == 0) {
		if (parse_bssid(value, bssid)) {
			return zephyr_supp_set_bssid(id, value);
		} else {
			shell_fprintf(context.shell,
			      SHELL_ERROR, "Invalid BSSID '%s'.", value);
			return -1;
		}
	} else if (os_strcmp(param, "scan_freq") == 0) {
		freqs = parse_int_array(value);
		if (freqs == NULL)
			return -1;
		if (freqs[0] == 0) {
			os_free(freqs);
			freqs = NULL;
			return -1;
		}
		return zephyr_supp_set_scan_freq(id, freqs);
	} else if (os_strcmp(param, "ieee80211w") == 0) {
		return zephyr_supp_set_ieee80211w(id, atoi(value));
	}

	return 0;
}

static int cmd_supplicant_set(const struct shell *shell, size_t argc,
			       const char *argv[])
{
	const char *param, *value;
	char ssid[SSID_MAX_LEN] = {'\0'};

	printf("argc: %d\n", argc);
	if (argc < 3) {
		shell_fprintf(context.shell,
			      SHELL_ERROR, "%s:Incorrect usage.\n", __func__);
		return -1;
	}

	param = argv[1];
	value = argv[2];

	if (os_strcmp(param, "country") == 0) {
		return zephyr_supp_set_country(value);
	} else if (os_strcmp(param, "pmf") == 0) {
		return zephyr_supp_set_pmf(value);
	}
	return 0;
}

static int cmd_supplicant_sta_autoconnet(const struct shell *shell, size_t argc,
			       const char *argv[])
{
	int value;

	value = atoi(argv[1]);
	return zephyr_supp_sta_autoconnect(value);
}

static int cmd_supplicant_signal_poll(const struct shell *shell, size_t argc,
			       const char *argv[])
{
	return zephyr_supp_signal_poll();
}

static int cmd_supplicant_status(const struct shell *shell, size_t argc,
			       const char *argv[])
{
	char *status_buf = NULL;
	int buflen = 4096;

	status_buf = os_zalloc(sizeof(char) * buflen);

	if (!status_buf){
		printf("Failed to allocate memory\n");
		return -1;
	}

	zephyr_supp_status(status_buf, buflen);
	printf("STATUS: %s\n", status_buf);

	os_free(status_buf);

	return 0;
}

#endif /* CONFIG_WPA_SUPP */

SHELL_STATIC_SUBCMD_SET_CREATE(
	wpa_cli_cmds,
	SHELL_CMD(scan,
		  NULL,
		  "Scan AP",
		  cmd_supplicant_scan),
#ifdef CONFIG_WPA_SUPP
	SHELL_CMD(connect,
		  NULL,
		  " \"<SSID>\""
		  "\nPassphrase (optional: valid only for secured SSIDs)>"
		  "\nKEY_MGMT (optional: 0-None, 1-WPA2, 2-WPA2-256, 3-WPA3)",
		  cmd_supplicant_connect),
	SHELL_CMD(add_network,
		  NULL,
		  "\"Add Network network id/number\"",
		  cmd_supplicant_add_network),
	SHELL_CMD(set_network,
		  NULL,
		  "\"Set Network params\"",
		  cmd_supplicant_set_network),
	SHELL_CMD(set,
		  NULL,
		  "\"Set Global params\"",
		  cmd_supplicant_set),
	SHELL_CMD(get,
		  NULL, "\"Get Global params\"",
		  cmd_supplicant),
	SHELL_CMD(enable_network,
		  NULL,
		  "\"Enable Network\"",
		  cmd_supplicant_enable_network),
	SHELL_CMD(remove_network,
		  NULL,
		  "\"Remove Network\"",
		  cmd_supplicant_remove_network),
	SHELL_CMD(get_network,
		  NULL,
		  "\"Get Network\"",
		  cmd_supplicant),
	SHELL_CMD(select_network,
		  NULL,
		  "\"Select Network which will be enabled; rest of the networks will be disabled\"",
		  cmd_supplicant_select_network),
	SHELL_CMD(disable_network,
		  NULL,
		  "\"\"",
		  cmd_supplicant_disable_network),
	SHELL_CMD(disconnect,
		  NULL,
		  "\"\"",
		  cmd_supplicant_disconnect),
	SHELL_CMD(reassociate,
		  NULL,
		  "\"\"",
		  cmd_supplicant_reassociate),
	SHELL_CMD(status,
		  NULL,
		  "\"Get client status\"",
		  cmd_supplicant_status),
	SHELL_CMD(bssid,
		  NULL,
		  "\"Associate with this BSSID\"",
		  cmd_supplicant),
	SHELL_CMD(sta_autoconnect,
		  NULL,
		  "\"\"",
		  cmd_supplicant_sta_autoconnet),
	SHELL_CMD(signal_poll,
		  NULL,
		  "\"\"",
		  cmd_supplicant_signal_poll),
#endif /* CONFIG_WPA_SUPP */
	SHELL_SUBCMD_SET_END);


SHELL_CMD_REGISTER(wpa_cli,
		   &wpa_cli_cmds,
		   "WPA supplicant commands",
		   NULL);


static int supplicant_shell_init(const struct device *unused)
{
	ARG_UNUSED(unused);

	context.shell = NULL;
	context.all = 0U;
	scan_result = 0U;

	return 0;
}


SYS_INIT(supplicant_shell_init,
	 APPLICATION,
	 CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
