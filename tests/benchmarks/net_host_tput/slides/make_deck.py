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
LLM_FOOTNOTE = (
    "Collected with an LLM (Cursor, Claude Opus 5.5 medium), not verified. Only used for: no "
    "Zephyr host shows TCP above ~230 Mbps, and Zephyr TCP is ~0.5-0.6x UDP elsewhere."
)

ZEPHYR_ROWS = [
    [
        "nRF54H20DK (ours)",
        "M33 @ 320",
        "null Ethernet",
        "UDP TX 258",
        "1.24",
        "csum offload, hot path in TCM",
        "this work",
    ],
    [
        "nRF7120 PDK (ours)",
        "M33 @ 256",
        "null Ethernet",
        "UDP TX 230",
        "1.11",
        "csum offload, hot path in RAM01",
        "this work",
    ],
    [
        "i.MX93 EVK",
        "A55 @ <= 1700",
        "1G ENET",
        "UDP RX 619",
        "<= 2.8",
        "cacheable RX buffers",
        "PR 82210 (merged)",
    ],
    [
        "MIMXRT1170-EVKB",
        "M7 @ 996",
        "1G ENET_1G",
        "UDP RX ~477, TX 200-512; TCP TX ~200",
        "2.0-5.0",
        "deeper rings, IRQ coalescing",
        "PR 118810 (open)",
    ],
    [
        "TI AM62L EVM",
        "A53 @ <= 1250",
        "1G CPSW3G",
        "UDP TX ~460; TCP TX ~230",
        "<= 2.7 / 5.6",
        "new DMA driver, SW csum",
        "PR 109327 (open)",
    ],
    [
        "Renesas EK-RA8M2",
        "M85 @ <= 1000",
        "1G RMAC",
        "UDP TX 279; TCP TX 158 (1000 B)",
        "<= 3.6 / 6.3",
        "driver fix",
        "issue 109078",
    ],
    [
        "Avnet ZedBoard",
        "A9 @ 667",
        "1G GEM",
        "UDP ~240; TCP ~130",
        "~2.8 / 5.3",
        "L2 cache on",
        "PR 111154 / 111833",
    ],
]
ZEPHYR_NOTES = [
    "Only UDP has crossed 400 on Zephyr, and only on ~1 GHz cores. Their 2-5 MHz per Mbps "
    "includes a real MAC driver and ours (1.1-1.2) has none, so per-MHz numbers are not "
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
        "1.24",
        "csum offload, hot path in TCM",
    ],
    [
        "nRF7120 PDK (ours)",
        "M33 @ 256",
        "Zephyr, sockets",
        "UDP TX 230",
        "1.11",
        "csum offload, hot path in RAM01",
    ],
    ["Zynq-7000 ZC702", "A9 @ 667", "lwIP, sockets", "TCP TX 542 / RX 521", "1.23", "HW csum"],
    [
        "Zynq-7000 ZC702",
        "A9 @ 667",
        "lwIP, RAW API",
        "TCP TX 949 / RX 943",
        "<= 0.70",
        "same HW, no socket layer (GbE cap)",
    ],
    ["ZynqMP ZCU102", "A53 @ 1100", "lwIP, sockets", "TCP RX 887", "1.24", "HW csum"],
    [
        "i.MX RT1170 (user report)",
        "M7 @ ~1000",
        "lwIP, bare metal",
        "TCP RX 390 / TX 150",
        "2.6 / 6.7",
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
        "SiWG917",
        "own network CPU",
        "TCP/IP on the Wi-Fi chip",
        "UDP 66, TCP 56",
        "-",
        "20 MHz channel, air-limited",
    ],
]
TASK3_NOTES = [
    "Our 1.1-1.2 MHz per Mbps (null driver) is close to lwIP sockets on A9/A53 (1.2, real MAC), so "
    "per MHz is not like-for-like. lwIP RAW callbacks are >= 1.75x its sockets on the same A9, "
    "but that is TCP capped by GbE; the gain for Zephyr UDP is unknown.",
    "Same NXP radio: 86-221 Mbps with lwIP on an MCU host, 405-502 with Linux. The host stack "
    "makes the gap. The RT1180 page (MCUXpresso SDK 25.03 Wi-Fi throughput) does not say which "
    "core ran the test, so no MHz per Mbps.",
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


def scope_tag(slide, x=5.6, y=0.7, w=3.73):
    card(slide, x, y, w, 0.34, fill=RED)
    text_box(
        slide,
        x,
        y,
        w,
        0.34,
        "UDP TX only. TCP and RX not measured.",
        size=11,
        color=WHITE,
        bold=True,
        align=PP_ALIGN.CENTER,
        anchor=MSO_ANCHOR.MIDDLE,
    )


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


def sub_slide(prs, title, sub):
    s = prs.slides.add_slide(layout(prs, "13_sub_full content"))
    set_title(s, title, sub)
    add_slide_number(s)
    ph = body_ph(s, 10)
    ph._element.getparent().remove(ph._element)
    return s


def build(src, out):
    prs = Presentation(src)
    drop_slides_after_first(prs)

    # 1. Title
    s = prs.slides[0]
    s.shapes.title.text = "TCP/IP throughput on CM33"
    body_ph(s, 1).text = "CAL-7627: best TCP/IP throughput on a CM33 host, and what it takes"
    body_ph(s, 10).text = "Chaitanya Tata\nOctober 2026"

    # 2. Answer
    s = sub_slide(
        prs,
        "Answer: ~250 Mbps TCP at best",
        "Question: best TCP/IP throughput on a 320 MHz Aludra derivative with an 80 MHz MAC/PHY",
    )
    card(s, 0.67, 1.6, 3.4, 3.3)
    text_box(s, 0.82, 1.65, 3.1, 0.55, "~250 Mbps TCP", size=28, color=NAVY, bold=True)
    text_box(s, 0.82, 2.17, 3.1, 0.3, "at best, range 200-265", size=11, color=NAVY)
    text_box(s, 0.82, 2.42, 3.1, 0.45, "~330 Mbps UDP", size=20, color=BLUE, bold=True)
    text_box(
        s,
        0.82,
        2.85,
        3.1,
        2.0,
        [
            "Best case, with every step on the right done.",
            "0.97 MHz per Mbps UDP, 1.3 TCP (old rule of thumb: 2, UDP).",
            "The CPU is the limit, not the air: 80 MHz HE MCS9 (480 Mbps PHY) carries "
            "~330-350 Mbps TCP goodput.",
            "If the TX thread has to stay: ~220 TCP.",
            "CAL-7627 targets (TCP > 350, UDP > 400) are out of reach on one CM33.",
        ],
        size=10,
    )
    text_box(s, 4.35, 1.55, 5.0, 0.35, "What it takes", size=14, color=NAVY, bold=True)
    tb = s.shapes.add_textbox(Inches(4.35), Inches(1.95), Inches(5.0), Inches(2.9))
    fill_bullets(
        tb.text_frame,
        [
            [("Checksum offload in the RPU, TX and RX. ", True), ("TX in place, +19%.", False)],
            [
                ("Whole net stack and Wi-Fi data path in single-cycle TCM. ", True),
                ("Measured ~135 KB (stack, nRF71 driver, kernel); budget 192 KB to grow.", False),
            ],
            [
                ("TCP window scaling in Zephyr. ", True),
                ("Without it TCP stops at ~260 Mbps (2 ms RTT) whatever the CPU.", False),
            ],
            [("~160 KB of packet buffers ", True), ("in normal RAM for the TCP window.", False)],
            [("Zero-copy TX in the nRF71 driver. ", True), ("~10% back.", False)],
            [
                ("Lean config: ", True),
                ("net stats and logging off, TX thread off (needs a real-driver test).", False),
            ],
        ],
        size=10,
        bullets=True,
    )
    for p in tb.text_frame.paragraphs:
        p.space_after = Pt(4)
    text_box(
        s,
        4.35,
        4.45,
        5.0,
        0.5,
        "Past ~250 TCP needs fewer packets through the stack (TSO/GRO). A second CM33 for the "
        "driver gives ~280-290.",
        size=10,
        color=NAVY,
        bold=True,
    )
    text_box(
        s,
        0.67,
        5.0,
        8.66,
        0.25,
        "UDP TX is measured. TCP, the Wi-Fi driver cost and 320 MHz are estimated from it, "
        "see slide 4. Terms on slide 3.",
        size=8,
        color=GREY,
        italic=True,
    )
    notes(
        s,
        "One number: ~250 Mbps TCP, ~330 UDP, on a 320 MHz CM33 with every item on the list. "
        "The chain behind it is on slide 4: measured 10,743 cycles per 1472 B UDP packet on the "
        "nRF7120 PDK (offload, hot path in single-cycle RAM, lean), scaled to 320 MHz, whole "
        "stack in TCM, x0.85 for the Wi-Fi driver (driver only, FPGA), x~0.75 for TCP. Both "
        "factors are estimates, so ~250 is a best case and 200-265 the range.\n\n"
        "If the TX thread has to stay, take ~10% off: ~220 TCP.\n\n"
        "Air: HE MCS9, 1 spatial stream, 80 MHz, 0.8 us GI is a 480 Mbps PHY; TCP goodput is "
        "typically ~70% of PHY rate, so ~330-350.",
    )

    # 3. Terms
    s = sub_slide(prs, "Terms used in this deck", "Every number in the deck uses these")
    table(
        s,
        0.67,
        1.55,
        8.66,
        [
            ["Term", "Meaning here"],
            [
                "UDP TX, stack only",
                "What we measured: zperf UDP send, 1472 B payload, through the Zephyr stack into "
                "a null Ethernet driver that frees each frame. No Wi-Fi, no TCP, no RX.",
            ],
            [
                "Goodput",
                "Payload delivered per second, after all headers, ACKs and retries. Every Mbps "
                "in this deck is goodput.",
            ],
            [
                "nRF54H20DK / nRF7120 PDK",
                "The two CM33 boards. 54H: 320 MHz, 32 KB TCM. 7120: 256 MHz, no TCM, four SRAM "
                "blocks (RAM00/01 single-cycle, RAM02/03 2-cycle).",
            ],
            [
                "MRAM + I-cache",
                "Default: code runs from MRAM through an 8 KB (7120) or 16 KB (54H) I-cache. "
                "Each miss costs ~11 cycles on the 7120.",
            ],
            [
                "TCM, local RAM",
                "Single-cycle RAM next to the core. Local RAM = TCM on the 54H, RAM01 on the 7120.",
            ],
            [
                "Hot path / whole stack",
                "Hot path: only the UDP TX files moved to local RAM (32 KB). Whole stack: all "
                "of the net stack libraries (128 KB in the loopback build).",
            ],
            [
                "Checksum offload",
                "The Wi-Fi chip (RPU) computes the IP/UDP/TCP checksum, not the host CPU.",
            ],
            [
                "Lean config",
                "Net statistics, net logging and the TX thread off. Not the shipping default.",
            ],
            [
                "Cycles per packet",
                "CPU clock / packets per second. Takes the clock out of the comparison.",
            ],
            [
                "MHz per Mbps",
                "CPU clock needed per Mbps of goodput, lower is better. The old rule of thumb "
                "is 2 (UDP TX, SW checksum, code in MRAM).",
            ],
            [
                "TSO / USO, GRO",
                "Segmentation offload: the stack hands down one large chunk and HW/FW cuts it "
                "into packets. GRO: the RX side merges packets before the stack.",
            ],
            [
                "TCP window scaling",
                "TCP option that allows windows above 64 KB. Zephyr parses it but ignores it.",
            ],
        ],
        [2.0, 6.66],
        size=8,
        row_h=0.255,
    )

    # 4. How we got there
    s = sub_slide(
        prs,
        "How we got to ~250 Mbps TCP",
        "Five UDP steps from the best measured number, then one step from UDP to TCP",
    )
    steps = [
        ("Measured\n7120 @ 256", 230, NAVY),
        ("+ lean\nconfig", 281, NAVY),
        ("at\n320 MHz", 351, BLUE),
        ("+ whole\nstack in TCM", 385, GREY),
        ("x0.85 Wi-Fi\ndriver", 327, GREY),
        ("x0.75\nTCP", 245, RED),
    ]
    cd = CategoryChartData()
    cd.categories = [x[0] for x in steps]
    cd.add_series("Mbps", [x[1] for x in steps])
    gf = s.shapes.add_chart(
        XL_CHART_TYPE.COLUMN_CLUSTERED, Inches(0.4), Inches(1.78), Inches(4.6), Inches(3.12), cd
    )
    for x, w, label, col in ((0.55, 3.6, "UDP TX", NAVY), (4.2, 0.72, "TCP", RED)):
        card(s, x, 1.52, w, 0.24, fill=col)
        text_box(
            s,
            x,
            1.52,
            w,
            0.24,
            label,
            size=9,
            color=WHITE,
            bold=True,
            align=PP_ALIGN.CENTER,
            anchor=MSO_ANCHOR.MIDDLE,
        )
    ch = gf.chart
    style_chart(ch, size=8)
    ch.has_legend = False
    plot = ch.plots[0]
    plot.gap_width = 40
    plot.has_data_labels = True
    plot.data_labels.font.size = Pt(10)
    plot.data_labels.font.bold = True
    plot.data_labels.number_format = "0"
    plot.data_labels.number_format_is_linked = False
    plot.data_labels.position = XL_LABEL_POSITION.OUTSIDE_END
    for i, (_, _, col) in enumerate(steps):
        pt = plot.series[0].points[i]
        pt.format.fill.solid()
        pt.format.fill.fore_color.rgb = col
    ch.value_axis.visible = False
    ch.value_axis.has_major_gridlines = False
    ch.value_axis.minimum_scale = 0
    ch.value_axis.maximum_scale = 440
    ch.category_axis.tick_labels.font.size = Pt(8)
    text_box(
        s,
        0.5,
        4.9,
        4.5,
        0.3,
        "Navy: measured. Blue: clock-scaled. Grey and red: estimated. Slide 2 rounds to "
        "~330 / ~250 at best.",
        size=8,
        color=GREY,
        italic=True,
    )
    table(
        s,
        5.15,
        1.55,
        4.2,
        [
            ["Step", "Mbps", "Basis"],
            [
                "Measured",
                "230",
                "7120 PDK at 256 MHz, checksum offload, hot path in RAM01: 13,091 cycles/pkt",
            ],
            ["+ lean config", "281", "Measured, same board: 10,743 cycles/pkt"],
            ["at 320 MHz", "351", "Same cycles/pkt at 320 MHz"],
            [
                "+ whole stack in TCM",
                "385",
                "Whole stack vs hot path in RAM01: -9% cycles (measured on loopback)",
            ],
            [
                "x0.85 Wi-Fi driver",
                "327",
                "Driver software only, FPGA keeps 85% (CAL-6689). IPC and FW not in it, so "
                "optimistic. UDP answer",
            ],
            [
                "x0.75 TCP",
                "245",
                "ACK RX and TCP state per segment. Not measured; range 0.6-0.8 gives 200-265",
            ],
        ],
        [1.2, 0.5, 2.5],
        size=7,
        row_h=0.46,
    )
    shp = s.shapes[-1]
    shp.table.rows[0].height = Inches(0.26)
    notes(
        s,
        "Step 3 assumes cycles per packet stay flat with clock. That holds once the code runs "
        "from TCM; from MRAM it does not (54H MRAM build: 17.7k cycles at 128 MHz, 23.6k at "
        "320), which is one more reason the whole stack has to be in TCM.\n\n"
        "Step 4: on the 7120, loopback with the whole net stack in RAM01 is 11,720 cycles vs "
        "12,851 with the hot path only. 10,743 x 0.912 = ~9.8k cycles, 385 Mbps at 320 MHz. "
        "Independent check: the lean build with its remaining 97 I-cache misses free is ~9.7k.\n\n"
        "Step 5: 0.85 is the FPGA ratio for the driver software alone, no IPC or FW "
        "(CAL-6689). On the PDK we only have air-limited runs (0.36-0.45 of loopback, "
        "WZN-10644), which say nothing about CPU cost. E12 measures it.\n\n"
        "Step 6: other Zephyr hosts show TCP at 0.5-0.6x UDP, but those are capped by the 64 KB "
        "window. With window scaling I'd expect 0.7-0.8x. TCP on the null driver on the PDK "
        "replaces it with a number.",
    )

    # 5. What each step is worth
    s = sub_slide(
        prs,
        "What each optimisation is worth",
        "Measured, UDP TX, 1472 B, same build compared on the same board",
    )
    table(
        s,
        0.67,
        1.55,
        8.66,
        [
            ["Optimisation", "Gain", "Measured on", "Note"],
            [
                "Hot path in single-cycle local RAM",
                "+45% / +61%",
                "7120 RAM01 / 54H TCM, vs MRAM",
                "Biggest lever. Code from MRAM spends ~44% of each packet on I-cache misses",
            ],
            [
                "Checksum offload",
                "+19%",
                "Both boards, hot path in local RAM",
                "+15-18% with code in MRAM. TX in the RPU today, RX to confirm",
            ],
            [
                "Lean config",
                "+22% / +4%",
                "7120 / 54H",
                "TX thread is ~60% of it on the 7120; needs a real-driver test",
            ],
            [
                "Whole stack instead of hot path",
                "+10%",
                "7120 RAM01, loopback",
                "54H only in slow general RAM: its 32 KB TCM is too small for 128 KB",
            ],
            [
                "Zero-copy TX in the driver",
                "+9-11%",
                "memcpy cost on both boards",
                "The copy cost is measured; zero-copy itself is not yet",
            ],
            [
                "RAM that is 2-cycle, not 1",
                "+10% only",
                "7120 RAM02/03 vs MRAM",
                "Same code as the RAM01 build. TCM has to be single-cycle",
            ],
        ],
        [2.2, 1.0, 2.1, 3.36],
        size=8,
        row_h=0.4,
    )
    shp = s.shapes[-1]
    shp.table.rows[0].height = Inches(0.26)
    text_box(
        s,
        0.67,
        4.45,
        8.66,
        0.6,
        "Even with all of these, 87-91% of the cost is per packet, not per byte. That is why "
        "the next big step is fewer packets (TSO/GRO), not faster copies.",
        size=10,
        color=NAVY,
        bold=True,
    )

    # 6. Sizing
    s = sub_slide(
        prs,
        "How much TCM and RAM",
        "Code sizes from the nRF71 Wi-Fi build link map; buffer sizes from the TCP window",
    )
    text_box(s, 0.67, 1.5, 4.2, 0.35, "TCM (code)", size=14, color=NAVY, bold=True)
    table(
        s,
        0.67,
        1.9,
        4.2,
        [
            ["What", "Size"],
            ["Net stack per packet: IPv4/6, TCP, UDP, sockets", "~94 KB"],
            ["nRF71 driver data path: TX, RX, events, IPC", "~21 KB"],
            ["Kernel, IPC service, memcpy, interrupt entry", "~19 KB"],
            ["Measured total", "~135 KB"],
            ["Future growth: window scaling, SACK, batching", "~57 KB"],
            ["Budget", "192 KB"],
        ],
        [3.3, 0.9],
        size=8,
        row_h=0.3,
        bold_rows=(4, 6),
    )
    text_box(
        s,
        0.67,
        4.1,
        4.2,
        0.9,
        [
            "Has to be single-cycle: the same code in 2-cycle RAM gave +10% instead of +45%.",
            "Keep code out of the RAM the Wi-Fi DMA writes into.",
        ],
        size=9,
    )
    text_box(s, 5.15, 1.5, 4.2, 0.35, "Normal RAM (packet buffers)", size=14, color=NAVY, bold=True)
    table(
        s,
        5.15,
        1.9,
        4.2,
        [
            ["TCP window for 250 Mbps", "Buffers"],
            ["2 ms RTT", "~63 KB"],
            ["5 ms RTT", "~160 KB"],
            ["10 ms RTT", "~315 KB"],
        ],
        [2.9, 1.3],
        size=8,
        row_h=0.36,
    )
    text_box(
        s,
        5.15,
        3.45,
        4.2,
        1.5,
        [
            "Window = throughput x round-trip time. Above 64 KB needs TCP window scaling.",
            "Plus Wi-Fi RX reorder buffering (64-frame block-ack window: ~99 KB).",
            "Model only; task 2 measures it (backup).",
        ],
        size=9,
    )
    notes(
        s,
        "Code sizes: link map of the nRF71 throughput sample (high-performance config, 7120 "
        "PDK, speed optimisations, TCP and IPv6 on). Only objects a packet goes through: net "
        "shell, zperf, the Wi-Fi control path and the WPA supplicant stay in MRAM. Whole "
        "objects, since relocation works per file; per function it is ~126 KB. The older "
        "128 KB came from the loopback build, which relocated whole libraries including "
        "zperf, the loopback driver and ~30 KB of net shell.\n\n"
        "Future growth is code that has to sit on the per-packet path but is not there yet: "
        "TCP window scaling and SACK in the TCP core, packet batching (GSO/GRO) in the stack "
        "and driver, RX path changes, paths the link map does not attribute (timers, work "
        "queues) and growth across releases. 160 KB is the floor with modest headroom; keep "
        "192 KB until a relocation build confirms it.\n\n"
        "Buffers: 250 Mbps x RTT / 8. RX reorder: 64 x 1.5 KB plus headroom.",
    )

    # 7. Risks
    s = sub_slide(
        prs,
        "What is not measured yet",
        "The assumptions behind ~250 Mbps, and how to firm each one up",
    )
    table(
        s,
        0.67,
        1.55,
        8.66,
        [
            ["Assumption", "If it is wrong", "How to check"],
            [
                "TCP costs 0.75x UDP per packet",
                "0.6-0.8 gives 200-265 Mbps TCP",
                "TCP on the null driver on the PDK (E6)",
            ],
            [
                "Wi-Fi driver, IPC and FW keep 85% (FPGA, driver only)",
                "PDK data so far is air-limited, so this is untested on silicon",
                "CPU-bound Wi-Fi run on the PDK (E12)",
            ],
            [
                "TX thread can go (part of lean)",
                "If it stays: ~220 Mbps TCP",
                "Real nRF71 driver with no TX thread, UDP and TCP, look for stalls",
            ],
            [
                "TCP window scaling gets done",
                "Without it TCP stops at ~260 Mbps at 2 ms RTT, ~105 at 5 ms",
                "Zephyr change, upstream",
            ],
            [
                "Cycles per packet stay flat from 256 to 320 MHz",
                "54H hot-path TCM build grew 15% from 128 to 320 MHz (rest in MRAM)",
                "Needs the whole stack in TCM; no board with enough TCM at 320 MHz",
            ],
            [
                "RX costs no more than TX",
                "RX is not measured at all",
                "RX on the null driver with injected frames (E7)",
            ],
            [
                "Host interface is free",
                "Companion config: socket data from the customer host over SPI/SDIO/USB also "
                "costs the CM33",
                "Not in the ~250 yet; measure with the host transport in the loop",
            ],
        ],
        [2.6, 3.1, 2.96],
        size=8,
        row_h=0.38,
    )
    shp = s.shapes[-1]
    shp.table.rows[0].height = Inches(0.26)

    # 8. Beyond 250
    s = sub_slide(
        prs,
        "Getting past ~250 Mbps TCP",
        "The CM33 runs only TCP/IP, the app has its own core. Not measured",
    )
    table(
        s,
        0.67,
        1.55,
        8.66,
        [
            ["Option", "What changes", "Expected", "Cost / risk"],
            [
                "TSO/USO + GRO",
                "Stack runs once per up to 64 KB instead of once per packet",
                "Linux sees 1.8-2.4x; at 2x, 350 TCP looks reachable at 320 MHz",
                "Not in Zephyr. Needs segmentation in Wi-Fi FW/HW, cost unknown (E8)",
            ],
            [
                "Second CM33 for the driver",
                "TCP/IP core keeps only the stack; Wi-Fi driver, RPU IPC and host transport "
                "move to another core",
                "~280-290 TCP, minus a handoff per packet between the cores",
                "More cores do not split one TCP stream: Zephyr has no SMP on Cortex-M",
            ],
            [
                "Wider core at 320 MHz",
                "Same stack on e.g. Cortex-M85",
                "M85 is ~1.5x M33 on CoreMark per MHz; network code likely ~1.3x",
                "Unmeasured. Clock stays capped at 256-320 MHz by the cell library",
            ],
        ],
        [1.6, 2.6, 2.4, 2.06],
        size=8,
        row_h=0.55,
    )
    shp = s.shapes[-1]
    shp.table.rows[0].height = Inches(0.26)
    text_box(
        s,
        0.67,
        3.85,
        8.66,
        0.8,
        "TSO/GRO is the only option that cuts the per-packet cost itself. It needs work in "
        "both Zephyr and the Wi-Fi datapath.",
        size=10,
        color=NAVY,
        bold=True,
    )

    # 9. Next steps
    s = sub_slide(prs, "Next steps", "In order; the first three firm up the ~250 Mbps number")
    tb = s.shapes.add_textbox(Inches(0.67), Inches(1.6), Inches(8.66), Inches(3.4))
    fill_bullets(
        tb.text_frame,
        [
            [("TCP on the null driver ", True), ("(E6): replaces the 0.75 guess.", False)],
            [
                ("CPU-bound Wi-Fi run on the PDK ", True),
                ("(E12): replaces the 0.85 driver factor from FPGA.", False),
            ],
            [
                ("Real nRF71 driver with no TX thread, ", True),
                ("UDP and TCP: decides whether lean can ship.", False),
            ],
            [("Zero-copy TX A/B on nRF71: ", True), ("confirms the ~10%.", False)],
            [("RX cost on the null driver ", True), ("(E7).", False)],
            [
                ("Zephyr: ", True),
                ("TCP window scaling, then a TSO/GRO prototype on the null driver (E8).", False),
            ],
            [
                ("Decision: ", True),
                ("which SoC items (TCM size, checksum RX, TSO/GRO) go into the chip spec.", False),
            ],
        ],
        size=12,
        bullets=True,
    )
    for p in tb.text_frame.paragraphs:
        p.space_after = Pt(6)

    # Backup divider
    s = prs.slides.add_slide(layout(prs, "13_full content"))
    set_title(s, "Backup")
    add_slide_number(s)
    fill_bullets(
        body_ph(s, 13).text_frame,
        [
            "Test setup and measured results",
            "Where the cycles go",
            "Network buffers (task 2)",
            "Published numbers from other platforms (task 3)",
            "SoC options and Zephyr gaps",
            "Why lean is not the default",
            "Reproduce, all runs, related Jiras",
        ],
        size=14,
    )

    # 3. How we measured
    s = prs.slides.add_slide(layout(prs, "13_sub_full content"))
    set_title(
        s,
        "Backup: test setup",
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
        "Hot path code in TCM (54H) or single-cycle RAM01 (7120)",
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
                    "used only for the whole-stack placement runs. Every packet is dropped "
                    "at send(), so no RX. The loopback driver cannot claim checksum offload, "
                    "so the stack always computes it; offload numbers use the null driver.",
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
                    "path in local RAM (54H TCM, 7120 RAM01). Offload on/off by Kconfig.",
                    False,
                ),
            ],
            [
                ("Clock and runs: ", True),
                (
                    "clock measured once per clock step (SysTick vs GRTC), 2 x 10 s per point. "
                    "Runs and reboots agree within ~0.1%; a rebuilt image moves results ~1-2% "
                    "(code layout vs cache).",
                    False,
                ),
            ],
            [
                ("Config: ", True),
                (
                    "zperf sample defaults (TX thread, net stats and log on). A lean build is "
                    "+4% (54H) / +22% (7120).",
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
        "Cycles per packet = CPU Hz / packets per second. MHz/Mbps = measured CPU MHz / Mbps. "
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
        "with a free driver. A real driver adds its copy (9-11%) and bus time on top.\n\n"
        "Partial hot path: CMakeLists.txt relocates net_context, net_pkt, udp, ipv4, net_core, "
        "utils (checksum), net_buf, sockets, the zperf uploader and the L2/driver file. "
        "net_if.c, arp.c, net_tc.c, connection.c and the kernel stay in MRAM, so the TCM/RAM01 "
        "gains are for this subset only and do not bound a full relocation. On the 7120 the "
        "whole net stack in RAM01 (loopback) does 257.2 vs 234.6 for the hot path.\n\n"
        "7120 RAM blocks: sramcode-ram01.overlay puts relocated code in RAM01 (0x20080000, "
        "256 KB) and leaves data in RAM00 (512 KB). RAM02/03 (sramcode.overlay, the older runs) "
        "are 2-cycle for the CM33. sramcode-ram00.overlay puts code in the top of RAM00 next to "
        "the data, as a control: within 1% of RAM01 here, but this bench has no Wi-Fi DMA "
        "writing into data RAM.\n\n"
        "Why two boards: 54H has TCM and DVFS, so it can split core cycles from MRAM stalls and "
        "show the TCM case task 1 asks for. nRF7120 is the actual Wi-Fi part at 256 MHz. "
        "Everything needed to rebuild is in the appendix.\n\n"
        "Repeatability: two back-to-back runs per point, plus three reboots of the 7120 offload "
        "+ RAM01 build (results/pdk_boot2boot_ram01.csv, within 0.03%) and of the 54H lean "
        "offload + TCM build (results/boot2boot.csv): within 0.07% at 1472 B, worst 0.11% "
        "(292 B, eth_sw_mram @ 64 MHz). A/B percentages always compare builds run in the same "
        "session. Rebuilding with unrelated code changes moved the same variant 176.8 to 177.9 "
        "and 162.4 to 160.9 / 159.3 (7120) and 257.7 to 255.9 (54H, "
        "results/eth_off_rebuild.csv), so treat ~1-2% as the error bar. The exception is "
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
        "Backup: measured results",
        "UDP TX, 1472 B, null Ethernet driver, each board at its own clock, bench config",
    )
    add_slide_number(s)
    ph = body_ph(s, 10)
    ph._element.getparent().remove(ph._element)
    res_rows = [
        ("Code in MRAM, SW checksum", 135.4, 139.0, "27,764", "21,682"),
        ("Code in MRAM, offload", 159.2, 159.3, "23,620", "18,928"),
        ("Hot path in local RAM, SW checksum", 216.3, 194.2, "17,400", "15,526"),
        ("Hot path in local RAM, offload", 257.7, 230.4, "14,592", "13,091"),
        ("Hot path in local RAM, offload, lean", 267.2, 280.7, "14,080", "10,743"),
    ]
    cd = CategoryChartData()
    cd.categories = [r[0] for r in res_rows]
    cd.add_series("nRF54H20DK @ 320 MHz", [r[1] for r in res_rows])
    cd.add_series("nRF7120 PDK @ 256 MHz", [r[2] for r in res_rows])
    gf = s.shapes.add_chart(
        XL_CHART_TYPE.BAR_CLUSTERED, Inches(0.4), Inches(1.55), Inches(5.0), Inches(3.3), cd
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
    ch.value_axis.maximum_scale = 320
    ch.category_axis.tick_labels.font.size = Pt(8)
    ch.category_axis.reverse_order = True
    ch.category_axis.format.line.fill.background()
    text_box(s, 5.6, 1.55, 3.73, 0.3, "Cycles per packet (lower is better)", size=9, bold=True)
    table(
        s,
        5.6,
        1.85,
        3.73,
        [["Build", "54H @ 320", "7120 @ 256"]] + [[r[0], r[3], r[4]] for r in res_rows],
        [2.03, 0.85, 0.85],
        size=7,
        row_h=0.28,
        bold_rows=(4,),
    )
    tb = s.shapes.add_textbox(Inches(5.6), Inches(3.65), Inches(3.73), Inches(1.5))
    fill_bullets(
        tb.text_frame,
        [
            "Local RAM = 32 KB TCM on the 54H, single-cycle RAM01 on the 7120.",
            "Hot path = the UDP TX files only (sockets, UDP, IPv4, net_pkt, net_buf, L2). "
            "Whole stack in RAM01 is another +10% (7120 loopback, 257 vs 235).",
            "Lean = net stats, net logging and the TX thread off. Not the shipping config.",
            "Rows are bench config unless marked lean.",
        ],
        size=8,
        bullets=True,
    )
    for p in tb.text_frame.paragraphs:
        p.space_after = Pt(3)
    notes(
        s,
        "Raw data: results/eth_*.csv (54H) and results/ramblk_*.csv (7120, one session). "
        "Cycles per packet = CPU Hz / packets per second.\n\n"
        "54H MRAM rows come from a later session than the TCM rows; against that session's "
        "rebuild of the offload + TCM build (14,695 cycles) the TCM gain is +62%.\n\n"
        "Why local RAM matters so much: with code in MRAM the 7120 misses its 8 KB I-cache 753 "
        "times per packet at ~11 cycles each, ~44% of the packet. RAM01 cuts that to 233. The "
        "same code in 2-cycle RAM02/03 gives only +10%, so the RAM has to be single-cycle.\n\n"
        "Driver copy: the null driver frees the packet without copying. A real driver's "
        "memcpy is 1,229 cycles (7120) / 1,598 (54H) at 1472 B, 9% / 11% of the offload case.",
    )

    # 5. Where the cycles go
    s = prs.slides.add_slide(layout(prs, "13_sub_full content"))
    set_title(
        s,
        "Backup: where the cycles go",
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
    cd.add_series("Per packet (fixed)", (9421, 12990, 7537, 11897))
    cd.add_series("Per byte (one copy)", (0, 1602, 0, 1194))
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
        "1472 B points: 54H 1.09 cycles/B + 12,990, 7120 0.81 cycles/B + 11,897; the 1024 B "
        "point lands within 5 cycles. Ethernet L2, offload, hot path in TCM / RAM01.",
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
                    "SW checksum adds ~2.4-2.8k/pkt from MRAM or 1-cycle RAM, 5.5k from 7120 "
                    "RAM02/03.",
                    False,
                ),
            ],
            [
                ("Per-packet dominates: ", True),
                (
                    "~13.0k (54H, TCM) / ~11.9k (7120, RAM01) fixed: alloc, socket, UDP/IP, L2, "
                    "scheduling. 87-91% of the total, lean builds included.",
                    False,
                ),
            ],
            [
                ("Gap: ", True),
                (
                    "1.55x on 54H @ 320, 1.74x on 7120 @ 256; lean 1.50x / 1.43x. For the "
                    "350 TCP target: 1.36x / 1.52x, lean 1.31x / 1.25x.",
                    False,
                ),
            ],
            [
                ("No copy at all: ", True),
                (
                    "dropping the socket copy too gives only ~290 Mbps (54H @ 320), ~253 "
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
        "that scale with packets, not bytes. Lean 7120 build (RAM01): 0.82 cycles/B + 9,543, 89% "
        "fixed. Lean 54H build @ 320: 1.28 cycles/B + 12,194, 87% fixed, but its 1024 B point "
        "is 156 cycles off the line, so take that split as rough; the slope is outside the "
        "0.8-1.1 bench-config range for the same reason.\n\n"
        "No copy at all = drop the per-byte term, i.e. the socket copy from the app buffer "
        "into net_pkt, which the bench still does. This is not driver zero-copy: the null "
        "driver has no copy to remove.\n\n"
        "Budgets use nominal clocks; with measured 319.4 / 256.2 MHz they are 9,404 / 7,542.",
    )

    # 6. Task 2: buffers
    s = prs.slides.add_slide(layout(prs, "13_sub_full content"))
    set_title(
        s,
        "Backup: network buffers (task 2)",
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
        "Backup: other platforms (task 3)",
        "Collected with an LLM, not verified; see the footnote for what we use from it",
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
            [["Platform", "CPU @ MHz", "Stack", "Result", "MHz/Mbps", "What makes it work"]]
            + TASK3_ROWS,
            [1.6, 1.3, 1.15, 1.85, 0.75, 2.35],
            size=7,
            row_h=0.23,
        )
    if TASK3_NOTES:
        text_box(s, 0.5, 4.25, 9.0, 0.9, TASK3_NOTES, size=8)
    text_box(s, 0.5, 5.17, 8.7, 0.25, LLM_FOOTNOTE, size=7, color=GREY, italic=True)

    # 7b. Task 3: Zephyr hosts
    s = prs.slides.add_slide(layout(prs, "13_sub_full content"))
    set_title(
        s,
        "Backup: other Zephyr hosts (task 3)",
        "Zephyr zperf numbers from upstream PRs, collected with an LLM, not verified",
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
                "MHz/Mbps",
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
    text_box(s, 0.5, 5.17, 8.7, 0.25, LLM_FOOTNOTE, size=7, color=GREY, italic=True)

    # 8b. Zephyr gaps
    s = prs.slides.add_slide(layout(prs, "13_sub_full content"))
    set_title(
        s,
        "Backup: Zephyr gaps to raise upstream",
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
                "Per-packet cost is 87-91% of the cycles; stack runs once per 1472 B",
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
                "With offload, partial hot path: 54H TCM +61%, 7120 RAM01 +45%. Done by hand "
                "with zephyr_code_relocate per file",
                "__net_hot annotation and a Kconfig to place it in TCM/SRAM",
                "Medium",
            ],
            [
                "Socket TX always copies",
                "One memcpy per packet, 9-11% of the offload case (measured)",
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
        "Backup: SoC options for an nRF71-based host",
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
                "Single-cycle local RAM (TCM) for the net path and kernel",
                "+61% (54H TCM), +45% (7120 RAM01) over MRAM, partial hot path. All code "
                "zero-wait: ~334 Mbps @ 320 (54H), ~310 @ 256 (7120), est.",
                "Measured on both boards. No SW model change; driver and kernel gain too",
                "Area. Must be single-cycle: 7120 RAM02/03 (2-cycle) gave only +10%. Alone "
                "it stays below 400",
            ],
            [
                "Bigger I-cache, code RAM apart from DMA'd data RAM",
                "7120 (8 KB) misses 753/pkt vs 508 on 54H (16 KB), ~11 cycles each: ~44% of "
                "the MRAM packet is stall. Code next to data: -1% (no Wi-Fi DMA here)",
                "Cheaper than TCM, no linker work",
                "Misses still pay MRAM wait states; 32 KB already proposed for the next part",
            ],
            [
                "Higher CM33 clock (or M55/M85)",
                "400 needs ~365-480 MHz lean, ~330-380 MHz with all code zero-wait",
                "Every SW path gains, nothing to port",
                "256-320 MHz cap comes from the backend cell choice; faster cells cost "
                "power, leakage, area. MRAM miss cost grows with clock (7 to 18 cycles)",
            ],
            [
                "Checksum in the HW datapath, TX and RX",
                "+15-19% UDP TX (measured, both boards)",
                "Cheapest big win; nRF71 FW already does TX",
                "FW cycles per packet today (WZN-9251). RX is advertised, FW validation "
                "unconfirmed",
            ],
            [
                "Zero-copy DMA from host net_buf",
                "Removes the driver copy, 9-11% (measured memcpy)",
                "Less bus load as well",
                "Buffer ownership across cores, alignment and headroom; buffers held "
                "until TX-done (task 2 pool size)",
            ],
            [
                "HW TSO/USO and GRO in the Wi-Fi datapath",
                "Stack runs once per up to 64 KB. Linux GSO 2.4x, GRO 1.8x; at 2x, "
                "400 Mbps needs ~180-240 MHz (est., E8)",
                "Only option that cuts the 87-91% per-packet cost. Fits A-MPDU",
                "No TSO/GRO in Zephyr yet. HW segments and builds headers. UDP gain "
                "only for big datagrams; TCP also needs window scaling",
            ],
            [
                "Dedicated network processor (TCP/IP off host)",
                "Host stack cost replaced by socket offload + IPC per call (TO MEASURE)",
                "Host CPU free for the app; network core sized for the job",
                "Network core needs ~365-480 MHz CM33-class unless it has the rows above. "
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
        "Local RAM: 54H offload, code in MRAM 159.2 vs partial hot path in TCM 255.9 (same "
        "session). 7120 offload: MRAM 159.3, RAM02/03 175.5, RAM01 230.4, RAM00 228.7 (same "
        "session, results/ramblk_*.csv). Zero-wait ceiling, 54H: lean at 64 MHz is 11,289 "
        "cycles/pkt (MRAM wait closest to zero), scaled to 320 MHz. 7120: lean RAM01 is 10,743 "
        "with 97 misses left; at ~11 cycles per miss that is ~9.7k, ~310 Mbps at 256.\n\n"
        "Clock: 33,967 pkt/s x 14,080 (54H lean) = 478 MHz, x 10,743 (7120 lean) = 365 MHz; "
        "zero-wait x 11,289 = 383 MHz (54H), x ~9.7k = ~330 MHz (7120). 54H per-miss cost "
        "fit: ~4 cycles + ~43 ns, so wait states do not "
        "shrink with clock.\n\n"
        "TSO/GRO: 2x is a planning number taken from the Linux results (LPC 2018), not ours: "
        "14,080 / 2 = 7,040 cycles x 33,967 pkt/s = 239 MHz (54H); 10,743 / 2 = 182 MHz "
        "(7120). Part of the gain comes from SW GSO "
        "in the stack alone; HW makes the segmentation free. E8 measures it on the null "
        "driver.\n\n"
        "Network processor: the stack cost per packet does not go away, it moves. The only "
        "Wi-Fi offload part found (SiWG917) is 20 MHz class.",
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
        "./pdk_build.sh                          # every variant",
        "./repro_extra.sh pdk_ramblk pdk_boot2boot_ram01  # current",
        "./pdk_sweep.sh base 1500 reloc hot eth_sw eth_off \\",
        "    eth_sw_hot eth_off_hot eth_off_hot_abl  # RAM02/03",
        "./repro_extra.sh pdk_mem pdk_crossover  # older probes",
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
            "7120 local-RAM builds add sramcode-ram01.overlay: RAM01 becomes the SRAMCODE "
            "region and data stays in RAM00 (the KMU push area has to stay at RAM_ADDR). "
            "sramcode.overlay (RAM02/03, 2-cycle) is the older runs.",
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
            ["Loopback", "SW", "MRAM", "171.7", "125.7", "39.1", "17,556"],
            ["Loopback", "SW", "whole stack RAM01", "257.2", "193.7", "64.0", "11,720"],
            ["Loopback", "SW", "hot path RAM01", "234.6", "175.5", "57.0", "12,851"],
            ["Loopback", "SW", "hot path RAM02/03", "156.8", "118.6", "39.5", "19,233"],
            ["Null Ethernet", "SW", "MRAM", "139.0", "100.7", "30.8", "21,682"],
            ["Null Ethernet", "SW", "hot path RAM01", "194.2", "143.4", "45.4", "15,526"],
            ["Null Ethernet", "offload", "MRAM", "159.3", "113.0", "33.3", "18,928"],
            ["Null Ethernet", "offload", "hot path RAM02/03", "175.5", "124.7", "36.9", "17,185"],
            ["Null Ethernet", "offload", "hot path RAM00", "228.7", "163.6", "48.9", "13,190"],
            ["Null Ethernet", "offload", "hot path RAM01", "230.4", "164.9", "49.3", "13,091"],
            ["Eth, lean", "offload", "MRAM (earlier)", "185.5", "132.0", "39.1", "16,261"],
            ["Eth, lean", "offload", "hot path RAM01", "280.7", "202.2", "61.2", "10,743"],
        ],
        [1.7, 0.9, 1.6, 0.9, 0.9, 0.9, 1.76],
        size=7,
        row_h=0.2,
        bold_rows=(10,),
    )
    text_box(
        s,
        0.67,
        4.3,
        8.66,
        0.9,
        [
            "One session (results/ramblk_*.csv) except lean MRAM. RAM02/03 are 2-cycle for the "
            "CM33, RAM00/01 single-cycle; RAM00 rows put code in the same block as the data. "
            "Loopback MRAM at 1 KB (125.7) is within 2% of the old PDK baseline (124).",
            "calc_chksum at 1472 B: 1,734 cycles (cached MRAM), 1,831 (RAM01), 4,116 (RAM02/03); "
            "memcpy 1,229 (0.84/B). Lean = overlay-ablate.conf. Zephyr SDK 1.0.0 (GCC 14.3).",
        ],
        size=8,
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
            "+ TCM (7120: 230 with RAM01); the MRAM penalty is not linear in clock, so treat it "
            "as rough.",
            "zephyr 2bbcaa7ddf2, sdk-nrf 02edfdd44a0 (loopback) / b419792d143 (Ethernet), "
            "Zephyr SDK 1.0.0 (GCC 14.3).",
        ],
        size=8,
    )

    s = prs.slides.add_slide(layout(prs, "13_sub_full content"))
    set_title(
        s,
        "Backup: why lean is not the default",
        "overlay-ablate.conf is three Kconfig switches, no stack code change",
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
            ["Switch", "What it removes per packet", "Measured gain", "Production?"],
            [
                "NET_TC_TX_COUNT=0",
                "TX queue, wake-up, 2 context switches; on 7120 that code runs from MRAM",
                "54H +3.1%, 7120 +9.4% (RAM02/03 run)",
                "Only after real-driver test",
            ],
            [
                "NET_STATISTICS=n",
                "Per-layer counters",
                "Rest of the lean gain, with logging",
                "Yes, lose net stats",
            ],
            [
                "NET_LOG=n",
                "Log level checks in net code",
                "(combined above)",
                "Yes, 7120 sta sample already does",
            ],
            [
                "All three (lean)",
                "I-cache misses 233 to 97 per packet on 7120",
                "54H +3.7% (258 to 267), 7120 +22% (230 to 281)",
                "-",
            ],
        ],
        [1.6, 3.2, 2.2, 1.66],
        size=8,
        row_h=0.3,
        bold_rows=(4,),
    )
    tb = s.shapes.add_textbox(Inches(0.67), Inches(3.25), Inches(8.66), Inches(1.9))
    fill_bullets(
        tb.text_frame,
        [
            [
                ("Every sender runs the driver. ", True),
                (
                    "With no TX thread, socket to Wi-Fi driver runs in the caller's context. Each "
                    "sending thread needs that stack depth; only the one 4 KB TX stack goes away.",
                    False,
                ),
            ],
            [
                ("TCP ACKs go out from the RX thread. ", True),
                (
                    "Retransmits from the TCP work queue. If the driver waits for FW TX credits "
                    "or IPC, RX stalls behind it, and shared driver locks risk a deadlock. "
                    "send() blocks too. Null Ethernet never blocks, so the bench cannot show it.",
                    False,
                ),
            ],
            [
                ("Not what NCS ships. ", True),
                (
                    "nRF71 Kconfig forces TC_TX_COUNT=1 (from nRF70) and every Wi-Fi sample sets "
                    "it; Zephyr's own default is 0. Net shell over telnet/MQTT needs the thread.",
                    False,
                ),
            ],
            [
                ("So: ", True),
                (
                    "headline = bench config, lean = upside once the real nRF71 driver passes "
                    "UDP + TCP with TC_TX_COUNT=0. Verdict unchanged: lean is 1.43x short.",
                    False,
                ),
            ],
        ],
        size=9,
        bullets=True,
    )
    for p in tb.text_frame.paragraphs:
        p.space_after = Pt(4)
    notes(
        s,
        "Lean gains more on the 7120 because the code it removes (k_fifo, scheduler, net_tc.c, "
        "stats) is not in the relocated hot path and runs from MRAM behind an 8 KB I-cache. "
        "136 fewer misses at ~11 cycles is ~1.5k of the 2.35k cycles saved. The 54H gains only "
        "+3.7%; we have not fully explained the difference. The per-switch split on the 7120 is "
        "from the older RAM02/03 build (TX thread +9.4% of +15%); not yet redone for RAM01.\n\n"
        "The 4 KB TX stack is the nRF71 default (NET_TX_STACK_SIZE). One sending thread is "
        "roughly a wash on RAM; several senders each pay for the full TX path.\n\n"
        "To make lean a recommendation: per-switch split on RAM01, then the real nRF71 driver "
        "on the PDK with TC_TX_COUNT=0, UDP and TCP, watching for RX stalls and deadlocks. If "
        "clean, propose dropping the forced default in drivers/wifi/nrf71/Kconfig.",
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


def build_prb(src, out):
    """One slide for the PRB: answer, what it takes, how sure we are. No jargon."""
    prs = Presentation(src)
    drop_slides_after_first(prs)
    sld_ids = prs.slides._sldIdLst
    first = list(sld_ids)[0]
    prs.part.drop_rel(first.get(qn("r:id")))
    sld_ids.remove(first)

    s = sub_slide(
        prs,
        "Wi-Fi throughput on one 320 MHz processor",
        "CAL-7627: best TCP throughput on a 320 MHz nRF71 derivative, 80 MHz radio channel",
    )
    from pptx.enum.shapes import MSO_SHAPE

    boxes = (
        (
            0.67,
            "Today: nRF7120",
            GREY,
            "About 30 Mbps TCP",
            DARK,
            "About 55 Mbps UDP",
            DARK,
            "256 MHz, 20 MHz channel",
        ),
        (
            3.92,
            "320 MHz derivative",
            NAVY,
            "About 250 Mbps TCP",
            NAVY,
            "About 330 Mbps UDP",
            BLUE,
            "80 MHz channel, with what it takes",
        ),
    )
    for x, head, head_col, tcp, tcp_col, udp, udp_col, foot in boxes:
        card(s, x, 1.6, 2.75, 2.3)
        text_box(s, x + 0.13, 1.68, 2.5, 0.3, head, size=12, color=head_col, bold=True)
        text_box(s, x + 0.13, 2.05, 2.5, 0.42, tcp, size=16, color=tcp_col, bold=True)
        text_box(s, x + 0.13, 2.85, 2.5, 0.4, udp, size=13, color=udp_col, bold=True)
        text_box(s, x + 0.13, 3.4, 2.5, 0.4, foot, size=10, color=GREY)
    text_box(s, 4.05, 2.47, 2.5, 0.3, "at best, range 200-265", size=10, color=NAVY)

    arrow = s.shapes.add_shape(
        MSO_SHAPE.RIGHT_ARROW, Inches(3.5), Inches(2.5), Inches(0.36), Inches(0.42)
    )
    arrow.fill.solid()
    arrow.fill.fore_color.rgb = BLUE
    arrow.line.fill.background()

    text_box(
        s,
        0.67,
        4.05,
        6.0,
        0.35,
        "Limited by the processor, not the radio.",
        size=12,
        color=DARK,
        bold=True,
    )

    text_box(s, 6.88, 1.55, 2.5, 0.35, "What it takes", size=14, color=NAVY, bold=True)
    tb = s.shapes.add_textbox(Inches(6.88), Inches(1.95), Inches(2.5), Inches(2.0))
    fill_bullets(
        tb.text_frame,
        [
            "Run the software from fast on-chip memory",
            "Checksums in the Wi-Fi chip (already done today)",
            "Larger TCP window in Zephyr",
            "Trim software overhead",
        ],
        size=11,
        bullets=True,
    )
    for p in tb.text_frame.paragraphs:
        p.space_after = Pt(5)
    text_box(
        s,
        6.8,
        3.95,
        2.6,
        0.55,
        "More than 250 needs packet batching (software, Wi-Fi chip or both), or more "
        "processor power.",
        size=11,
        color=NAVY,
        bold=True,
    )

    text_box(
        s,
        0.67,
        4.75,
        8.68,
        0.35,
        "Measured on today's chips. The 320 MHz, Wi-Fi and TCP parts are estimates.",
        size=10,
        color=GREY,
        italic=True,
    )
    notes(
        s,
        "Details and the full chain: Confluence page 3562799288 (CAL-7627).\n\n"
        "230 measured, 281 with the trimmed config (measured), 351 at 320 MHz, 385 with the "
        "whole stack in on-chip memory, x0.85 Wi-Fi driver = 327 UDP, x0.75 TCP = 245. The "
        "Wi-Fi factor is from the driver alone on FPGA, so the answer is a best case.\n\n"
        "What it takes, in detail: network stack, Wi-Fi driver and kernel in 192 KB of "
        "single-cycle TCM; checksum offload in the RPU; TCP window scaling plus ~160 KB of "
        "packet buffers; zero-copy TX; net stats, logging and the TX thread off.\n\n"
        "More than 250: TSO/GRO in the Wi-Fi FW/HW (up to ~2x) or a second CM33 for the "
        "driver (~280-290).",
    )
    prs.save(out)


if __name__ == "__main__":
    if len(sys.argv) > 3 and sys.argv[3] == "prb":
        build_prb(sys.argv[1], sys.argv[2])
    else:
        build(sys.argv[1], sys.argv[2])
