import time
import struct
import threading
from typing import Dict, Any, Optional, Callable

import serial
import mesh_framing as framing
import mesh_protocol as proto

class MeshNodeEngine:
    def __init__(self, port: str = "/dev/ttyACM0", baudrate: int = 115200, name: str = "Base-Laptop", short_id: int = 0x00C3):
        self.port = port
        self.baudrate = baudrate
        self.name = name
        self.short_id = short_id
        self.channel = 1
        self.is_lr = True
        
        self.ser: Optional[serial.Serial] = None
        self.running = False
        self.seen_packet_ids = set()
        self.discovered_nodes: Dict[int, Dict[str, Any]] = {}

        # Range Test & Auto-Echo
        self.auto_echo = True
        self.range_test_active = False
        self.range_test_interval = 3
        self.range_test_seq = 0
        self.last_rx_rssi = 0
        
        # Telemetría
        self.stat_tx = 0
        self.stat_rx = 0

        # Callbacks
        self.on_node_discovered: Optional[Callable[[Dict[str, Any]], None]] = None
        self.on_chat_received: Optional[Callable[[Dict[str, Any]], None]] = None
        self.on_log: Optional[Callable[[str], None]] = None

    def log(self, msg: str):
        if self.on_log:
            self.on_log(msg)
        else:
            print(msg)

    def start(self) -> bool:
        try:
            self.ser = serial.Serial(self.port, self.baudrate, timeout=0.1)
            self.running = True
            self.log(f"📻 [Radio Bridge] Conectado al Dongle USB en {self.port} @ {self.baudrate} bps")
            self.log(f"🪪 [Identidad] Nombre: '{self.name}' | Short ID: 0x{self.short_id:04X}")

            threading.Thread(target=self._reader_loop, daemon=True).start()
            threading.Thread(target=self._beacon_loop, daemon=True).start()
            threading.Thread(target=self._range_loop, daemon=True).start()

            self.send_beacon()
            return True
        except Exception as e:
            self.log(f"❌ Error conectando a {self.port}: {e}")
            return False

    def stop(self):
        self.running = False
        self.range_test_active = False
        if self.ser and self.ser.is_open:
            self.ser.close()
        self.log("\n🛑 Nodo MeshCore detenido.")

    def set_radio_mode(self, mode: str):
        self.is_lr = (mode.lower() == "lr")
        val = 0x02 if self.is_lr else 0x01
        cmd_payload = bytes([framing.RADIO_CMD_SET_MODE, val])
        frame = framing.encode_serial_frame(framing.DIR_CTRL_CMD, cmd_payload)
        self._write_serial(frame)
        self.log(f"📡 Modo de radio configurado a: {'LONG RANGE (LR)' if self.is_lr else 'NORMAL (1M)'}")

    def set_channel(self, channel: int):
        if not (1 <= channel <= 13):
            self.log("❌ Canal inválido (debe ser 1 a 13)")
            return
        self.channel = channel
        cmd_payload = bytes([framing.RADIO_CMD_SET_CHAN, channel])
        frame = framing.encode_serial_frame(framing.DIR_CTRL_CMD, cmd_payload)
        self._write_serial(frame)
        freq = 2412 + (channel - 1) * 5
        self.log(f"📻 Canal ESP-NOW cambiado a: Canal {channel} ({freq} MHz)")
        # Emitir baliza inmediatamente en el nuevo canal para que nos descubran con el nombre real
        time.sleep(0.1)
        self.send_beacon()


    def _write_serial(self, data: bytes):
        if self.ser and self.ser.is_open:
            try:
                self.ser.write(data)
                self.ser.flush()
            except Exception as e:
                self.log(f"⚠️ Error escribiendo al puerto serie: {e}")

    def send_raw_mesh(self, payload: bytes):
        if not payload:
            return
        frame = framing.encode_serial_frame(framing.DIR_PC_TO_DONGLE, payload)
        self._write_serial(frame)

    def send_beacon(self):
        pkt = proto.pack_beacon(self.short_id, self.name)
        self.send_raw_mesh(pkt)

    def send_chat(self, text: str, target_id: int = 0xFFFF, channel_id: int = 0):
        if not text:
            return
        msg_id, pkt = proto.pack_chat(self.short_id, target_id, channel_id, text)
        self.seen_packet_ids.add(msg_id)
        self.send_raw_mesh(pkt)
        self.stat_tx += 1

    def _beacon_loop(self):
        while self.running:
            time.sleep(30)
            if self.running:
                self.send_beacon()

    def _range_loop(self):
        while self.running:
            if self.range_test_active:
                self.range_test_seq += 1
                hora = time.strftime("%H:%M:%S")
                last_sig = f"{self.last_rx_rssi}dBm" if self.last_rx_rssi != 0 else "N/A"
                ping_text = f"📡 [PING #{self.range_test_seq}] {hora} | Base RSSI:{last_sig}"
                self.send_chat(ping_text, target_id=0xFFFF, channel_id=0)
                self.log(f"📤 [Tu -> #general]: {ping_text}")
                time.sleep(self.range_test_interval)
            else:
                time.sleep(1)

    def _reader_loop(self):
        buf = bytearray()
        while self.running:
            try:
                chunk = self.ser.read(128)
                if not chunk:
                    continue
                buf.extend(chunk)

                while len(buf) >= 6:
                    if buf[0] != framing.FRAME_MAGIC_0 or buf[1] != framing.FRAME_MAGIC_1:
                        buf.pop(0)
                        continue

                    dir_byte = buf[2]
                    plen = (buf[3] << 8) | buf[4]
                    frame_total = 5 + plen + 1

                    if len(buf) < frame_total:
                        break

                    payload = bytes(buf[5:5 + plen])
                    crc_byte = buf[5 + plen]
                    del buf[:frame_total]

                    if framing.crc8_calc(payload) == crc_byte and dir_byte == framing.DIR_DONGLE_TO_PC:
                        if len(payload) >= 7:
                            src_mac = payload[0:6]
                            rssi = struct.unpack("b", payload[6:7])[0]
                            mesh_data = payload[7:]
                            self._handle_incoming_packet(mesh_data, src_mac=src_mac, rssi=rssi)
            except Exception as e:
                if self.running:
                    self.log(f"⚠️ Error en lectura serial: {e}")
                time.sleep(0.5)

    def _handle_incoming_packet(self, data: bytes, src_mac: bytes, rssi: int):
        parsed = proto.unpack_packet(data)
        if not parsed:
            return

        src_id = parsed["src_id"]
        if src_id == self.short_id:
            return

        self.last_rx_rssi = rssi
        self.stat_rx += 1
        mac_str = ":".join(f"{b:02X}" for b in src_mac) if src_mac else "Desconocida"
        parsed["mac"] = mac_str
        parsed["rssi"] = rssi
        parsed["timestamp"] = time.strftime("%H:%M:%S")

        if parsed["type"] == "BEACON":
            is_new = src_id not in self.discovered_nodes
            self.discovered_nodes[src_id] = {
                "name": parsed["name"],
                "last_seen": time.time(),
                "hops": parsed["hops"],
                "rssi": rssi,
                "mac": mac_str
            }
            if is_new and self.on_node_discovered:
                self.on_node_discovered(parsed)

        elif parsed["type"] == "CHAT":
            msg_id = parsed["msg_id"]
            if msg_id in self.seen_packet_ids:
                return
            self.seen_packet_ids.add(msg_id)

            # Auto-asignar nombre si ya se conocía
            parsed["sender_name"] = self.discovered_nodes.get(src_id, {}).get("name", f"Nodo_{src_id:04X}")

            if self.on_chat_received:
                self.on_chat_received(parsed)

            # Procesamiento de comandos remotos
            self._handle_remote_command(parsed)

    def _handle_remote_command(self, chat_info: Dict[str, Any]):
        text = chat_info["text"].strip().lower()
        if text.startswith("/"):
            text = text[1:]

        src_id = chat_info["src_id"]
        chan_id = chat_info["channel_id"]
        rssi = chat_info["rssi"]
        hora = chat_info["timestamp"]

        if text.startswith("range") or text.startswith("test"):
            parts = text.split()
            if len(parts) >= 2 and parts[1] in ("stop", "off", "0"):
                self.range_test_active = False
                resp = f"⏹️ [BASE] Range Test DETENIDO remotamente [{hora}]"
                self.send_chat(resp, target_id=src_id, channel_id=chan_id)
            else:
                sec = 3
                if len(parts) >= 2 and parts[1].isdigit():
                    sec = max(1, int(parts[1]))
                self.range_test_interval = sec
                self.range_test_active = True
                resp = f"📡 [BASE] Range Test INICIADO (emitiendo cada {sec}s) [{hora}]"
                self.send_chat(resp, target_id=src_id, channel_id=chan_id)
                self.log(f"\n📡 [Range Test Activado por Cyberdeck]: Emitiendo cada {sec}s...")

        elif text in ("stop", "range stop", "range off"):
            self.range_test_active = False
            resp = f"⏹️ [BASE] Range Test DETENIDO remotamente [{hora}]"
            self.send_chat(resp, target_id=src_id, channel_id=chan_id)

        elif text.startswith("chan"):
            parts = text.split()
            if len(parts) >= 2 and parts[1].isdigit():
                ch = int(parts[1])
                if 1 <= ch <= 13:
                    self.set_channel(ch)
                    resp = f"📻 [BASE] Canal cambiado a {ch} ({2412 + (ch - 1) * 5} MHz) [{hora}]"
                    self.send_chat(resp, target_id=src_id, channel_id=chan_id)

        elif text == "ping":
            resp = f"🏓 PONG | Base RX: {rssi}dBm [{hora}]"
            self.send_chat(resp, target_id=src_id, channel_id=chan_id)

        elif self.auto_echo and not text.startswith("🔄") and not text.startswith("📡 [PING") and not text.startswith("⏹️") and not text.startswith("🏓") and not text.startswith("📻"):
            echo_text = f"🔄 [ECHO] '{chat_info['text'][:30]}' | RX Base: {rssi}dBm [{hora}]"
            self.send_chat(echo_text, target_id=src_id, channel_id=chan_id)
