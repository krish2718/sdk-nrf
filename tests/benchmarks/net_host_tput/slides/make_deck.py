#!/usr/bin/env python3
#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
"""Build the CAL-7627 first-cut deck on the Nordic corporate template.

Usage: make_deck.py <template.pptx> <out.pptx>

The template is any deck that carries the NOD_PresentationTemplate masters;
its first slide (photo intro) is reused and every other slide is dropped.
"""

import sys

from lxml import etree
from pptx import Presentation
from pptx.chart.data import CategoryChartData
from pptx.dml.color import RGBColor
from pptx.enum.chart import XL_CHART_TYPE, XL_LABEL_POSITION, XL_LEGEND_POSITION
from pptx.enum.text import MSO_ANCHOR, PP_ALIGN
from pptx.oxml.ns import qn
from pptx.util import Inches, Pt

BLUE = RGBColor(0x00, 0xA9, 0xCE)
NAVY = RGBColor(0x00, 0x33, 0xA0)
MID = RGBColor(0x00, 0x77, 0xC8)
DARK = RGBColor(0x33, 0x3F, 0x48)
GREY = RGBColor(0x76, 0x86, 0x92)
LIGHT = RGBColor(0xD9, 0xE1, 0xE2)
RED = RGBColor(0xFF, 0x58, 0x5D)
YELLOW = RGBColor(0xFF, 0xCD, 0x00)
WHITE = RGBColor(0xFF, 0xFF, 0xFF)
THEME_FONT = "+mn-lt"
MEAS = "TO MEASURE"

ZEPHYR_ROWS = [
    [
        "nRF54H20DK (ours)",
        "M33 @ 320",
        "null Ethernet",
        "UDP TX 258",
        "0.81",
        "csum offload, hot path in TCM",
        "this work",
    ],
    [
        "nRF7120 PDK (ours)",
        "M33 @ 256",
        "null Ethernet",
        "UDP TX 177",
        "0.69",
        "csum offload, hot path in SRAM",
        "this work",
    ],
    [
        "i.MX93 EVK",
        "A55 @ <= 1700",
        "1G ENET",
        "UDP RX 619",
        ">= 0.36",
        "cacheable RX buffers",
        "PR 82210 (merged)",
    ],
    [
        "MIMXRT1170-EVKB",
        "M7 @ 996",
        "1G ENET_1G",
        "UDP RX ~477, TX 200-512; TCP TX ~200",
        "0.2-0.51",
        "deeper rings, IRQ coalescing",
        "PR 118810 (open)",
    ],
    [
        "TI AM62L EVM",
        "A53 @ <= 1250",
        "1G CPSW3G",
        "UDP TX ~460; TCP TX ~230",
        ">= 0.37 / 0.18",
        "new DMA driver, SW csum",
        "PR 109327 (open)",
    ],
    [
        "Renesas EK-RA8M2",
        "M85 @ <= 1000",
        "1G RMAC",
        "UDP TX 279; TCP TX 158 (1000 B)",
        ">= 0.28 / 0.16",
        "driver fix",
        "issue 109078",
    ],
    [
        "Avnet ZedBoard",
        "A9 @ 667",
        "1G GEM",
        "UDP ~240; TCP ~130",
        "~0.36 / 0.19",
        "L2 cache on",
        "PR 111154 / 111833",
    ],
]
ZEPHYR_NOTES = [
    "Only UDP has crossed 400 on Zephyr, and only on ~1 GHz cores. Their 0.2-0.5 Mbps/MHz "
    "includes a real MAC driver and ours (0.69-0.81) has none, so per-MHz numbers are not "
    "like-for-like, here or against other stacks.",
    "Same RT1170 board: NXP bare-metal SDK does ~916 Mbps UDP vs ~477 on Zephyr. The stack is the "
    "limit, not the MAC.",
    "No Zephyr TCP above ~230 Mbps anywhere. tcp.c parses window scaling but never applies it, "
    "so the window is capped at 64 KB: ~262 Mbps at 2 ms RTT, ~105 at 5 ms. TCP > 350 needs "
    "this fixed first.",
]
TASK3_ROWS = [
    [
        "nRF54H20DK (ours)",
        "M33 @ 320",
        "Zephyr, sockets",
        "UDP TX 258",
        "0.81",
        "csum offload, hot path in TCM",
    ],
    [
        "nRF7120 PDK (ours)",
        "M33 @ 256",
        "Zephyr, sockets",
        "UDP TX 177",
        "0.69",
        "csum offload, hot path in SRAM",
    ],
    ["Zynq-7000 ZC702", "A9 @ 667", "lwIP, sockets", "TCP TX 542 / RX 521", "0.81", "HW csum"],
    [
        "Zynq-7000 ZC702",
        "A9 @ 667",
        "lwIP, RAW API",
        "TCP TX 949 / RX 943",
        ">= 1.42",
        "same HW, no socket layer (GbE cap)",
    ],
    ["ZynqMP ZCU102", "A53 @ 1100", "lwIP, sockets", "TCP RX 887", "0.81", "HW csum"],
    [
        "i.MX RT1170 (user report)",
        "M7 @ ~1000",
        "lwIP, bare metal",
        "TCP RX 390 / TX 150",
        "0.39 / 0.15",
        "GbE MAC, driver/buffer bound",
    ],
    [
        "RT1180 + AW611, HE80",
        "M33 @ 240 / M7 @ 792",
        "lwIP + FreeRTOS",
        "UDP TX 221 / RX 190, TCP 86 / 95",
        "n/a",
        "core not stated; same radio as below",
    ],
    [
        "i.MX 8M + IW612, HE80",
        "4x A53",
        "Linux",
        "UDP 461 / 502, TCP 405 / 418",
        "multi-core",
        "GSO/GRO, big windows, 5 streams",
    ],
    [
        "SiWG917, 20 MHz",
        "network CPU",
        "TCP/IP offloaded",
        "UDP 66, TCP 56",
        "-",
        "the one offload part found",
    ],
]
TASK3_NOTES = [
    "Our 0.69-0.81 Mbps/MHz (null driver) matches lwIP sockets on A9/A53 (0.81, real MAC), so "
    "per MHz is not like-for-like. lwIP RAW callbacks are >= 1.75x its sockets on the same A9, "
    "but that is TCP capped by GbE; the gain for Zephyr UDP is unknown.",
    "Same NXP radio: 86-221 Mbps with lwIP on an MCU host, 405-502 with Linux. The host stack "
    "makes the gap. The RT1180 page (MCUXpresso SDK 25.03 Wi-Fi throughput) does not say which "
    "core ran the test, so no Mbps/MHz.",
    "Linux UDP GSO 2.4x, GRO 1.8x bytes per cycle (LPC 2018). No published Zephyr GbE numbers on "
    "any MCU.",
]


def layout(prs, name):
    for m in prs.slide_masters:
        for lay in m.slide_layouts:
            if lay.name == name:
                return lay
    raise KeyError(name)


def drop_slides_after_first(prs):
    sld_ids = prs.slides._sldIdLst
    for sld in list(sld_ids)[1:]:
        prs.part.drop_rel(sld.get(qn("r:id")))
        sld_ids.remove(sld)


def add_slide_number(slide):
    tb = slide.shapes.add_textbox(Inches(9.33), Inches(5.21), Inches(0.42), Inches(0.3))
    p = tb.text_frame.paragraphs[0]
    p.alignment = PP_ALIGN.RIGHT
    fld = etree.SubElement(
        p._p, qn("a:fld"), id="{B6F15528-21DE-4FAA-801E-634DDDAF4B2B}", type="slidenum"
    )
    rpr = etree.SubElement(fld, qn("a:rPr"), lang="en-US", sz="800")
    fill = etree.SubElement(rpr, qn("a:solidFill"))
    etree.SubElement(fill, qn("a:srgbClr"), val="768692")
    t = etree.SubElement(fld, qn("a:t"))
    t.text = "<#>"


def set_title(slide, text, sub=None):
    slide.shapes.title.text = text
    if sub is not None:
        for ph in slide.placeholders:
            if ph.placeholder_format.idx == 15:
                ph.text = sub
                ph.text_frame.paragraphs[0].runs[0].font.size = Pt(14)


def bullet(p, char="\u2022"):
    ppr = p._p.get_or_add_pPr()
    ppr.set("marL", "171450")
    ppr.set("indent", "-171450")
    etree.SubElement(ppr, qn("a:buFont"), typeface="Arial")
    etree.SubElement(ppr, qn("a:buChar"), char=char)


def fill_bullets(tf, items, size=12, bullets=False):
    """items: list of str or (str, level) or (runs, level) where runs = [(text, bold)]."""
    tf.clear()
    tf.word_wrap = True
    first = True
    for it in items:
        lvl = 0
        if isinstance(it, tuple):
            it, lvl = it
        p = tf.paragraphs[0] if first else tf.add_paragraph()
        first = False
        p.level = lvl
        if bullets:
            bullet(p)
        runs = it if isinstance(it, list) else [(it, False)]
        for text, bold in runs:
            r = p.add_run()
            r.text = text
            r.font.bold = bold
            r.font.name = THEME_FONT
            r.font.size = Pt(size - 1 if lvl else size)


def body_ph(slide, idx):
    for ph in slide.placeholders:
        if ph.placeholder_format.idx == idx:
            return ph
    raise KeyError(idx)


def text_box(
    slide,
    x,
    y,
    w,
    h,
    runs,
    size=11,
    color=DARK,
    bold=False,
    align=PP_ALIGN.LEFT,
    anchor=MSO_ANCHOR.TOP,
    italic=False,
):
    tb = slide.shapes.add_textbox(Inches(x), Inches(y), Inches(w), Inches(h))
    tf = tb.text_frame
    tf.word_wrap = True
    tf.vertical_anchor = anchor
    tf.margin_left = tf.margin_right = Inches(0.05)
    if isinstance(runs, str):
        runs = [runs]
    for i, line in enumerate(runs):
        p = tf.paragraphs[0] if i == 0 else tf.add_paragraph()
        p.alignment = align
        r = p.add_run()
        r.text = line
        r.font.name = THEME_FONT
        r.font.size = Pt(size)
        r.font.bold = bold
        r.font.italic = italic
        r.font.color.rgb = color
    return tb


def card(slide, x, y, w, h, fill=LIGHT):
    from pptx.enum.shapes import MSO_SHAPE

    shp = slide.shapes.add_shape(
        MSO_SHAPE.ROUNDED_RECTANGLE, Inches(x), Inches(y), Inches(w), Inches(h)
    )
    shp.adjustments[0] = 0.08
    shp.fill.solid()
    shp.fill.fore_color.rgb = fill
    shp.line.fill.background()
    shp.shadow.inherit = False
    return shp


def stat(slide, x, y, w, value, label, color):
    card(slide, x, y, w, 1.15)
    text_box(slide, x + 0.12, y + 0.08, w - 0.24, 0.6, value, size=22, color=color, bold=True)
    text_box(slide, x + 0.12, y + 0.66, w - 0.24, 0.45, label, size=9, color=DARK)


def table(slide, x, y, w, rows, col_w, size=9, header_fill=NAVY, row_h=0.26, bold_rows=()):
    shp = slide.shapes.add_table(
        len(rows), len(rows[0]), Inches(x), Inches(y), Inches(w), Inches(row_h * len(rows))
    )
    tbl = shp.table
    for i, cw in enumerate(col_w):
        tbl.columns[i].width = Inches(cw)
    for r, row in enumerate(rows):
        tbl.rows[r].height = Inches(row_h)
        for c, val in enumerate(row):
            cell = tbl.cell(r, c)
            cell.margin_left = cell.margin_right = Inches(0.05)
            cell.margin_top = cell.margin_bottom = Inches(0.02)
            cell.vertical_anchor = MSO_ANCHOR.MIDDLE
            tf = cell.text_frame
            tf.word_wrap = True
            tf.paragraphs[0].text = ""
            run = tf.paragraphs[0].add_run()
            run.text = str(val)
            run.font.name = THEME_FONT
            run.font.size = Pt(size)
            cell.fill.solid()
            if r == 0:
                cell.fill.fore_color.rgb = header_fill
                run.font.bold = True
                run.font.color.rgb = WHITE
            else:
                cell.fill.fore_color.rgb = WHITE if r % 2 else RGBColor(0xF2, 0xF5, 0xF6)
                run.font.color.rgb = DARK
                run.font.bold = r in bold_rows
    return shp


def style_chart(chart, size=9):
    chart.has_title = False
    chart.font.name = THEME_FONT
    chart.font.size = Pt(size)
    chart.font.color.rgb = DARK


def notes(slide, text):
    slide.notes_slide.notes_text_frame.text = text


def build(src, out):
    prs = Presentation(src)
    drop_slides_after_first(prs)

    # 1. Title
    s = prs.slides[0]
    s.shapes.title.text = "TCP/IP throughput on CM33"
    body_ph(
        s, 1
    ).text = "CAL-7627 first cut: what the Zephyr stack does today, and what 400 Mbps needs"
    body_ph(s, 10).text = "Chaitanya Tata\nOctober 2026"

    # 2. Bottom line
    s = prs.slides.add_slide(layout(prs, "13_full content"))
    set_title(s, "Bottom line")
    add_slide_number(s)
    body_ph(s, 13)._element.getparent().remove(body_ph(s, 13)._element)
    stat(
        s,
        0.67,
        1.3,
        2.75,
        "258 / 177 Mbps",
        "Best UDP TX, stack only, bench config\nnRF54H20 @ 320 / nRF7120 @ 256",
        BLUE,
    )
    stat(
        s,
        3.62,
        1.3,
        2.75,
        "> 400 Mbps",
        "CAL-7627 UDP target\n(TCP > 350, 80 MHz MCS9, 480 PHY)",
        RED,
    )
    stat(
        s,
        6.57,
        1.3,
        2.76,
        "1.55x / 2.26x",
        "short of 400, stack only, bench config\nnRF54H20 @ 320 / nRF7120 @ 256",
        NAVY,
    )
    tb = s.shapes.add_textbox(Inches(0.67), Inches(2.65), Inches(8.66), Inches(2.4))
    fill_bullets(
        tb.text_frame,
        [
            [
                ("Task 1: NO-GO on both boards. ", True),
                (
                    "With nothing below the stack, nRF54H20DK does 258 Mbps at 320 MHz and the "
                    "nRF7120 PDK 177 at 256. A lean config (no TX thread, no net stats/log) "
                    "gives 267 / 205, still 1.50x / 1.95x short. Driver, IPC and FW add on top. "
                    "UDP TX measured, TCP inferred.",
                    False,
                ),
            ],
            [
                ("Checksum offload pays on both. ", True),
                (
                    "+19% (TCM) / +18% (MRAM) on 54H, +32% (SRAM) / +14% (MRAM) on 7120. "
                    "At 1472 B (54H @ 320, 7120 @ 256) the stack costs 1.2-2.4 MHz per Mbps.",
                    False,
                ),
            ],
            [
                ("Local RAM is where they differ. ", True),
                (
                    "With offload, moving part of the UDP TX path into 54H TCM is +61% over MRAM "
                    "(159 to 256); into 7120 SRAM only +9%. MRAM stalls cost far more at 320 MHz.",
                    False,
                ),
            ],
            [
                ("Per-packet cost is the wall. ", True),
                (
                    "Best case 14.1k (54H) / 14.7k (7120) cycles per 1472 B packet in the lean "
                    "build, ~90% fixed per packet. 400 Mbps needs far fewer packets through the "
                    "stack (USO/TSO, GRO) or TCP/IP off the host.",
                    False,
                ),
            ],
        ],
        size=11,
        bullets=True,
    )
    for p in tb.text_frame.paragraphs:
        p.space_after = Pt(5)
    notes(
        s,
        "All numbers are UDP TX only, zperf, Zephyr stack, measured on each board's app core. "
        "Mbps is UDP payload goodput (1472 B payload per packet, no L2/L3 headers); the 400 Mbps "
        "target is taken on the same basis. 54H at its 320 MHz DVFS point, nRF7120 at its fixed "
        "256 MHz. TCP and RX are not measured yet. TCP > 350 is inferred from UDP TX being the "
        "cheaper path: TCP adds ACK RX, timers and retransmit state on top.\n\n"
        "Every A/B percentage compares builds run in the same session; the 258 / 177 headlines "
        "are the original sweeps. 54H TCM vs MRAM uses the same-day rebuild (255.9 vs 159.2, "
        "+61%; +62% against 257.7). 7120 lean uses its same-day probe build (177.9).\n\n"
        "Bench config = zperf sample defaults: NET_TC_TX_COUNT=1 (TX thread), net stats and net "
        "logging on. Lean config (overlay-ablate.conf) on the 7120, offload + SRAM hot path: "
        "TX thread off 177.9 (same-day rebuild) to 194.7 (+9.4%), stats/log off as well 204.7 "
        "(+15%, 16,945 to 14,734 cycles/pkt; +15.8% against the 176.8 sweep). On the 54H, "
        "offload + TCM, against a same-day rebuild of the bench "
        "config (255.9): TX thread off 265.7 (+3.8%), lean 267.2 (+4.4%, 14,080 cycles/pkt). "
        "With code in MRAM lean helps much more, 159.2 to 202.1 (+27%): less code, fewer "
        "misses. The 258 headline is the original run; the rebuild came in 0.7% lower, inside "
        "the ~1% layout error bar.\n\n"
        "Offload gain in MRAM on 54H: 135.4 to 159.2 (+18%, results/eth_*_mram.csv).\n\n"
        "MHz per Mbps at 1472 B: 54H 1.20 (lean, offload, TCM) to 2.36 (SW csum, MRAM); 7120 "
        "1.25 (lean) to 1.91 (SW csum, SRAM hot path). The earlier 1.8-1.9 rule only held for "
        "one pairing.",
    )

    # 3. How we measured
    s = prs.slides.add_slide(layout(prs, "13_sub_full content"))
    set_title(
        s,
        "How we measured",
        "Same Zephyr stack on two CM33 app cores, Wi-Fi replaced by a null driver",
    )
    add_slide_number(s)
    ph = body_ph(s, 10)
    ph._element.getparent().remove(ph._element)
    stack = [
        ("zperf UDP upload (1472 B, 1000M)", NAVY),
        ("BSD sockets", MID),
        ("UDP / IPv4 / net_pkt", MID),
        ("Ethernet L2 (ARP static)", MID),
        ("null Ethernet driver: count and return", BLUE),
    ]
    for i, (t, col) in enumerate(stack):
        c = card(s, 0.67, 1.8 + i * 0.62, 3.6, 0.5, fill=col)
        tf = c.text_frame
        tf.text = t
        tf.paragraphs[0].runs[0].font.size = Pt(10)
        tf.paragraphs[0].runs[0].font.color.rgb = WHITE
        tf.paragraphs[0].runs[0].font.bold = True
    text_box(
        s,
        0.67,
        4.9,
        3.6,
        0.3,
        "Hot path code in TCM (54H) or a carved-out SRAM region (7120)",
        size=8,
        color=GREY,
        italic=True,
    )
    tb = s.shapes.add_textbox(Inches(4.55), Inches(1.75), Inches(4.78), Inches(3.4))
    fill_bullets(
        tb.text_frame,
        [
            [
                ("nRF54H20DK: ", True),
                ("M33, 16 KB I + 16 KB D cache, 32 KB TCM, DVFS 64/128/320 MHz.", False),
            ],
            [
                ("nRF7120 PDK: ", True),
                ("M33F, 8 KB I-cache, no TCM, fixed 256 MHz (nrf7120dk/nrf7120/cpuapp).", False),
            ],
            [
                ("Loopback TX-only: ", True),
                (
                    "zperf UDP to the peer address, dummy L2. The driver drops every packet at "
                    "send() (zperf sample, drop ratio 1), so no RX. Checksum always in SW.",
                    False,
                ),
            ],
            [
                ("Ethernet L2: ", True),
                (
                    "zperf UDP to a static ARP peer. The null driver counts and returns; the "
                    "stack frees the packet after send(). TX only, no copy, no wire.",
                    False,
                ),
            ],
            [
                ("Code placement: ", True),
                (
                    "MRAM + cache (default), whole net stack in RAM, or a partial UDP TX hot "
                    "path in local RAM. Offload on/off by Kconfig.",
                    False,
                ),
            ],
            [
                ("Clock and runs: ", True),
                (
                    "clock measured once per clock step (SysTick vs GRTC), 2 x 10 s per point. "
                    "Runs and reboots agree within ~0.1%; a rebuilt image moves results ~1% "
                    "(code layout vs cache).",
                    False,
                ),
            ],
            [
                ("Config: ", True),
                (
                    "zperf sample defaults (TX thread, net stats and log on). A lean build is "
                    "+4% (54H) / +15% (7120).",
                    False,
                ),
            ],
        ],
        size=10,
        bullets=True,
    )
    for p in tb.text_frame.paragraphs:
        p.space_after = Pt(4)
    notes(
        s,
        "Cycles per packet = CPU Hz / packets per second. Mbps/MHz = Mbps / measured CPU MHz. "
        "Mbps is the zperf client send rate in both setups.\n\n"
        "Loopback vs Ethernet L2: both are TX-only and send to 192.0.2.2 (run.py --dest). "
        "Loopback (CONFIG_NET_LOOPBACK) uses the dummy L2, so no Ethernet header or ARP. "
        "prj.conf sets CONFIG_NET_LOOPBACK_SIMULATE_PACKET_DROP=y and the zperf sample main.c "
        "then calls loopback_set_packet_drop_ratio(1), so loopback_send() returns 0 before "
        "the RX clone (drivers/net/loopback.c:114-125) and dummy_send() frees the packet. The "
        "clone, address swap and net_recv_data() further down never run. The dummy L2 has no "
        "offload caps, so need_calc_checksum() always computes the checksum in SW and loopback "
        "can't show offload. Ethernet L2 (overlay-nulleth.conf, src/null_eth.c) runs the real "
        "Ethernet TX path up to the driver: header, ARP lookup (static entry for 192.0.2.2), "
        "TX queue. null_eth_send() counts and returns 0; ethernet_send() then drops the last "
        "reference (subsys/net/l2/ethernet/ethernet.c:791). So this is the stack-only TX cost "
        "with a free driver. A real driver adds its copy (7-11%) and bus time on top.\n\n"
        "Partial hot path: CMakeLists.txt relocates net_context, net_pkt, udp, ipv4, net_core, "
        "utils (checksum), net_buf, sockets, the zperf uploader and the L2/driver file. "
        "net_if.c, arp.c, net_tc.c, connection.c and the kernel stay in MRAM, so the TCM/SRAM "
        "gains are for this subset only and do not bound a full relocation.\n\n"
        "Why two boards: 54H has TCM and DVFS, so it can split core cycles from MRAM stalls and "
        "show the TCM case task 1 asks for. nRF7120 is the actual Wi-Fi part at 256 MHz. "
        "Everything needed to rebuild is in the appendix.\n\n"
        "Repeatability: two back-to-back runs per point, plus three reboots of the 7120 offload "
        "+ SRAM build (results/pdk_boot2boot.csv) and of the 54H lean offload + TCM build "
        "(results/boot2boot.csv): within 0.07% at 1472 B, worst 0.11% (292 B, eth_sw_mram @ 64 "
        "MHz). A/B percentages always compare builds run in the same session. Rebuilding with "
        "unrelated code changes "
        "moved the same variant 176.8 to 177.9 and 162.4 to 160.9 (7120) and 257.7 to 255.9 "
        "(54H, results/eth_off_rebuild.csv), so treat ~1% as the error bar. The exception is "
        "the 54H lean MRAM build: rebuilt for the cache probe after a bench.c change it moved "
        "202.1 to 209.7 (+3.8%); MRAM-heavy builds are the most layout-sensitive. One board "
        "per type, so board-to-board spread is not covered.\n\n"
        "Gotcha: zperf silently clamps the payload to 1064 B unless "
        "CONFIG_NET_ZPERF_MAX_PACKET_SIZE is raised.",
    )

    # 4. Results
    s = prs.slides.add_slide(layout(prs, "13_sub_full content"))
    set_title(
        s,
        "Results: UDP TX, 1472 B",
        "Bench config, each board at its own clock. All rows TX-only, loopback drops at send()",
    )
    add_slide_number(s)
    ph = body_ph(s, 10)
    ph._element.getparent().remove(ph._element)
    cd = CategoryChartData()
    cd.categories = [
        "Loopback, MRAM",
        "Loopback, stack in RAM",
        "Loopback, hot path local",
        "Ethernet L2, local, SW csum",
        "Ethernet L2, local, offload",
    ]
    cd.add_series("nRF54H20DK @ 320 MHz", (168.8, 144.3, 249.7, 216.3, 257.7))
    cd.add_series("nRF7120 PDK @ 256 MHz", (172.9, 161.8, 156.2, 133.9, 176.8))
    gf = s.shapes.add_chart(
        XL_CHART_TYPE.BAR_CLUSTERED, Inches(0.4), Inches(1.6), Inches(5.0), Inches(3.6), cd
    )
    ch = gf.chart
    style_chart(ch, size=8)
    ch.has_legend = True
    ch.legend.position = XL_LEGEND_POSITION.BOTTOM
    ch.legend.include_in_layout = False
    ch.legend.font.size = Pt(9)
    plot = ch.plots[0]
    plot.gap_width = 50
    plot.overlap = -10
    plot.has_data_labels = True
    plot.data_labels.font.size = Pt(8)
    plot.data_labels.font.bold = True
    plot.data_labels.number_format = "0"
    plot.data_labels.number_format_is_linked = False
    plot.data_labels.position = XL_LABEL_POSITION.OUTSIDE_END
    for ser, col in zip(plot.series, (NAVY, BLUE), strict=True):
        ser.format.fill.solid()
        ser.format.fill.fore_color.rgb = col
    ch.value_axis.visible = False
    ch.value_axis.has_major_gridlines = False
    ch.value_axis.minimum_scale = 0
    ch.value_axis.maximum_scale = 300
    ch.category_axis.tick_labels.font.size = Pt(8)
    ch.category_axis.reverse_order = True
    ch.category_axis.format.line.fill.background()
    table(
        s,
        5.6,
        1.65,
        3.73,
        [
            ["Cycles/pkt, 1472 B", "54H @128", "54H @320", "7120 @256"],
            ["Loopback, MRAM", "17,202", "22,284", "17,460"],
            ["Loopback, stack in RAM", "20,262", "26,055", "18,646"],
            ["Loopback, hot path local", "13,196", "15,052", "19,316"],
            ["Eth, local, SW csum", "15,391", "17,400", "22,536"],
            ["Eth, local, offload", "12,672", "14,592", "17,069"],
            ["Eth, MRAM, offload", "17,699", "23,620", "18,574"],
            ["Eth, local, offload, lean", "12,006", "14,080", "14,734"],
        ],
        [1.43, 0.76, 0.76, 0.78],
        size=8,
        row_h=0.22,
        bold_rows=(5,),
    )
    tb = s.shapes.add_textbox(Inches(5.6), Inches(3.4), Inches(3.73), Inches(1.8))
    fill_bullets(
        tb.text_frame,
        [
            [
                ("Checksum offload: ", True),
                ("54H +19% (TCM) / +18% (MRAM); 7120 +32% (SRAM) / +14% (MRAM).", False),
            ],
            [
                ("Partial hot path in local RAM, offload on: ", True),
                ("54H +61% (159 to 256), 7120 +9% (162 to 177). Lean: 267 / 205.", False),
            ],
            [
                ("7120 gains less: ", True),
                (
                    "SRAM cuts I-cache misses 734 to 213 (6 ws, >= 23% of the packet), but its "
                    "code and data share S-AHB (~1.3-1.6k/pkt).",
                    False,
                ),
            ],
            [
                ("Driver copy: ", True),
                ("none in the null driver; one memcpy adds 7% (7120) / 11% (54H).", False),
            ],
        ],
        size=9,
    )
    for p in tb.text_frame.paragraphs:
        p.space_after = Pt(3)
    notes(
        s,
        "Local = TCM on 54H, SRAMCODE region (RAM02+03) on 7120. Loopback rows compute "
        "checksums in SW by design and are TX-only like the Ethernet rows. Raw CSVs: results/*.csv "
        "(54H), results/pdk_*.csv (7120). The table shows the original runs. The 54H MRAM and "
        "lean rows come from a later session, so the +61% compares them with that session's "
        "rebuild of the offload + TCM build (14,695 cycles, 255.9), not the 14,592 in the "
        "table (+62% against that)."
        "\n\n54H MRAM vs TCM, Ethernet L2 with offload (eth_off_mram vs eth_off_rebuild, same "
        "day): 23,620 vs 14,695 cycles/pkt at 320 MHz, 17,699 vs 12,740 at 128, 15,579 vs "
        "12,072 at 64. The MRAM penalty grows from 3.5k to 8.9k cycles as the clock goes up, "
        "because the MRAM access time is fixed in ns. That is why TCM pays +61% on the 54H at "
        "320 MHz and SRAM only +9% on the 7120 at 256 MHz, where SRAM code also fights its "
        "own data on S-AHB. SW csum, MRAM: 135.4 (27,764 cycles); TCM 216.3, also +60%. "
        "I-cache counters (results/mem_eth_off*.csv): 508 misses/pkt with code in MRAM, 14 "
        "with the hot path in TCM. Putting the whole MRAM-TCM difference on those 494 misses "
        "gives 18 cycles per miss at 320 MHz, 10 at 128 and 7 at 64, i.e. ~4 cycles + ~43 ns "
        "per miss (the fit lands within 2% at 128 MHz). The 54H misses less often than the "
        "7120 (508 vs 734) but each miss costs about three times the 7120's 6 wait states. "
        "The lean config halves the MRAM misses (508 to 257), which is why lean helps the "
        "MRAM build by +27% (+31% in the probe rebuild) and the TCM build by only +4%."
        "\n\nWhy the 7120 SRAM hot path gains less: the M33 fetches MRAM over C-AHB through the "
        "8 KB direct-mapped I-cache, and SRAM over S-AHB, uncached, on the same port as its "
        "loads and stores. MRAMC reads back 6 wait states at 256 MHz; sweeping it (bench ws) "
        "adds exactly one cycle per wait state per miss. The Ethernet offload build misses "
        "734 times per packet (results/pdk_mem_eth_off.csv), so at least 734 x 6 / 18,743 = "
        "23% of the packet is MRAM stall (21% for the 610-miss loopback build). The SRAM hot "
        "path cuts that to 213, which should save >= 3.1k cycles, but only 1.5-1.8k shows up "
        "(18,743 to 16,945 in the probe, 18,574 to 17,069 in the sweep): SRAM code and its "
        "data share S-AHB and that costs ~1.3-1.6k per packet. With SW csum the same build "
        "drops 557 misses (>= 3.3k) but calc_chksum goes from 1,734 cycles (cached MRAM) to "
        "4,116 (SRAM), +2.4k, and contention adds ~1.9-2.2k, so SW csum Ethernet ends up "
        "+0.9k to +1.3k (21,492 to 22,407 probe, 21,250 to 22,536 sweep). In SRAM a SW "
        "checksum costs 5.5k per packet against 2.7k from MRAM. The penalty scales with bytes "
        "and the miss saving does not, so the loopback crossover is between ~300 and ~600 B "
        "(+3.2% at 128 B, +1.4% at 292 B, -1.6% at 600 B; single runs, about the ~1% build "
        "noise; results/pdk_crossover.csv). "
        "Leaving utils.c in MRAM (BENCH_HOTPATH_CSUM_IN_NVM) gives 181.8 loopback and 149.7 "
        "SW csum, both above MRAM. Data: results/pdk_mem_*.csv, mem_probe.py, bench ws / "
        "bench icache / bench prof in src/bench.c."
        "\n\nDriver zero-copy TX (NRF_WIFI_ZERO_COPY_TX) is not enabled on either board yet. "
        "The null driver frees the packet without copying, so the bench already has no "
        "driver copy and a real driver can only be slower. With the copy path a real driver "
        "adds one memcpy: bench memcpy at 1472 B is 1,229 cycles on the 7120 and 1,598 on the "
        "54H (same at 64/128/320 MHz, results/mem_*.csv), about 7% / 11% of the offload case; "
        "zero-copy avoids that. Both match the stack's own per-byte slope (0.81 / 1.09 "
        "cycles/B). It moves TX bytes from the "
        "driver heap into the Zephyr TX pool for the whole TX-done turnaround, so pool sizing "
        "(task 2) changes with it. The copy the bench still does is the socket one, app "
        "buffer to net_pkt; slide 5 prices that one."
        "\n\nLean row: overlay-ablate.conf (NET_TC_TX_COUNT=0, no net stats, no net log), "
        "267.2 Mbps on 54H @ 320 (results/eth_off_abl.csv), 204.7 on 7120 "
        "(results/pdk_eth_off_hot_abl.csv).",
    )

    # 5. Where the cycles go
    s = prs.slides.add_slide(layout(prs, "13_sub_full content"))
    set_title(
        s,
        "Where the cycles go",
        "Per 1472 B packet, best case on each board, against the budget for 400 Mbps",
    )
    add_slide_number(s)
    ph = body_ph(s, 10)
    ph._element.getparent().remove(ph._element)
    cd = CategoryChartData()
    cd.categories = [
        "Budget @ 320 MHz",
        "nRF54H20DK @ 320",
        "Budget @ 256 MHz",
        "nRF7120 PDK @ 256",
    ]
    cd.add_series("Per packet (fixed)", (9421, 12990, 7537, 15879))
    cd.add_series("Per byte (one copy)", (0, 1602, 0, 1190))
    gf = s.shapes.add_chart(
        XL_CHART_TYPE.BAR_STACKED, Inches(0.5), Inches(1.65), Inches(5.0), Inches(2.85), cd
    )
    ch = gf.chart
    style_chart(ch)
    ch.has_legend = True
    ch.legend.position = XL_LEGEND_POSITION.BOTTOM
    ch.legend.include_in_layout = False
    ch.legend.font.size = Pt(9)
    plot = ch.plots[0]
    plot.gap_width = 40
    plot.overlap = 100
    plot.has_data_labels = True
    plot.data_labels.number_format = "#,##0;;"
    plot.data_labels.number_format_is_linked = False
    plot.data_labels.font.size = Pt(9)
    plot.data_labels.font.color.rgb = WHITE
    plot.data_labels.position = XL_LABEL_POSITION.CENTER
    for ser, col in zip(plot.series, (NAVY, YELLOW), strict=True):
        ser.format.fill.solid()
        ser.format.fill.fore_color.rgb = col
    plot.series[1].data_labels.show_value = False
    for i in (0, 2):
        pt = plot.series[0].points[i]
        pt.format.fill.solid()
        pt.format.fill.fore_color.rgb = GREY
    ch.value_axis.visible = False
    ch.value_axis.has_major_gridlines = False
    ch.category_axis.tick_labels.font.size = Pt(9)
    ch.category_axis.reverse_order = True
    text_box(
        s,
        0.67,
        4.55,
        4.8,
        0.6,
        "Budget = nominal clock x 8 x 1472 / 400 Mbps. Split from a line through the 292 B and "
        "1472 B points: 54H 1.09 cycles/B + 12,990, 7120 0.81 cycles/B + 15,879; the 1024 B "
        "point lands within 5 cycles. Ethernet L2, offload, hot path in local RAM.",
        size=8,
        color=GREY,
        italic=True,
    )
    tb = s.shapes.add_textbox(Inches(5.7), Inches(1.7), Inches(3.63), Inches(3.4))
    fill_bullets(
        tb.text_frame,
        [
            [
                ("Per-byte is small: ", True),
                (
                    "0.8-1.1 cycles/B in the bench config, one memcpy (0.84 / 1.09 measured). "
                    "SW checksum adds ~2.7-2.8k/pkt from MRAM or TCM, ~5.5k from 7120 SRAM.",
                    False,
                ),
            ],
            [
                ("Per-packet dominates: ", True),
                (
                    "~13k (54H, TCM) / ~15.9k (7120) fixed: alloc, socket, UDP/IP, L2, "
                    "scheduling. 87-93% of the total, lean builds included.",
                    False,
                ),
            ],
            [
                ("Gap: ", True),
                (
                    "1.55x on 54H @ 320, 2.26x on 7120 @ 256; lean 1.50x / 1.95x. For the "
                    "350 TCP target: 1.36x / 1.98x, lean 1.31x / 1.71x.",
                    False,
                ),
            ],
            [
                ("No copy at all: ", True),
                (
                    "dropping the socket copy too gives only ~290 Mbps (54H @ 320), ~190 "
                    "(7120). The per-packet path has to shrink.",
                    False,
                ),
            ],
        ],
        size=10,
        bullets=True,
    )
    for p in tb.text_frame.paragraphs:
        p.space_after = Pt(5)
    notes(
        s,
        "Fit: NET_BUF_DATA_SIZE is 1500, so every size is one fragment and there is no "
        "per-fragment step between 292 and 1472 B. The intercept still carries cache effects "
        "that scale with packets, not bytes. Lean 7120 build: 0.82 cycles/B + 13,528, 92% "
        "fixed. Lean 54H build @ 320: 1.28 cycles/B + 12,194, 87% fixed, but its 1024 B point "
        "is 156 cycles off the line, so take that split as rough; the slope is outside the "
        "0.8-1.1 bench-config range for the same reason.\n\n"
        "No copy at all = drop the per-byte term, i.e. the socket copy from the app buffer "
        "into net_pkt, which the bench still does. This is not driver zero-copy (slide 4): "
        "the null driver has no copy to remove.\n\n"
        "Budgets use nominal clocks; with measured 319.4 / 256.2 MHz they are 9,404 / 7,542.",
    )

    # 6. Task 2: buffers
    s = prs.slides.add_slide(layout(prs, "13_sub_full content"))
    set_title(
        s,
        "Task 2: network buffers vs throughput",
        "Model only, nothing on this slide is measured yet",
    )
    add_slide_number(s)
    ph = body_ph(s, 10)
    ph._element.getparent().remove(ph._element)
    shp = table(
        s,
        0.67,
        1.62,
        6.0,
        [
            ["What holds the buffer", "Hold time", "Model @ 400 Mbps", "Measured"],
            ["Host TX until TX-done (IPC + FW + air)", "1 ms", "34 pkts / ~51 KB", MEAS + " (E13)"],
            [
                "  same, EMU TX-done latency (CAL-5883)",
                "3.5 ms",
                "119 pkts / ~178 KB",
                MEAS + " (E13)",
            ],
            [
                "A-MPDU of 64 in flight (air @ 480 PHY)",
                "~1.7 ms",
                "64 MPDUs / ~99 KB",
                MEAS + " (HW)",
            ],
            ["RX reorder, BA window 64 / 256", "-", "~99 / ~395 KB", MEAS + " (E14)"],
            [
                "TCP window = BDP @ 350 Mbps, RTT 2/5/10 ms",
                "-",
                "88 / 219 / 437 KB (Zephyr max 64)",
                MEAS + " (E15)",
            ],
        ],
        [2.55, 0.7, 1.35, 1.4],
        size=8,
        row_h=0.3,
    )
    for r in range(1, 6):
        run = shp.table.cell(r, 3).text_frame.paragraphs[0].runs[0]
        run.font.color.rgb = RED
        run.font.bold = True
    text_box(
        s,
        0.67,
        3.5,
        6.0,
        0.35,
        "Little's law: buffers in flight = packet rate x hold time. "
        "1472 B at 400 Mbps = 34 pkts/ms.",
        size=8,
        color=GREY,
        italic=True,
    )
    tb = s.shapes.add_textbox(Inches(0.67), Inches(3.85), Inches(6.0), Inches(1.3))
    fill_bullets(
        tb.text_frame,
        [
            [
                ("E13 TX: ", True),
                (
                    "null driver frees each packet D us late; sweep NET_BUF/NET_PKT TX "
                    "count x D on the PDK, Mbps vs pool size.",
                    False,
                ),
            ],
            [
                ("E14 RX: ", True),
                (
                    "injector at fixed rate, sweep RX buffer count and recycle delay, count drops.",
                    False,
                ),
            ],
            [
                ("E15 TCP: ", True),
                ("TCP TX/RX against a delay-emulating peer, sweep window vs RTT.", False),
            ],
        ],
        size=9,
        bullets=True,
    )
    card(s, 6.85, 1.62, 2.48, 2.75, fill=LIGHT)
    text_box(s, 6.95, 1.7, 2.3, 0.4, "Why it matters", size=11, color=NAVY, bold=True)
    text_box(
        s,
        6.95,
        2.1,
        2.3,
        3.0,
        [
            "~50 KB of TX buffers per ms a buffer is held. Short of that, throughput drops in "
            "proportion, whatever the CPU can do.",
            "",
            "Already seen at 24 Mbps RX: 26% of frames dropped at LMAC on rx_extram_buf_full, "
            "likely slow host RX buffer recycle (CAL-7137).",
            "",
            "Zephyr TCP has no window scaling, so 64 KB max window.",
        ],
        size=9,
    )
    notes(
        s,
        "1472 B at 400 Mbps = 33,967 pkt/s = 34 pkts per ms. Buffer cost assumes a 1.5 KB net_buf "
        "per packet, net_pkt overhead not included. A-MPDU 64 x 1544 B MPDU. BDP = rate x RTT. "
        "Every Measured cell is a placeholder until E13-E15 run.",
    )
    veil = card(s, 0.5, 1.5, 9.0, 3.75, fill=WHITE)
    clr = veil._element.spPr.find(qn("a:solidFill")).find(qn("a:srgbClr"))
    etree.SubElement(clr, qn("a:alpha"), val="70000")
    text_box(
        s,
        0.5,
        1.8,
        9.0,
        1.5,
        "TBD",
        size=80,
        color=RED,
        bold=True,
        align=PP_ALIGN.CENTER,
    )
    text_box(
        s,
        0.5,
        3.75,
        9.0,
        0.4,
        "Model only. Numbers come after the TX turnaround and buffer sweeps (E13-E15).",
        size=14,
        color=DARK,
        align=PP_ALIGN.CENTER,
    )

    # 7. Task 3: what others do
    s = prs.slides.add_slide(layout(prs, "13_sub_full content"))
    set_title(
        s,
        "Task 3: who gets to 400+ Mbps, and how",
        "Published numbers only; 20 MHz Wi-Fi parts and FPGA setups left out",
    )
    add_slide_number(s)
    ph = body_ph(s, 10)
    ph._element.getparent().remove(ph._element)
    if TASK3_ROWS:
        table(
            s,
            0.5,
            1.65,
            9.0,
            [["Platform", "CPU @ MHz", "Stack", "Result", "Mbps/MHz", "What makes it work"]]
            + TASK3_ROWS,
            [1.6, 1.3, 1.15, 1.85, 0.75, 2.35],
            size=7,
            row_h=0.23,
        )
    if TASK3_NOTES:
        text_box(s, 0.5, 4.25, 9.0, 0.9, TASK3_NOTES, size=8)

    # 7b. Task 3: Zephyr hosts
    s = prs.slides.add_slide(layout(prs, "13_sub_full content"))
    set_title(
        s,
        "Task 3: Zephyr hosts at 400+ Mbps (any link)",
        "Published Zephyr zperf numbers from upstream PRs; max clock where not stated",
    )
    add_slide_number(s)
    ph = body_ph(s, 10)
    ph._element.getparent().remove(ph._element)
    table(
        s,
        0.5,
        1.62,
        9.0,
        [
            [
                "Platform",
                "Core @ MHz",
                "Link / driver",
                "Result (Mbps)",
                "Mbps/MHz",
                "What helped",
                "Source",
            ]
        ]
        + ZEPHYR_ROWS,
        [1.35, 1.1, 1.2, 1.95, 0.8, 1.6, 1.0],
        size=7,
        row_h=0.25,
    )
    text_box(s, 0.5, 3.85, 9.0, 1.3, ZEPHYR_NOTES, size=8)

    # 8. Architecture options
    s = prs.slides.add_slide(layout(prs, "18_three messages title + text"))
    set_title(s, "Architectures that can reach 400 Mbps")
    add_slide_number(s)
    heads = ["A. Fewer packets", "B. More host CPU", "C. TCP/IP off host"]
    bodies = [
        [
            "USO/TSO on TX, GRO on RX, fewer ACKs, and a thin raw/callback path under the socket "
            "API.",
            "lwIP RAW is >= 1.75x its sockets on the same A9. Zephyr has no GSO/GRO today; "
            "driver/FW "
            "segmentation cost unknown (E8).",
        ],
        [
            "Same stack, faster core: higher clock, core-local RAM, wider core (M55/M85 or "
            "A-class).",
            "400 Mbps is 34k pkts/s. At 14.1k (54H, TCM) to 14.7k (7120) cycles/pkt, lean, "
            "that is ~480-500 MHz of CM33. No headroom left for driver or app.",
        ],
        [
            "Stack on a network core or in Wi-Fi FW; host uses socket offload (WZN-108, WZN-8705).",
            "Host cost becomes IPC + copy, network core has the B problem. The one offload "
            "Wi-Fi part found (SiWG917) is 20 MHz, 66 Mbps.",
        ],
    ]
    for i, h in enumerate(heads):
        body_ph(s, 16 + i).text = h
        fill_bullets(body_ph(s, 19 + i).text_frame, bodies[i], size=10)
    notes(
        s,
        "B: 400 Mbps at 1472 B is 33,967 pkt/s. Lean builds: x 14,080 cycles (54H @ 320, "
        "offload + TCM) = 478 MHz; x 14,734 (7120 @ 256, offload + SRAM hot path) = 500 MHz. "
        "Bench config: 14,592 = 496 MHz, 17,069 = 580 MHz. Lean 54H at 64 MHz (11,289 cycles, "
        "closest to zero MRAM wait) = 383 MHz. "
        "A: assumes stack per-packet cost is paid once per super-packet; driver segmentation cost "
        "is unknown and is what E8 measures.",
    )

    # 8b. Zephyr gaps
    s = prs.slides.add_slide(layout(prs, "13_sub_full content"))
    set_title(
        s,
        "Zephyr gaps to raise upstream",
        "Checked against Zephyr main in our tree; candidates for feature requests",
    )
    add_slide_number(s)
    ph = body_ph(s, 10)
    ph._element.getparent().remove(ph._element)
    table(
        s,
        0.5,
        1.6,
        9.0,
        [
            ["Gap", "Why it matters (our data)", "Ask", "Impact"],
            [
                "No TX segmentation offload (TSO/USO)",
                "Per-packet cost is 87-93% of the cycles; stack runs once per 1472 B",
                "L2 capability + MSS hint; stack hands down up to 64 KB, driver/FW segments",
                "High",
            ],
            [
                "No RX coalescing (GRO)",
                "Every MPDU walks IP/UDP/TCP and the socket layer on its own",
                "GRO hook in net_core before L3, merge in-order TCP/UDP segments",
                "High",
            ],
            [
                "TCP window scaling ignored",
                "WS option is parsed but never used; windows capped at 64 KB. "
                "BDP at 350 Mbps, 5 ms RTT is 219 KB",
                "RFC 7323 window scaling, send and receive",
                "High (TCP)",
            ],
            [
                "No batched socket I/O",
                "One syscall, net_context and net_pkt alloc per datagram",
                "sendmmsg/recvmmsg, or a batch send API on net_context",
                "Medium",
            ],
            [
                "No hot-path placement",
                "With offload, partial hot path: 54H TCM +61%, 7120 SRAM +9%. Done by hand "
                "with zephyr_code_relocate per file",
                "__net_hot annotation and a Kconfig to place it in TCM/SRAM",
                "Medium",
            ],
            [
                "Socket TX always copies",
                "One memcpy per packet, 7-11% of the offload case (measured)",
                "Zero-copy send from app buffers (MSG_ZEROCOPY-like)",
                "Low-Med",
            ],
            [
                "No buffer lifetime stats",
                "TXTIME stats stop at driver send; task 2 needs alloc-to-free hold time",
                "net_pkt/net_buf lifetime histogram and pool high-water in net mem",
                "Medium",
            ],
            [
                "Checksum is generic C",
                "calc_chksum 1.18 cycles/B on M33; only matters without offload",
                "Arch hook for an optimised checksum (e.g. Armv8-M DSP)",
                "Low",
            ],
        ],
        [2.0, 3.1, 3.1, 0.8],
        size=7,
        row_h=0.36,
    )
    notes(
        s,
        "TSO/GRO: enum ethernet_hw_caps (include/zephyr/net/ethernet.h:145-208) has no "
        "TSO/USO/GRO bit. Window scaling: tcp.c:1172-1175 parses option 3 into "
        "recv_options.window but nothing reads it, the windows are uint16_t "
        "(tcp_private.h:334-337) and cwnd is clamped to UINT16_MAX (tcp.c:547, 563, 2266). "
        "No sendmmsg/recvmmsg in include/zephyr/net/socket.h. Socket send copies into net_pkt "
        "(net_pkt_write, net_context.c:2252, 2264); "
        "NRF_WIFI_ZERO_COPY_TX only removes the driver copy. TSO and GRO are the ones that "
        "attack the per-packet cost, the rest are percent-level.",
    )

    # 8c. SoC options
    s = prs.slides.add_slide(layout(prs, "13_sub_full content"))
    set_title(
        s,
        "SoC options for an nRF71-based host",
        "What a new chip could change, with the gain our data supports",
    )
    add_slide_number(s)
    ph = body_ph(s, 10)
    ph._element.getparent().remove(ph._element)
    table(
        s,
        0.5,
        1.55,
        9.0,
        [
            ["Option", "Gain (our data)", "Pros", "Cons"],
            [
                "TCM (I+D) for the net path and kernel",
                "54H partial hot path +61% over MRAM. 7120 est. ~+30% (>= 4.3k stall "
                "cycles/pkt). All code zero-wait: ~334 Mbps @ 320, ~267 @ 256",
                "Proven on 54H. No SW model change; driver and kernel gain too",
                "Area. 32 KB holds only part of the path. Alone it stays below 400",
            ],
            [
                "Bigger I-cache, own code bus for SRAM",
                "7120 (8 KB) misses 734/pkt vs 508 on 54H (16 KB). SRAM code loses "
                "~1.3-1.6k/pkt to S-AHB contention (~8%)",
                "Cheaper than TCM, no linker work",
                "Percent-level. Misses still pay MRAM wait states",
            ],
            [
                "Higher CM33 clock (or M55/M85)",
                "400 needs ~480-500 MHz lean, ~383 MHz with all code zero-wait",
                "Every SW path gains, nothing to port",
                "256-320 MHz cap comes from the backend cell choice; faster cells cost "
                "power, leakage, area. MRAM miss cost grows with clock (7 to 18 cycles)",
            ],
            [
                "Checksum in the HW datapath, TX and RX",
                "+18-32% UDP TX (measured)",
                "Cheapest big win; nRF71 FW already does TX",
                "FW cycles per packet today (WZN-9251). RX is advertised, FW validation "
                "unconfirmed",
            ],
            [
                "Zero-copy DMA from host net_buf",
                "Removes the driver copy, 7-11% (measured memcpy)",
                "Less bus load as well",
                "Buffer ownership across cores, alignment and headroom; buffers held "
                "until TX-done (task 2 pool size)",
            ],
            [
                "HW TSO/USO and GRO in the Wi-Fi datapath",
                "Stack runs once per up to 64 KB. Linux GSO 2.4x, GRO 1.8x; at 2x, "
                "400 Mbps needs ~240 MHz (est., E8)",
                "Only option that cuts the 87-93% per-packet cost. Fits A-MPDU",
                "No TSO/GRO in Zephyr yet. HW segments and builds headers. UDP gain "
                "only for big datagrams; TCP also needs window scaling",
            ],
            [
                "Dedicated network processor (TCP/IP off host)",
                "Host stack cost replaced by socket offload + IPC per call (TO MEASURE)",
                "Host CPU free for the app; network core sized for the job",
                "Network core needs ~383-500 MHz CM33-class unless it has the rows above. "
                "IPC per packet, socket offload gaps (WZN-108)",
            ],
        ],
        [1.55, 2.65, 2.0, 2.8],
        size=7,
        row_h=0.42,
    )
    shp = s.shapes[-1]
    shp.table.rows[0].height = Inches(0.26)
    shp.height = Inches(0.26 + 0.42 * 7)
    notes(
        s,
        "TCM: 54H offload, code in MRAM 159.2 vs partial hot path in TCM 255.9 (same session). "
        "7120 estimate: the offload MRAM build takes 734 I-cache misses at 6 wait states, at "
        "least 4.4k of 18,574 cycles; removing ~4.3k gives ~14.3k, ~211 Mbps at 256 MHz. Zero-"
        "wait ceiling: lean 54H at 64 MHz is 11,289 cycles/pkt (MRAM wait closest to zero), "
        "scaled to 320 / 256 MHz.\n\n"
        "Clock: 33,967 pkt/s x 14,080 (54H lean) = 478 MHz, x 14,734 (7120 lean) = 500 MHz, x "
        "11,289 = 383 MHz. 54H per-miss cost fit: ~4 cycles + ~43 ns, so wait states do not "
        "shrink with clock.\n\n"
        "TSO/GRO: 2x is a planning number taken from the Linux results (LPC 2018), not ours: "
        "14,080 / 2 = 7,040 cycles x 33,967 pkt/s = 239 MHz. Part of the gain comes from SW GSO "
        "in the stack alone; HW makes the segmentation free. E8 measures it on the null "
        "driver.\n\n"
        "Network processor: the stack cost per packet does not go away, it moves. The only "
        "Wi-Fi offload part found (SiWG917) is 20 MHz class.",
    )

    # 8d. Two takeaways
    s = prs.slides.add_slide(layout(prs, "13_full content"))
    set_title(s, "Two takeaways")
    add_slide_number(s)
    body_ph(s, 13)._element.getparent().remove(body_ph(s, 13)._element)
    cols = []
    for i, head in enumerate(("1. Zephyr network stack (SW)", "2. SoC / HW offloads")):
        x = 0.67 + i * 4.42
        card(s, x, 1.5, 4.24, 3.0, fill=LIGHT)
        text_box(s, x + 0.15, 1.58, 3.9, 0.4, head, size=14, color=NAVY, bold=True)
        cols.append(s.shapes.add_textbox(Inches(x + 0.15), Inches(2.0), Inches(3.94), Inches(2.4)))
    fill_bullets(
        cols[0].text_frame,
        [
            "Lean config and hot-path placement: 267 Mbps measured on 54H @ 320. Upstream a "
            "__net_hot placement so it is not hand-made per file",
            "GSO/GRO in the stack: the only SW change that cuts the per-packet cost. SW GSO "
            "helps alone, HW segmentation makes it pay",
            "TCP window scaling: needed for TCP > 350 (64 KB window, ~105 Mbps at 5 ms RTT)",
            "Batched socket I/O, zero-copy send: percent-level",
        ],
        size=10,
        bullets=True,
    )
    fill_bullets(
        cols[1].text_frame,
        [
            "TCM sized for net path + kernel: biggest proven gain (+61% on 54H), ceiling "
            "~334 Mbps @ 320 with all code zero-wait",
            "HW TSO/USO + GRO, checksum (TX + RX) and zero-copy DMA in the Wi-Fi datapath",
            "Clock is capped at 256-320 MHz by the cell choice; 400 needs ~383 MHz even "
            "with all code zero-wait",
            "Network processor only moves the stack cost and adds IPC, unless it brings "
            "the items above",
        ],
        size=10,
        bullets=True,
    )
    for tb in cols:
        for p in tb.text_frame.paragraphs:
            p.space_after = Pt(5)
    text_box(
        s,
        0.67,
        4.62,
        8.66,
        0.5,
        "Neither alone gets to 400 at 256-320 MHz. Fewer packets per byte needs both: the "
        "stack API and segmentation in HW.",
        size=11,
        color=NAVY,
        bold=True,
    )
    notes(
        s,
        "Stack-only numbers, UDP TX, null driver: the Wi-Fi driver, IPC and FW come on top. "
        "At 320 MHz 400 Mbps allows 9,421 cycles/pkt; the best zero-wait lean build is 11,289, "
        "so even full TCM needs another ~17% cut. At 256 MHz the budget is 7,537, 33% below "
        "zero-wait lean. TSO/GRO is the item that closes that, and it needs work on both sides.",
    )

    # 9. Next
    s = prs.slides.add_slide(layout(prs, "13_full content"))
    set_title(s, "Next steps")
    add_slide_number(s)
    fill_bullets(
        body_ph(s, 13).text_frame,
        [
            [
                ("Task 1: ", True),
                (
                    "TCP TX/RX and UDP RX on the null driver with injected ACKs/frames (E6, E7).",
                    False,
                ),
            ],
            [
                ("Task 1/A: ", True),
                (
                    "prototype USO in the null driver (stack hands down N x MSS) and GRO "
                    "before the stack; measure cycles per byte delivered (E8).",
                    False,
                ),
            ],
            [
                ("Task 2: ", True),
                (
                    "buffer sweep with emulated TX-done latency, fit to the Little's law model.",
                    False,
                ),
            ],
            [
                ("Task 3: ", True),
                ("pick 1-2 comparable platforms from the table and reproduce on our bench.", False),
            ],
            [
                ("Upstream: ", True),
                ("raise Zephyr feature requests for TSO/USO, GRO and TCP window scaling.", False),
            ],
            [
                ("Decision for the review: ", True),
                (
                    "which stack items to take upstream and which SoC options go into the "
                    "nRF71-based chip spec.",
                    False,
                ),
            ],
        ],
        size=12,
    )

    # 10. Appendix: reproduce
    s = prs.slides.add_slide(layout(prs, "13_sub_full content"))
    set_title(
        s,
        "Appendix: reproduce the numbers",
        "krish2718/sdk-nrf, benchmarks/cal-7627-host-tput, tests/benchmarks/net_host_tput",
    )
    add_slide_number(s)
    ph = body_ph(s, 10)
    ph._element.getparent().remove(ph._element)
    code = card(s, 0.67, 1.7, 5.4, 2.75, fill=DARK)
    tf = code.text_frame
    tf.word_wrap = True
    tf.vertical_anchor = MSO_ANCHOR.TOP
    tf.margin_left = tf.margin_top = Inches(0.12)
    lines = [
        "cd nrf/tests/benchmarks/net_host_tput",
        "# nRF54H20DK",
        "./build.sh && ./sweep.sh && ./sweep_eth.sh  # results/*.csv",
        "./sweep_eth.sh /dev/ttyACM0 eth_sw_mram eth_off_mram \\",
        "    eth_off_tc0 eth_off_abl eth_off_mram_abl",
        "# nRF7120 PDK",
        "./pdk_build.sh",
        "./pdk_sweep.sh base 1500 reloc hot eth_sw eth_off \\",
        "    eth_sw_hot eth_off_hot              # results/pdk_*.csv",
        "./pdk_sweep.sh eth_off_hot_tc0 eth_off_hot_abl eth_off_abl",
        "# probes, crossover, reboots, rebuild (case lists inside)",
        "./repro_extra.sh pdk_mem pdk_crossover pdk_boot2boot",
        "./repro_extra.sh boot2boot rebuild mem   # 54H",
        "",
        "# by hand, offload build:",
        "uart:~$ bench clk 320   # 54H only; bench freq on 7120",
        "uart:~$ net arp add 192.0.2.2 02:00:5e:10:00:02",
        "uart:~$ zperf udp upload 192.0.2.2 5001 10 1472 1000M",
        "# read the client rate in brackets",
    ]
    for i, ln in enumerate(lines):
        p = tf.paragraphs[0] if i == 0 else tf.add_paragraph()
        p.alignment = PP_ALIGN.LEFT
        r = p.add_run()
        r.text = ln or " "
        r.font.name = "Courier New"
        r.font.size = Pt(8)
        r.font.color.rgb = WHITE
    text_box(
        s,
        6.3,
        1.7,
        3.03,
        3.4,
        [
            "Bench code: the branch above (pdk_build.sh / build.sh build every variant).",
            "All runs: zephyr 2bbcaa7ddf2 (local, not the manifest pin), Zephyr SDK 1.0.0 "
            "(GCC 14.3, checked in each zephyr.elf), CONFIG_SPEED_OPTIMIZATIONS, picolibc.",
            "sdk-nrf module: b419792d143 (54H loopback rows: 02edfdd44a0). The diff is Wi-Fi, "
            "security (cracen, linked but off the UDP TX path), samples and manifest pins. "
            "results/west_list.txt has the workspace.",
            "54H loopback / eth_sw / eth_off rows were built from an older copy of the bench; "
            "build.sh reproduces them within ~1%.",
            "",
            "SRAM builds add sramcode.overlay: RAM02+03 become a separate SRAMCODE region, "
            "since the KMU push area has to stay at RAM_ADDR, ahead of any relocated code.",
            "",
            "Wiki with all data: WCG page 3562799288.",
        ],
        size=9,
    )

    # Backup: every run, both boards
    s = prs.slides.add_slide(layout(prs, "13_sub_full content"))
    set_title(
        s,
        "Backup: all nRF7120 PDK runs",
        "UDP TX Mbps at 256 MHz, 2 x 10 s per point; cycles/pkt at the largest size run",
    )
    add_slide_number(s)
    ph = body_ph(s, 10)
    ph._element.getparent().remove(ph._element)
    table(
        s,
        0.67,
        1.62,
        8.66,
        [
            ["Variant", "Checksum", "Code", "1472 B", "1024 B", "292 B", "Cycles/pkt"],
            ["Loopback, 1 KB MTU", "SW", "MRAM", "-", "126.5", "39.4", "16,596 (1 KB)"],
            ["Loopback", "SW", "MRAM", "172.9", "126.5", "39.4", "17,460"],
            ["Loopback", "SW", "whole stack SRAM", "161.8", "122.9", "41.3", "18,646"],
            ["Loopback", "SW", "hot path SRAM", "156.2", "118.3", "39.4", "19,316"],
            ["Null Ethernet", "SW", "MRAM", "142.0", "103.0", "31.5", "21,250"],
            ["Null Ethernet", "SW", "hot path SRAM", "133.9", "100.1", "32.5", "22,536"],
            ["Null Ethernet", "offload", "MRAM", "162.4", "115.2", "33.9", "18,574"],
            ["Null Ethernet", "offload", "hot path SRAM", "176.8", "125.7", "37.1", "17,069"],
            ["Null Ethernet, lean", "offload", "MRAM", "185.5", "132.0", "39.1", "16,261"],
            ["Null Ethernet, lean", "offload", "hot path SRAM", "204.7", "146.0", "43.4", "14,734"],
        ],
        [1.7, 0.9, 1.6, 0.9, 0.9, 0.9, 1.76],
        size=8,
        row_h=0.235,
        bold_rows=(8,),
    )
    text_box(
        s,
        0.67,
        4.3,
        8.66,
        0.9,
        [
            "The first row lands within 2% of the old PDK baseline (124 Mbps at 1 KB). Lean = "
            "overlay-ablate.conf: no TX thread, no net stats, no net logging.",
            "Microbench at 1472 B: checksum 1,734 cycles (1.18/B), memcpy 1,229 (0.84/B).",
            "Zephyr SDK 1.0.0 (GCC 14.3) on both boards.",
        ],
        size=9,
    )

    s = prs.slides.add_slide(layout(prs, "13_sub_full content"))
    set_title(
        s,
        "Backup: all nRF54H20DK runs",
        "UDP TX Mbps at 1472 B per DVFS point, 2 x 10 s each; cycles/pkt in brackets",
    )
    add_slide_number(s)
    ph = body_ph(s, 10)
    ph._element.getparent().remove(ph._element)
    table(
        s,
        0.67,
        1.62,
        8.66,
        [
            ["Variant", "Checksum", "Code", "64 MHz", "128 MHz", "320 MHz"],
            ["Loopback", "SW", "MRAM", "49.0 (15,349)", "87.4 (17,202)", "168.8 (22,284)"],
            [
                "Loopback",
                "SW",
                "whole stack RAM",
                "41.7 (18,046)",
                "74.2 (20,262)",
                "144.3 (26,055)",
            ],
            ["Loopback", "SW", "hot path TCM", "59.9 (12,564)", "113.9 (13,196)", "249.7 (15,052)"],
            [
                "Null Ethernet",
                "SW",
                "hot path TCM",
                "51.2 (14,703)",
                "97.6 (15,391)",
                "216.3 (17,400)",
            ],
            [
                "Null Ethernet",
                "offload",
                "hot path TCM",
                "62.6 (12,022)",
                "118.6 (12,672)",
                "257.7 (14,592)",
            ],
            ["Null Ethernet", "SW", "MRAM", "40.1 (18,776)", "71.1 (21,155)", "135.4 (27,764)"],
            [
                "Null Ethernet",
                "offload",
                "MRAM",
                "48.3 (15,579)",
                "84.9 (17,699)",
                "159.2 (23,620)",
            ],
            [
                "Null Ethernet, lean",
                "offload",
                "MRAM",
                "57.8 (13,020)",
                "103.8 (14,480)",
                "202.1 (18,620)",
            ],
            [
                "Null Ethernet, lean",
                "offload",
                "hot path TCM",
                "66.7 (11,289)",
                "125.2 (12,006)",
                "267.2 (14,080)",
            ],
        ],
        [1.7, 0.9, 1.6, 1.4, 1.5, 1.56],
        size=8,
        row_h=0.24,
        bold_rows=(9,),
    )
    text_box(
        s,
        0.67,
        4.25,
        8.66,
        0.8,
        [
            "Cycles/pkt rise with clock because MRAM wait is fixed in ns: MRAM vs TCM with "
            "offload is +3.5k cycles at 64 MHz and +8.9k at 320 (same-session rebuild; 3.6k / "
            "9.0k against the rows above). I-cache misses/pkt: 508 MRAM, 14 TCM, 257 MRAM lean "
            "(results/mem_*.csv), ~4 cycles + ~43 ns each. TX thread off alone: 265.7 at 320.",
            "No 256 MHz DVFS point. Interpolating cycles/pkt gives a rough ~216 Mbps for offload "
            "+ TCM (7120: 177); the MRAM penalty is not linear in clock, so treat it as rough.",
            "zephyr 2bbcaa7ddf2, sdk-nrf 02edfdd44a0 (loopback) / b419792d143 (Ethernet), "
            "Zephyr SDK 1.0.0 (GCC 14.3).",
        ],
        size=8,
    )

    # 11. Appendix: Jira map
    s = prs.slides.add_slide(layout(prs, "2_Table"))
    set_title(s, "Appendix: related Jiras (linked to CAL-7627)")
    add_slide_number(s)
    for ph in list(s.placeholders):
        if ph.placeholder_format.idx == 10:
            ph._element.getparent().remove(ph._element)
    table(
        s,
        0.67,
        1.45,
        8.66,
        [
            ["Task", "Jiras"],
            [
                "1. Checksum offload, TCM, 256 MHz",
                "CAL-5883, CAL-5999, CAL-5997, CAL-6233, CAL-6689, CAL-7132, CAL-7134, CAL-7135, "
                "WZN-10644, WZN-10952",
            ],
            ["1. Checksum offload work", "WZN-9251, CAL-6241, CAL-6794, WZN-9216, CAL-6760"],
            [
                "1. CPU / memory / IPC",
                "CAL-4871, WZN-2280, WZN-4570, SHEL-4236, WZN-8128, SHEL-4053",
            ],
            [
                "2. Buffers",
                "CAL-7137 (RX host/FW stats, 26% LMAC drops on buffer full), CAL-7136 (RX "
                "air-level max)",
            ],
            [
                "3. Architecture / offload",
                "WZN-10911 (throughput ceiling, acceleration), WZN-108 (TCP/IP offload), "
                "WZN-8705 (IP stack offload serialization)",
            ],
        ],
        [2.4, 6.26],
        size=9,
        row_h=0.42,
    )

    prs.save(out)


if __name__ == "__main__":
    build(sys.argv[1], sys.argv[2])
