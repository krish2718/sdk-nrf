#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(x509_amazon_s3_chain)

set(gen_dir ${CMAKE_CURRENT_BINARY_DIR}/certs)
zephyr_include_directories(${gen_dir})

generate_inc_file_for_target(app data/ca_amazon_root.pem ${gen_dir}/ca_amazon_root.pem.inc)
generate_inc_file_for_target(app data/server_chain.pem ${gen_dir}/server_chain.pem.inc)

target_sources(app PRIVATE src/main.c)
