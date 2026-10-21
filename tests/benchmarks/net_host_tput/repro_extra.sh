#!/bin/sh
#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
# Runs behind the CSVs that the sweep scripts do not produce. Expects the
# build directories from build.sh (54H) or pdk_build.sh (7120).
#
#   pdk_mem        results/pdk_mem_*.csv: MRAM wait states, I-cache, microbench
#   mem            results/mem_*.csv: 54H I-cache misses and microbench per clock
#   pdk_crossover  results/pdk_crossover.csv: MRAM vs SRAM hot path vs size
#   pdk_ramblk     results/ramblk_*.csv: hot path code in RAM02/03 vs RAM01 vs RAM00
#   pdk_boot2boot  results/pdk_boot2boot.csv: 3 resets, 7120 offload + SRAM
#   boot2boot      results/boot2boot.csv: 3 resets, 54H lean offload + TCM
#   rebuild        results/eth_off_rebuild.csv: 54H offload + TCM, fresh build
#
# Usage: PORT=/dev/ttyACM0 SNR=<J-Link serial> repro_extra.sh <step>...
cd "$(dirname "$0")" || exit 1
port=${PORT:-/dev/ttyACM0}
arp="net arp add 192.0.2.2 02:00:5e:10:00:02"
hdr="tag,clk_req_mhz,cpu_mhz,size,run,mbps,mbps_per_mhz,cycles_per_pkt"

flash()
{
	west flash -d "$1" > /dev/null 2>&1 || { echo "flash failed $1"; exit 1; }
	sleep 3
}

reset()
{
	nrfutil device reset ${SNR:+--serial-number "$SNR"} > /dev/null 2>&1
	sleep 3
}

# probe <build> <tag> [--pre cmd] -- <case>...
probe()
{
	build=$1
	tag=$2
	shift 2
	flash "$build"
	python3 mem_probe.py --port "$port" --tag "$tag" --dur 5 \
		--raw "results/pdk_mem_$tag.log" "$@" > "results/pdk_mem_$tag.csv"
}

pdk_mem()
{
	probe build_pdk_1500 mram -- \
		"ws6:bench ws 6" "ws8:bench ws 8" "ws10:bench ws 10" \
		"ws12:bench ws 12" "ws16:bench ws 16" \
		"nocache:bench ws 6;bench icache off" "cache:bench icache on"
	# ws4 and ws3 are refused by MRAMC; bench ws reports it and the row
	# keeps the read-back value in mram_ws.
	probe build_pdk_1500 mram_low -- \
		"ws5:bench ws 5" "ws4:bench ws 4" "ws3:bench ws 3" "ws6:bench ws 6"
	for v in 1500 reloc1500 hot1500 hot1500_nvmcsum; do
		probe "build_pdk_$v" "$v" -- \
			"ws6:bench ws 6" "ws10:bench ws 10" "nocache:bench ws 6;bench icache off"
	done
	for v in eth_sw eth_off eth_sw_hot eth_off_hot eth_sw_hot_nvmcsum; do
		probe "build_pdk_$v" "$v" --pre "$arp" -- \
			"ws6:bench ws 6" "ws10:bench ws 10" "nocache:bench ws 6;bench icache off"
	done
}

mem()
{
	for v in eth_off_mram eth_sw_mram eth_off eth_off_mram_abl eth_off_abl; do
		flash "build_$v"
		set -- "c320:bench clk 320" "c128:bench clk 128" "c64:bench clk 64"
		[ "$v" = eth_off_mram ] && set -- "$@" "nocache320:bench clk 320;bench icache off"
		python3 mem_probe.py --port "$port" --tag "$v" --dur 5 --pre "$arp" \
			--raw "results/mem_$v.log" "$@" > "results/mem_$v.csv"
	done
}

pdk_crossover()
{
	echo "$hdr" > results/pdk_crossover.csv
	for v in 1500 hot1500; do
		flash "build_pdk_$v"
		python3 run.py --port "$port" --tag "pdk_x_$v" --runs 1 --dur 10 \
			--sizes 128 292 600 1472 | tail -n +2 >> results/pdk_crossover.csv
	done
}

# Same session: MRAM, code in RAM02/03 (2-cycle), RAM01 and RAM00 (single-cycle).
pdk_ramblk()
{
	for v in "eth_off:eth_off_mram:1" "eth_off_hot:eth_off_ram02:1" \
		"eth_off_hot_ram01:eth_off_ram01:1" "eth_off_hot_ram00:eth_off_ram00:1" \
		"eth_sw:eth_sw_mram:1" "eth_sw_hot:eth_sw_ram02:1" \
		"eth_sw_hot_ram01:eth_sw_ram01:1" "eth_sw_hot_ram00:eth_sw_ram00:1" \
		"1500:lo_mram:0" "hot1500:lo_ram02:0" "hot1500_ram01:lo_ram01:0" \
		"hot1500_ram00:lo_ram00:0" "eth_off_hot_abl_ram01:eth_off_abl_ram01:1"; do
		build=build_pdk_${v%%:*}
		rest=${v#*:}
		tag=${rest%%:*}
		set --
		[ "${rest#*:}" = 1 ] && set -- --pre "$arp"
		flash "$build"
		python3 run.py --port "$port" --tag "$tag" --runs 2 --dur 10 "$@" \
			--sizes 1472 1024 292 --raw "results/ramblk_$tag.log" \
			> "results/ramblk_$tag.csv"
		python3 mem_probe.py --port "$port" --tag "$tag" --dur 5 "$@" \
			--raw "results/ramblk_mem_$tag.log" "ws6:bench ws 6" \
			> "results/ramblk_mem_$tag.csv"
	done
}

# boot <build> <tag> <out> [run.py args]
boot()
{
	build=$1
	tag=$2
	out=$3
	shift 3
	flash "$build"
	echo "$hdr" > "$out"
	for i in 1 2 3; do
		reset
		python3 run.py --port "$port" --tag "${tag}_boot$i" --dur 10 --pre "$arp" \
			--sizes 1472 "$@" | tail -n +2 >> "$out"
	done
}

for step in "$@"; do
	case $step in
	pdk_mem) pdk_mem ;;
	mem) mem ;;
	pdk_crossover) pdk_crossover ;;
	pdk_ramblk) pdk_ramblk ;;
	pdk_boot2boot)
		boot build_pdk_eth_off_hot pdk_eth_off_hot results/pdk_boot2boot.csv --runs 2 ;;
	boot2boot)
		boot build_eth_off_abl eth_off_abl results/boot2boot.csv --runs 1 --clk 320 ;;
	rebuild)
		west build -p -b nrf54h20dk/nrf54h20/cpuapp . -d build_eth_off -- \
			"-DEXTRA_CONF_FILE=overlay-mtu1500.conf;overlay-nulleth.conf" \
			-DCONFIG_BENCH_LOCAL_RAM_HOTPATH=y -DEXTRA_DTC_OVERLAY_FILE=tcm.overlay \
			-DCONFIG_BENCH_NULL_ETH_CSUM_OFFLOAD=y || exit 1
		flash build_eth_off
		python3 run.py --port "$port" --tag eth_off_rebuild --runs 2 --dur 10 \
			--pre "$arp" --clk 320 128 64 --sizes 1472 1024 292 \
			--raw results/eth_off_rebuild.log > results/eth_off_rebuild.csv ;;
	*) echo "unknown step $step" ;;
	esac
done
