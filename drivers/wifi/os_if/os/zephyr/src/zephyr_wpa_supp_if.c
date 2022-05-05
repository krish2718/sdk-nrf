/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @brief File containing WPA supplicant interface specific definitions for the
 * Zephyr OS layer of the Wi-Fi driver.
 */

#include <stdlib.h>
#include <device.h>
#include "zephyr_fmac_main.h"
#include "zephyr_wpa_supp_if.h"
#include "wpa_supplicant_i.h"
#include "bss.h"

/* TODO: Move this to driver_zephyr.c */
extern struct wpa_supplicant *wpa_s_0;

static int get_wifi_nrf_auth_type(int wpa_auth_alg)
{
	if (wpa_auth_alg & WPA_AUTH_ALG_OPEN)
		return IMG_AUTHTYPE_OPEN_SYSTEM;
	if (wpa_auth_alg & WPA_AUTH_ALG_SHARED)
		return IMG_AUTHTYPE_SHARED_KEY;
	if (wpa_auth_alg & WPA_AUTH_ALG_LEAP)
		return IMG_AUTHTYPE_NETWORK_EAP;
	if (wpa_auth_alg & WPA_AUTH_ALG_FT)
		return IMG_AUTHTYPE_FT;
	if (wpa_auth_alg & WPA_AUTH_ALG_SAE)
		return IMG_AUTHTYPE_SAE;

	return IMG_AUTHTYPE_MAX;
}


static unsigned int wpa_alg_to_cipher_suite(enum wpa_alg alg, size_t key_len)
{
	switch (alg) {
	case WPA_ALG_WEP:
		if (key_len == 5)
			return RSN_CIPHER_SUITE_WEP40;
		return RSN_CIPHER_SUITE_WEP104;
	case WPA_ALG_TKIP:
		return RSN_CIPHER_SUITE_TKIP;
	case WPA_ALG_CCMP:
		return RSN_CIPHER_SUITE_CCMP;
	case WPA_ALG_GCMP:
		return RSN_CIPHER_SUITE_GCMP;
	case WPA_ALG_CCMP_256:
		return RSN_CIPHER_SUITE_CCMP_256;
	case WPA_ALG_GCMP_256:
		return RSN_CIPHER_SUITE_GCMP_256;
	case WPA_ALG_BIP_CMAC_128:
		return RSN_CIPHER_SUITE_AES_128_CMAC;
	case WPA_ALG_BIP_GMAC_128:
		return RSN_CIPHER_SUITE_BIP_GMAC_128;
	case WPA_ALG_BIP_GMAC_256:
		return RSN_CIPHER_SUITE_BIP_GMAC_256;
	case WPA_ALG_BIP_CMAC_256:
		return RSN_CIPHER_SUITE_BIP_CMAC_256;
	case WPA_ALG_SMS4:
		return RSN_CIPHER_SUITE_SMS4;
	case WPA_ALG_KRK:
		return RSN_CIPHER_SUITE_KRK;
	case WPA_ALG_NONE:
		printk("%s: Unexpected encryption algorithm %d",
		       __func__,
		       alg);
		return 0;
	}

	printk("%s: Unsupported encryption algorithm %d",
	       __func__,
	       alg);
	return 0;
}

void wifi_nrf_wpa_supp_event_proc_scan_start(void *if_priv)
{
	struct wifi_nrf_vif_ctx_zep *vif_ctx_zep = NULL;

	vif_ctx_zep = if_priv;

	if (vif_ctx_zep->supp_callbk_fns.scan_start)
		vif_ctx_zep->supp_callbk_fns.scan_start(vif_ctx_zep->supp_drv_if_ctx);

	/* TODO: Fix this when FW starts sending SCAN_DONE before scan results */
	wpa_supp_event_handler(vif_ctx_zep,
			       NULL,
			       wifi_nrf_wpa_supp_event_proc_scan_done);
}


void wifi_nrf_wpa_supp_event_proc_scan_done(void *if_priv,
					 struct img_umac_event_trigger_scan *scan_done_event,
					 int aborted)
{
	struct wifi_nrf_vif_ctx_zep *vif_ctx_zep = NULL;
	union wpa_event_data *event = NULL;
	struct scan_info *info = NULL;
#ifdef notyet
	struct wpa_driver_scan_ssid *s = NULL;
#define MAX_REPORT_FREQS 50
	int freqs[MAX_REPORT_FREQS];
	int num_freqs = 0;
	int i = 0;
#endif /* notyet */

	/* TODO: Fix this when FW starts sending SCAN_DONE before scan results */
	if (scan_done_event)
		return;

	vif_ctx_zep = if_priv;

	event = k_calloc(sizeof(*event), sizeof(char));

	if (!event) {
		printk("%s: Unable to allocate memory for the wpa_event_data instance\n",
		       __func__);
		return;
	}

	info = &event->scan_info;

	info->aborted = aborted;
	info->external_scan = 0;
	info->nl_scan_event = 1;
#ifdef notyet
	for (i = 0; i < scan_done_event->num_scan_ssid; i++) {
		s = &info->ssids[info->num_ssids];

		memcpy((unsigned char *)s->ssid,
		       scan_done_event->scan_ssid[i].img_ssid,
		       scan_done_event->scan_ssid[i].img_ssid_len);

		s->ssid_len = scan_done_event->scan_ssid[i].img_ssid_len;

		info->num_ssids++;

		if (info->num_ssids == WPAS_MAX_SCAN_SSIDS)
			break;
	}

	for (i = 0; i < scan_done_event->num_scan_frequencies; i++) {
		freqs[num_freqs] = scan_done_event->scan_frequencies[i];

		num_freqs++;

		if (num_freqs == (MAX_REPORT_FREQS - 1))
			break;
	}

	/* TODO: Check why scan frequencies are not getting updated */
	info->freqs = freqs;
	info->num_freqs = scan_done_event->num_scan_frequencies;

#endif /* notyet */
	vif_ctx_zep->supp_callbk_fns.scan_done(vif_ctx_zep->supp_drv_if_ctx,
					       event);
}


void wifi_nrf_wpa_supp_event_proc_scan_res(void *if_priv,
					struct img_umac_event_new_scan_results *scan_res,
					bool more_res)
{
	struct wifi_nrf_vif_ctx_zep *vif_ctx_zep = NULL;
	struct wpa_scan_res *r = NULL;
	const unsigned char *ie = NULL;
	const unsigned char *beacon_ie = NULL;
	unsigned int ie_len = 0;
	unsigned int beacon_ie_len = 0;
	unsigned char *pos = NULL;

	vif_ctx_zep = if_priv;

	if (scan_res->valid_fields & IMG_EVENT_NEW_SCAN_RESULTS_IES_VALID) {
		ie = scan_res->ies.ie;
		ie_len = scan_res->ies.ie_len;
	}

	if (scan_res->valid_fields & IMG_EVENT_NEW_SCAN_RESULTS_BEACON_IES_VALID) {
		beacon_ie = scan_res->beacon_ies.ie;
		beacon_ie_len = scan_res->beacon_ies.ie_len;
	}

#ifdef notyet
	/* TODO: See what this is needed for */
	if (nl80211_scan_filtered(drv, ie ? ie : beacon_ie,
				  ie ? ie_len : beacon_ie_len))
		goto out;
#endif

	r = k_calloc(sizeof(*r) + ie_len + beacon_ie_len, sizeof(char));

	if (!r) {
		printk("%s: Unable to allocate memory for scan result\n", __func__);
		return;
	}

	if (scan_res->valid_fields & IMG_EVENT_NEW_SCAN_RESULTS_MAC_ADDR_VALID) {
		memcpy(r->bssid,
		       scan_res->mac_addr,
		       ETH_ALEN);
	}

	r->freq = scan_res->frequency;

	if (scan_res->valid_fields & IMG_EVENT_NEW_SCAN_RESULTS_BEACON_INTERVAL_VALID)
		r->beacon_int = scan_res->beacon_interval;

	r->caps = scan_res->capability;

	r->flags |= WPA_SCAN_NOISE_INVALID;

	if (scan_res->signal.signal_type == IMG_SIGNAL_TYPE_MBM) {
		r->level = scan_res->signal.signal.mbm_signal;
		r->level /= 100; /* mBm to dBm */
		r->flags |= (WPA_SCAN_LEVEL_DBM | WPA_SCAN_QUAL_INVALID);
	} else if (scan_res->signal.signal_type == IMG_SIGNAL_TYPE_UNSPEC) {
		r->level = scan_res->signal.signal.unspec_signal;
		r->flags |= WPA_SCAN_QUAL_INVALID;
	} else
		r->flags |= (WPA_SCAN_LEVEL_INVALID | WPA_SCAN_QUAL_INVALID);

	if (scan_res->valid_fields & IMG_EVENT_NEW_SCAN_RESULTS_IES_TSF_VALID)
		r->tsf = scan_res->ies_tsf;

	if (scan_res->valid_fields & IMG_EVENT_NEW_SCAN_RESULTS_BEACON_IES_TSF_VALID) {
		if (scan_res->beacon_ies_tsf > r->tsf)
			r->tsf = scan_res->beacon_ies_tsf;
	}

	if (scan_res->seen_ms_ago)
		r->age = scan_res->seen_ms_ago;

	r->ie_len = ie_len;

	pos = (unsigned char *) (r + 1);

	if (ie) {
		memcpy(pos,
		       ie,
		       ie_len);

		pos += ie_len;
	}

	r->beacon_ie_len = beacon_ie_len;

	if (beacon_ie)
		memcpy(pos,
		       beacon_ie,
		       beacon_ie_len);

	if (scan_res->valid_fields & IMG_EVENT_NEW_SCAN_RESULTS_STATUS_VALID) {
		switch (scan_res->status) {
		case IMG_BSS_STATUS_ASSOCIATED:
			r->flags |= WPA_SCAN_ASSOCIATED;
			break;
		default:
			break;
		}
	}

	vif_ctx_zep->supp_callbk_fns.scan_res(vif_ctx_zep->supp_drv_if_ctx,
					      r,
					      more_res);
}


void wifi_nrf_wpa_supp_event_proc_auth_resp(void *if_priv,
					 struct img_umac_event_mlme *auth_resp)
{
	struct wifi_nrf_vif_ctx_zep *vif_ctx_zep = NULL;
	union wpa_event_data event;
	const struct ieee80211_mgmt *mgmt = NULL;
	const unsigned char *frame = NULL;
	unsigned int frame_len = 0;

	vif_ctx_zep = if_priv;

	frame = auth_resp->frame.frame;
	frame_len = auth_resp->frame.frame_len;
	mgmt = (const struct ieee80211_mgmt *)frame;

	if (frame_len < 4 + (2 * IMG_ETH_ALEN)) {
		printk("%s: MLME event too short\n", __func__);
		return;
	}

#ifdef notyet
	if (!(frame[4] & 0x01) &&
	    memcmp(bss->addr, recv_data + 4, ETH_ALEN) != 0 &&
	    (is_zero_ether_addr(bss->rand_addr) ||
	     os_memcmp(bss->rand_addr, recv_data + 4, ETH_ALEN) != 0) &&
	    os_memcmp(bss->addr, recv_data + 4 + ETH_ALEN, ETH_ALEN) != 0) {
		wpa_printf(MSG_MSGDUMP, "nl80211: %s: Ignore MLME frame event for foreign address",
			   bss->ifname);
		return;
	}
#endif /* notyet */

	if (frame_len < 24 + sizeof(mgmt->u.auth)) {
		printk("%s: Authentication response frame too short\n", __func__);
		return;
	}

	memset(&event,
	       0,
	       sizeof(event));

	memcpy(event.auth.peer,
	       mgmt->sa,
	       ETH_ALEN);

	event.auth.auth_type = le_to_host16(mgmt->u.auth.auth_alg);

	event.auth.auth_transaction = le_to_host16(mgmt->u.auth.auth_transaction);

	event.auth.status_code = le_to_host16(mgmt->u.auth.status_code);

	if (frame_len > 24 + sizeof(mgmt->u.auth)) {
		event.auth.ies = mgmt->u.auth.variable;
		event.auth.ies_len = (frame_len - 24 - sizeof(mgmt->u.auth));
	}

	vif_ctx_zep->supp_callbk_fns.auth_resp(vif_ctx_zep->supp_drv_if_ctx,
					       &event);
}


void wifi_nrf_wpa_supp_event_proc_assoc_resp(void *if_priv,
					  struct img_umac_event_mlme *assoc_resp)
{
	struct wifi_nrf_vif_ctx_zep *vif_ctx_zep = NULL;
	union wpa_event_data event;
	const struct ieee80211_mgmt *mgmt = NULL;
	const unsigned char *frame = NULL;
	unsigned int frame_len = 0;
	unsigned short status = WLAN_STATUS_UNSPECIFIED_FAILURE;

	vif_ctx_zep = if_priv;

	frame = assoc_resp->frame.frame;
	frame_len = assoc_resp->frame.frame_len;
	mgmt = (const struct ieee80211_mgmt *)frame;

	if (frame_len < 24 + sizeof(mgmt->u.assoc_resp)) {
		printk("%s: Association response frame too short\n", __func__);
		return;
	}

	memset(&event,
	       0,
	       sizeof(event));

	status = le_to_host16(mgmt->u.assoc_resp.status_code);

	if (status != WLAN_STATUS_SUCCESS) {
		event.assoc_reject.bssid = mgmt->bssid;

		if (frame_len > 24 + sizeof(mgmt->u.assoc_resp)) {
			event.assoc_reject.resp_ies = (unsigned char *)mgmt->u.assoc_resp.variable;
			event.assoc_reject.resp_ies_len = (frame_len - 24 - sizeof(mgmt->u.assoc_resp));
		}

		event.assoc_reject.status_code = status;
	} else {
		event.assoc_info.addr = mgmt->bssid;
		event.assoc_info.resp_frame = frame;
		event.assoc_info.resp_frame_len = frame_len;

		if (frame_len > 24 + sizeof(mgmt->u.assoc_resp)) {
			event.assoc_info.resp_ies = (unsigned char *)mgmt->u.assoc_resp.variable;
			event.assoc_info.resp_ies_len = (frame_len - 24 - sizeof(mgmt->u.assoc_resp));
		}

#ifdef notyet
		/* TODO: See what is needed here */
		if (req_ie) {
			event.assoc_info.req_ies = nla_data(req_ie);
			event.assoc_info.req_ies_len = nla_len(req_ie);
		}

		event.assoc_info.freq = drv->assoc_freq;
		nl80211_parse_wmm_params(wmm, &event.assoc_info.wmm_params);
#endif /* notyet */
	}

	vif_ctx_zep->supp_callbk_fns.assoc_resp(vif_ctx_zep->supp_drv_if_ctx,
						&event,
						status);
}


void wifi_nrf_wpa_supp_event_proc_deauth(void *if_priv,
				      struct img_umac_event_mlme *deauth)
{
	/* TODO */
	printk("%s: To be implemented\n", __func__);
}


void wifi_nrf_wpa_supp_event_proc_disassoc(void *if_priv,
					struct img_umac_event_mlme *disassoc)
{
	/* TODO */
	printk("%s: To be implemented\n", __func__);
}


void *wifi_nrf_wpa_supp_dev_init(void *supp_drv_if_ctx,
			      const char *iface_name,
			      struct zep_wpa_supp_dev_callbk_fns *supp_callbk_fns)
{
	const struct device *dev = NULL;
	struct wifi_nrf_vif_ctx_zep *vif_ctx_zep = NULL;

	/* TODO: Need to figure out what needs to be done here */
#ifdef notyet
	dev = DEVICE_GET(iface_name);
#endif /* notyet */

	dev = net_if_get_device(net_if_get_default());
	vif_ctx_zep = dev->data;

	vif_ctx_zep->supp_drv_if_ctx = supp_drv_if_ctx;

	memcpy(&vif_ctx_zep->supp_callbk_fns,
	       supp_callbk_fns,
	       sizeof(vif_ctx_zep->supp_callbk_fns));

	return vif_ctx_zep;
}


void wifi_nrf_wpa_supp_dev_deinit(void *if_priv)
{
	struct wifi_nrf_vif_ctx_zep *vif_ctx_zep = NULL;

	vif_ctx_zep = if_priv;

	vif_ctx_zep->supp_drv_if_ctx = NULL;
}


int wifi_nrf_wpa_supp_scan2(void *if_priv,
			 struct wpa_driver_scan_params *params)
{
	enum wifi_nrf_status status = NVLSI_RPU_STATUS_FAIL;
	struct wifi_nrf_vif_ctx_zep *vif_ctx_zep = NULL;
	struct wifi_nrf_ctx_zep *rpu_ctx_zep = NULL;
	struct img_umac_scan_info scan_info;
	int indx = 0;
#ifdef notyet
	unsigned int scan_flags = 0;
#endif /* notyet */
	int ret = -1;

	if (!if_priv || !params) {
		printk("%s: Invalid params\n", __func__);
		goto out;
	}

	vif_ctx_zep = if_priv;
	rpu_ctx_zep = vif_ctx_zep->rpu_ctx_zep;

	memset(&scan_info,
	       0x0,
	       sizeof(scan_info));
#ifdef notyet
	if (params->freqs) {
		for (indx = 0; params->freqs[indx]; indx++) {
			scan_info.scan_params.channels[indx].center_frequency =
				params->freqs[indx];
		}

		scan_info.scan_params.num_scan_channels = indx;
	}

	if (params->duration)
		scan_info.scan_params.oper_ch_duration = params->duration;

	if (params->num_ssids) {
		scan_info.scan_params.num_scan_ssids = params->num_ssids;

		for (indx = 0; indx < params->num_ssids; indx++) {
			memcpy(scan_info.scan_params.scan_ssids[indx].img_ssid,
			       params->ssids[indx].ssid,
			       params->ssids[indx].ssid_len);

			scan_info.scan_params.scan_ssids[indx].img_ssid_len =
				params->ssids[indx].ssid_len;
		}
	}
#endif /* notyet */

	if (params->filter_ssids) {
		scan_info.scan_params.num_scan_ssids = params->num_filter_ssids;

		for (indx = 0; indx < params->num_filter_ssids; indx++) {
			memcpy(scan_info.scan_params.scan_ssids[indx].img_ssid,
			       params->filter_ssids[indx].ssid,
			       params->filter_ssids[indx].ssid_len);

			scan_info.scan_params.scan_ssids[indx].img_ssid_len =
				params->filter_ssids[indx].ssid_len;
		}

	}

#ifdef notyet
	if (params->only_new_results)
		/* The value to be set for scan_flags should be exposed as an enum by the UMAC shared header files (CAL-2149) */
		scan_flags |= IMG_SCAN_FLAG_FLUSH;

	if (params->low_priority && drv->have_low_prio_scan)
		scan_flags |= IMG_SCAN_FLAG_LOW_PRIORITY;

	if (params->oce_scan)
		scan_flags |= IMG_SCAN_FLAG_FILS_MAX_CHANNEL_TIME |
			      IMG_SCAN_FLAG_ACCEPT_BCAST_PROBE_RESP |
			      IMG_SCAN_FLAG_OCE_PROBE_REQ_HIGH_TX_RATE |
			      IMG_SCAN_FLAG_OCE_PROBE_REQ_DEFERRAL_SUPPRESSION;

	/* TODO: no_cck is set to 1 , put proper macro */
	if (params->p2p_probe)
		scan_info.scan_params.no_cck = 1;

	if (params->mac_addr_rand) {
		scan_flags |= IMG_SCAN_FLAG_RANDOM_ADDR;

		memcpy(scan_info.scan_params.mac_addr,
		       params->mac_addr,
		       ETH_ALEN);

		scan_info.scan_params.valid_fields |=
			IMG_SCAN_PARAMS_MAC_ADDR_VALID;

		memcpy(scan_info.scan_params.mac_addr_mask,
		       params->mac_addr_mask,
		       ETH_ALEN);

		scan_info.scan_params.valid_fields |=
			IMG_SCAN_PARAMS_MAC_ADDR_MASK_VALID;
	}

	if (scan_flags) {
		scan_info.scan_params.scan_flags = scan_flags;
		scan_info.scan_params.valid_fields |=
			IMG_SCAN_PARAMS_SCAN_FLAGS_VALID;
	}

	if (params->extra_ies) {
		scan_info.scan_params.ie.ie_len = params->extra_ies_len;

		memcpy(&scan_info.scan_params.ie.ie,
		       params->extra_ies,
		       params->extra_ies_len);
	}
#endif /* notyet */

	scan_info.scan_mode = 0;
	scan_info.scan_reason = SCAN_CONNECT;

	status = wifi_nrf_fmac_scan(rpu_ctx_zep->rpu_ctx,
				      vif_ctx_zep->vif_idx,
				      &scan_info);

	if (status != NVLSI_RPU_STATUS_SUCCESS) {
		printk("%s: Scan trigger failed\n", __func__);
		goto out;
	}

	vif_ctx_zep->scan_type = SCAN_CONNECT;
	vif_ctx_zep->scan_in_progress = true;

	ret = 0;
out:
	return ret;
}


int wifi_nrf_wpa_supp_scan_abort(void *if_priv)
{
	struct wifi_nrf_vif_ctx_zep *vif_ctx_zep = NULL;

	vif_ctx_zep = if_priv;

	/* TODO: No support in UMAC for scan abort yet */
	printk("%s: Scan abort not supported yet\n", __func__);

	return 0;

}


int wifi_nrf_wpa_supp_scan_results_get(void *if_priv)
{
	enum wifi_nrf_status status = NVLSI_RPU_STATUS_FAIL;
	struct wifi_nrf_vif_ctx_zep *vif_ctx_zep = NULL;
	struct wifi_nrf_ctx_zep *rpu_ctx_zep = NULL;
	int ret = -1;

	if (!if_priv) {
		printk("%s: Invalid params\n", __func__);
		goto out;
	}

	vif_ctx_zep = if_priv;
	rpu_ctx_zep = vif_ctx_zep->rpu_ctx_zep;

	status = wifi_nrf_fmac_scan_res_get(rpu_ctx_zep->rpu_ctx,
					      vif_ctx_zep->vif_idx,
					      SCAN_CONNECT);

	if (status != NVLSI_RPU_STATUS_SUCCESS) {
		printk("%s: wifi_nrf_fmac_scan_res_get failed\n", __func__);
		goto out;
	}

	ret = 0;
out:
	return ret;
}


int wifi_nrf_wpa_supp_deauthenticate(void *if_priv,
				  const char *addr,
				  unsigned short reason_code)
{
	enum wifi_nrf_status status = NVLSI_RPU_STATUS_FAIL;
	struct wifi_nrf_vif_ctx_zep *vif_ctx_zep = NULL;
	struct wifi_nrf_ctx_zep *rpu_ctx_zep = NULL;
	struct img_umac_disconn_info deauth_info;
	int ret = -1;

	if ((!if_priv) || (!addr)) {
		printk("%s: Invalid params\n", __func__);
		goto out;
	}

	vif_ctx_zep = if_priv;
	rpu_ctx_zep = vif_ctx_zep->rpu_ctx_zep;

	memset(&deauth_info,
	       0,
	       sizeof(deauth_info));

	deauth_info.reason_code = reason_code;

	memcpy(deauth_info.mac_addr,
	       addr,
	       sizeof(deauth_info.mac_addr));

	status = wifi_nrf_fmac_deauth(rpu_ctx_zep->rpu_ctx,
					vif_ctx_zep->vif_idx,
					&deauth_info);

	if (status != NVLSI_RPU_STATUS_SUCCESS) {
		printk("%s: wifi_nrf_fmac_scan_res_get failed\n", __func__);
		goto out;
	}

	ret = 0;
out:
	return ret;
}


int wifi_nrf_wpa_supp_add_key(struct img_umac_key_info *key_info,
			   enum wpa_alg alg,
			   int key_idx,
			   int defkey,
			   const unsigned char *seq,
			   size_t seq_len,
			   const unsigned char *key,
			   size_t key_len)
{
	unsigned int suite = 0;

	suite = wpa_alg_to_cipher_suite(alg, key_len);

	if (!suite)
		return -1;

	if (defkey && alg == WPA_ALG_BIP_CMAC_128)
		key_info->img_flags = IMG_KEY_DEFAULT_MGMT;
	else if (defkey)
		key_info->img_flags = IMG_KEY_DEFAULT;

	key_info->key_idx = key_idx;
	key_info->cipher_suite = suite;

	if (key && key_len) {
		memcpy(key_info->key.img_key,
		       key,
		       key_len);
		key_info->key.img_key_len = key_len;
	}
	if (seq && seq_len) {
		memcpy(key_info->seq.img_seq,
		       seq,
		       seq_len);
		key_info->seq.img_seq_len = seq_len;
	}

	return 0;
}


int wifi_nrf_wpa_supp_authenticate(void *if_priv,
				struct wpa_driver_auth_params *params)
{
	enum wifi_nrf_status status = NVLSI_RPU_STATUS_FAIL;
	struct wpa_bss *curr_bss = NULL;
	struct wifi_nrf_vif_ctx_zep *vif_ctx_zep = NULL;
	struct wifi_nrf_ctx_zep *rpu_ctx_zep = NULL;
	struct img_umac_auth_info auth_info;
	int ret = -1;
#ifdef notyet
	int i = 0;
#endif /* notyet */
	int type;
	int count = 0;

	if ((!if_priv) || (!params)) {
		printk("%s: Invalid params\n", __func__);
		goto out;
	}

	vif_ctx_zep = if_priv;
	rpu_ctx_zep = vif_ctx_zep->rpu_ctx_zep;

	memset(&auth_info,
	       0,
	       sizeof(auth_info));

#ifdef notyet
	for (i = 0; i < 4; i++) {
		if (!params->wep_key[i])
			continue;

		wifi_nrf_wpa_supp_set_key(vif_ctx_zep,
				       NULL,
				       WPA_ALG_WEP,
				       NULL,
				       i,
				       i == params->wep_tx_keyidx,
				       NULL,
				       0,
				       params->wep_key[i],
				       params->wep_key_len[i]);

		if (params->wep_tx_keyidx != i)
			continue;

		if (wifi_nrf_wpa_supp_add_key(&auth_info.key_info,
					   WPA_ALG_WEP,
					   i,
					   1,
					   NULL,
					   0,
					   params->wep_key[i],
					   params->wep_key_len[i]) < 0)
			goto out;
	}
#endif /* notyet */

	if (params->bssid) {
		memcpy(auth_info.img_bssid,
		       params->bssid,
		       ETH_ALEN);
	}

	if (params->freq)
		auth_info.frequency = params->freq;

	if (params->ssid) {
		memcpy(auth_info.ssid.img_ssid,
		       params->ssid,
		       params->ssid_len);

		auth_info.ssid.img_ssid_len = params->ssid_len;
	}

	if (params->ie) {
		memcpy(auth_info.bss_ie.ie,
		       params->ie,
		       params->ie_len);
	} else {
		/* TODO: Move this to driver_zephyr.c */
		/* TODO: See why the ie info is not being passed as part of
		 * wpa_driver_auth_params
		 */
		curr_bss = wpa_bss_get(wpa_s_0,
				       params->bssid,
				       params->ssid,
				       params->ssid_len);

		memcpy(auth_info.bss_ie.ie,
		       (const unsigned char *)(curr_bss + 1),
		       IMG_MAX_IE_LEN);

		auth_info.bss_ie.ie_len = curr_bss->ie_len;
		auth_info.scan_width = 0; /* hard coded */
		auth_info.img_signal = curr_bss->level;
		auth_info.capability = curr_bss->caps;
		auth_info.beacon_interval = curr_bss->beacon_int;
		auth_info.tsf = curr_bss->tsf;
		auth_info.from_beacon = 0; /* hard coded */
	}

	if (params->auth_data) {
		auth_info.sae.sae_data_len = params->auth_data_len;

		memcpy(auth_info.sae.sae_data,
		       params->auth_data,
		       params->auth_data_len);
	}

	type = get_wifi_nrf_auth_type(params->auth_alg);

	if (type != IMG_AUTHTYPE_MAX)
		auth_info.auth_type = type;

	if (params->local_state_change)
		auth_info.img_flags |= IMG_CMD_AUTHENTICATE_LOCAL_STATE_CHANGE;

	status = wifi_nrf_fmac_auth(rpu_ctx_zep->rpu_ctx,
				      vif_ctx_zep->vif_idx,
				      &auth_info);

	if (status != NVLSI_RPU_STATUS_SUCCESS) {
		ret = -1;

		printk("%s: MLME command failed (auth): count=%d ret=%d\n",
		       __func__,
		       count,
		       ret);

		count++;
	} else {
		printk("%s:Authentication request sent successfully\n",
		       __func__);
		ret = 0;
	}
out:
	return ret;
}


int wifi_nrf_wpa_supp_associate(void *if_priv,
			     struct wpa_driver_associate_params *params)
{
	enum wifi_nrf_status status = NVLSI_RPU_STATUS_FAIL;
	struct wifi_nrf_vif_ctx_zep *vif_ctx_zep = NULL;
	struct wifi_nrf_ctx_zep *rpu_ctx_zep = NULL;
	struct img_umac_assoc_info assoc_info;
	int ret = -1;

	if ((!if_priv) || (!params)) {
		printk("%s: Invalid params\n", __func__);
		goto out;
	}

	vif_ctx_zep = if_priv;
	rpu_ctx_zep = vif_ctx_zep->rpu_ctx_zep;

	memset(&assoc_info,
	       0,
	       sizeof(assoc_info));

	if (params->bssid)
		memcpy(assoc_info.img_bssid,
		       params->bssid,
		       sizeof(assoc_info.img_bssid));

	if (params->freq.freq) {
		assoc_info.center_frequency = params->freq.freq;
		vif_ctx_zep->assoc_freq = params->freq.freq;
	} else
		vif_ctx_zep->assoc_freq = 0;

	if (params->ssid) {
		assoc_info.ssid.img_ssid_len = params->ssid_len;

		memcpy(assoc_info.ssid.img_ssid,
		       params->ssid,
		       params->ssid_len);

#ifdef notyet
		if (params->ssid_len > sizeof(vif_ctx_zep->ssid))
			goto out;

		memcpy(vif_ctx_zep->ssid,
		       params->ssid,
		       params->ssid_len);

		vif_ctx_zep->ssid_len = params->ssid_len;
#endif /* notyet */
	}

	if (params->wpa_ie) {
		assoc_info.wpa_ie.ie_len = params->wpa_ie_len;
		memcpy(assoc_info.wpa_ie.ie,
		       params->wpa_ie,
		       params->wpa_ie_len);
	}

	assoc_info.control_port = 1;

	if (params->mgmt_frame_protection == MGMT_FRAME_PROTECTION_REQUIRED)
		assoc_info.use_mfp = IMG_MFP_REQUIRED;

	status = wifi_nrf_fmac_assoc(rpu_ctx_zep->rpu_ctx,
				       vif_ctx_zep->vif_idx,
				       &assoc_info);

		if (status != NVLSI_RPU_STATUS_SUCCESS) {
			printk("%s: MLME command failed (assoc)\n",
			       __func__);
		} else {
			printk("%s: Association request sent successfully\n",
			       __func__);
			ret = 0;
		}

out:
	return ret;
}


int wifi_nrf_wpa_supp_set_key(void *if_priv,
			   const unsigned char *ifname,
			   enum wpa_alg alg,
			   const unsigned char *addr,
			   int key_idx,
			   int set_tx,
			   const unsigned char *seq,
			   size_t seq_len,
			   const unsigned char *key,
			   size_t key_len)
{
	enum wifi_nrf_status status = NVLSI_RPU_STATUS_FAIL;
	struct wifi_nrf_vif_ctx_zep *vif_ctx_zep = NULL;
	struct wifi_nrf_ctx_zep *rpu_ctx_zep = NULL;
	struct img_umac_key_info key_info;
	const unsigned char *mac_addr = NULL;
	unsigned int suite;
	int ret = -1;

#ifdef notyet
	/* Ignore for P2P Device */
	if (drv->nlmode == NL80211_IFTYPE_P2P_DEVICE)
		return 0;
#endif /* notyet */

	if ((!if_priv) || (!ifname)) {
		printk("%s: Invalid params\n", __func__);
		goto out;
	}

	vif_ctx_zep = if_priv;
	rpu_ctx_zep = vif_ctx_zep->rpu_ctx_zep;

	memset(&key_info,
	       0,
	       sizeof(key_info));

	if (alg != WPA_ALG_NONE) {
		suite = wpa_alg_to_cipher_suite(alg, key_len);

		if (!suite)
			goto out;

		memcpy(key_info.key.img_key,
		       key,
		       key_len);

		key_info.key.img_key_len = key_len;
		key_info.cipher_suite = suite;

		key_info.valid_fields |= (IMG_CIPHER_SUITE_VALID | IMG_KEY_VALID);
	}

	if (seq && seq_len) {
		memcpy(key_info.seq.img_seq,
		       seq,
		       seq_len);

		key_info.seq.img_seq_len = seq_len;
		key_info.valid_fields |= IMG_SEQ_VALID;
	}


	// TODO: Implement/check set_tx
	if (addr && !is_broadcast_ether_addr(addr)) {
		mac_addr = addr;
		key_info.key_type = IMG_KEYTYPE_PAIRWISE;
		key_info.valid_fields |= IMG_KEY_TYPE_VALID;
	} else if (addr && is_broadcast_ether_addr(addr)) {
		mac_addr = NULL;
		key_info.key_type = IMG_KEYTYPE_GROUP;
		key_info.valid_fields |= IMG_KEY_TYPE_VALID;
		key_info.img_flags |= IMG_KEY_DEFAULT_TYPE_MULTICAST;
	}

	key_info.key_idx = key_idx;
	key_info.valid_fields |= IMG_KEY_IDX_VALID;

	if (alg == WPA_ALG_NONE) {
		status = wifi_nrf_fmac_del_key(rpu_ctx_zep->rpu_ctx,
						 vif_ctx_zep->vif_idx,
						 &key_info,
						 mac_addr);

		if (status != NVLSI_RPU_STATUS_SUCCESS)
			printk("%s: wifi_nrf_fmac_del_key failed\n", __func__);
		else
			ret = 0;
	} else {
		status = wifi_nrf_fmac_add_key(rpu_ctx_zep->rpu_ctx,
						 vif_ctx_zep->vif_idx,
						 &key_info,
						 mac_addr);

		if (status != NVLSI_RPU_STATUS_SUCCESS)
			printk("%s: wifi_nrf_fmac_add_key failed\n", __func__);
		else
			ret = 0;
	}

	/*
	 * If we failed or don't need to set the default TX key (below),
	 * we're done here.
	 */
	if (ret || !set_tx || alg == WPA_ALG_NONE)
		goto out;

#ifdef notyet
	if (is_ap_interface(drv->nlmode) && addr &&
	    !is_broadcast_ether_addr(addr)) {
		wpa_printf(MSG_ERROR, "nl80211: %s: Not going for SET_KEY Command.\n", __func__);
		goto out;
	}
#endif /* notyet */

	memset(&key_info,
	       0,
	       sizeof(key_info));

	key_info.key_idx = key_idx;
	key_info.valid_fields |= IMG_KEY_IDX_VALID;

	if (alg == WPA_ALG_BIP_CMAC_128 ||
	    alg == WPA_ALG_BIP_GMAC_128 ||
	    alg == WPA_ALG_BIP_GMAC_256 ||
	    alg == WPA_ALG_BIP_CMAC_256) {
		key_info.img_flags = IMG_KEY_DEFAULT_MGMT;
	} else
		key_info.img_flags = IMG_KEY_DEFAULT;

	if (addr && is_broadcast_ether_addr(addr))
		key_info.img_flags |= IMG_KEY_DEFAULT_TYPE_MULTICAST;
	else if (addr)
		key_info.img_flags |= IMG_KEY_DEFAULT_TYPE_UNICAST;

	status = wifi_nrf_fmac_set_key(rpu_ctx_zep->rpu_ctx,
					 vif_ctx_zep->vif_idx,
					 &key_info);

	if (status != NVLSI_RPU_STATUS_SUCCESS) {
		printk("%s: wifi_nrf_fmac_set_key failed\n", __func__);
		ret = -1;
	} else
		ret = 0;
out:
	return ret;
}

int wifi_nrf_wpa_set_supp_port(void *if_priv, int authorized, char *bssid)
{
	struct wifi_nrf_vif_ctx_zep *vif_ctx_zep = NULL;
	struct img_umac_chg_sta_info chg_sta_info;
	struct wifi_nrf_ctx_zep *rpu_ctx_zep = NULL;

	vif_ctx_zep = if_priv;
	rpu_ctx_zep = vif_ctx_zep->rpu_ctx_zep;

	os_memset(&chg_sta_info, 0x0, sizeof(chg_sta_info));

	os_memcpy(chg_sta_info.mac_addr, bssid, ETH_ALEN);

	if(authorized){
		chg_sta_info.sta_flags2.img_mask = 1 << 1; // BIT(NL80211_STA_FLAG_AUTHORIZED)
		chg_sta_info.sta_flags2.img_set  = 1 << 1; // BIT(NL80211_STA_FLAG_AUTHORIZED)
	}

	return  wifi_nrf_fmac_chg_sta(rpu_ctx_zep->rpu_ctx, vif_ctx_zep->vif_idx, &chg_sta_info);
}
