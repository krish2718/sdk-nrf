/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/ztest.h>
#include <zephyr/sys/printk.h>

#include <mbedtls/pk.h>
#include <mbedtls/x509.h>
#include <mbedtls/x509_crt.h>

#define AWS_S3_HOSTNAME "nrfconnectsdk.s3.eu-central-1.amazonaws.com"

static const unsigned char ca_pem[] = {
#include "ca_amazon_root.pem.inc"
	0x00
};

static const unsigned char server_chain_pem[] = {
#include "server_chain.pem.inc"
	0x00
};

/**
 * Print mbedTLS verify failure details. mbedtls_debug does not trace x509_crt_verify();
 * mbedtls_x509_crt_verify_info() and mbedtls_x509_crt_info() require
 * MBEDTLS_X509_REMOVE_INFO unset (see prj.conf).
 */
static void x509_dump_verify_failure(int ret, uint32_t flags,
				     const mbedtls_x509_crt *chain)
{
	const mbedtls_x509_crt *crt;
	int info_len;

	/* nrf_security often omits mbedtls_strerror (no MBEDTLS_ERROR_C); print code + flags. */
	printk("x509 verify: ret=%d flags=0x%x", ret, flags);
	if (ret == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED) {
		printk(" (MBEDTLS_ERR_X509_CERT_VERIFY_FAILED)");
	}
	printk("\n");

	printk("leaf public key type: %d\n", mbedtls_pk_get_type(&chain->pk));

#if !defined(MBEDTLS_X509_REMOVE_INFO)
	{
		char vinfobuf[384];
		char crtbuf[1024];

		info_len = mbedtls_x509_crt_verify_info(vinfobuf, sizeof(vinfobuf), "", flags);
		if (info_len >= 0) {
			printk("%s", vinfobuf);
		}

		for (crt = chain; crt != NULL; crt = crt->next) {
			info_len = mbedtls_x509_crt_info(crtbuf, sizeof(crtbuf), "  ", crt);
			if (info_len >= 0) {
				printk("%s", crtbuf);
			}
		}
	}
#else
	ARG_UNUSED(flags);
	printk("Set CONFIG_MBEDTLS_X509_REMOVE_INFO=n for verify_info + PEM dump.\n");
#endif
}

ZTEST(x509_amazon_s3_chain, test_verify_pem_chain)
{
	mbedtls_x509_crt ca;
	mbedtls_x509_crt chain;
	int ret;
	uint32_t flags = 0;

	mbedtls_x509_crt_init(&ca);
	mbedtls_x509_crt_init(&chain);

	ret = mbedtls_x509_crt_parse(&ca, ca_pem, sizeof(ca_pem));
	zassert_equal(ret, 0, "parse CA failed: %d", ret);

	ret = mbedtls_x509_crt_parse(&chain, server_chain_pem, sizeof(server_chain_pem));
	zassert_equal(ret, 0, "parse server chain failed: %d", ret);

	ret = mbedtls_x509_crt_verify(&chain, &ca, NULL, AWS_S3_HOSTNAME, &flags, NULL, NULL);
	if (ret != 0) {
		x509_dump_verify_failure(ret, flags, &chain);
	}
	zassert_equal(ret, 0, "verify failed: ret=%d flags=0x%x", ret, flags);

	mbedtls_x509_crt_free(&chain);
	mbedtls_x509_crt_free(&ca);
}

ZTEST_SUITE(x509_amazon_s3_chain, NULL, NULL, NULL, NULL, NULL);
