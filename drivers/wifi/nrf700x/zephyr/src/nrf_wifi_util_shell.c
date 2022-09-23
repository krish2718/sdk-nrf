/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/* @file
 * @brief NRF Wi-Fi utility shell module
 */

#include <zephyr.h>
#include <stdio.h>
#include <stdlib.h>
#include <shell/shell.h>
#include <init.h>
#include <ctype.h>
#include <zephyr_util.h>
#include <zephyr_fmac_main.h>

extern struct wifi_nrf_drv_priv_zep rpu_drv_priv_zep;
struct wifi_nrf_ctx_zep *ctx = &rpu_drv_priv_zep.rpu_ctx_zep;

static int nrf_wifi_util_bt_coex(const struct shell *shell,
				 size_t argc,
				 const char *argv[])
{
	enum wifi_nrf_status status = WIFI_NRF_STATUS_FAIL;
	char *ptr = NULL;
	struct rpu_btcoex params = {0};

	if (argc < 9) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid number of parameters\n");
		shell_help(shell);
		return -ENOEXEC;
	}

	params.coex_cmd_ctrl = strtoul(argv[1], &ptr, 10);

	if (params.coex_cmd_ctrl > 1) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid BT coex command control value(%d)\n",
			      params.coex_cmd_ctrl);
		shell_help(shell);
		return -ENOEXEC;
	}

	params.bt_mode = strtoul(argv[2], &ptr, 10);

	if (params.bt_mode > 1) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid BT mode value(%d)\n",
			      params.bt_mode);
		shell_help(shell);
		return -ENOEXEC;
	}

	params.bt_ctrl = strtoul(argv[3], &ptr, 10);

	if (params.bt_ctrl > 1) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid BT control value(%d)\n",
			      params.bt_ctrl);
		shell_help(shell);
		return -ENOEXEC;
	}

	params.pta_params.tx_rx_pol = strtoul(argv[2], &ptr, 10);

	if (params.pta_params.tx_rx_pol > 1) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid PTA TX/RX polarity value(%d)\n",
			      params.pta_params.tx_rx_pol);
		shell_help(shell);
		return -ENOEXEC;
	}

	params.pta_params.lead_time = strtoul(argv[2], &ptr, 10);

	if (params.pta_params.lead_time > 1) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid lead time value(%d)\n",
			      params.pta_params.lead_time);
		shell_help(shell);
		return -ENOEXEC;
	}

	params.pta_params.pti_samp_time = strtoul(argv[2], &ptr, 10);

	if (params.pta_params.pti_samp_time > 1) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid PTI sample time value(%d)\n",
			      params.pta_params.pti_samp_time);
		shell_help(shell);
		return -ENOEXEC;
	}

	params.pta_params.tx_rx_samp_time = strtoul(argv[2], &ptr, 10);

	if (params.pta_params.tx_rx_samp_time > 1) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid TX/RX sample time value(%d)\n",
			      params.pta_params.tx_rx_samp_time);
		shell_help(shell);
		return -ENOEXEC;
	}

	params.pta_params.dec_time = strtoul(argv[2], &ptr, 10);

	if (params.pta_params.dec_time > 1) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "Invalid DEC sample time value(%d)\n",
			      params.pta_params.dec_time);
		shell_help(shell);
		return -ENOEXEC;
	}

	status = wifi_nrf_fmac_conf_btcoex(ctx->rpu_ctx,
					   &params);


	if (status != WIFI_NRF_STATUS_SUCCESS) {
		shell_fprintf(shell,
			      SHELL_ERROR,
			      "BT coex configuration failed\n");
		return -ENOEXEC;
	}
	
	return 0;
}


SHELL_STATIC_SUBCMD_SET_CREATE(
	nrf_wifi_util_subcmds,
	SHELL_CMD_ARG(bt_coex,
		      NULL,
		      "BT coexistence configuration\nArguments: [Command control] [BT mode] [BT control] [PTA TX/RX polarity] [PTA lead time] [PTA PTI sample time] [PTA TX/RX sample time] [PTA dec time]",
		      nrf_wifi_util_bt_coex,
		      9,
		      0),
	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(wifi_util,
		   &nrf_wifi_util_subcmds,
		   "nRF Wi-Fi utility commands",
		   NULL);


static int nrf_wifi_util_shell_init(const struct device *unused)
{
	ARG_UNUSED(unused);

	return 0;
}


SYS_INIT(nrf_wifi_util_shell_init,
	 APPLICATION,
	 CONFIG_APPLICATION_INIT_PRIORITY);

