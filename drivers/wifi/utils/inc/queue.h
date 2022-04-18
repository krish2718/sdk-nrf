/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @brief Header containing queue specific declarations
 * for the Wi-Fi driver.
 */

#ifndef __QUEUE_H__
#define __QUEUE_H__

#include <stddef.h>
#include "osal_ops.h"

void *nvlsi_wlan_utils_q_alloc(struct nvlsi_rpu_osal_priv *opriv);

void nvlsi_wlan_utils_q_free(struct nvlsi_rpu_osal_priv *opriv,
			     void *q);

enum nvlsi_rpu_status nvlsi_wlan_utils_q_enqueue(struct nvlsi_rpu_osal_priv *opriv,
						 void *q,
						 void *q_node);

void *nvlsi_wlan_utils_q_dequeue(struct nvlsi_rpu_osal_priv *opriv,
				 void *q);

void *nvlsi_wlan_utils_q_peek(struct nvlsi_rpu_osal_priv *opriv,
			      void *q);

unsigned int nvlsi_wlan_utils_q_len(struct nvlsi_rpu_osal_priv *opriv,
				    void *q);
#endif /* __QUEUE_H__ */
