#!/usr/bin/env python3
"""
Compilador HTML / CBML a Binario TLVGL Super Denso para CBDos.
Integra compresión de texto con diccionarios híbridos SIMD (1B a 3B) y soporte de widgets.
"""

import sys
import struct
import argparse
import re
from html.parser import HTMLParser
from pathlib import Path

from tlv_dict import encode_hybrid_text
from mesh_proto import (
    TYPE_ABS_PAGE, TYPE_ABS_TEXT, TYPE_ABS_LINK, TYPE_ABS_INPUT,
    TYPE_ABS_IMAGE, TYPE_ABS_CHECKBOX, TYPE_ABS_SWITCH, TYPE_ABS_SLIDER,
    TYPE_ABS_PROGRESS, TYPE_ABS_DROPDOWN, TYPE_ABS_PANEL, TYPE_ABS_CHART,
    TYPE_ABS_ARC, TYPE_ABS_SPINNER, TYPE_END
)

MAX_W = 480
MAX_H = 800

STYLE_MAP = {
    'h1': 1,
    'h2': 2,
    'h3': 3,
    'p': 0,
    'span': 0,
    'i': 0,
}

HEIGHT_MAP = {
    'h1': 28,
    'h2': 24,
    'h3': 20,
    'p': 18,
    'span': 18,
    'a': 36,
    'button': 38,
    'input': 38,
    'chart': 110,
    'slider': 26,
    'progress': 22,
    'switch': 32,
    'checkbox': 28,
    'panel': 100,
}

MARGIN_Y_AFTER = {
    'h1': 4,
    'h2': 4,
    'h3': 3,
    'p': 4,
    'span': 2,
    'a': 6,
    'button': 6,
    'input': 6,
    'chart': 8,
    'slider': 6,
    'progress': 6,
    'switch': 6,
    'checkbox': 4,
    'panel': 8,
}


def _parse_css_color(style_str: str) -> int:
    """Convierte background-color: #RRGGBB a RGB565 (uint16)."""
    m = re.search(r'background-color:\s*#([0-9a-fA-F]{6})', style_str)
    if m:
        hex_str = m.group(1)
        r = int(hex_str[0:2], 16)
        g = int(hex_str[2:4], 16)
        b = int(hex_str[4:6], 16)
        return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
    return 0x0000


def _parse_geometry(attrs: dict, default_y: int, screen_w: int, tag: str, parent_panel=None):
    """Calcula x, y, w, h desde los atributos o calcula el layout fluido."""
    style_str = attrs.get('style', '')

    # Posición X
    m_left = re.search(r'left:\s*(\d+)px', style_str)
    if m_left:
        x = int(m_left.group(1))
    elif 'data-x' in attrs:
        x = int(attrs['data-x'])
    elif parent_panel:
        x = parent_panel['x'] + 8
    else:
        x = 12

    # Posición Y
    m_top = re.search(r'top:\s*(\d+)px', style_str)
    if m_top:
        y = int(m_top.group(1))
    elif 'data-y' in attrs:
        y = int(attrs['data-y'])
    else:
        y = default_y

    # Ancho W
    m_w = re.search(r'width:\s*(\d+)px', style_str)
    if m_w:
        w = int(m_w.group(1))
    elif 'data-w' in attrs:
        w = int(attrs['data-w'])
    elif parent_panel:
        w = max(parent_panel['w'] - 16, 60)
    else:
        w = max(screen_w - 24, 100)

    # Alto H
    m_h = re.search(r'height:\s*(\d+)px', style_str)
    if m_h:
        h = int(m_h.group(1))
    elif 'data-h' in attrs:
        h = int(attrs['data-h'])
    else:
        h = HEIGHT_MAP.get(tag, 24)

    return x, y, w, h


class _LayoutParser(HTMLParser):
    def __init__(self, screen_w: int, screen_h: int):
        super().__init__()
        self.screen_w = screen_w
        self.screen_h = screen_h
        self.elements = []
        self._y = 10
        self._link_id = 1
        self._elem_id = 1
        self._active_panel = None
        self._current_tag = None
        self._current_attrs = {}
        self._text_buf = []
        self._capture_text = False

    def handle_starttag(self, tag: str, attrs: list):
        tag = tag.lower()
        attr_dict = dict(attrs)

        if tag in ('panel', 'div'):
            x, y, w, h = _parse_geometry(attr_dict, self._y, self.screen_w, 'panel', self._active_panel)
            bg_color = _parse_css_color(attr_dict.get('style', ''))
            panel_data = {
                'tag': 'panel',
                'x': x, 'y': y, 'w': w, 'h': h,
                'bg_color': bg_color,
                'attrs': attr_dict
            }
            self.elements.append(panel_data)
            self._active_panel = panel_data
            self._y = y + 8
            return

        self._current_tag = tag
        self._current_attrs = attr_dict
        self._text_buf = []
        self._capture_text = True

    def handle_endtag(self, tag: str):
        tag = tag.lower()
        if tag in ('panel', 'div'):
            if self._active_panel:
                self._y = self._active_panel['y'] + self._active_panel['h'] + 8
                self._active_panel = None
            return

        if tag != self._current_tag:
            return

        self._capture_text = False
        texto = " ".join(self._text_buf).strip()

        x, y, w, h = _parse_geometry(self._current_attrs, self._y, self.screen_w, tag, self._active_panel)

        el_data = {
            'tag': tag,
            'texto': texto,
            'x': x, 'y': y, 'w': w, 'h': h,
            'style': STYLE_MAP.get(tag, 0),
            'attrs': self._current_attrs,
            'link_id': self._link_id if tag in ('a', 'button') else 0,
            'elem_id': self._elem_id
        }

        if tag in ('a', 'button'):
            self._link_id += 1
        elif tag in ('input', 'select', 'slider', 'switch', 'checkbox'):
            self._elem_id += 1

        self.elements.append(el_data)

        if 'top' not in self._current_attrs.get('style', '') and 'data-y' not in self._current_attrs:
            self._y += h + MARGIN_Y_AFTER.get(tag, 4)

        self._current_tag = None

    def handle_data(self, data: str):
        if self._capture_text:
            stripped = data.strip()
            if stripped:
                self._text_buf.append(stripped)


class TLVGLCompiler:
    def __init__(self):
        self.last_link_map = {}

    def compile(self, html: str, screen_w: int = MAX_W, screen_h: int = MAX_H) -> bytes:
        parser = _LayoutParser(screen_w, screen_h)
        parser.feed(html)

        self.last_link_map = {}
        for el in parser.elements:
            if el['tag'] in ('a', 'button'):
                lid = el['link_id']
                href = el['attrs'].get('href', '')
                if href:
                    self.last_link_map[lid] = href

        output = bytearray()
        # Página base
        output.extend(struct.pack(">BH", TYPE_ABS_PAGE, 0))

        for el in parser.elements:
            tag = el['tag']
            x, y, w, h = el['x'], el['y'], el['w'], el['h']
            texto = el.get('texto', '')
            attrs = el.get('attrs', {})

            if tag == 'panel':
                bg_color = el.get('bg_color', 0)
                payload = struct.pack(">HHHHH", x, y, w, h, bg_color)
                output.extend(struct.pack(">BH", TYPE_ABS_PANEL, len(payload)) + payload)

            elif tag in ('h1', 'h2', 'h3', 'p', 'span', 'i'):
                style = el['style']
                text_bytes = encode_hybrid_text(texto)
                payload = struct.pack(">HHHHB", x, y, w, h, style) + text_bytes
                output.extend(struct.pack(">BH", TYPE_ABS_TEXT, len(payload)) + payload)

            elif tag in ('a', 'button'):
                link_id = el['link_id'] & 0xFF
                text_bytes = encode_hybrid_text(texto)
                payload = struct.pack(">HHHHB", x, y, w, h, link_id) + text_bytes
                output.extend(struct.pack(">BH", TYPE_ABS_LINK, len(payload)) + payload)

            elif tag == 'input':
                itype = attrs.get('type', 'text').lower()
                elem_id = el['elem_id'] & 0xFF
                is_toggle = 'toggle' in attrs.get('class', '').lower()

                if itype == 'range' or tag == 'slider':
                    min_v = int(attrs.get('min', 0))
                    max_v = int(attrs.get('max', 100))
                    cur_v = int(attrs.get('value', min_v))
                    payload = struct.pack(">HHHHBhhh", x, y, w, h, elem_id, min_v, max_v, cur_v)
                    output.extend(struct.pack(">BH", TYPE_ABS_SLIDER, len(payload)) + payload)

                elif itype == 'checkbox' and is_toggle:
                    state = 1 if 'checked' in attrs else 0
                    payload = struct.pack(">HHHHBB", x, y, w, h, elem_id, state)
                    output.extend(struct.pack(">BH", TYPE_ABS_SWITCH, len(payload)) + payload)

                elif itype == 'checkbox':
                    state = 1 if 'checked' in attrs else 0
                    text_bytes = encode_hybrid_text(texto)
                    payload = struct.pack(">HHHHBB", x, y, w, h, elem_id, state) + text_bytes
                    output.extend(struct.pack(">BH", TYPE_ABS_CHECKBOX, len(payload)) + payload)

                else: # Texto estándar
                    ph = attrs.get('placeholder', attrs.get('name', ''))
                    ph_bytes = encode_hybrid_text(ph)
                    payload = struct.pack(">HHHHB", x, y, w, h, elem_id) + ph_bytes
                    output.extend(struct.pack(">BH", TYPE_ABS_INPUT, len(payload)) + payload)

            elif tag == 'progress':
                min_v = int(attrs.get('min', 0))
                max_v = int(attrs.get('max', 100))
                cur_v = int(attrs.get('value', min_v))
                payload = struct.pack(">HHHHhhh", x, y, w, h, min_v, max_v, cur_v)
                output.extend(struct.pack(">BH", TYPE_ABS_PROGRESS, len(payload)) + payload)

            elif tag == 'chart':
                chart_type = 1 if attrs.get('type', 'line').lower() == 'bar' else 0
                val_str = attrs.get('values', '')
                vals = []
                for v in val_str.split(','):
                    v = v.strip()
                    if v:
                        try:
                            vals.append(int(v))
                        except ValueError:
                            pass
                pts_bytes = bytearray()
                for val in vals:
                    pts_bytes.extend(struct.pack(">h", val))
                payload = struct.pack(">HHHHBH", x, y, w, h, chart_type, len(vals)) + pts_bytes
                output.extend(struct.pack(">BH", TYPE_ABS_CHART, len(payload)) + payload)

        output.append(TYPE_END)
        return bytes(output)


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description="Compilador HTML → TLVGL Super Denso para CBDos")
    ap.add_argument("html_file", help="Ruta al archivo HTML")
    ap.add_argument("--w", type=int, default=MAX_W, dest="screen_w")
    ap.add_argument("--h", type=int, default=MAX_H, dest="screen_h")
    args = ap.parse_args()

    html_path = Path(args.html_file)
    with open(html_path, "r", encoding="utf-8") as f:
        html_content = f.read()

    compiler = TLVGLCompiler()
    tlv_bytes = compiler.compile(html_content, args.screen_w, args.screen_h)

    out_path = html_path.with_suffix(".tlvgl")
    with open(out_path, "wb") as f:
        f.write(tlv_bytes)

    print(f"✅ '{html_path}' → '{out_path}' ({len(tlv_bytes)} bytes) @ {args.screen_w}×{args.screen_h}px")
