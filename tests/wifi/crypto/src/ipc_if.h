/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef IPC_IF_H
#define IPC_IF_H

#include <zephyr/kernel.h>
#include <zephyr/ipc/ipc_service.h>

typedef enum {
	IPC_INSTANCE_CMD_CTRL = 0,
	IPC_INSTANCE_CMD_TX,
	IPC_INSTANCE_EVT,
	IPC_INSTANCE_RX
} ipc_instances_nrf71_t;

typedef enum {
	IPC_EPT_UMAC = 0,
	IPC_EPT_LMAC
} ipc_epts_nrf71_t;

typedef struct ipc_ctx {
	ipc_instances_nrf71_t inst;
	ipc_epts_nrf71_t ept;
} ipc_ctx_t;

int ipc_init(void);
int ipc_deinit(void);
int ipc_send(ipc_ctx_t ctx, const void *data, int len);
int ipc_recv(ipc_ctx_t ctx, void *data, int len);
int ipc_register_rx_cb(int (*rx_handler)(void *priv), void *data);

#endif /* IPC_IF_H */
