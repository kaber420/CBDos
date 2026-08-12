import argparse
import struct
import sys
from html.parser import HTMLParser
from pathlib import Path

MAX_W = 480
MAX_H = 640

# Absolute TLV Types (compatible with gateway/server/encoder.py)
TYPE_ABS_PAGE  = 0x10
TYPE_ABS_TEXT  = 0x11
TYPE_ABS_LINK  = 0x12
TYPE_ABS_INPUT = 0x13
TYPE_ABS_IMAGE = 0x14
TYPE_ABS_CHECKBOX = 0x15
TYPE_ABS_PANEL = 0x1A
TYPE_ABS_CHART = 0x1B


# Altura visual de cada componente LVGL (incluye padding interno)
HEIGHT_MAP = {
    'h1': 32, 'h2': 26, 'h3': 22,
    'p': 18, 'span': 18, 'i': 18,
    'a': 28, 'button': 28,
    'input': 32,
    'chart': 140,
    'div': 0,
}

STYLE_MAP = {
    'h1': 1, 'h2': 1, 'h3': 1,
    'p': 0, 'span': 0, 'i': 2,
}

# Ancho estimado por carácter (px) según el componente
CHAR_W = {
    'h1': 16, 'h2': 12, 'h3': 10,
    'p': 8, 'span': 8, 'i': 16,
    'a': 9, 'button': 9,
    'input': 8,
}

# Padding horizontal interno del componente
PAD_X = {
    'h1': 6, 'h2': 6, 'h3': 6,
    'p': 6, 'span': 6, 'i': 2,
    'a': 20, 'button': 20,   # botones: padding generoso
    'input': 10,
    'div': 0,
}

# Margen exterior entre elemento y borde de pantalla
MARGIN_X = {
    'h1': 4, 'h2': 4, 'h3': 4,
    'p': 4, 'span': 4, 'i': 4,
    'a': 0, 'button': 0,
    'input': 4,
    'div': 2,
}

# Margen vertical entre elementos
MARGIN_Y_AFTER = {
    'h1': 4, 'h2': 4, 'h3': 4,
    'p': 2, 'span': 2, 'i': 2,
    'a': 6, 'button': 6,
    'input': 6,
    'div': 4,
}

# Tags que no generan elemento visual (solo contenedor)
SKIP_TAGS = {'html', 'head', 'body', 'section', 'article',
             'main', 'nav', 'header', 'footer', 'script', 'style',
             'meta', 'link', 'title', 'noscript', 'form', 'ul', 'ol', 'li'}

def parse_style(style_str):
    styles = {}
    if not style_str:
        return styles
    for part in style_str.split(';'):
        if ':' in part:
            k, v = part.split(':', 1)
            styles[k.strip()] = v.strip()
    return styles

def parse_px(val_str, default=0):
    try:
        return int(val_str.replace('px', '').strip())
    except:
        return default

def _estimate_layout(tag: str, texto: str, screen_w: int, attrs: dict, current_y: int, parent_rect: dict = None):
    styles = parse_style(attrs.get('style', ''))
    
    if 'left' in styles and 'top' in styles:
        x = parse_px(styles['left'])
        y = parse_px(styles['top'])
        w = parse_px(styles.get('width'), 100)
        h = parse_px(styles.get('height'), HEIGHT_MAP.get(tag, 20))
        return x, y, w, h
    
    if 'data-x' in attrs:
        x = int(attrs.get('data-x', 0))
        y = int(attrs.get('data-y', 0))
        w = int(attrs.get('data-w', 100))
        h = int(attrs.get('data-h', HEIGHT_MAP.get(tag, 20)))
        return x, y, w, h

    mx = MARGIN_X.get(tag, 0)
    px = PAD_X.get(tag, 0)
    
    parent_x = parent_rect['x'] if parent_rect else 0
    parent_w = parent_rect['w'] if parent_rect else screen_w

    if tag in ('a', 'button'):
        text_w = len(texto) * CHAR_W.get(tag, 9)
        w = min(text_w + px * 2, parent_w - mx * 2)
        w = max(w, 40)
        x = parent_x + (parent_w - w) // 2
    else:
        w = parent_w - mx * 2
        x = parent_x + mx

    h = HEIGHT_MAP.get(tag, 20)
    y = current_y
    return x, y, w, h


class _LayoutParser(HTMLParser):
    def __init__(self, screen_w: int, screen_h: int):
        super().__init__()
        self.screen_w = screen_w
        self.screen_h = screen_h
        self.elements = []
        self._y = 4
        self._link_id = 1
        self._current_tag = None
        self._current_attrs = {}
        self._text_buf = []
        self._capture_text = False
        self._active_panel = None

    def handle_starttag(self, tag: str, attrs):
        tag = tag.lower()
        attr_dict = dict(attrs)
        
        if tag == 'div':
            x, y, w, h = _estimate_layout(tag, "", self.screen_w, attr_dict, self._y)
            self._active_panel = {'x': x, 'y': y, 'w': w, 'h': h}
            self.elements.append({
                'tag': 'panel',
                'texto': '',
                'x': x, 'y': y, 'w': w, 'h': h,
                'style': 0, 'link_id': 0,
                'attrs': attr_dict,
                'z_index': int(attr_dict.get('data-z', 0)),
                'overflows_x': False, 'overflows_y': False,
            })
            self._y = y + 4
            return

        if tag in SKIP_TAGS:
            return
            
        if tag in HEIGHT_MAP:
            self._current_tag = tag
            self._current_attrs = attr_dict
            self._text_buf = []
            self._capture_text = True
            
            if tag == 'i':
                classes = attr_dict.get('class', '')
                if 'fa-' in classes:
                    icon_map = {
'fa-audio': '\uf001', 'fa-video': '\uf008', 'fa-list': '\uf00b', 'fa-check': '\uf00c', 'fa-times': '\uf00d', 'fa-power-off': '\uf011', 'fa-cog': '\uf013', 'fa-home': '\uf015', 'fa-download': '\uf019', 'fa-hdd': '\uf01c', 'fa-sync': '\uf021', 'fa-volume-mute': '\uf026', 'fa-volume-down': '\uf027', 'fa-volume-up': '\uf028', 'fa-image': '\uf03e', 'fa-tint': '\uf043', 'fa-edit': '\uf044', 'fa-step-backward': '\uf048', 'fa-play': '\uf04b', 'fa-pause': '\uf04c', 'fa-stop': '\uf04d', 'fa-step-forward': '\uf051', 'fa-eject': '\uf052', 'fa-chevron-left': '\uf053', 'fa-chevron-right': '\uf054', 'fa-plus': '\uf067', 'fa-minus': '\uf068', 'fa-eye': '\uf06e', 'fa-eye-slash': '\uf070', 'fa-exclamation-triangle': '\uf071', 'fa-random': '\uf074', 'fa-chevron-up': '\uf077', 'fa-chevron-down': '\uf078', 'fa-redo': '\uf01e', 'fa-folder': '\uf07b', 'fa-upload': '\uf093', 'fa-phone': '\uf095', 'fa-cut': '\uf0c4', 'fa-copy': '\uf0c5', 'fa-save': '\uf0c7', 'fa-bars': '\uf0c9', 'fa-envelope': '\uf0e0', 'fa-bolt': '\uf0e7', 'fa-bell': '\uf0f3', 'fa-keyboard': '\uf11c', 'fa-map-marker-alt': '\uf3c5', 'fa-wifi': '\uf1eb', 'fa-battery-full': '\uf240', 'fa-battery-three-quarters': '\uf241', 'fa-battery-half': '\uf242', 'fa-battery-quarter': '\uf243', 'fa-battery-empty': '\uf244', 'fa-usb': '\uf287', 'fa-bluetooth': '\uf293', 'fa-trash': '\uf1f8', 'fa-backspace': '\uf55a', 'fa-sd-card': '\uf7c2', 'fa-user': '\uf007', 'fa-thermometer-half': '\uf2c9'
                    }
                    for cls in classes.split():
                        if cls in icon_map:
                            self._text_buf.append(icon_map[cls])

    def handle_endtag(self, tag: str):
        tag = tag.lower()
        if tag == 'div':
            if self._active_panel:
                self._y = self._active_panel['y'] + self._active_panel['h'] + 4
                self._active_panel = None
            return

        if tag != self._current_tag:
            return
            
        self._capture_text = False
        texto = " ".join(self._text_buf).strip()

        if tag == 'input':
            texto = (self._current_attrs.get('placeholder') or self._current_attrs.get('name', ''))

        if not texto and tag != 'input' and tag != 'i':
            self._current_tag = None
            return

        x, y, w, h = _estimate_layout(tag, texto, self.screen_w, self._current_attrs, self._y, self._active_panel)

        overflows_x = (x + w > self.screen_w)
        overflows_y = (y + h > self.screen_h)

        self.elements.append({
            'tag': tag,
            'texto': texto,
            'x': x, 'y': y, 'w': w, 'h': h,
            'style': STYLE_MAP.get(tag, 0),
            'link_id': self._link_id if tag in ('a', 'button') else 0,
            'attrs': self._current_attrs,
            'z_index': int(self._current_attrs.get('data-z', 1)),
            'overflows_x': overflows_x,
            'overflows_y': overflows_y,
        })

        if tag in ('a', 'button'):
            self._link_id += 1

        if 'data-y' not in self._current_attrs and 'top' not in self._current_attrs.get('style', ''):
            self._y += h + MARGIN_Y_AFTER.get(tag, 2)
            
        self._current_tag = None

    def handle_data(self, data: str):
        if self._capture_text:
            stripped = data.strip()
            if stripped:
                self._text_buf.append(stripped)


class TLVGLCompiler:
    def compile(self, html: str, screen_w: int, screen_h: int) -> bytes:
        screen_w = min(max(int(screen_w), 1), MAX_W)
        screen_h = min(max(int(screen_h), 1), MAX_H)

        parser = _LayoutParser(screen_w, screen_h)
        parser.feed(html)

        parser.elements.sort(key=lambda e: e.get('z_index', 0))

        output = bytearray()
        output.extend(struct.pack(">BH", TYPE_ABS_PAGE, 0))

        for el in parser.elements:
            tag = el['tag']
            x, y, w, h = el['x'], el['y'], el['w'], el['h']
            texto = el['texto']
            
            if tag == 'panel':
                # Color oscuro por defecto (Negro puro para evitar tintes verdes en pantallas TFT baratas)
                bg_color = 0x0000 # RGB565 de #000000
                
                style_str = el['attrs'].get('style', '').lower()
                import re
                m = re.search(r'background-color:\s*#([0-9a-f]{6})', style_str)
                if m:
                    hex_str = m.group(1)
                    r = int(hex_str[0:2], 16)
                    g = int(hex_str[2:4], 16)
                    b = int(hex_str[4:6], 16)
                    # Convertir RGB888 a RGB565 (Little/Big endian handling se hace en pack)
                    bg_color = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
                    
                payload = struct.pack(">HHHH H", x, y, w, h, bg_color)
                output.extend(struct.pack(">BH", TYPE_ABS_PANEL, len(payload)) + payload)

            elif tag in ('h1', 'h2', 'h3', 'p', 'span', 'i'):
                style = el['style']
                text_bytes = texto.encode('utf-8', errors='replace')
                payload = struct.pack(">HHHHB", x, y, w, h, style) + text_bytes
                output.extend(struct.pack(">BH", TYPE_ABS_TEXT, len(payload)) + payload)

            elif tag in ('a', 'button'):
                link_id = el['link_id'] & 0xFF
                text_bytes = texto.encode('utf-8', errors='replace')
                payload = struct.pack(">HHHHB", x, y, w, h, link_id) + text_bytes
                output.extend(struct.pack(">BH", TYPE_ABS_LINK, len(payload)) + payload)

            elif tag == 'input':
                attrs = el['attrs']
                action = attrs.get('action', '')
                param = attrs.get('name', '')
                placeholder = attrs.get('placeholder', '')
                str_payload = f"{action}\x00{param}\x00{placeholder}".encode('utf-8', errors='replace')
                payload = struct.pack(">HHHH", x, y, w, h) + str_payload
                output.extend(struct.pack(">BH", TYPE_ABS_INPUT, len(payload)) + payload)

            elif tag == 'chart':
                attrs = el['attrs']
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

        return bytes(output)

if __name__ == '__main__':
    ap = argparse.ArgumentParser(description="Compilador HTML → TLVGL Bento")
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

    print(f"✅ '{html_path}' → '{out_path}' ({len(tlv_bytes)} bytes) "
          f"@ {args.screen_w}×{args.screen_h}px")
