#!/usr/bin/env python3
#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
"""Drive the net_host_tput shell over UART and collect zperf UDP TX results."""

import argparse
import re
import sys
import time

import serial

# TX-only loopback never gets a server report, so take the client rate.
RATE_RE = re.compile(r"Rate:\t+\S+ \S+\t\(([\d.]+) (Kbps|Mbps|Gbps|bps)\)")
HZ_RE = re.compile(r"cpu_hz=(\d+)")
SCALE = {"bps": 1e-6, "Kbps": 1e-3, "Mbps": 1.0, "Gbps": 1e3}


class Shell:
    def __init__(self, port, baud=115200):
        self.ser = serial.Serial(port, baud, timeout=0.2)
        self.log = []

    def cmd(self, line, until, timeout):
        self.ser.reset_input_buffer()
        self.ser.write((line + "\r\n").encode())
        buf = ""
        end = time.time() + timeout
        while time.time() < end:
            chunk = self.ser.read(4096).decode(errors="replace")
            if chunk:
                buf += chunk
                m = until.search(buf)
                if m:
                    self.log.append(buf)
                    return m, buf
        self.log.append(buf)
        return None, buf


def main():
    ap = argparse.ArgumentParser(allow_abbrev=False)
    ap.add_argument("--port", default="/dev/ttyACM0")
    ap.add_argument("--clk", type=int, nargs="*", default=[])
    ap.add_argument("--sizes", nargs="+", default=["1024", "292"])
    ap.add_argument("--dur", type=int, default=10)
    ap.add_argument("--runs", type=int, default=3)
    ap.add_argument("--rate", default="1000M")
    ap.add_argument("--dest", default="192.0.2.2")
    ap.add_argument("--tag", default="")
    ap.add_argument("--raw", default=None)
    ap.add_argument(
        "--pre", action="append", default=[], help="shell command to run once before the sweep"
    )
    args = ap.parse_args()

    sh = Shell(args.port)
    sh.ser.write(b"\r\n")
    time.sleep(0.5)
    for line in args.pre:
        _, out = sh.cmd(line, re.compile(r"uart:~\$ \S*$"), 3)
        print(f"# {line}: {out.strip()[-120:]!r}", file=sys.stderr)

    clks = args.clk or [None]
    print("tag,clk_req_mhz,cpu_mhz,size,run,mbps,mbps_per_mhz,cycles_per_pkt")
    for clk in clks:
        if clk is not None:
            m, out = sh.cmd(f"bench clk {clk}", HZ_RE, 10)
        else:
            m, out = sh.cmd("bench freq", HZ_RE, 5)
        if not m:
            print(f"# no cpu_hz for clk={clk}: {out!r}", file=sys.stderr)
            continue
        hz = int(m.group(1))
        mhz = hz / 1e6
        for size in args.sizes:
            for run in range(args.runs):
                line = f"zperf udp upload {args.dest} 5001 {args.dur} {size} {args.rate}"
                m, out = sh.cmd(line, RATE_RE, args.dur + 15)
                if not m:
                    print(f"# no rate: {out[-400:]!r}", file=sys.stderr)
                    continue
                mbps = float(m.group(1)) * SCALE[m.group(2)]
                payload = int(size[:-1]) * 1024 if size.endswith("K") else int(size)
                pps = mbps * 1e6 / (8 * payload)
                cyc = hz / pps if pps else 0
                print(
                    f"{args.tag},{clk or ''},{mhz:.1f},{payload},{run},"
                    f"{mbps:.2f},{mbps / mhz:.3f},{cyc:.0f}",
                    flush=True,
                )
                time.sleep(1)

    if args.raw:
        with open(args.raw, "w") as f:
            f.write("\n".join(sh.log))


if __name__ == "__main__":
    main()
