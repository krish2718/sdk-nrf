#!/bin/sh
#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
# Flash each loopback variant and run the zperf UDP TX sweep.
# Expects the build directories from build.sh. Usage: sweep.sh [port]
set -e
cd "$(dirname "$0")"
port=${1:-/dev/ttyACM0}

run() {
	build=$1
	tag=$2
	shift 2
	west flash -d "$build" > /dev/null 2>&1
	sleep 3
	python3 run.py --port "$port" --tag "$tag" --runs 2 --dur 10 \
		--raw "results/$tag.log" "$@" | tee "results/$tag.csv"
}

run build_1500 mtu1500 --clk 320 128 64 --sizes 1472 1024 292
run build_local1500 tcm1500 --clk 320 128 64 --sizes 1472 1024 292
run build_reloc1500 reloc1500 --clk 320 128 64 --sizes 1472 1024 292
