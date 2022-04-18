/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @brief Header containing linked list specific declarations
 * for the Wi-Fi driver.
 */

#ifndef __LIST_H__
#define __LIST_H__

#include <stddef.h>
#include "osal_api.h"

void *nvlsi_wlan_utils_list_alloc(struct nvlsi_rpu_osal_priv *opriv);

void nvlsi_wlan_utils_list_free(struct nvlsi_rpu_osal_priv *opriv,
				void *list);

enum nvlsi_rpu_status nvlsi_wlan_utils_list_add_tail(struct nvlsi_rpu_osal_priv *opriv,
						     void *list,
						     void *data);

void nvlsi_wlan_utils_list_del_node(struct nvlsi_rpu_osal_priv *opriv,
				    void *list,
				    void *data);

void *nvlsi_wlan_utils_list_del_head(struct nvlsi_rpu_osal_priv *opriv,
				     void *list);

void *nvlsi_wlan_utils_list_peek(struct nvlsi_rpu_osal_priv *opriv,
				 void *list);

unsigned int nvlsi_wlan_utils_list_len(struct nvlsi_rpu_osal_priv *opriv,
				       void *list);

enum nvlsi_rpu_status nvlsi_wlan_utils_list_traverse(struct nvlsi_rpu_osal_priv *opriv,
						     void *list,
						     void *callbk_data,
						     enum nvlsi_rpu_status (*callbk_func)(void *callbk_data,
											  void *data));
#endif /* __LIST_H__ */
