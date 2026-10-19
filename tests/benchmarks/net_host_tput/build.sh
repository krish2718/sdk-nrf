#!/bin/sh
#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
# Build every variant used for the CAL-7627 numbers on nRF54H20DK.
# Build directories land next to this script, where sweep*.sh expect them.
#
#   build_base       loopback, MTU 1100, code in MRAM       -> results/base.csv
#   build_1500       loopback, MTU 1500, code in MRAM       -> results/mtu1500.csv
#   build_reloc      loopback, net code in global RAM       -> results/reloc.csv
#   build_reloc1500  same, MTU 1500                         -> results/reloc1500.csv
#   build_local1500  loopback, UDP TX hot path in TCM       -> results/tcm1500.csv
#   build_eth_sw     null Ethernet L2, TCM, SW checksum     -> results/eth_sw.csv
#   build_eth_off    null Ethernet L2, TCM, csum offload    -> results/eth_off.csv
set -e
cd "$(dirname "$0")"
board=nrf54h20dk/nrf54h20/cpuapp
reloc="-DCONFIG_NET_SAMPLE_CODE_RELOCATE=y -DCONFIG_CODE_DATA_RELOCATION_SRAM=y"
tcm="-DCONFIG_BENCH_LOCAL_RAM_HOTPATH=y -DEXTRA_DTC_OVERLAY_FILE=tcm.overlay"
eth="-DEXTRA_CONF_FILE=overlay-mtu1500.conf;overlay-nulleth.conf"

b() {
	dir=$1
	shift
	west build -p -b "$board" . -d "$dir" -- "$@"
}

b build_base
b build_1500 -DEXTRA_CONF_FILE=overlay-mtu1500.conf
# shellcheck disable=SC2086
b build_reloc $reloc
# shellcheck disable=SC2086
b build_reloc1500 -DEXTRA_CONF_FILE=overlay-mtu1500.conf $reloc
# shellcheck disable=SC2086
b build_local1500 -DEXTRA_CONF_FILE=overlay-mtu1500.conf $tcm
# shellcheck disable=SC2086
b build_eth_sw "$eth" $tcm
# shellcheck disable=SC2086
b build_eth_off "$eth" $tcm -DCONFIG_BENCH_NULL_ETH_CSUM_OFFLOAD=y
