#!/usr/bin/env python3
#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
"""Memory probe: MRAM wait states (nRF7120), I-cache hit/miss per packet and
checksum/memcpy cost, using the bench shell commands over UART.

Each case is "<label>:<setup>" where setup is a ';' separated list of shell
commands run before the measurement, for example "ws5:bench ws 5" or
"nocache:bench icache off". An empty setup measures the current state.
"""

import argparse
import re
import sys
import time

from run import RATE_RE, SCALE, Shell

PROMPT = re.compile(r"uart:~\$ (\x1b\[m)?$")
HZ_RE = re.compile(r"cpu_hz=(\d+)")
WS_RE = re.compile(r"mram_ws=(\d+)")
PROF_RE = re.compile(r"icache_hit=(\d+) icache_miss=(\d+)")
# TX-only runs print "Num packets: 0 (<sent>)", the sent count is in brackets.
PKT_RE = re.compile(r"Num packets:\s+\d+\s+\((\d+)\)")
MICRO_RE = re.compile(r"cycles_per_call=(\d+) milli_cycles_per_byte=(\d+)")


def run_cmd(sh, line, until=PROMPT, timeout=5):
    m, out = sh.cmd(line, until, timeout)
    if not m:
        print(f"# no match for {line!r}: {out[-300:]!r}", file=sys.stderr)
    return m, out


def main():
    ap = argparse.ArgumentParser(allow_abbrev=False)
    ap.add_argument("--port", default="/dev/ttyACM0")
    ap.add_argument("--tag", default="")
    ap.add_argument("--size", type=int, default=1472)
    ap.add_argument("--dur", type=int, default=10)
    ap.add_argument("--dest", default="192.0.2.2")
    ap.add_argument("--pre", action="append", default=[])
    ap.add_argument("--raw", default=None)
    ap.add_argument("cases", nargs="+")
    args = ap.parse_args()

    sh = Shell(args.port)
    sh.ser.write(b"\r\n")
    time.sleep(0.5)

    _, out = run_cmd(sh, "bench mem")
    for line in out.splitlines():
        if "=" in line:
            print(f"# {line.strip()}")
    for line in args.pre:
        run_cmd(sh, line)

    print("tag,case,mram_ws,size,mbps,cycles_per_pkt,hit_per_pkt,miss_per_pkt,csum_cyc,memcpy_cyc")
    for case in args.cases:
        label, _, setup = case.partition(":")
        for line in filter(None, (s.strip() for s in setup.split(";"))):
            run_cmd(sh, line)

        # A setup step may change the clock (bench clk on nRF54H20).
        m, _ = run_cmd(sh, "bench freq", HZ_RE)
        hz = int(m.group(1))

        _, out = run_cmd(sh, "bench mem")
        m = WS_RE.search(out)
        ws = m.group(1) if m else ""

        m, _ = run_cmd(sh, f"bench csum {args.size} 20000", MICRO_RE, 20)
        csum = m.group(1) if m else ""
        m, _ = run_cmd(sh, f"bench memcpy {args.size} 20000", MICRO_RE, 20)
        mcpy = m.group(1) if m else ""

        run_cmd(sh, "bench prof clear")
        line = f"zperf udp upload {args.dest} 5001 {args.dur} {args.size} 1000M"
        m, out = run_cmd(sh, line, RATE_RE, args.dur + 15)
        if not m:
            continue
        mbps = float(m.group(1)) * SCALE[m.group(2)]
        pm = PKT_RE.search(out)
        mp, _ = run_cmd(sh, "bench prof show", PROF_RE)

        pkts = int(pm.group(1)) if pm else 0
        pps = mbps * 1e6 / (8 * args.size)
        cyc = hz / pps if pps else 0
        hit = miss = ""
        if pkts and mp:
            hit = f"{int(mp.group(1)) / pkts:.0f}"
            miss = f"{int(mp.group(2)) / pkts:.0f}"
        print(
            f"{args.tag},{label},{ws},{args.size},{mbps:.2f},{cyc:.0f},{hit},{miss},{csum},{mcpy}",
            flush=True,
        )
        time.sleep(1)

    if args.raw:
        with open(args.raw, "w") as f:
            f.write("\n".join(sh.log))


if __name__ == "__main__":
    main()
