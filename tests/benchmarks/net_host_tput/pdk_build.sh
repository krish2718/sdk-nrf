#!/bin/sh
#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
# Build every variant used for the CAL-7627 numbers on the nRF7120 PDK.
# nRF7120 has no core-local RAM, so relocated code goes to RAM02+03
# (sramcode.overlay). Usage: pdk_build.sh [all|base|sram|abl]
#
#   build_pdk_base         loopback, MTU 1100, MRAM            -> results/pdk_base.csv
#   build_pdk_1500         loopback, MTU 1500, MRAM            -> results/pdk_mtu1500.csv
#   build_pdk_reloc1500    loopback, net stack in SRAM         -> results/pdk_reloc1500.csv
#   build_pdk_hot1500      loopback, UDP TX hot path in SRAM   -> results/pdk_hot1500.csv
#   build_pdk_eth_sw       null Ethernet L2, SW checksum       -> results/pdk_eth_sw.csv
#   build_pdk_eth_off      null Ethernet L2, csum offload      -> results/pdk_eth_off.csv
#   build_pdk_eth_sw_hot   SW checksum, hot path in SRAM       -> results/pdk_eth_sw_hot.csv
#   build_pdk_eth_off_hot  same, UDP TX hot path in SRAM       -> results/pdk_eth_off_hot.csv
#   build_pdk_*_nvmcsum    hot path in SRAM, checksum left in cached MRAM
#   build_pdk_*_tc0        no TX thread (NET_TC_TX_COUNT=0)
#   build_pdk_*_abl        no TX thread, no net stats, no net logging (overlay-ablate.conf)
#
# mem_probe.py runs on the same builds for MRAM wait-state and I-cache
# numbers (results/pdk_mem_*.csv).
cd "$(dirname "$0")" || exit 1
board=nrf7120dk/nrf7120/cpuapp
dtc="-DEXTRA_DTC_OVERLAY_FILE=sramcode.overlay"

b()
{
	dir=$1
	shift
	if west build -p -b "$board" . -d "$dir" -- "$@" > "$dir.buildlog" 2>&1; then
		echo "ok $dir"
	else
		echo "FAIL $dir, see $dir.buildlog"
	fi
}

base()
{
	b build_pdk_base
	b build_pdk_1500 -DEXTRA_CONF_FILE=overlay-mtu1500.conf
	b build_pdk_eth_sw "-DEXTRA_CONF_FILE=overlay-mtu1500.conf;overlay-nulleth.conf"
	b build_pdk_eth_off "-DEXTRA_CONF_FILE=overlay-mtu1500.conf;overlay-nulleth.conf" \
		-DCONFIG_BENCH_NULL_ETH_CSUM_OFFLOAD=y
}

sram()
{
	b build_pdk_reloc1500 "-DEXTRA_CONF_FILE=overlay-mtu1500.conf;overlay-pdk-reloc.conf" \
		$dtc
	b build_pdk_hot1500 "-DEXTRA_CONF_FILE=overlay-mtu1500.conf;overlay-pdk-hot.conf" $dtc
	b build_pdk_eth_sw_hot \
		"-DEXTRA_CONF_FILE=overlay-mtu1500.conf;overlay-nulleth.conf;overlay-pdk-hot.conf" \
		$dtc
	b build_pdk_eth_off_hot \
		"-DEXTRA_CONF_FILE=overlay-mtu1500.conf;overlay-nulleth.conf;overlay-pdk-hot.conf" \
		-DCONFIG_BENCH_NULL_ETH_CSUM_OFFLOAD=y $dtc
	b build_pdk_hot1500_nvmcsum \
		"-DEXTRA_CONF_FILE=overlay-mtu1500.conf;overlay-pdk-hot.conf" \
		-DCONFIG_BENCH_HOTPATH_CSUM_IN_NVM=y $dtc
	b build_pdk_eth_sw_hot_nvmcsum \
		"-DEXTRA_CONF_FILE=overlay-mtu1500.conf;overlay-nulleth.conf;overlay-pdk-hot.conf" \
		-DCONFIG_BENCH_HOTPATH_CSUM_IN_NVM=y $dtc
}

abl()
{
	eth="overlay-mtu1500.conf;overlay-nulleth.conf"
	off=-DCONFIG_BENCH_NULL_ETH_CSUM_OFFLOAD=y

	b build_pdk_eth_off_hot_tc0 "-DEXTRA_CONF_FILE=$eth;overlay-pdk-hot.conf" $off \
		-DCONFIG_NET_TC_TX_COUNT=0 $dtc
	b build_pdk_eth_off_hot_abl \
		"-DEXTRA_CONF_FILE=$eth;overlay-pdk-hot.conf;overlay-ablate.conf" $off $dtc
	b build_pdk_eth_off_abl "-DEXTRA_CONF_FILE=$eth;overlay-ablate.conf" $off
}

case ${1:-all} in
all) base; sram; abl ;;
base) base ;;
sram) sram ;;
abl) abl ;;
*) echo "usage: $0 [all|base|sram|abl]"; exit 1 ;;
esac
