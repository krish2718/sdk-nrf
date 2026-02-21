/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(wifi_keys, CONFIG_WIFI_KEYS_LOG_LEVEL);

#include <string.h>

#include <stdint.h>
#include <cracen/lib_kmu.h>

#include <zephyr/sys/__assert.h>

#include "wifi_keys.h"

static const uint32_t WIFI_KEYS_KEY_INDEX_INVALID = ~0;

/* Layout must match secure_crypto_reg_write() in fmac_api.c (RPU secure RAM).
 * See fmac_structs.h: VIF_* and PEER_* key layout constants.
 */
#define WIFI_KEYS_RAM_BASE           0x28400000
#define WIFI_KEYS_VIF_KEY_DB_OFFSET  0x1000
#define WIFI_KEYS_VIF_MIC_LEN_PER_VIF  0x40
#define WIFI_KEYS_VIF_KEY_LEN_PER_VIF  0x80
#define WIFI_KEYS_VIF_MIC_LEN_PER_KEYID  0x10
#define WIFI_KEYS_VIF_KEY_LEN_PER_KEYID  0x20
#define WIFI_KEYS_PEER_MIC_LEN_16    0x10
#define WIFI_KEYS_PEER_UCST_KEY_LEN  0x20
#define WIFI_KEYS_PEER_BCST_KEY_LEN  0x20
#define WIFI_KEYS_PEER_KEY_TOTAL_LEN 0xf0

static bool wifi_keys_key_type_is_mic(wifi_keys_key_type_t type)
{
	return type == PEER_UCST_MIC || type == PEER_BCST_MIC || type == VIF_MIC;
}

/* Reverse bytes within a single 16-byte half. Crypto expects each key half (slot) to be
 * stored with reversed byte order; the two halves must not be swapped. Reversing the
 * whole key would swap halves and break decryption.
 */
static void wifi_keys_reverse_16(uint8_t *dst, const uint8_t *src)
{
	for (int i = 0; i < 16; i++) {
		dst[i] = src[15 - i];
	}
}

static uint32_t wifi_keys_kmu_slot_id(wifi_keys_key_type_t type, uint32_t db_id, uint32_t key_index)
{
	/* TODO Coordinate with other users to reserve slots for Wi-Fi. */

	(void)type;
	(void)db_id;
	(void)key_index;

	return 97;
}

static int wifi_keys_set_key_id(psa_key_attributes_t *attr, uint32_t db_id, uint32_t key_index)
{
	if (db_id >= 8) {
		LOG_ERR("Invalid db_id: %d", db_id);
		return 1;
	}
	if (key_index >= 4) {
		LOG_ERR("Invalid key_index: %d", key_index);
		return 1;
	}

	/* Arbitrary key ID scheme - builtin key. */
	psa_key_id_t id = 0x7F000000 | ('W' << 16) | ('C' << 8) | (db_id << 2) | (key_index);

	psa_set_key_id(attr, id);
	return 0;
}

static uint32_t wifi_keys_get_key_start_addr(wifi_keys_key_type_t type, uint32_t db_id,
					     uint32_t key_index)
{
	uint32_t address_offset;
	bool mic = wifi_keys_key_type_is_mic(type);

	LOG_INF("wifi_keys_get_key_start_addr: type=%d, db_id=%u, key_index=%u, mic=%d",
		type, db_id, key_index, mic);

	if (type == VIF_ENC || type == VIF_MIC) {
		if (db_id >= 4) {
			LOG_INF("VIF: Invalid db_id %u (>=4)", db_id);
			return WIFI_KEYS_KEY_INDEX_INVALID;
		}
		if (key_index >= 4) {
			LOG_INF("VIF: Invalid key_index %u (>=4)", key_index);
			return WIFI_KEYS_KEY_INDEX_INVALID;
		}
		/* Same as program_if_key() in fmac_api.c */
		if (mic) {
			address_offset = WIFI_KEYS_VIF_KEY_DB_OFFSET +
				(db_id * (WIFI_KEYS_VIF_MIC_LEN_PER_VIF + WIFI_KEYS_VIF_KEY_LEN_PER_VIF)) +
				key_index * WIFI_KEYS_VIF_MIC_LEN_PER_KEYID;
		} else {
			address_offset = WIFI_KEYS_VIF_KEY_DB_OFFSET +
				(db_id * (WIFI_KEYS_VIF_MIC_LEN_PER_VIF + WIFI_KEYS_VIF_KEY_LEN_PER_VIF)) +
				WIFI_KEYS_VIF_MIC_LEN_PER_VIF +
				key_index * WIFI_KEYS_VIF_KEY_LEN_PER_KEYID;
		}
	} else { /* PEER: same as program_peer_key() in fmac_api.c */
		if (db_id >= 8) {
			LOG_INF("PEER: Invalid db_id %u (>=8)", db_id);
			return WIFI_KEYS_KEY_INDEX_INVALID;
		}
		if (type == PEER_UCST_ENC || type == PEER_UCST_MIC) {
			address_offset = db_id * WIFI_KEYS_PEER_KEY_TOTAL_LEN +
				(mic ? 0 : WIFI_KEYS_PEER_MIC_LEN_16);
		} else { /* PEER_BCST */
			if (key_index >= 4) {
				LOG_INF("PEER_BCST: Invalid key_index %u (>=4)", key_index);
				return WIFI_KEYS_KEY_INDEX_INVALID;
			}
			if (mic) {
				address_offset = db_id * WIFI_KEYS_PEER_KEY_TOTAL_LEN +
					WIFI_KEYS_PEER_MIC_LEN_16 + WIFI_KEYS_PEER_UCST_KEY_LEN +
					(key_index * WIFI_KEYS_PEER_MIC_LEN_16);
			} else {
				address_offset = db_id * WIFI_KEYS_PEER_KEY_TOTAL_LEN +
					WIFI_KEYS_PEER_MIC_LEN_16 + WIFI_KEYS_PEER_UCST_KEY_LEN +
					(4 * WIFI_KEYS_PEER_MIC_LEN_16) +
					(key_index * WIFI_KEYS_PEER_BCST_KEY_LEN);
			}
		}
	}

	uint32_t addr = WIFI_KEYS_RAM_BASE + address_offset;
	LOG_INF("Calculated RAM address: 0x%08X", addr);

	return addr;
}

uint32_t wifi_keys_get_key_size_in_bytes(wifi_keys_key_type_t type)
{
	return wifi_keys_key_type_is_mic(type) ? 16 : 32;
}

uint32_t wifi_keys_get_key_size_in_bits(wifi_keys_key_type_t type)
{
	return wifi_keys_get_key_size_in_bytes(type) * 8;
}

static int wifi_keys_kmu_provision_and_push(const uint8_t *key, wifi_keys_key_type_t type,
					    uint32_t db_id, uint32_t key_index, uint32_t key_slot)
{
	struct kmu_src src;
	int ret = 0;
	int ret_secondary = 0;
	uint32_t key_len = wifi_keys_get_key_size_in_bytes(type);

	/* PCIe path uses revMemCopy; reverse key bytes so RPU sees same order as PCIe path. */
	wifi_keys_reverse_16((uint8_t *)&src.value, key);
	src.rpolicy = LIB_KMU_REV_POLICY_ROTATING;
	src.dest = wifi_keys_get_key_start_addr(type, db_id, key_index);
	if (src.dest == WIFI_KEYS_KEY_INDEX_INVALID) {
		LOG_ERR("Invalid destination address (db_id: %d, key_index: %d)", db_id, key_index);
		return 0;
	}
	src.metadata = 0;

	char key_str[33] = {0};
	for (int i = 0; i < 16; i++) {
		snprintf(&key_str[i * 2], 3, "%02x", key[i]);
	}
	LOG_INF("wifi_keys: slot %u: key=%s", key_slot, key_str);

#ifdef CONFIG_WIFI_KEYS_KMU_VERIFY_COPY
	/* Verify: (1) push incrementing bytes 0x00..0x0f to readable buffer, compare for endianness.
	 * (2) push real key to same buffer, compare to key. KMU dest must be 16-byte aligned.
	 */
	{
		void *verify_buf = k_aligned_alloc(16, 16);
		int ret_verify;

		if (verify_buf != NULL) {
			struct kmu_src verify_src;
			uint8_t inc_bytes[16];
			uint32_t orig_dest = src.dest;
			uint8_t *dest_bytes = (uint8_t *)verify_buf;

			for (int i = 0; i < 16; i++) {
				inc_bytes[i] = (uint8_t)i;
			}
			memcpy(&verify_src.value, inc_bytes, 16);
			verify_src.rpolicy = LIB_KMU_REV_POLICY_ROTATING;
			verify_src.dest = (uint32_t)verify_buf;
			verify_src.metadata = 0;
			ret_verify = lib_kmu_provision_slot(key_slot, &verify_src);
			if (ret_verify == 0) {
				ret_verify = lib_kmu_push_slot(key_slot);
				if (ret_verify == 0) {
					LOG_INF("wifi_keys: KMU verify dest (inc): %02x%02x%02x%02x"
						"%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
						dest_bytes[0], dest_bytes[1], dest_bytes[2], dest_bytes[3],
						dest_bytes[4], dest_bytes[5], dest_bytes[6], dest_bytes[7],
						dest_bytes[8], dest_bytes[9], dest_bytes[10], dest_bytes[11],
						dest_bytes[12], dest_bytes[13], dest_bytes[14], dest_bytes[15]);
					if (memcmp(verify_buf, inc_bytes, 16) == 0) {
						LOG_INF("wifi_keys: KMU verify PASS (inc 0x00..0x0f preserved)");
					} else {
						LOG_ERR("wifi_keys: KMU verify FAIL (inc: dest != 0x00..0x0f)");
					}
				}
				lib_kmu_revoke_slot(key_slot);
			}
			if (ret_verify == 0) {
				uint8_t rev_key[16];

				wifi_keys_reverse_16(rev_key, key);
				memcpy(&verify_src.value, rev_key, 16);
				verify_src.dest = (uint32_t)verify_buf;
				ret_verify = lib_kmu_provision_slot(key_slot, &verify_src);
				if (ret_verify == 0) {
					ret_verify = lib_kmu_push_slot(key_slot);
					if (ret_verify == 0) {
						if (memcmp(verify_buf, rev_key, 16) == 0) {
							LOG_INF("wifi_keys: KMU verify PASS (key matches)");
						} else {
							LOG_ERR("wifi_keys: KMU verify FAIL (key: dest != key)");
						}
					}
					lib_kmu_revoke_slot(key_slot);
				}
			}
			k_free(verify_buf);
			src.dest = orig_dest;
		}
	}
#endif

	/* Provision and push primary slot */
	ret = lib_kmu_provision_slot(key_slot, &src);
	if (ret) {
		LOG_ERR("Failed to provision key (db_id: %d, key_index: %d): %d", db_id, key_index, ret);
		goto revoke_and_return;
	}
	ret = lib_kmu_push_slot(key_slot);
	if (ret) {
		LOG_ERR("Failed to push key (db_id: %d, key_index: %d): %d", db_id, key_index, ret);
		goto revoke_and_return;
	}

	/* If key is 32 bytes, provision and push secondary (next) slot */
	if (key_len == 32) {
		wifi_keys_reverse_16((uint8_t *)&src.value, key + 16);
		src.dest += 16;

		memset(key_str, 0, sizeof(key_str));
		for (int i = 0; i < 16; i++) {
			snprintf(&key_str[i * 2], 3, "%02x", key[16 + i]);
		}
		LOG_INF("wifi_keys: slot %u: key=%s", key_slot + 1, key_str);

		ret_secondary = lib_kmu_provision_slot(key_slot + 1, &src);
		if (ret_secondary) {
			LOG_ERR("Failed to provision second key (db_id: %d, key_index: %d): %d", db_id, key_index, ret_secondary);
			ret = ret_secondary;
			goto revoke_and_return;
		}
		ret_secondary = lib_kmu_push_slot(key_slot + 1);
		if (ret_secondary) {
			LOG_ERR("Failed to push second key (db_id: %d, key_index: %d): %d", db_id, key_index, ret_secondary);
			ret = ret_secondary;
			goto revoke_and_return;
		}
		LOG_INF("Successfully provisioned and pushed key (db_id: %d, key_index: %d)", db_id, key_index);

		/* Always attempt to revoke both slots, regardless of errors above */
		ret_secondary = lib_kmu_revoke_slot(key_slot + 1);
		if (ret_secondary) {
			LOG_ERR("Failed to revoke second key slot (db_id: %d, key_index: %d): %d", db_id, key_index, ret_secondary);
			/* Save the error but continue to revoke primary slot as well */
			ret = ret_secondary;
		} else {
			LOG_INF("Successfully revoked second key slot (db_id: %d, key_index: %d)", db_id, key_index);
		}
	}

	/* Always attempt to revoke the primary slot */
	{
		int revoke_primary = lib_kmu_revoke_slot(key_slot);
		if (revoke_primary) {
			LOG_ERR("Failed to revoke key (db_id: %d, key_index: %d): %d", db_id, key_index, revoke_primary);
			ret = revoke_primary;
		} else {
			LOG_INF("Successfully revoked key (db_id: %d, key_index: %d)", db_id, key_index);
		}
	}

revoke_and_return:
	return ret;
}

psa_key_attributes_t wifi_keys_key_attributes_init(wifi_keys_key_type_t type, uint32_t db_id,
						   uint32_t key_index)
{
	psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;

	wifi_keys_set_key_id(&attr, db_id, key_index);

	psa_key_persistence_t persistence = PSA_KEY_PERSISTENCE_VOLATILE;

	psa_key_lifetime_t lifetime = PSA_KEY_LIFETIME_FROM_PERSISTENCE_AND_LOCATION(
		persistence, PSA_KEY_LOCATION_WIFI_KEYS);

	psa_set_key_lifetime(&attr, lifetime);
	psa_set_key_type(&attr, (psa_key_type_t)type);

	return attr;

	/* Note: Usage flags and algorithm are deliberately not set (kept at 0),
	 * because software has no permission to use this key for any purpose
	 * (only Wi-Fi Crypto hardware can use it), and because Wi-Fi Crypto
	 * key locations can be reused for different algorithms).
	 */
}

psa_status_t wifi_keys_import_key(const psa_key_attributes_t *attr, const uint8_t *data,
				  size_t data_length, uint8_t *key_buffer, size_t key_buffer_size,
				  size_t *key_buffer_length, size_t *key_bits)
{
	__ASSERT_NO_MSG(key_buffer);
	__ASSERT_NO_MSG(key_buffer_length);
	__ASSERT_NO_MSG(key_bits);

	if (key_buffer_size == 0) {
		LOG_ERR("Invalid key buffer size: %d", key_buffer_size);
		return PSA_ERROR_INVALID_ARGUMENT;
	}

	psa_key_lifetime_t lifetime = psa_get_key_lifetime(attr);
	psa_key_location_t location = PSA_KEY_LIFETIME_GET_LOCATION(lifetime);
	psa_key_persistence_t persistence = PSA_KEY_LIFETIME_GET_PERSISTENCE(lifetime);
	psa_key_id_t id = psa_get_key_id(attr);

	LOG_INF("Importing key to PSA, location: %d, persistence: %d, lifetime: %d, id: 0x%08X",
		location, persistence, lifetime, id);
	if (location == PSA_KEY_LOCATION_WIFI_KEYS) {
		wifi_keys_key_type_t type = (wifi_keys_key_type_t)psa_get_key_type(attr);
		uint32_t db_id = (id >> 2) & 0x7;
		uint32_t key_index = id & 0x3;

		LOG_INF("Key ID: %d, DB ID: %d, Key Index: %d", id, db_id, key_index);

		if (data_length != wifi_keys_get_key_size_in_bytes(type)) {
			LOG_ERR("Invalid key data length: %d, expected: %d", data_length,
				wifi_keys_get_key_size_in_bytes(type));
			return PSA_ERROR_INVALID_ARGUMENT;
		}

		/* Output parameters not used, set to arbitrary values. */
		*key_buffer_length = 32; /* max key size in bytes */
		*key_bits = wifi_keys_get_key_size_in_bits(type);
		memset(key_buffer, 0, *key_buffer_length);

		if (persistence == PSA_KEY_PERSISTENCE_VOLATILE) {
			uint32_t key_slot = wifi_keys_kmu_slot_id(type, db_id, key_index);
			int ret = wifi_keys_kmu_provision_and_push((const uint8_t *)data, type,
								   db_id, key_index, key_slot);
			if (ret) {
				LOG_ERR("Failed to provision and push key: %d", ret);
			}
			return ret;
		}
		LOG_ERR("Invalid persistence: %d", persistence);
		return PSA_ERROR_INVALID_ARGUMENT;
	}
	return PSA_ERROR_NOT_SUPPORTED;
}

psa_status_t wifi_keys_destroy_key(const psa_key_attributes_t *attr)
{
	psa_key_lifetime_t lifetime = psa_get_key_lifetime(attr);
	psa_key_location_t location = PSA_KEY_LIFETIME_GET_LOCATION(lifetime);
	psa_key_persistence_t persistence = PSA_KEY_LIFETIME_GET_PERSISTENCE(lifetime);
	psa_key_id_t id = psa_get_key_id(attr);

	if (location == PSA_KEY_LOCATION_WIFI_KEYS) {
		wifi_keys_key_type_t type = (wifi_keys_key_type_t)psa_get_key_type(attr);
		uint32_t db_id = (id >> 2) & 0x7;
		uint32_t key_index = id & 0x3;

		if (persistence == PSA_KEY_PERSISTENCE_DEFAULT) {
			uint32_t key_slot = wifi_keys_kmu_slot_id(type, db_id, key_index);
			int ret = lib_kmu_revoke_slot(key_slot);

			if (ret) {
				LOG_ERR("Failed to revoke key: %d", ret);
			}
			return ret;
		}
		LOG_ERR("Invalid persistence: %d", persistence);
		return PSA_ERROR_INVALID_ARGUMENT;
	}
	return PSA_ERROR_NOT_SUPPORTED;
}
