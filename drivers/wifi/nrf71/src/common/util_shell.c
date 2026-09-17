/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* @file
 * @brief NRF Wi-Fi util shell module
 */
#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/sys_heap.h>
#include <common/fw_if/nrf71_wifi_ctrl.h>
#include <common/util.h>
#include <common/wifi_ipc.h>
#include <system/fmac_api.h>
#include <common/mem_mgmt.h>
#include <system/core.h>
#include <system/wifi_util.h>
#include <system/fmac_tx.h>
#include <common/mac_addr.h>


extern struct nrf_wifi_drv_priv_zep rpu_drv_priv_zep;
struct nrf_wifi_ctx_zep *ctx = &rpu_drv_priv_zep.rpu_ctx_zep;

static bool check_valid_data_rate(const struct shell *sh,
				  unsigned char rate_flag,
				  unsigned int data_rate)
{
	bool ret = false;

	switch (rate_flag) {
	case RPU_TPUT_MODE_LEGACY:
		if ((data_rate == 1) ||
		    (data_rate == 2) ||
		    (data_rate == 55) ||
		    (data_rate == 11) ||
		    (data_rate == 6) ||
		    (data_rate == 9) ||
		    (data_rate == 12) ||
		    (data_rate == 18) ||
		    (data_rate == 24) ||
		    (data_rate == 36) ||
		    (data_rate == 48) ||
		    (data_rate == 54)) {
			ret = true;
		}
		break;
	case RPU_TPUT_MODE_HT:
	case RPU_TPUT_MODE_HE_SU:
	case RPU_TPUT_MODE_VHT:
		if ((data_rate >= 0) && (data_rate <= 7)) {
			ret = true;
		}
		break;
	case RPU_TPUT_MODE_HE_ER_SU:
		if (data_rate >= 0 && data_rate <= 2) {
			ret = true;
		}
		break;
	default:
		shell_fprintf(sh,
			      SHELL_ERROR,
			      "%s: Invalid rate_flag %d\n",
			      __func__,
			      rate_flag);
		break;
	}

	return ret;
}


int nrf_wifi_util_conf_init(struct rpu_conf_params *conf_params)
{
	if (!conf_params) {
		return -ENOEXEC;
	}

	memset(conf_params, 0, sizeof(*conf_params));

	/* Initialize values which are other than 0 */
	conf_params->he_ltf = -1;
	conf_params->he_gi = -1;
	return 0;
}


static int nrf_wifi_util_set_he_ltf(const struct shell *sh,
				    size_t argc,
				    const char *argv[])
{
	char *ptr = NULL;
	unsigned long he_ltf = 0;

	if (ctx->conf_params.set_he_ltf_gi) {
		shell_fprintf(sh,
			      SHELL_ERROR,
			      "Disable 'set_he_ltf_gi', to set 'he_ltf'\n");
		return -ENOEXEC;
	}

	he_ltf = strtoul(argv[1], &ptr, 10);

	if (he_ltf > 2) {
		shell_fprintf(sh,
			      SHELL_ERROR,
			      "Invalid HE LTF value(%lu).\n",
			      he_ltf);
		shell_help(sh);
		return -ENOEXEC;
	}

	ctx->conf_params.he_ltf = he_ltf;

	return 0;
}


static int nrf_wifi_util_set_he_gi(const struct shell *sh,
				   size_t argc,
				   const char *argv[])
{
	char *ptr = NULL;
	unsigned long he_gi = 0;

	if (ctx->conf_params.set_he_ltf_gi) {
		shell_fprintf(sh,
			      SHELL_ERROR,
			      "Disable 'set_he_ltf_gi', to set 'he_gi'\n");
		return -ENOEXEC;
	}

	he_gi = strtoul(argv[1], &ptr, 10);

	if (he_gi > 2) {
		shell_fprintf(sh,
			      SHELL_ERROR,
			      "Invalid HE GI value(%lu).\n",
			      he_gi);
		shell_help(sh);
		return -ENOEXEC;
	}

	ctx->conf_params.he_gi = he_gi;

	return 0;
}


static int nrf_wifi_util_set_he_ltf_gi(const struct shell *sh,
				       size_t argc,
				       const char *argv[])
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	char *ptr = NULL;
	unsigned long val = 0;

	val = strtoul(argv[1], &ptr, 10);

	if ((val < 0) || (val > 1)) {
		shell_fprintf(sh,
			      SHELL_ERROR,
			      "Invalid value(%lu).\n",
			      val);
		shell_help(sh);
		return -ENOEXEC;
	}

	status = nrf_wifi_sys_fmac_conf_ltf_gi(ctx->rpu_ctx,
					       ctx->conf_params.he_ltf,
					       ctx->conf_params.he_gi,
					       val);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(sh,
			      SHELL_ERROR,
			      "Programming ltf_gi failed\n");
		return -ENOEXEC;
	}

	ctx->conf_params.set_he_ltf_gi = val;

	return 0;
}

#ifdef CONFIG_NRF71_STA_MODE
static int nrf_wifi_util_set_uapsd_queue(const struct shell *sh,
					 size_t argc,
					 const char *argv[])
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	char *ptr = NULL;
	unsigned long val = 0;

	val = strtoul(argv[1], &ptr, 10);

	if ((val < UAPSD_Q_MIN) || (val > UAPSD_Q_MAX)) {
		shell_fprintf(sh,
			      SHELL_ERROR,
			      "Invalid value(%lu).\n",
			      val);
		shell_help(sh);
		return -ENOEXEC;
	}

	if (ctx->conf_params.uapsd_queue != val) {
		status = nrf_wifi_sys_fmac_set_uapsd_queue(ctx->rpu_ctx,
						       0,
						       val);

		if (status != NRF_WIFI_STATUS_SUCCESS) {
			shell_fprintf(sh,
				      SHELL_ERROR,
				      "Programming uapsd_queue failed\n");
			return -ENOEXEC;
		}

		ctx->conf_params.uapsd_queue = val;
	}

	return 0;
}
#endif /* CONFIG_NRF71_STA_MODE */


static int nrf_wifi_util_show_cfg(const struct shell *sh,
				  size_t argc,
				  const char *argv[])
{
	struct rpu_conf_params *conf_params = NULL;

	conf_params = &ctx->conf_params;

	shell_fprintf(sh,
		      SHELL_INFO,
		      "************* Configured Parameters ***********\n");
	shell_fprintf(sh,
		      SHELL_INFO,
		      "\n");

	shell_fprintf(sh,
		      SHELL_INFO,
		      "he_ltf = %d\n",
		      conf_params->he_ltf);

	shell_fprintf(sh,
		      SHELL_INFO,
		      "he_gi = %u\n",
		      conf_params->he_gi);

	shell_fprintf(sh,
		      SHELL_INFO,
		      "set_he_ltf_gi = %d\n",
		      conf_params->set_he_ltf_gi);

	shell_fprintf(sh,
		      SHELL_INFO,
		      "uapsd_queue = %d\n",
		      conf_params->uapsd_queue);

	shell_fprintf(sh,
		      SHELL_INFO,
		      "rate_flag = %d,  rate_val = %d\n",
		      ctx->conf_params.tx_pkt_tput_mode,
		      ctx->conf_params.tx_pkt_rate);
	return 0;
}

#ifdef CONFIG_NRF71_STA_MODE
static const char *ac_str(int ac)
{
	static const char * const names[NRF_WIFI_FMAC_AC_MAX] = {
		"BK", "BE", "VI", "VO", "MC"
	};

	if ((ac < 0) || (ac >= NRF_WIFI_FMAC_AC_MAX)) {
		return "??";
	}

	return names[ac];
}

/* One line per topic: what the tokens carried, and what stopped them. */
static void tx_token_stats_summary(const struct shell *sh,
				   struct nrf_wifi_sys_fmac_dev_ctx *sys_dev_ctx,
				   struct nrf_wifi_sys_fmac_priv *sys_fpriv)
{
	struct tx_token_stats *ts = &sys_dev_ctx->tx_config.token_stats;
	unsigned int cap = sys_fpriv->avail_ampdu_len_per_token;
	unsigned int tokens_used = 0;
	unsigned int max_in_flight = 0;
	unsigned int busy = 0;
	unsigned int starved = 0;
	unsigned int q_full = 0;
	unsigned int avg_pkts_x100;
	unsigned int avg_bytes;
	unsigned int stops;
	const char *limiter = "none";
	unsigned int limiter_cnt = 0;
	unsigned int i;

	for (i = 0; i < NRF71_MAX_TX_TOKENS; i++) {
		if (ts->token_cmds[i]) {
			tokens_used++;
		}
	}

	for (i = 0; i < NRF_WIFI_FMAC_AC_MAX; i++) {
		if (ts->max_outstanding_descs[i] > max_in_flight) {
			max_in_flight = ts->max_outstanding_descs[i];
		}
		busy += ts->token_get_busy[i];
		starved += ts->token_get_starved[i];
		q_full += ts->pend_q_full_drops[i];
	}

	avg_pkts_x100 = ts->tx_cmds ?
		(unsigned int)((ts->tx_cmd_pkts * 100ULL) / ts->tx_cmds) : 0U;
	avg_bytes = ts->tx_cmds ? (unsigned int)(ts->tx_cmd_bytes / ts->tx_cmds) : 0U;

	stops = ts->aggr_stop_size + ts->aggr_stop_count + ts->aggr_stop_mismatch +
		ts->aggr_stop_twt + ts->aggr_stop_q_empty;

	/* aggr_stop_q_empty only means the pending queue was drained by this
	 * command, which is the normal case; the host is only really failing to
	 * keep up if the token had to be returned to the free pool for lack of
	 * packets (token_acq) or if the pipe ran idle.
	 */
	if (ts->aggr_stop_count > limiter_cnt) {
		limiter_cnt = ts->aggr_stop_count;
		limiter = "aggr count cap";
	}
	if (ts->aggr_stop_size > limiter_cnt) {
		limiter_cnt = ts->aggr_stop_size;
		limiter = "token size cap";
	}
	if (ts->aggr_stop_mismatch > limiter_cnt) {
		limiter_cnt = ts->aggr_stop_mismatch;
		limiter = "SA/RA mismatch";
	}
	if (ts->aggr_stop_twt > limiter_cnt) {
		limiter_cnt = ts->aggr_stop_twt;
		limiter = "TWT sleep";
	}

	if (ts->pipe_idle_us > ts->pipe_busy_us) {
		limiter = "host out of pkts";
		limiter_cnt = stops;
	} else if (limiter_cnt == 0) {
		limiter = "RPU turnaround";
		limiter_cnt = stops;
	}

	shell_fprintf(sh, SHELL_INFO,
		      "TX: %u cmds, %u pkts, %u dones | %u.%02u pkts/cmd (cap %u), "
		      "%u B/cmd (%u%% of %u B cap), %u B/pkt\n",
		      ts->tx_cmds,
		      ts->tx_cmd_pkts,
		      ts->tx_dones,
		      avg_pkts_x100 / 100U,
		      avg_pkts_x100 % 100U,
		      sys_fpriv->data_config.max_tx_aggregation,
		      avg_bytes,
		      cap ? ((avg_bytes * 100U) / cap) : 0U,
		      cap,
		      ts->tx_cmd_pkts ?
			      (unsigned int)(ts->tx_cmd_data_bytes / ts->tx_cmd_pkts) : 0U);
	shell_fprintf(sh, SHELL_INFO,
		      "    tokens: %u of %u used, %u max in flight, %u busy, %u starved | "
		      "limiter: %s (%u%%)\n",
		      tokens_used,
		      sys_fpriv->num_tx_tokens,
		      max_in_flight,
		      busy,
		      starved,
		      limiter,
		      stops ? ((limiter_cnt * 100U) / stops) : 0U);
	if (ts->tx_cmds) {
		unsigned long long inflight_us = 0;
		unsigned long long pipe_us = ts->pipe_busy_us + ts->pipe_idle_us;

		for (i = 0; i < NRF71_MAX_TX_TOKENS; i++) {
			inflight_us += ts->token_inflight_us[i];
		}

		shell_fprintf(sh, SHELL_INFO,
			      "    turnaround: %u us/cmd in flight, %u.%02u cmds in flight "
			      "avg (max %u), pipe busy %u%%, %u KB/s through the pipe\n",
			      (unsigned int)(inflight_us / ts->tx_cmds),
			      pipe_us ? (unsigned int)(inflight_us / pipe_us) : 0U,
			      pipe_us ? (unsigned int)((inflight_us * 100ULL / pipe_us) % 100U) :
					0U,
			      ts->max_cmds_in_flight,
			      pipe_us ? (unsigned int)((ts->pipe_busy_us * 100ULL) / pipe_us) :
					0U,
			      pipe_us ? (unsigned int)((ts->tx_cmd_bytes * 1000ULL) / pipe_us) :
					0U);
	}

	shell_fprintf(sh, SHELL_INFO,
		      "    host: %u pkts in, %u held on pend_q (%u%%), drops: %u q_full, "
		      "%u other%s\n",
		      ts->if_send_calls,
		      ts->pkts_queued,
		      ts->if_send_accepted ?
			      ((ts->pkts_queued * 100U) / ts->if_send_accepted) : 0U,
		      q_full,
		      ts->if_drop_no_nbuf + ts->if_drop_unknown_peer + ts->if_drop_not_ready,
		      starved ? " | STARVED, see -v" : "");
}

static void tx_token_stats_dump(const struct shell *sh,
				struct nrf_wifi_sys_fmac_dev_ctx *sys_dev_ctx,
				struct nrf_wifi_sys_fmac_priv *sys_fpriv)
{
	struct tx_token_stats *ts = &sys_dev_ctx->tx_config.token_stats;
	unsigned int num_tx_tokens = sys_fpriv->num_tx_tokens;
	unsigned int num_tx_tokens_per_ac = sys_fpriv->num_tx_tokens_per_ac;
	unsigned int cap = sys_fpriv->avail_ampdu_len_per_token;
	unsigned int max_aggr = sys_fpriv->data_config.max_tx_aggregation;
	unsigned int total_gets = 0;
	unsigned int total_busy = 0;
	unsigned int total_starved = 0;
	unsigned int aggr_stops;
	unsigned int i;

	shell_fprintf(sh, SHELL_INFO,
		      "\n--- Per-token efficiency ---\n"
		      "tokens: %u (reserved/AC: %u, spare: %u), "
		      "caps: %u B/token, %u pkts/token\n",
		      num_tx_tokens,
		      num_tx_tokens_per_ac,
		      num_tx_tokens - (num_tx_tokens_per_ac * NRF_WIFI_FMAC_AC_MAX),
		      cap,
		      max_aggr);
	shell_fprintf(sh, SHELL_INFO,
		      "bytes = packet data + %u B headroom per packet, i.e. what is\n"
		      "budgeted against the per-token size cap; acq = times the token was\n"
		      "taken from the free pool (a busy token is re-used in TX done and is\n"
		      "not returned, so acq stays low while cmds keeps rising)\n",
		      TX_BUF_HEADROOM);

	shell_fprintf(sh, SHELL_INFO,
		      "%-4s %6s %8s %9s %8s %9s %5s %8s %9s %8s %8s %6s %s\n",
		      "tok", "acq", "cmds", "pkts", "pkts/cmd", "bytes/cmd",
		      "fill", "max_pkts", "max_bytes", "us/cmd", "max_us", "KB/s",
		      "type");

	for (i = 0; i < num_tx_tokens && i < NRF71_MAX_TX_TOKENS; i++) {
		unsigned int cmds = ts->token_cmds[i];
		unsigned int pkts_x100 = cmds ? ((ts->token_pkts[i] * 100U) / cmds) : 0U;
		unsigned int bytes_per_cmd = cmds ?
			(unsigned int)(ts->token_bytes[i] / cmds) : 0U;
		bool spare = (i >= (num_tx_tokens_per_ac * NRF_WIFI_FMAC_AC_MAX));

		unsigned int us_per_cmd = cmds ?
			(unsigned int)(ts->token_inflight_us[i] / cmds) : 0U;

		shell_fprintf(sh, SHELL_INFO,
			      "%-4u %6u %8u %9u %5u.%02u %9u %4u%% %8u %9u %8u %8u %6u %s\n",
			      i,
			      ts->token_acq[i],
			      cmds,
			      ts->token_pkts[i],
			      pkts_x100 / 100U,
			      pkts_x100 % 100U,
			      bytes_per_cmd,
			      cap ? ((bytes_per_cmd * 100U) / cap) : 0U,
			      ts->token_max_pkts[i],
			      ts->token_max_bytes[i],
			      us_per_cmd,
			      ts->token_max_inflight_us[i],
			      ts->token_inflight_us[i] ?
				      (unsigned int)((ts->token_bytes[i] * 1000ULL) /
						     ts->token_inflight_us[i]) : 0U,
			      spare ? "spare" : ac_str(i % NRF_WIFI_FMAC_AC_MAX));
	}

	shell_fprintf(sh, SHELL_INFO,
		      "\n--- Aggregation: packets per TX command ---\n");

	for (i = 0; i < MAX_TX_AGG_SIZE; i++) {
		if (ts->pkts_per_cmd[i] == 0) {
			continue;
		}

		shell_fprintf(sh, SHELL_INFO,
			      "%2u pkt(s) - %u cmd(s) (%u.%02u%%)\n",
			      i + 1,
			      ts->pkts_per_cmd[i],
			      ts->tx_cmds ?
				      ((ts->pkts_per_cmd[i] * 100U) / ts->tx_cmds) : 0U,
			      ts->tx_cmds ?
				      (((ts->pkts_per_cmd[i] * 10000U) / ts->tx_cmds) % 100U) :
				      0U);
	}

	shell_fprintf(sh, SHELL_INFO,
		      "\n--- Aggregation: token fill level (%% of %u B cap) ---\n",
		      cap);

	for (i = 0; i < TX_TOKEN_FILL_BUCKETS; i++) {
		if (ts->fill_hist[i] == 0) {
			continue;
		}

		shell_fprintf(sh, SHELL_INFO,
			      "%3u-%3u%% - %u cmd(s)\n",
			      i * (100U / TX_TOKEN_FILL_BUCKETS),
			      (i + 1) * (100U / TX_TOKEN_FILL_BUCKETS),
			      ts->fill_hist[i]);
	}

	shell_fprintf(sh, SHELL_INFO,
		      "\nTX cmds: %u, pkts: %u, data: %llu B, occupancy: %llu B, "
		      "max cmd: %u B\n",
		      ts->tx_cmds,
		      ts->tx_cmd_pkts,
		      ts->tx_cmd_data_bytes,
		      ts->tx_cmd_bytes,
		      ts->max_cmd_bytes);
	shell_fprintf(sh, SHELL_INFO,
		      "TX dones: %u, pipe busy: %llu us, idle: %llu us, "
		      "max cmds in flight: %u\n",
		      ts->tx_dones,
		      ts->pipe_busy_us,
		      ts->pipe_idle_us,
		      ts->max_cmds_in_flight);

	if (ts->tx_cmds) {
		unsigned int avg_pkts_x100 = (unsigned int)((ts->tx_cmd_pkts * 100ULL) /
							    ts->tx_cmds);
		unsigned int avg_bytes = (unsigned int)(ts->tx_cmd_bytes / ts->tx_cmds);
		unsigned int avg_pkt_len = ts->tx_cmd_pkts ?
			(unsigned int)(ts->tx_cmd_data_bytes / ts->tx_cmd_pkts) : 0U;

		shell_fprintf(sh, SHELL_INFO,
			      "avg_tx pkts/TX cmd: %u.%02u (of %u), "
			      "avg bytes/TX cmd: %u (%u%% of cap), avg pkt: %u B\n",
			      avg_pkts_x100 / 100U,
			      avg_pkts_x100 % 100U,
			      max_aggr,
			      avg_bytes,
			      cap ? ((avg_bytes * 100U) / cap) : 0U,
			      avg_pkt_len);
	} else {
		shell_fprintf(sh, SHELL_INFO, "avg_tx pkts/TX cmd: n/a\n");
	}

	aggr_stops = ts->aggr_stop_size + ts->aggr_stop_count + ts->aggr_stop_mismatch +
		     ts->aggr_stop_twt + ts->aggr_stop_q_empty;

	shell_fprintf(sh, SHELL_INFO,
		      "\n--- Why aggregation stopped (%u events) ---\n",
		      aggr_stops);
	shell_fprintf(sh, SHELL_INFO,
		      "size cap hit   : %u\n"
		      "count cap hit  : %u\n"
		      "SA/RA mismatch : %u\n"
		      "TWT sleep      : %u\n"
		      "pend_q empty   : %u\n"
		      "forced single  : %u\n",
		      ts->aggr_stop_size,
		      ts->aggr_stop_count,
		      ts->aggr_stop_mismatch,
		      ts->aggr_stop_twt,
		      ts->aggr_stop_q_empty,
		      ts->aggr_forced_single);

	shell_fprintf(sh, SHELL_INFO,
		      "\n--- Per-AC token accounting ---\n"
		      "a token is requested for every packet handed to the driver, so once\n"
		      "the tokens of an AC are in flight every further request is 'busy'\n"
		      "and the packet waits on the pending queue for a TX done; only\n"
		      "'starved' (no token while the AC has nothing in flight) is a fault\n");
	shell_fprintf(sh, SHELL_INFO,
		      "%-4s %9s %7s %10s %8s %8s %9s %7s %7s\n",
		      "ac", "reserved", "spare", "busy", "starved", "in_fl",
		      "max_in_fl", "max_q", "q_full");

	for (i = 0; i < NRF_WIFI_FMAC_AC_MAX; i++) {
		total_gets += ts->reserved_token_get[i] + ts->spare_token_get[i];
		total_busy += ts->token_get_busy[i];
		total_starved += ts->token_get_starved[i];

		shell_fprintf(sh, SHELL_INFO,
			      "%-4s %9u %7u %10u %8u %8u %9u %7u %7u\n",
			      ac_str(i),
			      ts->reserved_token_get[i],
			      ts->spare_token_get[i],
			      ts->token_get_busy[i],
			      ts->token_get_starved[i],
			      sys_dev_ctx->tx_config.outstanding_descs[i],
			      ts->max_outstanding_descs[i],
			      ts->max_pending_qlen[i],
			      ts->pend_q_full_drops[i]);
	}

	shell_fprintf(sh, SHELL_INFO,
		      "token gets: %u, busy: %u, starved: %u, spare AC switches: %u\n",
		      total_gets,
		      total_busy,
		      total_starved,
		      ts->spare_token_ac_switch);
	shell_fprintf(sh, SHELL_INFO,
		      "token re-use: %u.%02u TX done(s) per token get\n",
		      total_gets ? (ts->tx_dones / total_gets) : 0U,
		      total_gets ? (((ts->tx_dones * 100U) / total_gets) % 100U) : 0U);

	shell_fprintf(sh, SHELL_INFO,
		      "\n--- Host TX path ---\n");
	shell_fprintf(sh, SHELL_INFO,
		      "if_send calls: %u, accepted: %u\n",
		      ts->if_send_calls,
		      ts->if_send_accepted);
	shell_fprintf(sh, SHELL_INFO,
		      "of which held on the pending queue (no token/aggregating/PS): %u "
		      "(%u%%)\n",
		      ts->pkts_queued,
		      ts->if_send_accepted ?
			      ((ts->pkts_queued * 100U) / ts->if_send_accepted) : 0U);
	shell_fprintf(sh, SHELL_INFO,
		      "if_send drops: no_nbuf: %u, unknown_peer: %u, not_ready: %u, "
		      "fmac_fail: %u\n",
		      ts->if_drop_no_nbuf,
		      ts->if_drop_unknown_peer,
		      ts->if_drop_not_ready,
		      ts->if_drop_fmac_fail);
	shell_fprintf(sh, SHELL_INFO,
		      "host pkts: tx: %llu, tx_done: %llu, tx_drop: %llu\n",
		      (unsigned long long)sys_dev_ctx->host_stats.total_tx_pkts,
		      (unsigned long long)sys_dev_ctx->host_stats.total_tx_done_pkts,
		      (unsigned long long)sys_dev_ctx->host_stats.total_tx_drop_pkts);
}

static int nrf_wifi_util_tx_stats(const struct shell *sh,
				  size_t argc,
				  const char *argv[])
{
	int vif_index = -1;
	int peer_index = 0;
	int max_vif_index = MAX(MAX_NUM_APS, MAX_NUM_STAS);
	struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx = NULL;
	struct nrf_wifi_sys_fmac_priv *sys_fpriv = NULL;
	void *queue = NULL;
	unsigned int tx_pending_pkts = 0;
	struct nrf_wifi_sys_fmac_dev_ctx *sys_dev_ctx = NULL;
	bool clear = false;
	bool verbose = false;
	int ret;

	for (size_t i = 2; i < argc; i++) {
		if (strcmp(argv[i], "clear") == 0) {
			clear = true;
		} else if (strcmp(argv[i], "-v") == 0) {
			verbose = true;
		} else {
			shell_fprintf(sh,
				      SHELL_ERROR,
				      "Invalid argument \"%s\".\n",
				      argv[i]);
			shell_help(sh);
			return -ENOEXEC;
		}
	}

	vif_index = atoi(argv[1]);
	if ((vif_index < 0) || (vif_index >= max_vif_index)) {
		shell_fprintf(sh,
			      SHELL_ERROR,
			      "Invalid vif index(%d).\n",
			      vif_index);
		shell_help(sh);
		return -ENOEXEC;
	}

	k_mutex_lock(&ctx->rpu_lock, K_FOREVER);
	if (!ctx->rpu_ctx) {
		shell_fprintf(sh,
			      SHELL_ERROR,
			      "RPU context not initialized\n");
		ret = -ENOEXEC;
		goto unlock;
	}

	fmac_dev_ctx = ctx->rpu_ctx;
	sys_dev_ctx = wifi_dev_priv(fmac_dev_ctx);
	sys_fpriv = wifi_fmac_priv(fmac_dev_ctx->fpriv);

	if (clear) {
		memset(&sys_dev_ctx->tx_config.token_stats,
		       0,
		       sizeof(sys_dev_ctx->tx_config.token_stats));
		shell_fprintf(sh,
			      SHELL_INFO,
			      "TX token stats cleared\n");
		ret = 0;
		goto unlock;
	}

	if (!verbose) {
		tx_token_stats_summary(sh, sys_dev_ctx, sys_fpriv);
		ret = 0;
		goto unlock;
	}

	/* TODO: Get peer_index from shell once AP mode is supported */
	shell_fprintf(sh,
		SHELL_INFO,
		"************* Tx Stats: vif(%d) peer(0) ***********\n",
		vif_index);

	for (int i = 0; i < NRF_WIFI_FMAC_AC_MAX ; i++) {
		queue = sys_dev_ctx->tx_config.data_pending_txq[peer_index][i];
		tx_pending_pkts = nrf_wifi_llist_len(queue);

		shell_fprintf(
			sh,
			SHELL_INFO,
			"Outstanding tokens: ac: %d (%s) -> %d (pending_q_len: %d)\n",
			i,
			ac_str(i),
			sys_dev_ctx->tx_config.outstanding_descs[i],
			tx_pending_pkts);
	}

	tx_token_stats_dump(sh, sys_dev_ctx, sys_fpriv);

	ret = 0;

unlock:
	k_mutex_unlock(&ctx->rpu_lock);
	return ret;
}
#endif /* CONFIG_NRF71_STA_MODE */


static int nrf_wifi_util_tx_rate(const struct shell *sh,
				 size_t argc,
				 const char *argv[])
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	char *ptr = NULL;
	long rate_flag = -1;
	long data_rate = -1;

	rate_flag = strtol(argv[1], &ptr, 10);

	if (rate_flag >= RPU_TPUT_MODE_MAX) {
		shell_fprintf(sh,
			      SHELL_ERROR,
			      "Invalid value %ld for rate_flags\n",
			      rate_flag);
		shell_help(sh);
		return -ENOEXEC;
	}


	if (rate_flag == RPU_TPUT_MODE_HE_TB) {
		data_rate = -1;
	} else {
		if (argc < 3) {
			shell_fprintf(sh,
				      SHELL_ERROR,
				      "rate_val needed for rate_flag = %ld\n",
				      rate_flag);
			shell_help(sh);
			return -ENOEXEC;
		}

		data_rate = strtol(argv[2], &ptr, 10);

		if (!(check_valid_data_rate(sh,
					    rate_flag,
					    data_rate))) {
			shell_fprintf(sh,
				      SHELL_ERROR,
				      "Invalid data_rate %ld for rate_flag %ld\n",
				      data_rate,
				      rate_flag);
			return -ENOEXEC;
		}

	}

	status = nrf_wifi_sys_fmac_set_tx_rate(ctx->rpu_ctx,
					       rate_flag,
					       data_rate);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(sh,
			      SHELL_ERROR,
			      "Programming tx_rate failed\n");
		return -ENOEXEC;
	}

	ctx->conf_params.tx_pkt_tput_mode = rate_flag;
	ctx->conf_params.tx_pkt_rate = data_rate;

	return 0;
}


static int nrf_wifi_util_show_vers(const struct shell *sh,
				  size_t argc,
				  const char *argv[])
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx = NULL;
	unsigned int fw_ver;

	fmac_dev_ctx = ctx->rpu_ctx;

	shell_fprintf(sh, SHELL_INFO, "Driver version: %s\n",
				  NRF71_DRIVER_VERSION);

	status = nrf_wifi_fmac_ver_get(fmac_dev_ctx, &fw_ver);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(sh,
		 SHELL_INFO,
		 "Failed to get firmware version\n");
		return -ENOEXEC;
	}

	shell_fprintf(sh, SHELL_INFO,
		 "Firmware version: %d.%d.%d.%d\n",
		  NRF_WIFI_UMAC_VER(fw_ver),
		  NRF_WIFI_UMAC_VER_MAJ(fw_ver),
		  NRF_WIFI_UMAC_VER_MIN(fw_ver),
		  NRF_WIFI_UMAC_VER_EXTRA(fw_ver));

	return status;
}

#if !defined(CONFIG_NRF71_RADIO_TEST) && !defined(CONFIG_NRF71_OFFLOADED_RAW_TX)
static int nrf_wifi_util_dump_rpu_stats(const struct shell *sh,
					size_t argc,
					const char *argv[])
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx = NULL;
	struct rpu_sys_op_stats stats;
	enum rpu_stats_type stats_type = RPU_STATS_TYPE_ALL;
	int ret;

	if (argc == 2) {
		const char *type  = argv[1];

		if (!strcmp(type, "umac")) {
			stats_type = RPU_STATS_TYPE_UMAC;
		} else if (!strcmp(type, "lmac")) {
			stats_type = RPU_STATS_TYPE_LMAC;
		} else if (!strcmp(type, "phy")) {
			stats_type = RPU_STATS_TYPE_PHY;
		} else if (!strcmp(type, "all")) {
			stats_type = RPU_STATS_TYPE_ALL;
		} else {
			shell_fprintf(sh,
				      SHELL_ERROR,
				      "Invalid stats type %s\n",
				      type);
			return -ENOEXEC;
		}
	}

	k_mutex_lock(&ctx->rpu_lock, K_FOREVER);
	if (!ctx->rpu_ctx) {
		shell_fprintf(sh,
			      SHELL_ERROR,
			      "RPU context not initialized\n");
		ret = -ENOEXEC;
		goto unlock;
	}
	fmac_dev_ctx = ctx->rpu_ctx;

	memset(&stats, 0, sizeof(struct rpu_sys_op_stats));
	status = nrf_wifi_sys_fmac_stats_get(fmac_dev_ctx, stats_type, &stats);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(sh,
			      SHELL_ERROR,
			      "Failed to get stats\n");
		ret = -ENOEXEC;
		goto unlock;
	}

	if (stats_type == RPU_STATS_TYPE_UMAC || stats_type == RPU_STATS_TYPE_ALL) {
		struct rpu_umac_stats *umac = &stats.fw.umac;

		shell_fprintf(sh, SHELL_INFO,
				  "UMAC TX debug stats:\n"
				  "======================\n"
				  "tx_cmd: %u\n"
				  "tx_non_coalesce_pkts_rcvd_from_host: %u\n"
				  "tx_coalesce_pkts_rcvd_from_host: %u\n"
				  "tx_max_coalesce_pkts_rcvd_from_host: %u\n"
				  "tx_cmds_max_used: %u\n"
				  "tx_cmds_currently_in_use: %u\n"
				  "tx_done_events_send_to_host: %u\n"
				  "tx_done_success_pkts_to_host: %u\n"
				  "tx_done_failure_pkts_to_host: %u\n"
				  "tx_cmds_with_crypto_pkts_rcvd_from_host: %u\n"
				  "tx_cmds_with_non_crypto_pkts_rcvd_from_host: %u\n"
				  "tx_cmds_with_broadcast_pkts_rcvd_from_host: %u\n"
				  "tx_cmds_with_multicast_pkts_rcvd_from_host: %u\n"
				  "tx_cmds_with_unicast_pkts_rcvd_from_host: %u\n"
				  "xmit: %u\n"
				  "send_addba_req: %u\n"
				  "addba_resp: %u\n"
				  "softmac_tx: %u\n"
				  "internal_pkts: %u\n"
				  "external_pkts: %u\n"
				  "tx_cmds_to_lmac: %u\n"
				  "tx_dones_from_lmac: %u\n"
				  "total_cmds_to_lmac: %u\n"
				  "cmdq_empty: %u\n"
				  "tx_packet_data_count: %u\n"
				  "tx_packet_mgmt_count: %u\n"
				  "tx_packet_beacon_count: %u\n"
				  "tx_packet_probe_req_count: %u\n"
				  "tx_packet_auth_count: %u\n"
				  "tx_packet_deauth_count: %u\n"
				  "tx_packet_assoc_req_count: %u\n"
				  "tx_packet_disassoc_count: %u\n"
				  "tx_packet_action_count: %u\n"
				  "tx_packet_other_mgmt_count: %u\n"
				  "tx_packet_non_mgmt_data_count: %u\n"
				  "tx_packet_eapol_count: %u\n\n",
				  umac->tx_dbg_params.tx_cmd,
				  umac->tx_dbg_params.tx_non_coalesce_pkts_rcvd_from_host,
				  umac->tx_dbg_params.tx_coalesce_pkts_rcvd_from_host,
				  umac->tx_dbg_params.tx_max_coalesce_pkts_rcvd_from_host,
				  umac->tx_dbg_params.tx_cmds_max_used,
				  umac->tx_dbg_params.tx_cmds_currently_in_use,
				  umac->tx_dbg_params.tx_done_events_send_to_host,
				  umac->tx_dbg_params.tx_done_success_pkts_to_host,
				  umac->tx_dbg_params.tx_done_failure_pkts_to_host,
				  umac->tx_dbg_params.tx_cmds_with_crypto_pkts_rcvd_from_host,
				  umac->tx_dbg_params.tx_cmds_with_non_crypto_pkts_rcvd_from_host,
				  umac->tx_dbg_params.tx_cmds_with_broadcast_pkts_rcvd_from_host,
				  umac->tx_dbg_params.tx_cmds_with_multicast_pkts_rcvd_from_host,
				  umac->tx_dbg_params.tx_cmds_with_unicast_pkts_rcvd_from_host,
				  umac->tx_dbg_params.xmit,
				  umac->tx_dbg_params.send_addba_req,
				  umac->tx_dbg_params.addba_resp,
				  umac->tx_dbg_params.softmac_tx,
				  umac->tx_dbg_params.internal_pkts,
				  umac->tx_dbg_params.external_pkts,
				  umac->tx_dbg_params.tx_cmds_to_lmac,
				  umac->tx_dbg_params.tx_dones_from_lmac,
				  umac->tx_dbg_params.total_cmds_to_lmac,
				  umac->tx_dbg_params.cmdq_empty,
				  umac->tx_dbg_params.tx_packet_data_count,
				  umac->tx_dbg_params.tx_packet_mgmt_count,
				  umac->tx_dbg_params.tx_packet_beacon_count,
				  umac->tx_dbg_params.tx_packet_probe_req_count,
				  umac->tx_dbg_params.tx_packet_auth_count,
				  umac->tx_dbg_params.tx_packet_deauth_count,
				  umac->tx_dbg_params.tx_packet_assoc_req_count,
				  umac->tx_dbg_params.tx_packet_disassoc_count,
				  umac->tx_dbg_params.tx_packet_action_count,
				  umac->tx_dbg_params.tx_packet_other_mgmt_count,
				  umac->tx_dbg_params.tx_packet_non_mgmt_data_count,
				  umac->tx_dbg_params.tx_packet_eapol_count);

		shell_fprintf(sh, SHELL_INFO,
				  "UMAC RX debug stats\n"
				  "======================\n"
				  "lmac_events: %u\n"
				  "rx_events: %u\n"
				  "rx_coalesce_events: %u\n"
				  "total_rx_pkts_from_lmac: %u\n"
				  "max_refill_gap: %u\n"
				  "current_refill_gap: %u\n"
				  "out_of_order_mpdus: %u\n"
				  "reorder_free_mpdus: %u\n"
				  "umac_consumed_pkts: %u\n"
				  "host_consumed_pkts: %u\n"
				  "reordering_ampdu: %u\n"
				  "userspace_offload_frames: %u\n"
				  "rx_packet_total_count: %u\n"
				  "rx_packet_data_count: %u\n"
				  "rx_packet_qos_data_count: %u\n"
				  "rx_packet_protected_data_count: %u\n"
				  "rx_packet_mgmt_count: %u\n"
				  "rx_packet_beacon_count: %u\n"
				  "rx_packet_probe_resp_count: %u\n"
				  "rx_packet_auth_count: %u\n"
				  "rx_packet_deauth_count: %u\n"
				  "rx_packet_assoc_resp_count: %u\n"
				  "rx_packet_disassoc_count: %u\n"
				  "rx_packet_action_count: %u\n"
				  "rx_packet_probe_req_count: %u\n"
				  "rx_packet_other_mgmt_count: %u\n"
				  "max_coalesce_pkts: %u\n"
				  "null_skb_pointer_from_lmac: %u\n"
				  "null_skb_pointer_from_host: %u\n"
				  "null_skb_pointer_resubmitted: %u\n"
				  "reorder_flush_pkt_count: %u\n"
				  "unsecured_data_error: %u\n"
				  "pkts_in_null_skb_pointer_event: %u\n"
				  "rx_buffs_resubmit_cnt: %u\n"
				  "rx_packet_amsdu_cnt: %u\n"
				  "rx_packet_mpdu_cnt: %u\n"
				  "rx_err_secondary_pkt: %u\n"
				  "rx_err_invalid_pkt_info_type: %u\n"
				  "unexpected_mgmt_pkt: %u\n\n",
				  umac->rx_dbg_params.lmac_events,
				  umac->rx_dbg_params.rx_events,
				  umac->rx_dbg_params.rx_coalesce_events,
				  umac->rx_dbg_params.total_rx_pkts_from_lmac,
				  umac->rx_dbg_params.max_refill_gap,
				  umac->rx_dbg_params.current_refill_gap,
				  umac->rx_dbg_params.out_of_order_mpdus,
				  umac->rx_dbg_params.reorder_free_mpdus,
				  umac->rx_dbg_params.umac_consumed_pkts,
				  umac->rx_dbg_params.host_consumed_pkts,
				  umac->rx_dbg_params.reordering_ampdu,
				  umac->rx_dbg_params.userspace_offload_frames,
				  umac->rx_dbg_params.rx_packet_total_count,
				  umac->rx_dbg_params.rx_packet_data_count,
				  umac->rx_dbg_params.rx_packet_qos_data_count,
				  umac->rx_dbg_params.rx_packet_protected_data_count,
				  umac->rx_dbg_params.rx_packet_mgmt_count,
				  umac->rx_dbg_params.rx_packet_beacon_count,
				  umac->rx_dbg_params.rx_packet_probe_resp_count,
				  umac->rx_dbg_params.rx_packet_auth_count,
				  umac->rx_dbg_params.rx_packet_deauth_count,
				  umac->rx_dbg_params.rx_packet_assoc_resp_count,
				  umac->rx_dbg_params.rx_packet_disassoc_count,
				  umac->rx_dbg_params.rx_packet_action_count,
				  umac->rx_dbg_params.rx_packet_probe_req_count,
				  umac->rx_dbg_params.rx_packet_other_mgmt_count,
				  umac->rx_dbg_params.max_coalesce_pkts,
				  umac->rx_dbg_params.null_skb_pointer_from_lmac,
				  umac->rx_dbg_params.null_skb_pointer_from_host,
				  umac->rx_dbg_params.null_skb_pointer_resubmitted,
				  umac->rx_dbg_params.reorder_flush_pkt_count,
				  umac->rx_dbg_params.unsecured_data_error,
				  umac->rx_dbg_params.pkts_in_null_skb_pointer_event,
				  umac->rx_dbg_params.rx_buffs_resubmit_cnt,
				  umac->rx_dbg_params.rx_packet_amsdu_cnt,
				  umac->rx_dbg_params.rx_packet_mpdu_cnt,
				  umac->rx_dbg_params.rx_err_secondary_pkt,
				  umac->rx_dbg_params.rx_err_invalid_pkt_info_type,
				  umac->rx_dbg_params.unexpected_mgmt_pkt);

		shell_fprintf(sh, SHELL_INFO,
				  "UMAC control path stats\n"
				  "======================\n"
				  "cmd_init: %u\n"
				  "event_init_done: %u\n"
				  "cmd_rf_test: %u\n"
				  "cmd_connect: %u\n"
				  "cmd_get_stats: %u\n"
				  "event_ps_state: %u\n"
				  "cmd_set_reg: %u\n"
				  "cmd_get_reg: %u\n"
				  "cmd_req_set_reg: %u\n"
				  "cmd_trigger_scan: %u\n"
				  "event_scan_done: %u\n"
				  "cmd_get_scan: %u\n"
				  "scan_cmd_fail: %u\n"
				  "umac_scan_req: %u\n"
				  "umac_scan_complete: %u\n"
				  "umac_scan_busy: %u\n"
				  "umac_scan_abort: %u\n"
				  "umac_scan_abort_complete: %u\n"
				  "umac_scan_abort_fail: %u\n"
				  "cmd_auth: %u\n"
				  "cmd_assoc: %u\n"
				  "cmd_deauth: %u\n"
				  "cmd_register_frame: %u\n"
				  "cmd_frame: %u\n"
				  "cmd_del_key: %u\n"
				  "cmd_new_key: %u\n"
				  "cmd_set_key: %u\n"
				  "cmd_get_key: %u\n"
				  "event_beacon_hint: %u\n"
				  "event_reg_change: %u\n"
				  "event_wiphy_reg_change: %u\n"
				  "cmd_set_station: %u\n"
				  "cmd_new_station: %u\n"
				  "cmd_del_station: %u\n"
				  "cmd_new_interface: %u\n"
				  "cmd_set_interface: %u\n"
				  "cmd_get_interface: %u\n"
				  "cmd_set_ifflags: %u\n"
				  "cmd_set_ifflags_done: %u\n"
				  "cmd_set_bss: %u\n"
				  "cmd_set_wiphy: %u\n"
				  "cmd_start_ap: %u\n"
				  "LMAC_CMD_PS: %u\n"
				  "CURR_STATE: %u\n\n",
				  umac->cmd_evnt_dbg_params.cmd_init,
				  umac->cmd_evnt_dbg_params.event_init_done,
				  umac->cmd_evnt_dbg_params.cmd_rf_test,
				  umac->cmd_evnt_dbg_params.cmd_connect,
				  umac->cmd_evnt_dbg_params.cmd_get_stats,
				  umac->cmd_evnt_dbg_params.event_ps_state,
				  umac->cmd_evnt_dbg_params.cmd_set_reg,
				  umac->cmd_evnt_dbg_params.cmd_get_reg,
				  umac->cmd_evnt_dbg_params.cmd_req_set_reg,
				  umac->cmd_evnt_dbg_params.cmd_trigger_scan,
				  umac->cmd_evnt_dbg_params.event_scan_done,
				  umac->cmd_evnt_dbg_params.cmd_get_scan,
				  umac->cmd_evnt_dbg_params.scan_cmd_fail,
				  umac->cmd_evnt_dbg_params.umac_scan_req,
				  umac->cmd_evnt_dbg_params.umac_scan_complete,
				  umac->cmd_evnt_dbg_params.umac_scan_busy,
				  umac->cmd_evnt_dbg_params.umac_scan_abort,
				  umac->cmd_evnt_dbg_params.umac_scan_abort_complete,
				  umac->cmd_evnt_dbg_params.umac_scan_abort_fail,
				  umac->cmd_evnt_dbg_params.cmd_auth,
				  umac->cmd_evnt_dbg_params.cmd_assoc,
				  umac->cmd_evnt_dbg_params.cmd_deauth,
				  umac->cmd_evnt_dbg_params.cmd_register_frame,
				  umac->cmd_evnt_dbg_params.cmd_frame,
				  umac->cmd_evnt_dbg_params.cmd_del_key,
				  umac->cmd_evnt_dbg_params.cmd_new_key,
				  umac->cmd_evnt_dbg_params.cmd_set_key,
				  umac->cmd_evnt_dbg_params.cmd_get_key,
				  umac->cmd_evnt_dbg_params.event_beacon_hint,
				  umac->cmd_evnt_dbg_params.event_reg_change,
				  umac->cmd_evnt_dbg_params.event_wiphy_reg_change,
				  umac->cmd_evnt_dbg_params.cmd_set_station,
				  umac->cmd_evnt_dbg_params.cmd_new_station,
				  umac->cmd_evnt_dbg_params.cmd_del_station,
				  umac->cmd_evnt_dbg_params.cmd_new_interface,
				  umac->cmd_evnt_dbg_params.cmd_set_interface,
				  umac->cmd_evnt_dbg_params.cmd_get_interface,
				  umac->cmd_evnt_dbg_params.cmd_set_ifflags,
				  umac->cmd_evnt_dbg_params.cmd_set_ifflags_done,
				  umac->cmd_evnt_dbg_params.cmd_set_bss,
				  umac->cmd_evnt_dbg_params.cmd_set_wiphy,
				  umac->cmd_evnt_dbg_params.cmd_start_ap,
				  umac->cmd_evnt_dbg_params.LMAC_CMD_PS,
				  umac->cmd_evnt_dbg_params.CURR_STATE);

			shell_fprintf(sh, SHELL_INFO,
				  "UMAC interface stats\n"
				  "======================\n"
				  "tx_unicast_pkt_count: %u\n"
				  "tx_multicast_pkt_count: %u\n"
				  "tx_broadcast_pkt_count: %u\n"
				  "tx_bytes: %u\n"
				  "rx_unicast_pkt_count: %u\n"
				  "rx_multicast_pkt_count: %u\n"
				  "rx_broadcast_pkt_count: %u\n"
				  "rx_beacon_success_count: %u\n"
				  "rx_beacon_miss_count: %u\n"
				  "rx_bytes: %u\n"
				  "rx_checksum_error_count: %u\n\n"
				  "replay_attack_drop_cnt: %u\n\n",
				  umac->interface_data_stats.tx_unicast_pkt_count,
				  umac->interface_data_stats.tx_multicast_pkt_count,
				  umac->interface_data_stats.tx_broadcast_pkt_count,
				  umac->interface_data_stats.tx_bytes,
				  umac->interface_data_stats.rx_unicast_pkt_count,
				  umac->interface_data_stats.rx_multicast_pkt_count,
				  umac->interface_data_stats.rx_broadcast_pkt_count,
				  umac->interface_data_stats.rx_beacon_success_count,
				  umac->interface_data_stats.rx_beacon_miss_count,
				  umac->interface_data_stats.rx_bytes,
				  umac->interface_data_stats.rx_checksum_error_count,
				  umac->interface_data_stats.replay_attack_drop_cnt);
	}

	if (stats_type == RPU_STATS_TYPE_LMAC || stats_type == RPU_STATS_TYPE_ALL) {
		struct rpu_lmac_stats *lmac = &stats.fw.lmac;

		shell_fprintf(sh, SHELL_INFO,
			      "LMAC stats\n"
				  "======================\n"
				  "reset_cmd_cnt: %u\n"
				  "reset_complete_event_cnt: %u\n"
				  "unable_gen_event: %u\n"
				  "ch_prog_cmd_cnt: %u\n"
				  "channel_prog_done: %u\n"
				  "tx_pkt_cnt: %u\n"
				  "tx_pkt_done_cnt: %u\n"
				  "scan_pkt_cnt: %u\n"
				  "internal_pkt_cnt: %u\n"
				  "internal_pkt_done_cnt: %u\n"
				  "ack_resp_cnt: %u\n"
				  "tx_timeout: %u\n"
				  "deagg_isr: %u\n"
				  "deagg_inptr_desc_empty: %u\n"
				  "deagg_circular_buffer_full: %u\n"
				  "lmac_rxisr_cnt: %u\n"
				  "rx_decryptcnt: %u\n"
				  "process_decrypt_fail: %u\n"
				  "prepa_rx_event_fail: %u\n"
				  "rx_core_pool_full_cnt: %u\n"
				  "rx_mpdu_crc_success_cnt: %u\n"
				  "rx_mpdu_crc_fail_cnt: %u\n"
				  "rx_ofdm_crc_success_cnt: %u\n"
				  "rx_ofdm_crc_fail_cnt: %u\n"
				  "rxDSSSCrcSuccessCnt: %u\n"
				  "rxDSSSCrcFailCnt: %u\n"
				  "rx_crypto_start_cnt: %u\n"
				  "rx_crypto_done_cnt: %u\n"
				  "rx_event_buf_full: %u\n"
				  "rx_extram_buf_full: %u\n"
				  "scan_req: %u\n"
				  "scan_complete: %u\n"
				  "scan_abort_req: %u\n"
				  "scan_abort_complete: %u\n"
				  "internal_buf_pool_null: %u\n"
				  "rpu_hw_lockup_count: %u\n"
				  "rpu_hw_lockup_recovery_done: %u\n"
				  "SQIThresholdCmdsCnt: %u\n"
				  "SQIConfigCmdsCnt: %u\n"
				  "SQIEventsCnt: %u\n"
				  "SleepType: %u\n"
				  "warmBootCnt: %u\n\n",
				  lmac->reset_cmd_cnt,
				  lmac->reset_complete_event_cnt,
				  lmac->unable_gen_event,
				  lmac->ch_prog_cmd_cnt,
				  lmac->channel_prog_done,
				  lmac->tx_pkt_cnt,
				  lmac->tx_pkt_done_cnt,
				  lmac->scan_pkt_cnt,
				  lmac->internal_pkt_cnt,
				  lmac->internal_pkt_done_cnt,
				  lmac->ack_resp_cnt,
				  lmac->tx_timeout,
				  lmac->deagg_isr,
				  lmac->deagg_inptr_desc_empty,
				  lmac->deagg_circular_buffer_full,
				  lmac->lmac_rxisr_cnt,
				  lmac->rx_decryptcnt,
				  lmac->process_decrypt_fail,
				  lmac->prepa_rx_event_fail,
				  lmac->rx_core_pool_full_cnt,
				  lmac->rx_mpdu_crc_success_cnt,
				  lmac->rx_mpdu_crc_fail_cnt,
				  lmac->rx_ofdm_crc_success_cnt,
				  lmac->rx_ofdm_crc_fail_cnt,
				  lmac->rxDSSSCrcSuccessCnt,
				  lmac->rxDSSSCrcFailCnt,
				  lmac->rx_crypto_start_cnt,
				  lmac->rx_crypto_done_cnt,
				  lmac->rx_event_buf_full,
				  lmac->rx_extram_buf_full,
				  lmac->scan_req,
				  lmac->scan_complete,
				  lmac->scan_abort_req,
				  lmac->scan_abort_complete,
				  lmac->internal_buf_pool_null,
				  lmac->rpu_hw_lockup_count,
				  lmac->rpu_hw_lockup_recovery_done,
				  lmac->SQIThresholdCmdsCnt,
				  lmac->SQIConfigCmdsCnt,
				  lmac->SQIEventsCnt,
				  lmac->SleepType,
				  lmac->warmBootCnt);
	}

	if (stats_type == RPU_STATS_TYPE_PHY || stats_type == RPU_STATS_TYPE_ALL) {
		struct rpu_phy_stats *phy = &stats.fw.phy;

		shell_fprintf(sh, SHELL_INFO,
			      "PHY stats\n"
				  "======================\n"
				  "rssi_avg: %d\n"
				  "pdout_val: %u\n"
				  "ofdm_crc32_pass_cnt: %u\n"
				  "ofdm_crc32_fail_cnt: %u\n"
				  "dsss_crc32_pass_cnt: %u\n"
				  "dsss_crc32_fail_cnt: %u\n\n",
				  phy->rssi_avg,
				  phy->pdout_val,
				  phy->ofdm_crc32_pass_cnt,
				  phy->ofdm_crc32_fail_cnt,
				  phy->dsss_crc32_pass_cnt,
				  phy->dsss_crc32_fail_cnt);
	}

	ret = 0;
unlock:
	k_mutex_unlock(&ctx->rpu_lock);
	return ret;
}
#endif /* !CONFIG_NRF71_RADIO_TEST && !CONFIG_NRF71_OFFLOADED_RAW_TX */

#ifdef CONFIG_NRF_WIFI_RPU_RECOVERY
static int nrf_wifi_util_trigger_rpu_recovery(const struct shell *sh,
					      size_t argc,
					      const char *argv[])
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx = NULL;
	int ret;

	k_mutex_lock(&ctx->rpu_lock, K_FOREVER);
	if (!ctx || !ctx->rpu_ctx) {
		shell_fprintf(sh,
			      SHELL_ERROR,
			      "RPU context not initialized\n");
		ret = -ENOEXEC;
		goto unlock;
	}

	fmac_dev_ctx = ctx->rpu_ctx;

	status = nrf_wifi_sys_fmac_rpu_recovery_callback(fmac_dev_ctx, NULL, 0);
	if (status != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(sh,
			      SHELL_ERROR,
			      "Failed to trigger RPU recovery\n");
		return -ENOEXEC;
	}

	shell_fprintf(sh,
		      SHELL_INFO,
		      "RPU recovery triggered\n");

	ret = 0;
unlock:
	k_mutex_unlock(&ctx->rpu_lock);
	return ret;
}

static int nrf_wifi_util_rpu_recovery_info(const struct shell *sh,
					   size_t argc,
					   const char *argv[])
{
	struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx = NULL;
	unsigned long current_time_ms = k_uptime_get();
	int ret;

	k_mutex_lock(&ctx->rpu_lock, K_FOREVER);
	if (!ctx || !ctx->rpu_ctx) {
		shell_fprintf(sh,
			      SHELL_ERROR,
			      "RPU context not initialized\n");
		ret = -ENOEXEC;
		goto unlock;
	}

	fmac_dev_ctx = ctx->rpu_ctx;
	if (!fmac_dev_ctx) {
		shell_fprintf(sh, SHELL_ERROR, "FMAC context not initialized\n");
		ret = -ENOEXEC;
		goto unlock;
	}

	if (!nrf_wifi_ipc_is_open(fmac_dev_ctx)) {
		shell_fprintf(sh, SHELL_ERROR, "Host-RPU transport not open\n");
		ret = -ENOEXEC;
		goto unlock;
	}

	shell_fprintf(sh, SHELL_INFO,
		      "wdt_irq_received: %d\n"
		      "wdt_irq_ignored: %d\n"
		      "last_wakeup_now_asserted_time_ms: %lu milliseconds\n"
		      "last_wakeup_now_deasserted_time_ms: %lu milliseconds\n"
		      "last_rpu_sleep_opp_time_ms: %lu milliseconds\n"
		      "current time: %lu milliseconds\n"
		      "rpu_recovery_success: %d\n"
		      "rpu_recovery_failure: %d\n\n",
		      ctx->wdt_irq_received, ctx->wdt_irq_ignored,
		      fmac_dev_ctx->wakeup_now_asserted_time_prev_ms,
		      fmac_dev_ctx->wakeup_now_deasserted_time_prev_ms,
		      fmac_dev_ctx->rpu_sleep_opp_time_prev_ms, current_time_ms,
		      ctx->rpu_recovery_success, ctx->rpu_recovery_failure);

	ret = 0;
unlock:
	k_mutex_unlock(&ctx->rpu_lock);
	return ret;
}
#endif /* !CONFIG_NRF71_RADIO_TEST && !CONFIG_NRF71_OFFLOADED_RAW_TX */

#if !defined(CONFIG_NRF71_RADIO_TEST) && !defined(CONFIG_NRF71_OFFLOADED_RAW_TX)
static int nrf_wifi_util_clear_rpu_stats(const struct shell *sh,
					 size_t argc,
					 const char *argv[])
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx = NULL;
	enum rpu_stats_type stats_type = RPU_STATS_TYPE_ALL;
	int ret = 0;

	if (argc == 2) {
		const char *type = argv[1];

		if (!strcmp(type, "umac")) {
			stats_type = RPU_STATS_TYPE_UMAC;
		} else if (!strcmp(type, "lmac")) {
			stats_type = RPU_STATS_TYPE_LMAC;
		} else if (!strcmp(type, "phy")) {
			stats_type = RPU_STATS_TYPE_PHY;
		} else if (!strcmp(type, "all")) {
			stats_type = RPU_STATS_TYPE_ALL;
		} else {
			shell_fprintf(sh, SHELL_ERROR, "Invalid stats type %s\n", type);
			return -ENOEXEC;
		}
	}

	k_mutex_lock(&ctx->rpu_lock, K_FOREVER);
	if (!ctx->rpu_ctx) {
		shell_fprintf(sh, SHELL_ERROR, "RPU context not initialized\n");
		ret = -ENOEXEC;
		goto unlock_clear;
	}
	fmac_dev_ctx = ctx->rpu_ctx;

	status = umac_cmd_sys_clear_stats(fmac_dev_ctx, stats_type);
	if (status != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(sh, SHELL_ERROR, "Failed to clear RPU stats\n");
		ret = -ENOEXEC;
	} else {
		shell_fprintf(sh, SHELL_INFO, "RPU stats cleared\n");
	}
unlock_clear:
	k_mutex_unlock(&ctx->rpu_lock);
	return ret;
}

static void dump_umac_rx_dbg_params(const struct shell *sh,
				    const struct umac_rx_dbg_params *rx)
{
	shell_fprintf(sh, SHELL_INFO, "  lmac_events=%u rx_events=%u rx_coalesce_events=%u\n",
		      rx->lmac_events, rx->rx_events, rx->rx_coalesce_events);
	shell_fprintf(sh, SHELL_INFO, "  total_rx_pkts_from_lmac=%u max_refill_gap=%u\n",
		      rx->total_rx_pkts_from_lmac, rx->max_refill_gap);
	shell_fprintf(sh, SHELL_INFO, "  current_refill_gap=%u out_of_order_mpdus=%u\n",
		      rx->current_refill_gap, rx->out_of_order_mpdus);
	shell_fprintf(sh, SHELL_INFO, "  reorder_free_mpdus=%u umac_consumed_pkts=%u\n",
		      rx->reorder_free_mpdus, rx->umac_consumed_pkts);
	shell_fprintf(sh, SHELL_INFO, "  host_consumed_pkts=%u reordering_ampdu=%u\n",
		      rx->host_consumed_pkts, rx->reordering_ampdu);
	shell_fprintf(sh, SHELL_INFO, "  userspace_offload_frames=%u rx_packet_total_count=%u\n",
		      rx->userspace_offload_frames, rx->rx_packet_total_count);
	shell_fprintf(sh, SHELL_INFO, "  rx_packet_data_count=%u rx_packet_qos_data_count=%u\n",
		      rx->rx_packet_data_count, rx->rx_packet_qos_data_count);
	shell_fprintf(sh, SHELL_INFO,
		      "  rx_packet_protected_data_count=%u rx_packet_mgmt_count=%u\n",
		      rx->rx_packet_protected_data_count, rx->rx_packet_mgmt_count);
	shell_fprintf(sh, SHELL_INFO,
		      "  rx_packet_beacon_count=%u rx_packet_probe_resp_count=%u\n",
		      rx->rx_packet_beacon_count, rx->rx_packet_probe_resp_count);
	shell_fprintf(sh, SHELL_INFO,
		      "  rx_packet_auth_count=%u rx_packet_deauth_count=%u\n",
		      rx->rx_packet_auth_count, rx->rx_packet_deauth_count);
	shell_fprintf(sh, SHELL_INFO,
		      "  rx_packet_assoc_resp_count=%u rx_packet_disassoc_count=%u\n",
		      rx->rx_packet_assoc_resp_count, rx->rx_packet_disassoc_count);
	shell_fprintf(sh, SHELL_INFO,
		      "  rx_packet_action_count=%u rx_packet_probe_req_count=%u\n",
		      rx->rx_packet_action_count, rx->rx_packet_probe_req_count);
	shell_fprintf(sh, SHELL_INFO,
		      "  rx_packet_other_mgmt_count=%u max_coalesce_pkts=%u\n",
		      rx->rx_packet_other_mgmt_count, rx->max_coalesce_pkts);
	shell_fprintf(sh, SHELL_INFO,
		      "  null_skb_pointer_from_lmac=%u null_skb_pointer_from_host=%u\n",
		      rx->null_skb_pointer_from_lmac, rx->null_skb_pointer_from_host);
	shell_fprintf(sh, SHELL_INFO,
		      "  null_skb_pointer_resubmitted=%u unexpected_mgmt_pkt=%u\n",
		      rx->null_skb_pointer_resubmitted, rx->unexpected_mgmt_pkt);
	shell_fprintf(sh, SHELL_INFO,
		      "  reorder_flush_pkt_count=%u unsecured_data_error=%u\n",
		      rx->reorder_flush_pkt_count, rx->unsecured_data_error);
	shell_fprintf(sh, SHELL_INFO,
		      "  pkts_in_null_skb_pointer_event=%u rx_buffs_resubmit_cnt=%u\n",
		      rx->pkts_in_null_skb_pointer_event, rx->rx_buffs_resubmit_cnt);
	shell_fprintf(sh, SHELL_INFO, "  rx_packet_amsdu_cnt=%u rx_packet_mpdu_cnt=%u\n",
		      rx->rx_packet_amsdu_cnt, rx->rx_packet_mpdu_cnt);
	shell_fprintf(sh, SHELL_INFO,
		      "  rx_err_secondary_pkt=%u rx_err_invalid_pkt_info_type=%u\n",
		      rx->rx_err_secondary_pkt, rx->rx_err_invalid_pkt_info_type);
}

static void dump_lmac_common_stats(const struct shell *sh,
				   const struct lmac_common_stats *c)
{
	shell_fprintf(sh, SHELL_INFO,
		      "  general_purpose_timer_isr_cnt=%u lmac_task_inprogress=%u\n",
		      c->general_purpose_timer_isr_cnt, c->lmac_task_inprogress);
	shell_fprintf(sh, SHELL_INFO, "  current_cmd=%u cmds_going_to_wait_list=%u\n",
		      c->current_cmd, c->cmds_going_to_wait_list);
	shell_fprintf(sh, SHELL_INFO,
		      "  unexpected_internal_cmd_during_umac_wait=%u\n",
		      c->unexpected_internal_cmd_during_umac_wait);
	shell_fprintf(sh, SHELL_INFO, "  lmac_rx_isr_inprogress=%u hw_timer_isr_inprogress=%u\n",
		      c->lmac_rx_isr_inprogress, c->hw_timer_isr_inprogress);
	shell_fprintf(sh, SHELL_INFO, "  deagg_isr_inprogress=%u tx_isr_inprogress=%u\n",
		      c->deagg_isr_inprogress, c->tx_isr_inprogress);
	shell_fprintf(sh, SHELL_INFO, "  channel_switch_inprogress=%u rpu_lockup_event=%u\n",
		      c->channel_switch_inprogress, c->rpu_lockup_event);
	shell_fprintf(sh, SHELL_INFO, "  rpu_lockup_cnt=%u rpu_lockup_recovery_done=%u\n",
		      c->rpu_lockup_cnt, c->rpu_lockup_recovery_done);
	shell_fprintf(sh, SHELL_INFO, "  reset_cmd_cnt=%u reset_complete_event_cnt=%u\n",
		      c->reset_cmd_cnt, c->reset_complete_event_cnt);
	shell_fprintf(sh, SHELL_INFO,
		      "  commad_ent_default=%u lmac_enable_cnt=%u lmac_disable_cnt=%u\n",
		      c->commad_ent_default, c->lmac_enable_cnt, c->lmac_disable_cnt);
	shell_fprintf(sh, SHELL_INFO,
		      "  lmac_error_cnt=%u unable_gen_event=%u mem_pool_full_cnt=%u\n",
		      c->lmac_error_cnt, c->unable_gen_event, c->mem_pool_full_cnt);
	shell_fprintf(sh, SHELL_INFO,
		      "  ch_prog_cmd_cnt=%u channel_prog_done=%u connect_lost_status=%u\n",
		      c->ch_prog_cmd_cnt, c->channel_prog_done, c->connect_lost_status);
	shell_fprintf(sh, SHELL_INFO, "  tx_core_pool_full_cnt=%u patch_debug_cnt=%u\n",
		      c->tx_core_pool_full_cnt, c->patch_debug_cnt);
	shell_fprintf(sh, SHELL_INFO,
		      "  fw_error_event_cnt=%u tx_deinit_cmd_cnt=%u tx_deinit_done_cnt=%u\n",
		      c->fw_error_event_cnt, c->tx_deinit_cmd_cnt, c->tx_deinit_done_cnt);
	shell_fprintf(sh, SHELL_INFO, "  internal_buf_pool_null=%u rx_buffer_cmd=%u\n",
		      c->internal_buf_pool_null, c->rx_buffer_cmd);
	shell_fprintf(sh, SHELL_INFO, "  wait_for_tx_done_loop=%u cca_busy=%u\n",
		      c->wait_for_tx_done_loop, c->cca_busy);
	shell_fprintf(sh, SHELL_INFO,
		      "  temp_measure_window_expired=%u temp_inter_pool_full=%u\n",
		      c->temp_measure_window_expired, c->temp_inter_pool_full);
	shell_fprintf(sh, SHELL_INFO,
		      "  lmac_internal_cmd=%u measure_temp_vbat=%u fresh_calib_cnt=%u\n",
		      c->lmac_internal_cmd, c->measure_temp_vbat, c->fresh_calib_cnt);
	shell_fprintf(sh, SHELL_INFO, "  coex_event_cnt=%u coex_cmd_cnt=%u coex_isr_cnt=%u\n",
		      c->coex_event_cnt, c->coex_cmd_cnt, c->coex_isr_cnt);
	shell_fprintf(sh, SHELL_INFO, "  block_wlan_traffic_cnt=%u un_block_wlan_traffic_cnt=%u\n",
		      c->block_wlan_traffic_cnt, c->un_block_wlan_traffic_cnt);
	shell_fprintf(sh, SHELL_INFO, "  p2p_no_a_cnt=%u scan_timer_task_pending=%u\n",
		      c->p2p_no_a_cnt, c->scan_timer_task_pending);
	shell_fprintf(sh, SHELL_INFO, "  scan_timer_task_complete=%u coex_request_fail_cnt=%u\n",
		      c->scan_timer_task_complete, c->coex_request_fail_cnt);
}

static void dump_phy_debug_stats(const struct shell *sh,
				 const struct phy_debug_stats *phy)
{
	shell_fprintf(sh, SHELL_INFO, "  stats_category=%u\n", phy->stats_category);
	for (unsigned int i = 0; i < 128; i++) {
		shell_fprintf(sh, SHELL_INFO, "  phy_stats[%u]=%u\n",
			      i, phy->phy_stats[i]);
	}
}

static void dump_debug_stats_detailed(const struct shell *sh,
				      const struct nrf_wifi_rpu_debug_stats *stats,
				      enum rpu_stats_type stats_type)
{
	if (stats_type == RPU_STATS_TYPE_UMAC) {
		shell_fprintf(sh, SHELL_INFO,
			      "UMAC debug stats (category=%u) umac_rx_dbg_params:\n",
			      stats->umac_stats.stats_category);
		dump_umac_rx_dbg_params(sh, &stats->umac_stats.rx_dbg_params);
	} else if (stats_type == RPU_STATS_TYPE_LMAC) {
		struct lmac_common_stats c;

		shell_fprintf(sh, SHELL_INFO, "LMAC debug stats (category=%u) lmac_common_stats:\n",
			      stats->lmac_stats.stats_category);
		memcpy(&c, (const char *)&stats->lmac_stats +
		       offsetof(struct lmac_debug_stats, lmac_common_stats), sizeof(c));
		dump_lmac_common_stats(sh, &c);
	} else if (stats_type == RPU_STATS_TYPE_PHY) {
		shell_fprintf(sh, SHELL_INFO, "PHY debug stats (full phy_debug_stats):\n");
		dump_phy_debug_stats(sh, &stats->phy_stats);
	}
}

static int nrf_wifi_util_debug_stats(const struct shell *sh,
				     size_t argc,
				     const char *argv[])
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx = NULL;
	struct nrf_wifi_rpu_debug_stats stats;
	enum rpu_stats_type stats_type = RPU_STATS_TYPE_UMAC;
	int ret = 0;

	if (argc == 2) {
		const char *type = argv[1];

		if (!strcmp(type, "umac")) {
			stats_type = RPU_STATS_TYPE_UMAC;
		} else if (!strcmp(type, "lmac")) {
			stats_type = RPU_STATS_TYPE_LMAC;
		} else if (!strcmp(type, "phy")) {
			stats_type = RPU_STATS_TYPE_PHY;
		} else {
			shell_fprintf(sh, SHELL_ERROR, "Invalid type %s (umac|lmac|phy)\n",
				      type);
			return -ENOEXEC;
		}
	}

	k_mutex_lock(&ctx->rpu_lock, K_FOREVER);
	if (!ctx->rpu_ctx) {
		shell_fprintf(sh, SHELL_ERROR, "RPU context not initialized\n");
		ret = -ENOEXEC;
		goto unlock_dbg;
	}
	fmac_dev_ctx = ctx->rpu_ctx;

	memset(&stats, 0, sizeof(stats));
	status = nrf_wifi_sys_fmac_debug_stats_get(fmac_dev_ctx, stats_type, &stats);
	if (status != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(sh, SHELL_ERROR, "Failed to get debug stats (timeout or error)\n");
		ret = -ENOEXEC;
	} else {
		shell_fprintf(sh, SHELL_INFO, "Debug stats received type=%d (%u bytes)\n",
			     stats_type, (unsigned int)sizeof(stats));
		dump_debug_stats_detailed(sh, &stats, stats_type);
	}
unlock_dbg:
	k_mutex_unlock(&ctx->rpu_lock);
	return ret;
}

static int nrf_wifi_util_umac_int_stats(const struct shell *sh,
					size_t argc,
					const char *argv[])
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	struct nrf_wifi_fmac_dev_ctx *fmac_dev_ctx = NULL;
	struct umac_int_stats stats;
	int ret = 0;
	unsigned int i;

	(void)argc;
	(void)argv;

	k_mutex_lock(&ctx->rpu_lock, K_FOREVER);
	if (!ctx->rpu_ctx) {
		shell_fprintf(sh, SHELL_ERROR, "RPU context not initialized\n");
		ret = -ENOEXEC;
		goto unlock_umac;
	}
	fmac_dev_ctx = ctx->rpu_ctx;

	memset(&stats, 0, sizeof(stats));
	status = nrf_wifi_sys_fmac_umac_int_stats_get(fmac_dev_ctx, &stats);
	if (status != NRF_WIFI_STATUS_SUCCESS) {
		shell_fprintf(sh, SHELL_ERROR,
			      "Failed to get UMAC int stats (timeout or error)\n");
		ret = -ENOEXEC;
		goto unlock_umac;
	}

	shell_fprintf(sh, SHELL_INFO, "UMAC internal (memory) stats received\n");
	shell_fprintf(sh, SHELL_INFO, "Scratch dynamic memory pools (first 8):\n");
	for (i = 0; i < 8; i++) {
		shell_fprintf(sh, SHELL_INFO, "  [%u] buffer_size=%u num_pool_items=%u\n",
			      i,
			      stats.scratch_dynamic_memory_info[i].buffer_size,
			      stats.scratch_dynamic_memory_info[i].num_pool_items);
	}
	shell_fprintf(sh, SHELL_INFO, "Retention dynamic memory pools (first 8):\n");
	for (i = 0; i < 8; i++) {
		shell_fprintf(sh, SHELL_INFO, "  [%u] buffer_size=%u num_pool_items=%u\n",
			      i,
			      stats.retention_dynamic_memory_info[i].buffer_size,
			      stats.retention_dynamic_memory_info[i].num_pool_items);
	}
unlock_umac:
	k_mutex_unlock(&ctx->rpu_lock);
	return ret;
}

#endif /* !CONFIG_NRF71_RADIO_TEST && !CONFIG_NRF71_OFFLOADED_RAW_TX */

static int nrf_wifi_util_heap(const struct shell *sh, size_t argc, char **argv)
{
	struct k_heap *ctrl_pool;
	struct k_heap *data_pool;
	struct sys_memory_stats stats;
	int err;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	nrf_wifi_mem_get_heaps(&ctrl_pool, &data_pool);

	err = sys_heap_runtime_stats_get(&ctrl_pool->heap, &stats);
	if (err) {
		shell_error(sh, "Failed to read control heap statistics (err %d)", err);
		return -ENOEXEC;
	}
	shell_print(sh, "Control pool:");
	shell_print(sh, "  free:           %zu", stats.free_bytes);
	shell_print(sh, "  allocated:      %zu", stats.allocated_bytes);
	shell_print(sh, "  max. allocated: %zu", stats.max_allocated_bytes);

	err = sys_heap_runtime_stats_get(&data_pool->heap, &stats);
	if (err) {
		shell_error(sh, "Failed to read data heap statistics (err %d)", err);
		return -ENOEXEC;
	}
	shell_print(sh, "Data pool:");
	shell_print(sh, "  free:           %zu", stats.free_bytes);
	shell_print(sh, "  allocated:      %zu", stats.allocated_bytes);
	shell_print(sh, "  max. allocated: %zu", stats.max_allocated_bytes);

	return 0;
}

static void nrf_wifi_util_dump_mac_addr_slots(const struct shell *sh, bool uicr)
{
	const char *block = uicr ? "UICR" : "FICR";

	for (unsigned char slot = 0; slot < NRF_WIFI_XICR_MAC_ADDR_SLOTS; slot++) {
		uint32_t low, high;
		int ret;

		ret = nrf_wifi_xicr_mac_addr_slot_read(uicr, slot, &low, &high);
		if (ret == -ENOTSUP) {
			shell_print(sh, "%s: not accessible", block);
			return;
		} else if (ret) {
			shell_error(sh, "%s MACADDR[%u]: read failed (err %d)",
				    block, slot, ret);
			continue;
		}

		shell_print(sh, "%s MACADDR[%u].LOW  = 0x%08X", block, slot, low);
		shell_print(sh, "%s MACADDR[%u].HIGH = 0x%08X", block, slot, high);

		if (nrf_wifi_mac_addr_regs_empty(low, high)) {
			shell_print(sh, "%s MACADDR[%u]      = not programmed",
				    block, slot);
			continue;
		}

		/* HIGH holds bits [47:24] of the address, LOW bits [23:0], both
		 * MSB first, so 00F4CE36 / 00112233 is F4:CE:36:11:22:33.
		 */
		shell_print(sh, "%s MACADDR[%u]      = %02X:%02X:%02X:%02X:%02X:%02X",
			    block, slot,
			    (uint8_t)(high >> 16), (uint8_t)(high >> 8), (uint8_t)high,
			    (uint8_t)(low >> 16), (uint8_t)(low >> 8), (uint8_t)low);
	}
}

static int nrf_wifi_util_mac_addr(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	nrf_wifi_util_dump_mac_addr_slots(sh, true);
	nrf_wifi_util_dump_mac_addr_slots(sh, false);

	for (unsigned char vif_idx = 0; vif_idx < MAX_NUM_VIFS; vif_idx++) {
		uint8_t mac_addr[WIFI_MAC_ADDR_LEN];
		int ret;

		ret = nrf_wifi_xicr_mac_addr_get(vif_idx, mac_addr, NULL);
		if (ret) {
			shell_error(sh, "VIF%u: no MAC address available (err %d)",
				    vif_idx, ret);
			continue;
		}

		shell_print(sh, "VIF%u resolved    = %02X:%02X:%02X:%02X:%02X:%02X",
			    vif_idx,
			    mac_addr[0], mac_addr[1], mac_addr[2],
			    mac_addr[3], mac_addr[4], mac_addr[5]);
	}

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
	nrf71_util,
	SHELL_CMD_ARG(he_ltf,
		      NULL,
		      "0 - 1x HE LTF\n"
		      "1 - 2x HE LTF\n"
		      "2 - 4x HE LTF                                        ",
		      nrf_wifi_util_set_he_ltf,
		      2,
		      0),
	SHELL_CMD_ARG(he_gi,
		      NULL,
		      "0 - 0.8 us\n"
		      "1 - 1.6 us\n"
		      "2 - 3.2 us                                           ",
		      nrf_wifi_util_set_he_gi,
		      2,
		      0),
	SHELL_CMD_ARG(set_he_ltf_gi,
		      NULL,
		      "0 - Disable\n"
		      "1 - Enable",
		      nrf_wifi_util_set_he_ltf_gi,
		      2,
		      0),
#ifdef CONFIG_NRF71_STA_MODE
	SHELL_CMD_ARG(uapsd_queue,
		      NULL,
		      "<val> - 0 to 15",
		      nrf_wifi_util_set_uapsd_queue,
		      2,
		      0),
#endif /* CONFIG_NRF71_STA_MODE */
	SHELL_CMD_ARG(show_config,
		      NULL,
		      "Display the current configuration values",
		      nrf_wifi_util_show_cfg,
		      1,
		      0),
#ifdef CONFIG_NRF71_STA_MODE
	SHELL_CMD_ARG(tx_stats,
		      NULL,
		      "Displays a TX statistics summary: aggregation, token usage\n"
		      "and what is limiting the TX path\n"
		      "Parameters:\n"
		      "    vif_index: 0 - 1\n"
		      "    -v       : (optional) full per-token, histogram and per-AC dump\n"
		      "    clear    : (optional) reset the TX token counters\n",
		      nrf_wifi_util_tx_stats,
		      2,
		      2),
#endif /* CONFIG_NRF71_STA_MODE */
	SHELL_CMD_ARG(tx_rate,
		      NULL,
		      "Sets TX data rate to either a fixed value or AUTO\n"
		      "Parameters:\n"
		      "    <rate_flag> : The TX data rate type to be set, where:\n"
		      "        0 - LEGACY\n"
		      "        1 - HT\n"
		      "        2 - VHT\n"
		      "        3 - HE_SU\n"
		      "        4 - HE_ER_SU\n"
		      "        5 - AUTO\n"
		      "    <rate_val> : The TX data rate value to be set, valid values are:\n"
		      "        Legacy : <1, 2, 55, 11, 6, 9, 12, 18, 24, 36, 48, 54>\n"
		      "        Non-legacy: <MCS index value between 0 - 7>\n"
		      "        AUTO: <No value needed>\n",
		      nrf_wifi_util_tx_rate,
		      2,
		      1),
	SHELL_CMD_ARG(show_vers,
		      NULL,
		      "Display the driver and the firmware versions",
		      nrf_wifi_util_show_vers,
		      1,
		      0),
	SHELL_CMD_ARG(heap,
		      NULL,
		      "Control and data pool heap usage statistics",
		      nrf_wifi_util_heap,
		      1,
		      0),
#if !defined(CONFIG_NRF71_RADIO_TEST) && !defined(CONFIG_NRF71_OFFLOADED_RAW_TX)
	SHELL_CMD_ARG(rpu_stats,
		      NULL,
		      "Display RPU stats "
		      "Parameters: umac or lmac or phy or all (default)",
		      nrf_wifi_util_dump_rpu_stats,
		      1,
		      1),
	SHELL_CMD_ARG(clear_rpu_stats,
		      NULL,
		      "Clear RPU stats. Parameters: umac|lmac|phy|all (default)",
		      nrf_wifi_util_clear_rpu_stats,
		      1,
		      1),
	SHELL_CMD_ARG(debug_stats,
		      NULL,
		      "Request debug stats from RPU. Parameters: umac|lmac|phy",
		      nrf_wifi_util_debug_stats,
		      1,
		      1),
	SHELL_CMD_ARG(umac_int_stats,
		      NULL,
		      "Request UMAC internal (memory pool) stats from RPU",
		      nrf_wifi_util_umac_int_stats,
		      1,
		      0),
#endif /* !CONFIG_NRF71_RADIO_TEST && !CONFIG_NRF71_OFFLOADED_RAW_TX*/
#ifdef CONFIG_NRF_WIFI_RPU_RECOVERY
	SHELL_CMD_ARG(rpu_recovery_test,
		      NULL,
		      "Trigger RPU recovery",
		      nrf_wifi_util_trigger_rpu_recovery,
		      1,
		      0),
	SHELL_CMD_ARG(rpu_recovery_info,
		      NULL,
		      "Dump RPU recovery information",
		      nrf_wifi_util_rpu_recovery_info,
		      1,
		      0),
#endif /* CONFIG_NRF_WIFI_RPU_RECOVERY */
	SHELL_CMD_ARG(mac_addr,
		      NULL,
		      "Dump the MAC addresses programmed in the xICR registers\n"
		      "and the address each interface resolves to",
		      nrf_wifi_util_mac_addr,
		      1,
		      0),
	SHELL_SUBCMD_SET_END);


SHELL_SUBCMD_ADD((nrf71), util, &nrf71_util, "nRF70 utility commands\n", NULL, 0, 0);


static int nrf_wifi_util_init(void)
{

	if (nrf_wifi_util_conf_init(&ctx->conf_params) < 0) {
		return -1;
	}

	return 0;
}


SYS_INIT(nrf_wifi_util_init,
	 APPLICATION,
	 CONFIG_APPLICATION_INIT_PRIORITY);
