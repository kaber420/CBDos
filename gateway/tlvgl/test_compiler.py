import io
import sys
import unittest
from gateway.tlvgl.tlvgl_compiler import (
    TLVGLCompiler,
    MAX_W,
    MAX_H,
    TYPE_ABS_PAGE,
    TYPE_ABS_TEXT,
    TYPE_ABS_LINK,
    TYPE_ABS_INPUT,
)


class TestTLVGLCompiler(unittest.TestCase):
    def setUp(self):
        self.compiler = TLVGLCompiler()

    def test_simple_html_h1_and_p(self):
        html = "<h1>Titulo</h1><p>Parrafo de prueba</p>"
        res = self.compiler.compile(html, 240, 320)
        
        # Debe retornar bytes no vacíos
        self.assertTrue(isinstance(res, bytes))
        self.assertGreater(len(res), 0)
        
        # Primeros 3 bytes deben ser TYPE_ABS_PAGE (0x10) con longitud 0 (b'\x10\x00\x00')
        self.assertEqual(res[:3], b"\x10\x00\x00")
        
        # Debe contener TYPE_ABS_TEXT (0x11)
        self.assertIn(bytes([TYPE_ABS_TEXT]), res)

    def test_link(self):
        html = '<a href="http://example.com">Enlace Importante</a>'
        res = self.compiler.compile(html, 240, 320)
        
        # Aparece TYPE_ABS_LINK (0x12) en los bytes
        self.assertIn(bytes([TYPE_ABS_LINK]), res)
        # El texto del enlace debe estar en los bytes codificado en UTF-8
        self.assertIn("Enlace Importante".encode("utf-8"), res)

    def test_clamped_resolution(self):
        html = "<h1>Header</h1><p>Content</p>"
        # Solicitar resolución desbordada W=9999 H=9999
        bytes_clamped = self.compiler.compile(html, 9999, 9999)
        # Solicitar resolución máxima permitida MAX_W x MAX_H
        bytes_max = self.compiler.compile(html, MAX_W, MAX_H)
        
        # Deben ser idénticos ya que 9999x9999 se clampa a MAX_W x MAX_H
        self.assertEqual(bytes_clamped, bytes_max)

    def test_linter(self):
        html = "<div><h1>Titulo</h1><p>Texto corto</p></div>"
        
        # Capturar stderr
        captured_stderr = io.StringIO()
        old_stderr = sys.stderr
        try:
            sys.stderr = captured_stderr
            # Con 480x640, el contenido cabe perfectamente y NO produce warnings
            self.compiler.compile(html, 480, 640)
        finally:
            sys.stderr = old_stderr

        self.assertEqual(captured_stderr.getvalue(), "")

        # Verificar que si desborda la pantalla (ejemplo: pantalla muy pequeña H=10), sí produce warning
        captured_stderr_overflow = io.StringIO()
        try:
            sys.stderr = captured_stderr_overflow
            self.compiler.compile(html, 480, 10)
        finally:
            sys.stderr = old_stderr

        output_log = captured_stderr_overflow.getvalue()
        self.assertIn("desborda en Y", output_log)

    def test_different_resolutions_produce_different_bytes(self):
        html = "<h1>Titulo</h1><p>Texto</p>"
        
        bytes_240_320 = self.compiler.compile(html, 240, 320)
        bytes_320_480 = self.compiler.compile(html, 320, 480)
        
        # Resoluciones distintas deben generar bytes distintos (debido al campo W en los payloads)
        self.assertNotEqual(bytes_240_320, bytes_320_480)


if __name__ == "__main__":
    unittest.main()
