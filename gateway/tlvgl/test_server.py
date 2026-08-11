import asyncio
from pathlib import Path
import struct
import sys
import unittest

pkg_dir = Path(__file__).parent
if str(pkg_dir) not in sys.path:
    sys.path.insert(0, str(pkg_dir))

from tlvgl_compiler import TLVGLCompiler
from tlvgl_server import TLVGLServer, CONTENT_DIR, MAX_W, MAX_H


class TestTLVGLServer(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.server_obj = TLVGLServer(content_dir=CONTENT_DIR, max_w=MAX_W, max_h=MAX_H)
        self.server = await asyncio.start_server(self.server_obj.handle_client, '127.0.0.1', 0)
        self.port = self.server.sockets[0].getsockname()[1]

    async def asyncTearDown(self):
        self.server.close()
        await self.server.wait_closed()

    async def _send_request(self, req_str: str) -> bytes:
        reader, writer = await asyncio.open_connection('127.0.0.1', self.port)
        writer.write(req_str.encode('utf-8'))
        await writer.drain()
        data = await reader.read()
        writer.close()
        await writer.wait_closed()
        return data

    async def test_1_ephemeral_port_connect(self):
        """1. Servidor levanta en puerto efímero y responde a conexión"""
        self.assertGreater(self.port, 0)
        resp = await self._send_request("GET /index.html W=240 H=320\r\n")
        self.assertGreaterEqual(len(resp), 4)

    async def test_2_get_index_html_with_resolution(self):
        """2. GET /index.html W=240 H=320 -> respuesta no vacía (size > 0)"""
        resp = await self._send_request("GET /index.html W=240 H=320\r\n")
        self.assertGreaterEqual(len(resp), 4)
        size = struct.unpack('>I', resp[:4])[0]
        self.assertGreater(size, 0)
        payload = resp[4:]
        self.assertEqual(len(payload), size)

    async def test_3_get_nonexistent_file(self):
        """3. GET /noexiste.html W=240 H=320 -> size = 0"""
        resp = await self._send_request("GET /noexiste.html W=240 H=320\r\n")
        self.assertEqual(len(resp), 4)
        size = struct.unpack('>I', resp[:4])[0]
        self.assertEqual(size, 0)

    async def test_4_legacy_get_without_wh(self):
        """4. Petición legada sin W/H GET /index.html -> usa MAX y responde OK"""
        resp = await self._send_request("GET /index.html\r\n")
        self.assertGreaterEqual(len(resp), 4)
        size = struct.unpack('>I', resp[:4])[0]
        self.assertGreater(size, 0)
        payload = resp[4:]
        self.assertEqual(len(payload), size)

    async def test_5_different_resolutions_different_bytes(self):
        """5. Dos peticiones con distinta resolución para el mismo archivo -> distintos bytes"""
        resp1 = await self._send_request("GET /index.html W=240 H=320\r\n")
        resp2 = await self._send_request("GET /index.html W=480 H=640\r\n")

        size1 = struct.unpack('>I', resp1[:4])[0]
        size2 = struct.unpack('>I', resp2[:4])[0]
        self.assertGreater(size1, 0)
        self.assertGreater(size2, 0)

        payload1 = resp1[4:]
        payload2 = resp2[4:]
        self.assertNotEqual(payload1, payload2)


if __name__ == '__main__':
    unittest.main()
