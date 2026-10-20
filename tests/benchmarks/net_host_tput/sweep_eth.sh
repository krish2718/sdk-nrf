#!/bin/sh
#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
# Checksum offload A/B through Ethernet L2 (null driver), hot path in TCM
# or MRAM, bench and lean config. Expects the build directories from
# build.sh. Usage: sweep_eth.sh [port] [variant...], default eth_sw eth_off
set -e
cd "$(dirname "$0")"
port=${1:-/dev/ttyACM0}
[ $# -gt 0 ] && shift
variants=${*:-eth_sw eth_off}

run() {
	build=$1
	tag=$2
	west flash -d "$build" > /dev/null 2>&1
	sleep 3
	python3 run.py --port "$port" --tag "$tag" --runs 2 --dur 10 \
		--pre "net arp add 192.0.2.2 02:00:5e:10:00:02" \
		--clk 320 128 64 --sizes 1472 1024 292 \
		--raw "results/$tag.log" | tee "results/$tag.csv"
}

for v in $variants; do
	run "build_$v" "$v"
done
