#!/usr/bin/env python3
"""
Nodo Interactivo MeshCore sobre ESP-NOW para CBDos (Ejecución Aislada en Contenedor).
Emite y recibe paquetes binarios reales de MeshCore ('MC' 0x4D43) a través del Dongle ESP32-C3.
"""

import sys
import os
import time
import struct
import threading
import argparse
from typing import Dict, Any, Optional

try:
    import serial
except ImportError:
    print("❌ Error: Se requiere pyserial (pip install pyserial)")
    sys.exit(1)

# Protocolo Serie Framing ESP-NOW Bridge
FRAME_MAGIC_0    = 0xAA
FRAME_MAGIC_1    = 0x55
DIR_PC_TO_DONGLE = 0x01
DIR_DONGLE_TO_PC = 0x02
DIR_CTRL_CMD     = 0x03
DIR_CTRL_RESP    = 0x04

RADIO_CMD_SET_MODE  = 0x02 # 0x01 = Normal, 0x02 = LR
RADIO_CMD_SET_CHAN  = 0x03 # Canal 1..13

# Protocolo Binario MeshCore
MESHCORE_MAGIC = 0x4D43 # 'MC'
PKT_BEACON     = 0x01
PKT_CHAT       = 0x02
PKT_ACK        = 0x03


def crc8_calc(data: bytes) -> int:
    crc = 0x00
    for byte in data:
        extract = byte
        for _ in range(8):
            sum_bit = (crc ^ extract) & 0x01
            crc >>= 1
            if sum_bit:
                crc ^= 0x8C
            extract >>= 1
    return crc & 0xFF


class MeshCoreNode:
    def __init__(self, port: str = "/dev/ttyACM1", baudrate: int = 115200, node_name: str = "Laptop-CLI", short_id: int = 0x00C3):
        self.port = port
        self.baudrate = baudrate
        self.node_name = node_name
        self.short_id = short_id
        self.active_channel = 0 # 0 = #general
        self.ser: Optional[serial.Serial] = None
        self.running = False
        self.msg_seq = int(time.time() * 1000) & 0x7FFFFFFF
        self.discovered_nodes: Dict[int, Dict[str, Any]] = {}
        self.seen_packet_ids = set()

        # --- Modo Prueba de Alcance (Range Test) & Auto-Echo ---
        self.channel = 1
        self.auto_echo = True
        self.range_test_active = False
        self.range_test_interval = 3 # Segundos
        self.range_test_seq = 0
        self.last_rx_rssi = 0
        self.stat_tx_chat = 0
        self.stat_rx_chat = 0
        self.stat_tx_beacons = 0

    def start(self):
        try:
            self.ser = serial.Serial(self.port, self.baudrate, timeout=0.1)
            self.running = True
            print(f"📻 [MeshCore Node] Conectado al Dongle USB en {self.port} @ {self.baudrate} bps")
            print(f"🪪 [Identidad Local] Nombre: '{self.node_name}' | ID: 0x{self.short_id:04X}")

            # Iniciar hilo de lectura de radio
            t_recv = threading.Thread(target=self._reader_loop, daemon=True)
            t_recv.start()

            # Iniciar emisor de baliza de presencia cada 30 segundos
            t_beacon = threading.Thread(target=self._beacon_loop, daemon=True)
            t_beacon.start()

            # Iniciar hilo de Range Test continuo
            t_range = threading.Thread(target=self._range_test_loop, daemon=True)
            t_range.start()

            # Emitir baliza inicial inmediata
            self.send_beacon()
            return True
        except Exception as e:
            print(f"❌ Error al abrir el puerto {self.port}: {e}")
            return False

    def stop(self):
        self.running = False
        self.range_test_active = False
        if self.ser and self.ser.is_open:
            self.ser.close()
        print("\n🛑 Nodo MeshCore detenido.")

    def set_radio_mode(self, mode: str):
        """Configura modo 'normal' (1 Mbps) o 'lr' (Long Range 250 kbps)."""
        val = 0x02 if mode.lower() == "lr" else 0x01
        payload = bytes([RADIO_CMD_SET_MODE, val])
        frame = bytearray([FRAME_MAGIC_0, FRAME_MAGIC_1, DIR_CTRL_CMD, 0, len(payload)]) + payload
        frame.append(crc8_calc(payload))
        if self.ser and self.ser.is_open:
            self.ser.write(frame)
            self.ser.flush()
            print(f"📡 Modo de radio configurado a: {mode.upper()}")

    def set_channel(self, chan: int):
        """Configura el canal Wi-Fi / ESP-NOW (1..13)."""
        if not (1 <= chan <= 13):
            print("❌ Canal inválido (debe ser 1 a 13).")
            return
        self.channel = chan
        payload = bytes([RADIO_CMD_SET_CHAN, chan])
        frame = bytearray([FRAME_MAGIC_0, FRAME_MAGIC_1, DIR_CTRL_CMD, 0, len(payload)]) + payload
        frame.append(crc8_calc(payload))
        if self.ser and self.ser.is_open:
            self.ser.write(frame)
            self.ser.flush()
            freq_mhz = 2412 + (chan - 1) * 5
            print(f"📻 Canal ESP-NOW cambiado a: Canal {chan} ({freq_mhz} MHz)")


    def _send_raw_mesh(self, payload: bytes):
        """Enmarca la trama MeshCore sobre el Dongle ESP32."""
        if not self.ser or not self.ser.is_open or not payload:
            return
        frame = bytearray([FRAME_MAGIC_0, FRAME_MAGIC_1, DIR_PC_TO_DONGLE, (len(payload) >> 8) & 0xFF, len(payload) & 0xFF])
        frame.extend(payload)
        frame.append(crc8_calc(payload))
        self.ser.write(frame)
        self.ser.flush()

    def send_beacon(self):
        """Emite una baliza PKT_BEACON (0x01) para que otros nodos nos descubran."""
        name_bytes = self.node_name.encode('utf-8')[:32]
        # Magic(2B) + Type(1B) + Hops(1B) + SrcID(2B) + DstID(2B: 0xFFFF) + Spare(2B) + NameLen(1B) + Name(N B)
        header = struct.pack("<HBBHHHB", MESHCORE_MAGIC, PKT_BEACON, 0, self.short_id, 0xFFFF, 0, len(name_bytes))
        pkt = header + name_bytes
        self._send_raw_mesh(pkt)
        self.stat_tx_beacons += 1

    def send_chat(self, text: str, target_id: int = 0xFFFF, channel_id: int = 0, quiet: bool = False):
        """Emite un mensaje de chat PKT_CHAT (0x02)."""
        if not text:
            return
        msg_id = self.msg_seq
        self.msg_seq += 1
        self.seen_packet_ids.add(msg_id)

        text_bytes = text.encode('utf-8')[:180]
        # Magic(2) + Type(1) + Hops(1) + SrcId(2) + DstId(2) + ChanId(2) + MsgId(4) + Flags(1) + PayLen(1) + Text
        header = struct.pack("<HBBHHHIBB", MESHCORE_MAGIC, PKT_CHAT, 0, self.short_id, target_id, channel_id, msg_id, 0x00, len(text_bytes))
        pkt = header + text_bytes
        self._send_raw_mesh(pkt)
        self.stat_tx_chat += 1
        
        if not quiet:
            target_str = "#general" if target_id == 0xFFFF else f"0x{target_id:04X}"
            print(f"📤 [Tu -> {target_str}]: {text}")

    def _beacon_loop(self):
        while self.running:
            time.sleep(30)
            if self.running:
                self.send_beacon()

    def _range_test_loop(self):
        """Emite pings continuos de telemetría si el Range Test está activo."""
        while self.running:
            if self.range_test_active:
                self.range_test_seq += 1
                hora_str = time.strftime("%H:%M:%S")
                last_sig = f"{self.last_rx_rssi}dBm" if self.last_rx_rssi != 0 else "N/A"
                ping_msg = f"📡 [PING #{self.range_test_seq}] {hora_str} | Base RSSI:{last_sig}"
                self.send_chat(ping_msg, target_id=0xFFFF, channel_id=0, quiet=False)
                time.sleep(self.range_test_interval)
            else:
                time.sleep(1)

    def _reader_loop(self):
        """Máquina de estados decodificadora para recibir paquetes por radio."""
        buf = bytearray()
        while self.running:
            try:
                chunk = self.ser.read(128)
                if not chunk:
                    continue
                buf.extend(chunk)

                while len(buf) >= 6:
                    if buf[0] != FRAME_MAGIC_0 or buf[1] != FRAME_MAGIC_1:
                        buf.pop(0)
                        continue

                    dir_byte = buf[2]
                    plen = (buf[3] << 8) | buf[4]
                    frame_total = 5 + plen + 1 # Header 5B + Payload + CRC 1B

                    if len(buf) < frame_total:
                        break # Esperar más bytes

                    payload = bytes(buf[5:5 + plen])
                    crc_byte = buf[5 + plen]
                    del buf[:frame_total]

                    if crc8_calc(payload) == crc_byte and dir_byte == DIR_DONGLE_TO_PC:
                        if len(payload) >= 7:
                            src_mac = payload[0:6]
                            rssi = struct.unpack("b", payload[6:7])[0]
                            mesh_data = payload[7:]
                            self._process_mesh_packet(mesh_data, src_mac=src_mac, rssi=rssi)
                        else:
                            self._process_mesh_packet(payload)

            except Exception as e:
                if self.running:
                    print(f"⚠️ Error en lectura serial: {e}")
                time.sleep(0.5)

    def _process_mesh_packet(self, data: bytes, src_mac: bytes = b"", rssi: int = 0):
        """Desempaqueta paquetes nativos de MeshCore."""
        if len(data) < 11:
            return

        magic, = struct.unpack_from("<H", data, 0)
        if magic != MESHCORE_MAGIC:
            return

        pkt_type = data[2]
        hops = data[3]
        src_id, dst_id = struct.unpack_from("<HH", data, 4)

        if src_id == self.short_id:
            return # Nuestro propio paquete

        self.last_rx_rssi = rssi
        mac_str = ":".join(f"{b:02X}" for b in src_mac) if src_mac else "Desconocida"
        sig_bar = "🟢" if rssi > -65 else ("🟡" if rssi > -80 else ("🔴" if rssi > -90 else "💀"))

        # --- 1. Baliza de Presencia (Beacon) ---
        if pkt_type == PKT_BEACON:
            name_len = data[10] if len(data) > 10 else 0
            name_str = data[11:11 + name_len].decode('utf-8', errors='ignore') if name_len > 0 else f"Nodo_{src_id:04X}"
            
            is_new = src_id not in self.discovered_nodes
            self.discovered_nodes[src_id] = {
                "name": name_str,
                "last_seen": time.time(),
                "hops": hops,
                "rssi": rssi,
                "mac": mac_str
            }
            if is_new:
                print(f"\n✨ [Nuevo Nodo Descubierto]: '{name_str}' (ID: 0x{src_id:04X} | MAC: {mac_str} | Señal: {sig_bar} {rssi} dBm | Saltos: {hops})")
                print("💬 > ", end="", flush=True)

        # --- 2. Mensaje de Chat ---
        elif pkt_type == PKT_CHAT:
            if len(data) < 16:
                return
            chan_id, msg_id, flags, pay_len = struct.unpack_from("<HIBB", data, 8)
            
            if msg_id in self.seen_packet_ids:
                return # Deduplicación
            self.seen_packet_ids.add(msg_id)
            self.stat_rx_chat += 1

            text = data[16:16 + pay_len].decode('utf-8', errors='ignore')
            sender_name = self.discovered_nodes.get(src_id, {}).get("name", f"0x{src_id:04X}")
            chan_str = "#general" if chan_id == 0 else f"Canal {chan_id}"
            hora_str = time.strftime("%H:%M:%S")

            print(f"\n📩 [{sender_name} @ {chan_str} | Señal: {sig_bar} {rssi} dBm | Hops: {hops} | {hora_str}]: {text}")
            print("💬 > ", end="", flush=True)

            # --- Control de Comandos Remotos desde el Cyberdeck ---
            clean_cmd = text.strip().lower()
            if clean_cmd.startswith("/"): clean_cmd = clean_cmd[1:]

            if clean_cmd.startswith("range") or clean_cmd.startswith("test"):
                parts = clean_cmd.split()
                if len(parts) >= 2 and parts[1] in ("stop", "off", "0"):
                    self.range_test_active = False
                    resp_msg = f"⏹️ [BASE] Range Test DETENIDO remotamente [{hora_str}]"
                    threading.Thread(target=lambda: (time.sleep(0.3), self.send_chat(resp_msg, target_id=src_id, channel_id=chan_id, quiet=True)), daemon=True).start()
                else:
                    sec = 3
                    if len(parts) >= 2 and parts[1].isdigit():
                        sec = max(1, int(parts[1]))
                    self.range_test_interval = sec
                    self.range_test_active = True
                    resp_msg = f"📡 [BASE] Range Test INICIADO (emitiendo cada {sec}s) [{hora_str}]"
                    print(f"\n📡 [Range Test Activado por Cyberdeck]: Emitiendo PINGS cada {sec}s por el aire...")
                    print("💬 > ", end="", flush=True)
                    threading.Thread(target=lambda: (time.sleep(0.3), self.send_chat(resp_msg, target_id=src_id, channel_id=chan_id, quiet=True)), daemon=True).start()

            elif clean_cmd.startswith("chan"):
                parts = clean_cmd.split()
                if len(parts) >= 2 and parts[1].isdigit():
                    ch = int(parts[1])
                    if 1 <= ch <= 13:
                        self.set_channel(ch)
                        resp_msg = f"📻 [BASE] Canal cambiado a {ch} ({2412 + (ch - 1) * 5} MHz) [{hora_str}]"
                        threading.Thread(target=lambda: (time.sleep(0.3), self.send_chat(resp_msg, target_id=src_id, channel_id=chan_id, quiet=True)), daemon=True).start()

            elif clean_cmd == "ping":
                pong_msg = f"🏓 PONG | Base RX: {rssi}dBm [{hora_str}]"
                threading.Thread(target=lambda: (time.sleep(0.3), self.send_chat(pong_msg, target_id=src_id, channel_id=chan_id, quiet=True)), daemon=True).start()

            # Auto-Echo para mensajes normales de telemetría
            elif self.auto_echo and not text.startswith("🔄") and not text.startswith("📡 [PING") and not text.startswith("⏹️") and not text.startswith("🏓") and not text.startswith("📻"):
                echo_text = f"🔄 [ECHO] '{text[:30]}' | RX Base: {rssi}dBm [{hora_str}]"
                threading.Thread(target=lambda: (time.sleep(0.3), self.send_chat(echo_text, target_id=src_id, channel_id=chan_id, quiet=True)), daemon=True).start()




def main():
    parser = argparse.ArgumentParser(description="Terminal de Chat MeshCore sobre ESP-NOW (CBDos)")
    parser.add_argument("--port", default="/dev/ttyACM1", help="Puerto serie del Dongle ESP32-C3 (default: /dev/ttyACM1)")
    parser.add_argument("--name", default="Laptop-Cyberdeck", help="Tu nombre de nodo Mesh")
    parser.add_argument("--id", type=lambda x: int(x, 0), default=0x00C3, help="Tu Short ID (ej: 0x00C3)")
    parser.add_argument("--mode", default="lr", choices=["normal", "lr"], help="Modo de radio (normal / lr)")
    parser.add_argument("--channel", "-c", type=int, default=1, choices=range(1, 14), help="Canal Wi-Fi / ESP-NOW (1..13, default: 1)")
    parser.add_argument("--test", action="store_true", help="Inicia automáticamente el Range Test periódico")
    args = parser.parse_args()

    node = MeshCoreNode(port=args.port, node_name=args.name, short_id=args.id)
    if not node.start():
        sys.exit(1)

    time.sleep(0.3)
    node.set_radio_mode(args.mode)
    time.sleep(0.1)
    if args.channel != 1:
        node.set_channel(args.channel)

    if args.test:
        node.range_test_active = True

    print("\n" + "=" * 68)
    print(" 🌐 TERMINAL INTERACTIVA MESHCORE (ESP-NOW 2.4 GHz + RANGE TEST)")
    print("=" * 68)
    print("  • Escribe texto y presiona [ENTER] para enviar a #general")
    print("  • /channel <1-13> - Cambia el canal de radio (ej: channel 13)")
    print("  • /range [seg]  - Inicia prueba continua de alcance (ej: /range 3)")
    print("  • /stop         - Detiene la prueba continua de alcance")
    print("  • /echo on|off  - Activa/desactiva respuesta de eco con RSSI")
    print("  • /stats        - Muestra telemetría completa y calidad de señal")
    print("  • /nodes        - Lista los nodos descubiertos y su último RSSI")
    print("  • /beacon       - Emite manualmente una baliza de presencia")
    print("  • /mode lr|normal - Conmuta modo Long Range (250k) o Normal (1M)")
    print("  • /exit         - Salir")
    print("=" * 68 + "\n")

    try:
        while node.running:
            try:
                line = input("💬 > ").strip()
            except EOFError:
                break

            if not line:
                continue

            # Aceptar comandos con o sin '/' inicial
            cmd_line = line[1:].strip() if line.startswith("/") else line

            if cmd_line in ("exit", "quit", "q"):
                break
            elif cmd_line == "nodes":
                print("\n📋 Nodos activos en la red MeshCore:")
                if not node.discovered_nodes:
                    print("   (Ningún vecino detectado aún. Esperando balizas...)")
                for sid, info in node.discovered_nodes.items():
                    ago = int(time.time() - info["last_seen"])
                    sig_bar = "🟢" if info["rssi"] > -65 else ("🟡" if info["rssi"] > -80 else ("🔴" if info["rssi"] > -90 else "💀"))
                    print(f"   • [{info['name']}] (ID: 0x{sid:04X}) | MAC: {info['mac']} | Señal: {sig_bar} {info['rssi']} dBm | Visto hace {ago}s | Saltos: {info['hops']}")
                print()
            elif cmd_line.startswith("chan"):
                parts = cmd_line.split()
                if len(parts) >= 2 and parts[1].isdigit():
                    ch = int(parts[1])
                    node.set_channel(ch)
                else:
                    print(f"📻 Canal activo actual: Canal {node.channel} ({2412 + (node.channel - 1) * 5} MHz)")
            elif cmd_line.startswith("range"):
                parts = cmd_line.split()
                if len(parts) >= 2 and parts[1].lower() in ("stop", "off", "0"):
                    node.range_test_active = False
                    print("⏹️ Prueba de alcance (Range Test) DETENIDA.")
                else:
                    sec = 3
                    if len(parts) >= 2 and parts[1].isdigit():
                        sec = max(1, int(parts[1]))
                    node.range_test_interval = sec
                    node.range_test_active = True
                    print(f"📡 Prueba de alcance (Range Test) ACTIVADA: Emitiendo un PING cada {sec}s automáticamente...")
            elif cmd_line in ("stop", "range stop", "range off"):
                node.range_test_active = False
                print("⏹️ Prueba de alcance (Range Test) DETENIDA.")
            elif cmd_line.startswith("echo"):
                parts = cmd_line.split()
                if len(parts) >= 2 and parts[1].lower() in ("off", "false", "0"):
                    node.auto_echo = False
                    print("🔇 Auto-Echo DESACTIVADO.")
                else:
                    node.auto_echo = True
                    print("🔊 Auto-Echo ACTIVADO (La base responderá a cada mensaje con el RSSI medido).")
            elif cmd_line in ("stats", "status"):
                print("\n📊 TELEMETRÍA DEL NODO MESHCORE:")
                print(f"   • Canal Activo: Canal {node.channel} ({2412 + (node.channel - 1) * 5} MHz)")
                print(f"   • Mensajes TX: {node.stat_tx_chat} | Mensajes RX: {node.stat_rx_chat}")
                print(f"   • Balizas TX:  {node.stat_tx_beacons}")
                print(f"   • Auto-Echo:   {'🟢 ACTIVO' if node.auto_echo else '⚪ DESACTIVADO'}")
                print(f"   • Range Test:  {'🟢 ACTIVO (' + str(node.range_test_interval) + 's)' if node.range_test_active else '⚪ INACTIVO'}")
                print(f"   • Último RSSI: {node.last_rx_rssi} dBm")
                print()
            elif cmd_line == "beacon":
                node.send_beacon()
                print("📡 Baliza emitida.")
            elif cmd_line.startswith("mode "):
                m = cmd_line.split(" ", 1)[1].strip()
                node.set_radio_mode(m)
            elif cmd_line in ("help", "?"):
                print("\nComandos disponibles (con o sin '/'):")
                print("  channel <1-13> - Cambia canal Wi-Fi / ESP-NOW (ej: channel 13)")
                print("  range [seg]    - Inicia prueba continua de alcance (ej: range 3)")
                print("  stop           - Detiene la prueba de alcance")
                print("  echo on|off    - Activa/desactiva bot de eco con RSSI")
                print("  stats          - Muestra estadísticas de paquetes, canal y señal")
                print("  nodes          - Lista nodos vecinos y su distancia/señal")
                print("  mode lr|normal - Cambia velocidad/alcance de radio")
                print("  exit           - Salir\n")
            else:
                node.send_chat(line)



    except KeyboardInterrupt:
        pass
    finally:
        node.stop()


if __name__ == '__main__':
    main()

