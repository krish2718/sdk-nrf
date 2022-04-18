/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @brief File containing linked list specific definitions
 * for the Wi-Fi driver.
 */


#include "list.h"

void *nvlsi_wlan_utils_list_alloc(struct nvlsi_rpu_osal_priv *opriv)
{
	void *list = NULL;

	list = nvlsi_rpu_osal_llist_alloc(opriv);

	if (!list) {
		nvlsi_rpu_osal_log_err(opriv,
				       "%s: Unable to allocate list\n",
				       __func__);
		goto out;
	}

	nvlsi_rpu_osal_llist_init(opriv,
				  list);

out:
	return list;

}


void nvlsi_wlan_utils_list_free(struct nvlsi_rpu_osal_priv *opriv,
				void *list)
{
	nvlsi_rpu_osal_llist_free(opriv,
				  list);
}


enum nvlsi_rpu_status nvlsi_wlan_utils_list_add_tail(struct nvlsi_rpu_osal_priv *opriv,
						     void *list,
						     void *data)
{
	void *list_node = NULL;

	list_node = nvlsi_rpu_osal_llist_node_alloc(opriv);

	if (!list_node) {
		nvlsi_rpu_osal_log_err(opriv,
				       "%s: Unable to allocate list node\n",
				       __func__);
		return NVLSI_RPU_STATUS_FAIL;
	}

	nvlsi_rpu_osal_llist_node_data_set(opriv,
					   list_node,
					   data);

	nvlsi_rpu_osal_llist_add_node_tail(opriv,
					   list,
					   list_node);

	return NVLSI_RPU_STATUS_SUCCESS;
}

void nvlsi_wlan_utils_list_del_node(struct nvlsi_rpu_osal_priv *opriv,
				    void *list,
				    void *data)
{
	void *stored_data;
	void *list_node = NULL;
	void *list_node_next = NULL;

	list_node = nvlsi_rpu_osal_llist_get_node_head(opriv,
						       list);

	while (list_node) {
		stored_data = nvlsi_rpu_osal_llist_node_data_get(opriv,
								 list_node);

		list_node_next = nvlsi_rpu_osal_llist_get_node_nxt(opriv,
								   list,
								   list_node);

		if (stored_data == data) {
			nvlsi_rpu_osal_llist_del_node(opriv,
						      list,
						      list_node);

			nvlsi_rpu_osal_llist_node_free(opriv,
						       list_node);
		}

		list_node = list_node_next;
	}
}

void *nvlsi_wlan_utils_list_del_head(struct nvlsi_rpu_osal_priv *opriv,
				     void *list)
{
	void *list_node = NULL;
	void *data = NULL;

	list_node = nvlsi_rpu_osal_llist_get_node_head(opriv,
						       list);

	if (!list_node)
		goto out;

	data = nvlsi_rpu_osal_llist_node_data_get(opriv,
						  list_node);

	nvlsi_rpu_osal_llist_del_node(opriv,
				      list,
				      list_node);
	nvlsi_rpu_osal_llist_node_free(opriv,
				       list_node);

out:
	return data;
}


void *nvlsi_wlan_utils_list_peek(struct nvlsi_rpu_osal_priv *opriv,
				 void *list)
{
	void *list_node = NULL;
	void *data = NULL;

	list_node = nvlsi_rpu_osal_llist_get_node_head(opriv,
						       list);

	if (!list_node)
		goto out;

	data = nvlsi_rpu_osal_llist_node_data_get(opriv,
						  list_node);

out:
	return data;
}


unsigned int nvlsi_wlan_utils_list_len(struct nvlsi_rpu_osal_priv *opriv,
				       void *list)
{
	return nvlsi_rpu_osal_llist_len(opriv,
					list);
}


enum nvlsi_rpu_status nvlsi_wlan_utils_list_traverse(struct nvlsi_rpu_osal_priv *opriv,
						     void *list,
						     void *callbk_data,
						     enum nvlsi_rpu_status (*callbk_func)(void *callbk_data,
											  void *data))
{
	void *list_node = NULL;
	void *data = NULL;
	enum nvlsi_rpu_status status = NVLSI_RPU_STATUS_FAIL;

	list_node = nvlsi_rpu_osal_llist_get_node_head(opriv,
						       list);

	while (list_node) {
		data = nvlsi_rpu_osal_llist_node_data_get(opriv,
							  list_node);

		status = callbk_func(callbk_data,
				     data);

		if (status != NVLSI_RPU_STATUS_SUCCESS)
			goto out;

		list_node = nvlsi_rpu_osal_llist_get_node_nxt(opriv,
							      list,
							      list_node);
	}

out:
	return status;
}
