#!/bin/sh
#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
# nRF7120 PDK sweep over the pdk_build.sh variants. The app core runs at a
# fixed 256 MHz, so there are no clock steps.
# Usage: PORT=/dev/ttyACM0 pdk_sweep.sh base 1500 reloc hot eth_sw eth_off eth_sw_hot eth_off_hot
cd "$(dirname "$0")" || exit 1
port=${PORT:-/dev/ttyACM0}

run()
{
	build=$1
	tag=$2
	shift 2
	if ! west flash -d "$build" > "results/$tag.flashlog" 2>&1; then
		echo "flash failed $build"
		return
	fi
	sleep 3
	python3 run.py --port "$port" --tag "$tag" --runs 2 --dur 10 \
		--raw "results/$tag.log" "$@" > "results/$tag.csv"
	echo "done $tag"
}

for v in "$@"; do
	case $v in
	base) run build_pdk_base pdk_base --sizes 1024 292 ;;
	1500) run build_pdk_1500 pdk_mtu1500 --sizes 1472 1024 292 ;;
	reloc) run build_pdk_reloc1500 pdk_reloc1500 --sizes 1472 1024 292 ;;
	hot) run build_pdk_hot1500 pdk_hot1500 --sizes 1472 1024 292 ;;
	eth_sw | eth_off | eth_sw_hot | eth_off_hot)
		run "build_pdk_$v" "pdk_$v" --pre "net arp add 192.0.2.2 02:00:5e:10:00:02" \
			--sizes 1472 1024 292 ;;
	*) echo "unknown variant $v" ;;
	esac
done
