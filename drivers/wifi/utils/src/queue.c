/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @brief File containing queue specific definitions
 * for the Wi-Fi driver.
 */

#include "list.h"
#include "queue.h"

void *nvlsi_wlan_utils_q_alloc(struct nvlsi_rpu_osal_priv *opriv)
{
	return nvlsi_wlan_utils_list_alloc(opriv);
}


void nvlsi_wlan_utils_q_free(struct nvlsi_rpu_osal_priv *opriv,
			     void *q)
{
	nvlsi_wlan_utils_list_free(opriv,
				   q);
}


enum nvlsi_rpu_status nvlsi_wlan_utils_q_enqueue(struct nvlsi_rpu_osal_priv *opriv,
						 void *q,
						 void *data)
{
	return nvlsi_wlan_utils_list_add_tail(opriv,
					      q,
					      data);
}


void *nvlsi_wlan_utils_q_dequeue(struct nvlsi_rpu_osal_priv *opriv,
				 void *q)
{
	return nvlsi_wlan_utils_list_del_head(opriv,
					      q);
}


void *nvlsi_wlan_utils_q_peek(struct nvlsi_rpu_osal_priv *opriv,
			      void *q)
{
	return nvlsi_wlan_utils_list_peek(opriv,
					  q);
}


unsigned int nvlsi_wlan_utils_q_len(struct nvlsi_rpu_osal_priv *opriv,
				    void *q)
{
	return nvlsi_wlan_utils_list_len(opriv,
					 q);
}
