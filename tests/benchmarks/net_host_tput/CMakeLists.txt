#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(net_host_tput)

set(ZPERF_DIR ${ZEPHYR_BASE}/samples/net/zperf)

target_sources(app PRIVATE
  ${ZPERF_DIR}/src/main.c
  src/bench.c
  )

target_sources_ifdef(CONFIG_BENCH_NULL_ETH app PRIVATE src/null_eth.c)

if(CONFIG_NET_SAMPLE_CODE_RELOCATE)
  zephyr_code_relocate(LIBRARY subsys__net__ip
    LOCATION "${CONFIG_NET_SAMPLE_CODE_RAM_NAME}_TEXT" NOKEEP)
  zephyr_code_relocate(LIBRARY subsys__net
    LOCATION "${CONFIG_NET_SAMPLE_CODE_RAM_NAME}_TEXT" NOKEEP)
  zephyr_code_relocate(LIBRARY drivers__net
    LOCATION "${CONFIG_NET_SAMPLE_CODE_RAM_NAME}_TEXT" NOKEEP)
  zephyr_code_relocate(LIBRARY lib__net_buf
    LOCATION "${CONFIG_NET_SAMPLE_CODE_RAM_NAME}_TEXT" NOKEEP)
  zephyr_code_relocate(LIBRARY zperf
    LOCATION "${CONFIG_NET_SAMPLE_CODE_RAM_NAME}_TEXT" NOKEEP)
endif()

# Core-local TCM (APP_RAM0, renamed by tcm.overlay) is 32 KB, so only the
# UDP TX hot path fits; net_if.c (24 KB) stays in MRAM.
if(CONFIG_BENCH_LOCAL_RAM_HOTPATH)
  if(CONFIG_BENCH_NULL_ETH)
    set(l2_hot
      ${CMAKE_CURRENT_SOURCE_DIR}/src/null_eth.c
      ${ZEPHYR_BASE}/subsys/net/l2/ethernet/ethernet.c
      )
  else()
    set(l2_hot ${ZEPHYR_BASE}/drivers/net/loopback.c)
  endif()

  zephyr_code_relocate(FILES
    ${l2_hot}
    ${ZEPHYR_BASE}/subsys/net/ip/net_context.c
    ${ZEPHYR_BASE}/subsys/net/ip/net_pkt.c
    ${ZEPHYR_BASE}/subsys/net/ip/utils.c
    ${ZEPHYR_BASE}/subsys/net/ip/udp.c
    ${ZEPHYR_BASE}/subsys/net/ip/ipv4.c
    ${ZEPHYR_BASE}/subsys/net/ip/net_core.c
    ${ZEPHYR_BASE}/lib/net_buf/buf.c
    ${ZEPHYR_BASE}/lib/net_buf/buf_simple.c
    ${ZEPHYR_BASE}/subsys/net/lib/sockets/sockets.c
    ${ZEPHYR_BASE}/subsys/net/lib/sockets/sockets_inet.c
    ${ZEPHYR_BASE}/subsys/net/lib/zperf/zperf_udp_uploader.c
    LOCATION ${CONFIG_BENCH_HOTPATH_REGION} NOKEEP)
endif()
