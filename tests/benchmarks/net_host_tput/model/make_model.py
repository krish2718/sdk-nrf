#!/usr/bin/env python3
#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
"""Write the CAL-7627 throughput model as an XLSX with live formulas.

Usage: make_model.py <out.xlsx>

Pick a profile in the Results sheet and the table updates; it is laid out to paste into
Jira. Every number is a formula over the Inputs sheet, which has one column per profile.
"""

import sys

from openpyxl import Workbook
from openpyxl.styles import Alignment, Border, Font, PatternFill, Side
from openpyxl.utils import get_column_letter
from openpyxl.worksheet.datavalidation import DataValidation

FONT = "Arial"
NAVY = "0033A0"
WIKI = "https://nordicsemi.atlassian.net/wiki/spaces/~/pages/3562799288"
JIRA = "https://nordicsemi.atlassian.net/browse/CAL-7627"
RN00104 = "https://www.nxp.jp/docs/en/release-note/RN00104.pdf"

TYPE_FILL = {
    "Given": "E7E6E6",
    "Measured": "D9EAD3",
    "Estimate": "FCE5CD",
    "Cushion": "F4CCCC",
}
PROFILES = ("Optimistic", "Expected", "Cushioned", "Pessimistic")
PROFILE_TEXT = {
    "Optimistic": "Measured values and best-case estimates.",
    "Expected": "Middle value for every estimate.",
    "Cushioned": "Expected plus margin on every estimate.",
    "Pessimistic": "Every worst case at once: a floor, not a likely outcome.",
}
FIRST_PCOL = 3  # Inputs column C holds the first profile

# Inputs: (id, description, values per profile, unit, type, source, lever) or a section name
INPUTS = [
    "Clocks and packets",
    ("clk_lo", "Lower CM33 clock", (256,) * 4, "MHz", "Given", "CAL-7627 description", 0),
    ("clk_hi", "Higher CM33 clock", (320,) * 4, "MHz", "Given", "CAL-7627 description", 0),
    (
        "udp_payload",
        "UDP payload per datagram",
        (1472,) * 4,
        "B",
        "Given",
        "zperf 1472 B, 1500 B MTU",
        0,
    ),
    ("mss", "TCP payload per segment (MSS)", (1460,) * 4, "B", "Given", "1500 B MTU", 0),
    "Measured stack cost",
    (
        "cyc_lean",
        "UDP send through the stack into a null driver, checksum offload, hot path "
        "in single-cycle RAM, lean config",
        (10743,) * 4,
        "cycles/pkt",
        "Measured",
        "nRF7120 PDK at 256 MHz, wiki section 5 and appendix A",
        0,
    ),
    (
        "cpb_meas",
        "memcpy cost inside that measurement (data warm)",
        (1.09,) * 4,
        "cycles/B",
        "Measured",
        "1,598 cycles per 1472 B, wiki appendix B",
        0,
    ),
    (
        "tcm_gain",
        "Whole stack in TCM instead of only the hot path (cycle factor)",
        (0.91, 0.91, 0.93, 0.95),
        "x",
        "Measured",
        "-9% measured on loopback (wiki section 6); less in the margin profiles",
        0,
    ),
    (
        "clk_pen",
        "Cycle growth at the higher clock",
        (1.00, 1.03, 1.05, 1.08),
        "x",
        "Estimate",
        "Flat if all hot code is in TCM. The 54H hot-path build grew 15% from 128 to 320 MHz "
        "with the rest in MRAM",
        1,
    ),
    "Software path",
    (
        "drv_factor",
        "Throughput kept after adding the Wi-Fi driver",
        (0.85, 0.85, 0.82, 0.80),
        "x",
        "Estimate",
        "FPGA, driver only; IPC and RPU firmware not included",
        1,
    ),
    (
        "tcp_ratio",
        "TCP throughput as a fraction of UDP on the same CPU",
        (0.80, 0.75, 0.70, 0.65),
        "x",
        "Estimate",
        "ACKs, timers, retransmit queue. Other Zephyr hosts show 0.5-0.6 with a 64 KB window",
        1,
    ),
    "Network processor over SDIO",
    (
        "cpb",
        "memcpy of host data into the stack",
        (1.09, 1.6, 2.0, 2.4),
        "cycles/B",
        "Cushion",
        "1.09 warm (measured). Streaming data is cold; from 2-cycle RAM a checksum "
        "loop ran 2.25x slower (wiki appendix A)",
        1,
    ),
    (
        "sdio_cyc",
        "SDIO device side per 1460 B of payload: interrupts, DMA setup, host "
        "protocol and flow control",
        (200, 700, 1000, 1500),
        "cycles",
        "Estimate",
        "One interrupt per 16-32 KB transfer at best, per 512 B block or command at worst. "
        "No SDIO device exists on nRF71 today",
        1,
    ),
    (
        "contention",
        "Bus contention from SDIO, TSO and RPU DMA (cycle factor)",
        (1.00, 1.10, 1.12, 1.15),
        "x",
        "Cushion",
        "Each payload byte crosses SRAM about 4 times. Not measured",
        1,
    ),
    "TSO/GSO",
    (
        "segs_block",
        "Segments per TSO block",
        (44, 22, 16, 11),
        "segments",
        "Estimate",
        "64 / 32 / 23 / 16 KB blocks",
        0,
    ),
    (
        "tso_ovh",
        "CM33 cost of TSO per segment: header template, scatter list, completion",
        (100, 300, 400, 500),
        "cycles",
        "Estimate",
        "Zephyr chains small buffers, so a block needs a long scatter list",
        1,
    ),
    (
        "ack_cyc",
        "ACK receive per segment with TSO and software GRO",
        (1300, 2200, 2600, 3000),
        "cycles",
        "Estimate",
        "One ACK per two segments, every frame through the driver. Receive is not measured",
        1,
    ),
    "Radio and SDIO bus",
    (
        "radio_tcp",
        "Radio limit, TCP (80 MHz MCS9)",
        (385,) * 4,
        "Mbps",
        "Given",
        "throughput_targets_v6.xlsx: 128 A-MPDU, 0.8 us GI, 0% margin",
        0,
    ),
    (
        "radio_udp",
        "Radio limit, UDP (80 MHz MCS9)",
        (425,) * 4,
        "Mbps",
        "Given",
        "throughput_targets_v6.xlsx",
        0,
    ),
    ("phy", "PHY rate, HE MCS9, 1 stream, 80 MHz", (480,) * 4, "Mbps", "Given", "802.11ax", 0),
    (
        "eff_tcp",
        "Practical TCP efficiency (goodput / PHY)",
        (0.70,) * 4,
        "x",
        "Measured",
        "NXP IW612 on Linux, RN00104 4.6.2.2",
        0,
    ),
    (
        "eff_udp",
        "Practical UDP efficiency (goodput / PHY)",
        (0.78,) * 4,
        "x",
        "Measured",
        "NXP IW612 on Linux, RN00104 4.6.2.2",
        0,
    ),
    ("sdio_raw", "SDIO 3.0 SDR104 raw rate", (832,) * 4, "Mbps", "Given", "4 bit x 208 MHz", 0),
    (
        "sdio_eff",
        "SDIO usable fraction after commands, CRC and flow control",
        (0.75, 0.70, 0.65, 0.60),
        "x",
        "Estimate",
        "IW612 does 461-503 UDP over SDR104 on Linux",
        0,
    ),
]

COMPETITORS = [
    (
        "NXP IW612 (Murata 2EL)",
        "i.MX 8M Mini, 4x Cortex-A53 1.8 GHz, Linux",
        "802.11ac 80 MHz, MCS9 (433 PHY)",
        325,
        375,
        "NXP RN00104, 4.6.2.2",
        RN00104,
    ),
    (
        "NXP IW612 (Murata 2EL)",
        "i.MX 8M Mini, 4x Cortex-A53 1.8 GHz, Linux",
        "802.11ax 80 MHz, up to MCS11 (600 PHY)",
        405,
        461,
        "NXP RN00104, 4.6.2.2",
        RN00104,
    ),
    (
        "NXP IW612",
        "i.MX RT1060, Cortex-M7 600 MHz, FreeRTOS + lwIP",
        "802.11ax 80 MHz",
        88,
        219,
        "NXP MCUXpresso IW61x release notes",
        "https://mcuxpresso.nxp.com/mcuxsdk/25.06.00/html/middleware/wireless/"
        "WiFi-Bluetooth-802.15.4/topics/iw611-iw612-release-notes.html",
    ),
    (
        "NXP IW611 (Ezurio Sona NX611)",
        "i.MX RT1060",
        "802.11ax 80 MHz",
        86,
        221,
        "NXP SDK throughput page",
        "https://mcuxpresso.nxp.com/mcuxsdk/25.03.00-pvw2/html/middleware/wireless/"
        "WiFi-Bluetooth-802.15.4/topics/wi-fi_throughput_04.html",
    ),
]

# Model: (key, label, unit, formula, number format). {I:id} is an input of the column's
# profile, {M:key} another model row in the same column, {I:CLK} the column's clock.
MODEL = [
    "Software path",
    ("clock", "CM33 clock", "MHz", "={I:CLK}", "0"),
    ("pen", "Cycle growth at this clock", "x", "=IF({M:clock}={I:clk_lo},1,{I:clk_pen})", "0.00"),
    (
        "core",
        "Stack incl. one copy, UDP, per packet",
        "cycles",
        "={I:cyc_lean}*{I:tcm_gain}*{M:pen}",
        "#,##0",
    ),
    ("copy_meas", "of which the measured copy", "cycles", "={I:cpb_meas}*{I:udp_payload}", "#,##0"),
    ("stack", "of which the stack itself", "cycles", "={M:core}-{M:copy_meas}", "#,##0"),
    ("driver", "Wi-Fi driver per packet", "cycles", "={M:core}*(1/{I:drv_factor}-1)", "#,##0"),
    (
        "sw_udp",
        "UDP cycles per datagram",
        "cycles",
        "=({M:core}+{M:driver}+{I:sdio_cyc}+({I:cpb}-{I:cpb_meas})*{I:udp_payload})*{I:contention}",
        "#,##0",
    ),
    ("sw_udp_ceil", "UDP CPU ceiling", "Mbps", "={M:clock}*{I:udp_payload}*8/{M:sw_udp}", "0"),
    (
        "sw_tcp",
        "TCP cycles per segment",
        "cycles",
        "=(({M:core}+{M:driver})/{I:tcp_ratio}+{I:sdio_cyc}+({I:cpb}-{I:cpb_meas})*{I:mss})"
        "*{I:contention}",
        "#,##0",
    ),
    ("sw_tcp_ceil", "TCP CPU ceiling", "Mbps", "={M:clock}*{I:mss}*8/{M:sw_tcp}", "0"),
    "With TSO/GSO in the Wi-Fi chip",
    (
        "t_stack",
        "Stack per segment (once per block)",
        "cycles",
        "={M:stack}/{I:segs_block}",
        "#,##0",
    ),
    (
        "t_drv",
        "Driver and TSO handling per segment",
        "cycles",
        "={M:driver}/{I:segs_block}+{I:tso_ovh}",
        "#,##0",
    ),
    ("t_copy", "Payload copy per segment", "cycles", "={I:cpb}*{I:mss}", "#,##0"),
    ("t_ack", "ACK receive per segment", "cycles", "={I:ack_cyc}", "#,##0"),
    ("t_sdio", "SDIO per segment", "cycles", "={I:sdio_cyc}", "#,##0"),
    (
        "tso_tcp",
        "TCP cycles per segment",
        "cycles",
        "=({M:t_stack}+{M:t_drv}+{M:t_copy}+{M:t_ack}+{M:t_sdio})*{I:contention}",
        "#,##0",
    ),
    ("tso_tcp_ceil", "TCP CPU ceiling", "Mbps", "={M:clock}*{I:mss}*8/{M:tso_tcp}", "0"),
    (
        "tso_udp",
        "UDP cycles per datagram (USO)",
        "cycles",
        "=({M:t_stack}+{M:t_drv}+{I:cpb}*{I:udp_payload}+{M:t_sdio})*{I:contention}",
        "#,##0",
    ),
    ("tso_udp_ceil", "UDP CPU ceiling", "Mbps", "={M:clock}*{I:udp_payload}*8/{M:tso_udp}", "0"),
    "Other limits",
    ("radio_tcp", "Radio, TCP (target)", "Mbps", "={I:radio_tcp}", "0"),
    ("radio_udp", "Radio, UDP (target)", "Mbps", "={I:radio_udp}", "0"),
    ("prac_tcp", "Radio, TCP (practical)", "Mbps", "={I:phy}*{I:eff_tcp}", "0"),
    ("prac_udp", "Radio, UDP (practical)", "Mbps", "={I:phy}*{I:eff_udp}", "0"),
    ("sdio_bus", "SDIO bus usable", "Mbps", "={I:sdio_raw}*{I:sdio_eff}", "0"),
    "Result: lowest of CPU, radio target and SDIO",
]
for kind, label in (("sw", "Software only"), ("tso", "TSO/GSO")):
    for proto in ("tcp", "udp"):
        P = proto.upper()
        ceil = f"{{M:{kind}_{proto}_ceil}}"
        radio = f"{{M:radio_{proto}}}"
        MODEL += [
            (
                f"r_{kind}_{proto}",
                f"{label}, {P}",
                "Mbps",
                f"=MIN({ceil},{radio},{{M:sdio_bus}})",
                "0",
            ),
            (
                f"b_{kind}_{proto}",
                f"CM33 busy, {label.lower()} {P}",
                "%",
                f"={{M:r_{kind}_{proto}}}/{ceil}",
                "0%",
            ),
            (
                f"l_{kind}_{proto}",
                f"Limited by, {label.lower()} {P}",
                "",
                f'=IF({ceil}<=MIN({radio},{{M:sdio_bus}}),"CM33",'
                f'IF({radio}<={{M:sdio_bus}},"Radio","SDIO"))',
                "@",
            ),
        ]

# Model columns: profile x clock.
MCOLS = []
for i, p in enumerate(PROFILES):
    for j, clk in enumerate(("clk_lo", "clk_hi")):
        MCOLS.append((get_column_letter(3 + 2 * i + j), p, clk))
MFIRST, MLAST = MCOLS[0][0], MCOLS[-1][0]

OPTIONS = [
    ("256 MHz CM33 with SW optimisations and checksum offload (1)", "sw", 1),
    ("320 MHz CM33 with SW optimisations and checksum offload (1)", "sw", 2),
    ("256 MHz CM33 with additional TSO/GSO offload IP (2)", "tso", 1),
    ("320 MHz CM33 with additional TSO/GSO offload IP (2)", "tso", 2),
]

NOTES = [
    "(1) Code in TCM, checksum offload in RPU, extended TCP window, software overhead trim.",
    "(2) As per 1, with TSO/GSO hardware offload IP. Software GRO for the returning ACKs.",
    "(3) throughput_targets_v6.xlsx: 128 A-MPDU, 0.8 us GI, 0% margin.",
    "(4) NXP RN00104 4.6.2.2: Murata 2EL, iperf 5 streams, 2 MB window. See Competitors.",
]

thin = Side(style="thin", color="BFBFBF")
BORDER = Border(left=thin, right=thin, top=thin, bottom=thin)


def font(**kw):
    kw.setdefault("name", FONT)
    kw.setdefault("size", 10)
    return Font(**kw)


def fill(rgb):
    return PatternFill("solid", start_color=rgb, end_color=rgb)


def header(ws, row, values, col=1):
    for i, v in enumerate(values):
        c = ws.cell(row=row, column=col + i, value=v)
        c.font = font(bold=True, color="FFFFFF")
        c.fill = fill(NAVY)
        c.alignment = Alignment(wrap_text=True, vertical="center")
        c.border = BORDER


def title(ws, text, sub=None):
    ws["A1"] = text
    ws["A1"].font = font(size=14, bold=True, color=NAVY)
    if sub:
        ws["A2"] = sub
        ws["A2"].font = font(italic=True, color="595959")


def widths(ws, w):
    for i, v in enumerate(w, 1):
        ws.column_dimensions[get_column_letter(i)].width = v


def boxed(ws, row, ncols):
    for col in range(1, ncols + 1):
        ws.cell(row=row, column=col).border = BORDER


def build_inputs(wb):
    ws = wb.create_sheet("Inputs")
    title(
        ws,
        "Model inputs",
        "Edit the blue cells. Yellow cells are the main levers. Every result is a formula "
        "over this sheet.",
    )
    header(ws, 4, ["ID", "Input", *PROFILES, "Unit", "Type", "Source / why"])
    rows = {}
    r = 5
    for item in INPUTS:
        if isinstance(item, str):
            ws.cell(row=r, column=1, value=item).font = font(bold=True, color=NAVY)
            r += 1
            continue
        iid, desc, vals, unit, typ, src, lever = item
        ws.cell(row=r, column=1, value=iid).font = font(color="595959")
        ws.cell(row=r, column=2, value=desc).font = font()
        for j, v in enumerate(vals):
            c = ws.cell(row=r, column=FIRST_PCOL + j, value=v)
            c.font = font(color="0000FF")
            c.number_format = "#,##0" if isinstance(v, int) else "0.00"
            if lever:
                c.fill = fill("FFFF00")
        ucol = FIRST_PCOL + len(PROFILES)
        ws.cell(row=r, column=ucol, value=unit).font = font()
        t = ws.cell(row=r, column=ucol + 1, value=typ)
        t.font = font()
        t.fill = fill(TYPE_FILL[typ])
        ws.cell(row=r, column=ucol + 2, value=src).font = font(color="595959")
        for col in range(1, ucol + 3):
            ws.cell(row=r, column=col).border = BORDER
            ws.cell(row=r, column=col).alignment = Alignment(wrap_text=True, vertical="top")
        rows[iid] = r
        r += 1
    widths(ws, [14, 52, 12, 12, 12, 12, 11, 11, 70])
    ws.freeze_panes = "C5"
    return rows


def build_model(wb, irow):
    ws = wb.create_sheet("Model")
    title(
        ws,
        "Cycle model",
        "Formulas only. Columns are profile x clock; each column reads its profile column "
        "in Inputs.",
    )
    clk_name = {"clk_lo": "256 MHz", "clk_hi": "320 MHz"}
    header(ws, 4, ["Quantity", "Unit"] + [f"{p} {clk_name[k]}" for _, p, k in MCOLS])

    mrow = {}
    r = 5
    for item in MODEL:
        if not isinstance(item, str):
            mrow[item[0]] = r
        r += 1

    r = 5
    for item in MODEL:
        if isinstance(item, str):
            ws.cell(row=r, column=1, value=item).font = font(bold=True, color=NAVY)
            r += 1
            continue
        key, label, unit, tmpl, fmt = item
        ws.cell(row=r, column=1, value=label).font = font(bold=key.startswith("r_"))
        ws.cell(row=r, column=2, value=unit).font = font(color="595959")
        for col, prof, clk in MCOLS:
            pc = get_column_letter(FIRST_PCOL + PROFILES.index(prof))
            f = tmpl.replace("{I:CLK}", f"Inputs!${pc}${irow[clk]}")
            for iid, ir in irow.items():
                f = f.replace(f"{{I:{iid}}}", f"Inputs!${pc}${ir}")
            for mk, mr in mrow.items():
                f = f.replace(f"{{M:{mk}}}", f"{col}{mr}")
            assert "{" not in f, f
            c = ws[f"{col}{r}"]
            c.value = f
            c.number_format = fmt
            link_only = "{M:" not in tmpl
            c.font = font(color="008000" if link_only else "000000", bold=key.startswith("r_"))
            c.border = BORDER
        r += 1
    widths(ws, [44, 9] + [13] * len(MCOLS))
    ws.freeze_panes = "C5"
    return mrow


def build_results(wb, irow, mrow):
    ws = wb.create_sheet("Results", 0)
    title(ws, "CAL-7627 bottom line: upload, Wi-Fi 6, 1 stream, 80 MHz, MCS9 (480 Mbps PHY)")
    plist = (
        f"Inputs!${get_column_letter(FIRST_PCOL)}$4:"
        f"${get_column_letter(FIRST_PCOL + len(PROFILES) - 1)}$4"
    )

    ws["A3"] = "Profile"
    ws["A3"].font = font(bold=True)
    ws["B3"] = "Expected"
    ws["B3"].font = font(bold=True, color="0000FF")
    ws["B3"].fill = fill("FFFF00")
    ws["B3"].border = BORDER
    dv = DataValidation(type="list", formula1='"' + ",".join(PROFILES) + '"', allow_blank=False)
    dv.prompt = "Choose a profile; the table updates."
    dv.promptTitle = "Profile"
    ws.add_data_validation(dv)
    dv.add("B3")
    ws["C3"] = f"=MATCH(B3,{plist},0)"
    ws["C3"].font = font(color="FFFFFF")
    choose = ",".join(f'"{PROFILE_TEXT[p]}"' for p in PROFILES)
    ws["D3"] = f"=CHOOSE($C$3,{choose})"
    ws["D3"].font = font(italic=True, color="595959")
    ws["A4"] = "Copy A6:C13 into Jira. Columns D-E are detail."
    ws["A4"].font = font(italic=True, color="595959")

    def pick(key, clk):
        return f"INDEX(Model!${MFIRST}${mrow[key]}:${MLAST}${mrow[key]},($C$3-1)*2+{clk})"

    def r5(expr):
        return f"ROUND({expr}/5,0)*5"

    header(
        ws,
        6,
        ["Option", "TCP (Mbps)", "UDP (Mbps)", "Limited by (TCP / UDP)", "CM33 busy (TCP / UDP)"],
    )
    r = 7
    for name, kind, clk in OPTIONS:
        ws.cell(row=r, column=1, value=name)
        ws.cell(row=r, column=2, value="=" + r5(pick(f"r_{kind}_tcp", clk)))
        ws.cell(row=r, column=3, value="=" + r5(pick(f"r_{kind}_udp", clk)))
        ws.cell(
            row=r,
            column=4,
            value=f'={pick(f"l_{kind}_tcp", clk)}&" / "&{pick(f"l_{kind}_udp", clk)}',
        )
        ws.cell(
            row=r,
            column=5,
            value=f'=TEXT({pick(f"b_{kind}_tcp", clk)},"0%")&" / "&'
            f'TEXT({pick(f"b_{kind}_udp", clk)},"0%")',
        )
        r += 1

    def inp(iid):
        return (
            f"INDEX(Inputs!${get_column_letter(FIRST_PCOL)}${irow[iid]}:"
            f"${get_column_letter(FIRST_PCOL + len(PROFILES) - 1)}${irow[iid]},$C$3)"
        )

    extra = [
        (
            "Theoretical target for 80 MHz MCS9 (PHY = 480 Mbps) (3)",
            "=" + inp("radio_tcp"),
            "=" + inp("radio_udp"),
            "Radio",
            "",
        ),
        (
            "Radio, practical (IW612 efficiency at 480 Mbps PHY)",
            f"=ROUND({inp('phy')}*{inp('eff_tcp')},0)",
            f"=ROUND({inp('phy')}*{inp('eff_udp')},0)",
            "Radio",
            "",
        ),
        (
            "Best competitor, measured: NXP IW612 + i.MX 8M Mini, 4x Cortex-A53 1.8 GHz, Linux, "
            "802.11ac 80 MHz MCS9 (4)",
            "=Competitors!D5",
            "=Competitors!E5",
            "Radio",
            "4 cores at 1.8 GHz",
        ),
    ]
    for name, tcp, udp, lim, busy in extra:
        for col, v in enumerate((name, tcp, udp, lim, busy), 1):
            ws.cell(row=r, column=col, value=v)
        r += 1

    for row in range(7, r):
        for col in range(1, 6):
            c = ws.cell(row=row, column=col)
            c.border = BORDER
            c.alignment = Alignment(wrap_text=col == 1, vertical="center")
            c.font = font(bold=col in (2, 3))
            if col in (2, 3):
                c.number_format = "0"

    r += 1
    for n in NOTES:
        ws.cell(row=r, column=1, value=n).font = font(italic=True, color="595959")
        r += 1
    ws.cell(row=r, column=1, value=f"Study: {WIKI}").font = font(italic=True, color="595959")
    widths(ws, [70, 13, 13, 24, 22])
    ws.freeze_panes = "A7"


def build_compare(wb, mrow):
    ws = wb.create_sheet("Compare", 1)
    title(ws, "All profiles side by side", "Same numbers as Results, unrounded.")
    head = ["Option"] + [f"{p} {proto}" for p in PROFILES for proto in ("TCP", "UDP")]
    header(ws, 4, head)
    r = 5
    for name, kind, clk in OPTIONS:
        ws.cell(row=r, column=1, value=name).font = font()
        for i, _ in enumerate(PROFILES):
            mc = MCOLS[2 * i + clk - 1][0]
            for k, proto in enumerate(("tcp", "udp")):
                c = ws.cell(
                    row=r, column=2 + 2 * i + k, value=f"=Model!{mc}{mrow[f'r_{kind}_{proto}']}"
                )
                c.number_format = "0"
                c.font = font(color="008000")
        boxed(ws, r, len(head))
        r += 1
    widths(ws, [62] + [12] * (len(head) - 1))


def build_competitors(wb):
    ws = wb.create_sheet("Competitors")
    title(ws, "Published 1x1, 80 MHz throughput (upload = TX)", "Measured by the vendors.")
    header(ws, 4, ["Device", "Host and stack", "Mode", "TCP TX (Mbps)", "UDP TX (Mbps)", "Source"])
    for i, (dev, host, mode, tcp, udp, src, url) in enumerate(COMPETITORS, 5):
        for j, v in enumerate((dev, host, mode, tcp, udp, src), 1):
            c = ws.cell(row=i, column=j, value=v)
            c.font = font(color="0000FF" if j in (4, 5) else "000000")
            c.border = BORDER
            c.alignment = Alignment(wrap_text=True, vertical="top")
        ws.cell(row=i, column=6).hyperlink = url
        ws.cell(row=i, column=6).font = font(color="0563C1", underline="single")
    widths(ws, [28, 40, 34, 14, 14, 40])


def build_readme(wb):
    ws = wb.active
    ws.title = "README"
    title(ws, "CAL-7627 throughput model")
    lines = [
        (
            "What",
            "Upload throughput of a CM33 running Zephyr TCP/IP and the nRF71 Wi-Fi driver, "
            "fed over SDIO, with and without TSO/GSO hardware.",
        ),
        (
            "How to use",
            "Pick a profile in Results (cell B3); the table updates and can be "
            "pasted into Jira. To change the model, edit the blue cells in Inputs; yellow cells "
            "are the main levers.",
        ),
        ("Profiles", "; ".join(f"{p}: {PROFILE_TEXT[p]}" for p in PROFILES)),
        (
            "Chain",
            "Measured cycles per UDP packet (null driver) x whole-stack TCM factor x "
            "clock growth, plus the Wi-Fi driver, divided by the TCP ratio for TCP, plus SDIO and "
            "the extra cost of copying cold data, times bus contention. With TSO/GSO the stack "
            "and driver run once per block; copy, ACKs and SDIO stay per segment. The result is "
            "the lowest of the CPU ceiling, the radio target and the SDIO bus, rounded to 5.",
        ),
        ("Type colours", "Grey = given, green = measured, orange = estimate, red = cushion."),
        ("Cell colours", "Blue text = input, black = formula, green = link to another sheet."),
        (
            "Not modelled",
            "Silicon cost of TSO and SDIO device IP; RPU-side segmentation rate; "
            "TLS (needs a crypto engine); receive/download; host-side cost of moving data.",
        ),
        ("Study", WIKI),
        ("Jira", JIRA),
        (
            "Generator",
            "tests/benchmarks/net_host_tput/model/make_model.py (benchmarks/"
            "cal-7627-host-tput). Regenerate rather than hand-edit the structure.",
        ),
    ]
    for i, (k, v) in enumerate(lines, 3):
        ws.cell(row=i, column=1, value=k).font = font(bold=True)
        c = ws.cell(row=i, column=2, value=v)
        c.font = font()
        c.alignment = Alignment(wrap_text=True, vertical="top")
    r = len(lines) + 4
    ws.cell(row=r, column=1, value="Legend").font = font(bold=True)
    for j, (typ, rgb) in enumerate(TYPE_FILL.items(), 2):
        c = ws.cell(row=r, column=j, value=typ)
        c.fill = fill(rgb)
        c.font = font()
    widths(ws, [16, 100, 12, 12, 12])


def main(out):
    wb = Workbook()
    build_readme(wb)
    irow = build_inputs(wb)
    mrow = build_model(wb, irow)
    build_competitors(wb)
    build_results(wb, irow, mrow)
    build_compare(wb, mrow)
    for ws in wb.worksheets:
        ws.page_setup.orientation = "landscape"
        ws.sheet_properties.pageSetUpPr.fitToPage = True
        ws.page_setup.fitToWidth = 1
        ws.page_setup.fitToHeight = 0
    wb.active = 0
    wb.save(out)


if __name__ == "__main__":
    main(sys.argv[1])
