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
        "~0.37 / 0.18",
        "new DMA driver, SW csum",
        "PR 109327 (open)",
    ],
    [
        "Renesas EK-RA8M2",
        "M85 @ <= 1000",
        "1G RMAC",
        "UDP TX 279; TCP TX 158 (1000 B)",
        "0.28 / 0.16",
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
    "Only UDP has crossed 400 on Zephyr, and only on ~1 GHz cores. Per MHz every one of them is "
    "below our 0.69 (0.2-0.5): they win on clock, not on a cheaper per-packet path.",
    "Same RT1170 board: NXP bare-metal SDK does ~916 Mbps UDP vs ~477 on Zephyr. The stack is the "
    "limit, not the MAC.",
    "No Zephyr TCP above ~230 Mbps anywhere. tcp.c parses window scaling but never applies it, "
    "so the window is capped at 64 KB: ~262 Mbps at 2 ms RTT, ~105 at 5 ms. TCP > 350 needs "
    "this fixed first.",
]
TASK3_ROWS = [
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
        "UDP 221 / 190, TCP 86 / 95",
        "0.28-0.92",
        "same radio as below, MCU host",
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
        "all offload Wi-Fi parts < 70",
    ],
]
TASK3_NOTES = [
    "Our 0.69 Mbps/MHz is close to lwIP sockets on A9/A53 (0.81). Dropping the socket layer "
    "(lwIP RAW callbacks) is >= 1.75x on the same A9. We are 2.26x short, so that alone is "
    "not enough.",
    "Same NXP radio: 86-221 Mbps with lwIP on an MCU host, 405-502 with Linux. The host stack "
    "makes the gap.",
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
        "177 Mbps",
        "UDP TX, stack only, nRF7120 @ 256 MHz\nchecksum offload + hot path in SRAM",
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
        "1.45 MHz/Mbps",
        "with offload + SRAM hot path\n1.8 with SW csum, all in MRAM",
        NAVY,
    )
    tb = s.shapes.add_textbox(Inches(0.67), Inches(2.65), Inches(8.66), Inches(2.4))
    fill_bullets(
        tb.text_frame,
        [
            [
                ("Task 1: NO-GO. ", True),
                (
                    "Measured on the nRF7120 PDK. The stack alone, with nothing below it, "
                    "is 2.26x short of 400 (1.98x of 350). Driver, IPC and FW only add to that.",
                    False,
                ),
            ],
            [
                ("Rule of thumb holds. ", True),
                (
                    "SW checksum, code in MRAM: 142 Mbps, 1.8 MHz per Mbps. "
                    "Offload +14%, hot path in SRAM another +9%.",
                    False,
                ),
            ],
            [
                ("Per-packet cost is the wall. ", True),
                (
                    "17.1k cycles per 1472 B packet, ~15.9k of it fixed per packet. "
                    "The per-byte part is one memcpy. Even at 0 cycles/byte the stack "
                    "would top out at ~190 Mbps.",
                    False,
                ),
            ],
            [
                ("400 Mbps needs a different architecture: ", True),
                (
                    "far fewer packets through the stack (USO/TSO, GRO) or TCP/IP off the host.",
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
        "All numbers are UDP TX only, 1472 B payload, zperf, Zephyr stack, nRF7120 PDK app core "
        "at its fixed 256 MHz (measured 256.2-256.4). TCP and RX are not measured yet. Earlier "
        "nRF54H20DK runs (TCM, 64-320 MHz) are on the wiki and point the same way.",
    )

    # 3. How we measured
    s = prs.slides.add_slide(layout(prs, "13_sub_full content"))
    set_title(
        s,
        "How we measured",
        "Zephyr stack on the nRF7120 PDK app core, Wi-Fi replaced by a null driver",
    )
    add_slide_number(s)
    ph = body_ph(s, 10)
    ph._element.getparent().remove(ph._element)
    stack = [
        ("zperf UDP upload (1472 B, 1000M)", NAVY),
        ("BSD sockets", MID),
        ("UDP / IPv4 / net_pkt", MID),
        ("Ethernet L2 (ARP static)", MID),
        ("null Ethernet driver: count + free at send()", BLUE),
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
        "Hot path code in SRAM (RAM02+03, 248 KB carved out)",
        size=8,
        color=GREY,
        italic=True,
    )
    tb = s.shapes.add_textbox(Inches(4.55), Inches(1.75), Inches(4.78), Inches(3.4))
    fill_bullets(
        tb.text_frame,
        [
            [
                ("Board: ", True),
                (
                    "nRF7120 PDK app core (nrf7120dk/nrf7120/cpuapp), CM33F, MRAM + 8 KB I-cache.",
                    False,
                ),
            ],
            [
                ("Checksum A/B: ", True),
                (
                    "the null driver advertises TX/RX checksum offload or nothing "
                    "(Kconfig). Loopback can't show offload, it always computes in SW.",
                    False,
                ),
            ],
            [
                ("Code placement: ", True),
                (
                    "MRAM + I-cache (default), whole net stack in SRAM, or only the UDP TX "
                    "hot path in SRAM. No TCM on nRF7120.",
                    False,
                ),
            ],
            [
                ("Clock: ", True),
                (
                    "fixed 256 MHz (HFPLL), measured each run with SysTick vs GRTC. "
                    "No interpolation.",
                    False,
                ),
            ],
            [("Runs: ", True), ("2 x 10 s per point, the two runs agree within 0.1%.", False)],
            [
                ("Gotcha: ", True),
                (
                    "zperf silently clamps payload to 1064 B unless "
                    "CONFIG_NET_ZPERF_MAX_PACKET_SIZE is raised.",
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
        "Everything needed to rebuild is on the last slide.",
    )

    # 4. Results
    s = prs.slides.add_slide(layout(prs, "13_sub_full content"))
    set_title(
        s,
        "Results: UDP TX, 1472 B",
        "nRF7120 PDK at 256 MHz; table shows all payload sizes for the Ethernet L2 runs",
    )
    add_slide_number(s)
    ph = body_ph(s, 10)
    ph._element.getparent().remove(ph._element)
    cd = CategoryChartData()
    cd.categories = [
        "Loopback, MRAM (SW csum)",
        "Loopback, stack in SRAM",
        "Ethernet L2, MRAM (SW csum)",
        "Ethernet L2, MRAM (offload)",
        "Ethernet L2, SRAM hot path (offload)",
    ]
    cd.add_series("Mbps @ 256 MHz", (172.9, 161.8, 142.0, 162.4, 176.8))
    gf = s.shapes.add_chart(
        XL_CHART_TYPE.BAR_CLUSTERED, Inches(0.5), Inches(1.65), Inches(4.9), Inches(3.5), cd
    )
    ch = gf.chart
    style_chart(ch)
    ch.has_legend = False
    plot = ch.plots[0]
    plot.gap_width = 60
    plot.has_data_labels = True
    plot.data_labels.font.size = Pt(10)
    plot.data_labels.font.bold = True
    plot.data_labels.number_format = "0"
    plot.data_labels.number_format_is_linked = False
    plot.data_labels.position = XL_LABEL_POSITION.OUTSIDE_END
    for i, col in enumerate((GREY, LIGHT, MID, NAVY, BLUE)):
        pt = plot.series[0].points[i]
        pt.format.fill.solid()
        pt.format.fill.fore_color.rgb = col
    ch.value_axis.visible = False
    ch.value_axis.has_major_gridlines = False
    ch.value_axis.minimum_scale = 0
    ch.value_axis.maximum_scale = 215
    ch.category_axis.tick_labels.font.size = Pt(9)
    ch.category_axis.reverse_order = True
    ch.category_axis.format.line.fill.background()
    table(
        s,
        5.6,
        1.75,
        3.73,
        [
            ["Ethernet L2", "SW csum", "Offload", "+ SRAM"],
            ["1472 B", "142.0", "162.4", "176.8"],
            ["1024 B", "103.0", "115.2", "125.7"],
            ["292 B", "31.5", "33.9", "37.1"],
            ["Mbps/MHz, 1472", "0.55", "0.63", "0.69"],
            ["Cycles/pkt, 1472", "21,250", "18,575", "17,069"],
        ],
        [1.21, 0.84, 0.84, 0.84],
        size=9,
        bold_rows=(1,),
    )
    text_box(
        s,
        5.6,
        3.5,
        3.73,
        1.6,
        [
            "Offload: +14% at 1472 B, -2.7k cycles/pkt (+8% at 292 B).",
            "Hot path in SRAM: another +9% with offload.",
            "But on loopback SRAM is slower than MRAM (-6% whole stack, -10% hot path). "
            "Likely uncached SRAM fetch sharing the bus with packet data; not root-caused.",
        ],
        size=9,
    )
    notes(
        s,
        "Loopback rows compute checksums in SW by design and run TX and RX on one core, so they "
        "are not comparable 1:1 with the Ethernet L2 rows. Every point is 2 x 10 s, runs agree "
        "within 0.1%. Raw CSVs: results/pdk_*.csv.",
    )

    # 5. Where the cycles go
    s = prs.slides.add_slide(layout(prs, "13_sub_full content"))
    set_title(
        s, "Where the cycles go", "Per 1472 B packet at 256 MHz, against the budget for 400 Mbps"
    )
    add_slide_number(s)
    ph = body_ph(s, 10)
    ph._element.getparent().remove(ph._element)
    cd = CategoryChartData()
    cd.categories = ["Budget for 400 Mbps", "Today (offload + SRAM hot path)"]
    cd.add_series("Per packet (fixed)", (7537, 15879))
    cd.add_series("Per byte (one copy, 1,190)", (0, 1190))
    gf = s.shapes.add_chart(
        XL_CHART_TYPE.BAR_STACKED, Inches(0.5), Inches(1.7), Inches(5.0), Inches(2.6), cd
    )
    ch = gf.chart
    style_chart(ch)
    ch.has_legend = True
    ch.legend.position = XL_LEGEND_POSITION.BOTTOM
    ch.legend.include_in_layout = False
    ch.legend.font.size = Pt(9)
    plot = ch.plots[0]
    plot.gap_width = 50
    plot.overlap = 100
    plot.has_data_labels = True
    plot.data_labels.number_format = "#,##0;;"
    plot.data_labels.number_format_is_linked = False
    plot.data_labels.font.size = Pt(10)
    plot.data_labels.font.color.rgb = WHITE
    plot.data_labels.position = XL_LABEL_POSITION.CENTER
    for ser, col in zip(plot.series, (NAVY, YELLOW), strict=True):
        ser.format.fill.solid()
        ser.format.fill.fore_color.rgb = col
    plot.series[1].data_labels.show_value = False
    ch.value_axis.visible = False
    ch.value_axis.has_major_gridlines = False
    ch.category_axis.tick_labels.font.size = Pt(9)
    ch.category_axis.reverse_order = True
    text_box(
        s,
        0.67,
        4.4,
        4.8,
        0.7,
        "Budget = 256 MHz x 8 x 1472 / 400 Mbps = 7,537. Today = 17,069 measured. Split from "
        "a line through 292 B (16,115) and 1472 B: 0.81 cycles/B + 15,879 fixed.",
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
                    "0.81 cycles/B with offload, same as memcpy on the bench (0.84). "
                    "SW checksum adds ~2.7k/pkt.",
                    False,
                ),
            ],
            [
                ("Per-packet dominates: ", True),
                ("~15.9k cycles: alloc, socket, UDP/IP, L2, scheduling. 93% of the total.", False),
            ],
            [
                ("Gap: ", True),
                ("2.26x for 400 Mbps, 1.98x for 350 (TCP target).", False),
            ],
            [
                ("Zero-copy alone: ", True),
                ("~190 Mbps. The per-packet path has to shrink 2x.", False),
            ],
        ],
        size=10,
        bullets=True,
    )
    for p in tb.text_frame.paragraphs:
        p.space_after = Pt(5)

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
            row_h=0.25,
        )
    if TASK3_NOTES:
        text_box(s, 0.5, 4.2, 9.0, 0.9, TASK3_NOTES, size=8)

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
        row_h=0.27,
    )
    text_box(s, 0.5, 3.75, 9.0, 1.4, ZEPHYR_NOTES, size=8)

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
            "400 Mbps is 34k pkts/s. At 17.1k cycles/pkt that is ~580 MHz of CM33, ~540 MHz "
            "even with zero-copy. No headroom left for driver or app.",
        ],
        [
            "Stack on a network core or in Wi-Fi FW; host uses socket offload (WZN-108, WZN-8705).",
            "Host cost becomes IPC + copy, network core has the B problem. Offload Wi-Fi parts "
            "shipping today are 20 MHz, < 70 Mbps.",
        ],
    ]
    for i, h in enumerate(heads):
        body_ph(s, 16 + i).text = h
        fill_bullets(body_ph(s, 19 + i).text_frame, bodies[i], size=10)
    notes(
        s,
        "B: 400 Mbps at 1472 B is 33,967 pkt/s; x 17,069 cycles (PDK, offload + SRAM hot path) "
        "= 580 MHz; x 15,879 (fixed part only) = 539 MHz. "
        "A: assumes stack per-packet cost is paid once per super-packet; driver segmentation cost "
        "is unknown and is what E8 measures.",
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
                ("Decision for the review: ", True),
                ("which of A / B / C to take to architecture.", False),
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
    code = card(s, 0.67, 1.7, 5.4, 2.2, fill=DARK)
    tf = code.text_frame
    tf.word_wrap = True
    tf.vertical_anchor = MSO_ANCHOR.TOP
    tf.margin_left = tf.margin_top = Inches(0.12)
    lines = [
        "cd nrf/tests/benchmarks/net_host_tput",
        "./pdk_build.sh          # 7 variants, nrf7120dk/nrf7120/cpuapp",
        "./pdk_sweep.sh base 1500 reloc hot eth_sw eth_off eth_off_hot",
        "                        # -> results/pdk_*.csv",
        "",
        "# by hand, offload build:",
        "uart:~$ bench freq",
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
            "nRF7120 PDK, zephyr 2bbcaa7ddf2, sdk-nrf b419792d143.",
            "Zephyr SDK 1.0.0 (GCC 14.3), CONFIG_SPEED_OPTIMIZATIONS, picolibc module.",
            "",
            "SRAM builds add sramcode.overlay: RAM02+03 become a separate SRAMCODE region, "
            "since the KMU push area has to stay at RAM_ADDR, ahead of any relocated code.",
            "",
            "Wiki with all data: WCG page 3562799288.",
        ],
        size=9,
    )

    # Backup: every PDK run
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
            ["Loopback", "SW", "hot path SRAM", "156.2", "118.3", "39.4", "19,317"],
            ["Null Ethernet", "SW", "MRAM", "142.0", "103.0", "31.5", "21,250"],
            ["Null Ethernet", "offload", "MRAM", "162.4", "115.2", "33.9", "18,575"],
            ["Null Ethernet", "offload", "hot path SRAM", "176.8", "125.7", "37.1", "17,069"],
        ],
        [1.7, 0.9, 1.6, 0.9, 0.9, 0.9, 1.76],
        size=8,
        row_h=0.27,
        bold_rows=(7,),
    )
    text_box(
        s,
        0.67,
        3.9,
        8.66,
        1.2,
        [
            "The first row reproduces the old PDK baseline (124 Mbps at 1 KB).",
            "Cross-check: nRF54H20DK at 320 MHz with the hot path in 32 KB TCM did 14,593 "
            "cycles/pkt (257.7 Mbps), against 17,069 here from SRAM. Different toolchain "
            "(SDK 0.17.0). Both are about 2x over the 7,537 budget.",
            "Microbench at 1472 B: checksum 1,734 cycles (1.18/B), memcpy 1,229 (0.84/B).",
        ],
        size=9,
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
