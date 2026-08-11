"""
tlvgl_preview_server.py
Servidor HTTP de preview del compilador TLVGL.
Sirve la UI web y un endpoint /compile para compilar HTML en tiempo real.

Uso:
    python3 tlvgl_preview_server.py [--port 8766]
"""
import argparse
import io
import json
import sys
import os
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
from urllib.parse import urlparse

# Añadir el directorio padre al path para importar tlvgl_compiler
sys.path.insert(0, str(Path(__file__).parent))
from tlvgl_compiler import TLVGLCompiler, _LayoutParser, MAX_W, MAX_H

CONTENT_DIR = Path(__file__).parent / "content"


def get_elements_and_bytes(html: str, screen_w: int, screen_h: int) -> dict:
    """Retorna la lista de elementos visuales y el conteo de bytes compilados."""
    screen_w = min(max(int(screen_w), 1), MAX_W)
    screen_h = min(max(int(screen_h), 1), MAX_H)

    # Capturar warnings del linter
    old_stderr = sys.stderr
    sys.stderr = buf = io.StringIO()
    try:
        parser = _LayoutParser(screen_w, screen_h)
        parser.feed(html)
        elements = parser.elements
        warnings = buf.getvalue().strip().splitlines()
    finally:
        sys.stderr = old_stderr

    # Compilar para obtener byte count real
    compiler = TLVGLCompiler()
    tlv_bytes = compiler.compile(html, screen_w, screen_h)

    return {
        "screen_w": screen_w,
        "screen_h": screen_h,
        "byte_count": len(tlv_bytes),
        "element_count": len(elements),
        "warnings": warnings,
        "elements": [
            {
                "tag":     el["tag"],
                "texto":   el["texto"],
                "x":       el["x"],
                "y":       el["y"],
                "w":       el["w"],
                "h":       el["h"],
                "style":   el["style"],
                "link_id": el["link_id"],
                "overflows_x": el["x"] + el["w"] > screen_w,
                "overflows_y": el["y"] + el["h"] > screen_h,
            }
            for el in elements
        ]
    }


# Leer la UI HTML desde archivo hermano
UI_HTML_PATH = Path(__file__).parent / "tlvgl_preview.html"


class PreviewHandler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        pass  # silenciar logs por defecto

    def _send_json(self, data: dict, status: int = 200):
        body = json.dumps(data, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(body)

    def _send_html(self, html: str):
        body = html.encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_OPTIONS(self):
        self.send_response(204)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "POST, GET, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()

    def do_GET(self):
        parsed = urlparse(self.path)
        path = parsed.path

        if path in ("/", "/index.html"):
            if UI_HTML_PATH.exists():
                self._send_html(UI_HTML_PATH.read_text("utf-8"))
            else:
                self._send_json({"error": "tlvgl_preview.html not found"}, 404)

        elif path in ("/editor.css", "/editor.js"):
            file_path = Path(__file__).parent / path.lstrip('/')
            if file_path.exists():
                body = file_path.read_bytes()
                self.send_response(200)
                ctype = "text/css" if path.endswith(".css") else "application/javascript"
                self.send_header("Content-Type", f"{ctype}; charset=utf-8")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)
            else:
                self._send_json({"error": "not found"}, 404)

        elif path.startswith("/content/"):
            filename = path[len("/content/"):]
            file_path = CONTENT_DIR / filename
            if file_path.exists() and file_path.is_file():
                body = file_path.read_text("utf-8")
                self.send_response(200)
                self.send_header("Content-Type", "text/plain; charset=utf-8")
                self.send_header("Content-Length", str(len(body.encode("utf-8"))))
                self.end_headers()
                self.wfile.write(body.encode("utf-8"))
            else:
                self._send_json({"error": "not found"}, 404)

        else:
            self._send_json({"error": "not found"}, 404)

    def do_POST(self):
        parsed = urlparse(self.path)
        if parsed.path != "/compile":
            self._send_json({"error": "not found"}, 404)
            return

        length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(length)
        try:
            payload = json.loads(body.decode("utf-8"))
        except Exception:
            self._send_json({"error": "invalid JSON"}, 400)
            return

        html     = payload.get("html", "")
        screen_w = int(payload.get("w", MAX_W))
        screen_h = int(payload.get("h", MAX_H))

        try:
            result = get_elements_and_bytes(html, screen_w, screen_h)
        except Exception as e:
            self._send_json({"error": str(e)}, 500)
            return

        print(f"[compile] {screen_w}×{screen_h} → {result['byte_count']}B "
              f"({result['element_count']} elems, {len(result['warnings'])} warnings)")
        self._send_json(result)


def run(port: int = 8766):
    server = HTTPServer(("0.0.0.0", port), PreviewHandler)
    print(f"🖥️  TLVGL Preview Server  →  http://localhost:{port}/")
    print(f"   Compilador: {Path(__file__).parent / 'tlvgl_compiler.py'}")
    print(f"   Contenido:  {CONTENT_DIR}")
    print("   Ctrl+C para detener\n")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nDetenido.")


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description="Servidor de preview TLVGL")
    ap.add_argument("--port", type=int, default=8766, help="Puerto HTTP (default: 8766)")
    args = ap.parse_args()
    run(args.port)
