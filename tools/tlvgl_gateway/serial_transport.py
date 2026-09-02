#!/usr/bin/env python3
"""
Transporte Serial USB <-> ESP-NOW para el Gateway TLVGL de CBDos.
Se comunica con el Dongle ESP32-C3 / S3 enmarcando tramas binarias con CRC8.
"""

import threading
import time
import struct
import serial
import mesh_proto as MESH

FRAME_MAGIC_0    = 0xAA
FRAME_MAGIC_1    = 0x55
DIR_PC_TO_DONGLE = 0x01
DIR_DONGLE_TO_PC = 0x02
DIR_CTRL_CMD     = 0x03
DIR_CTRL_RESP    = 0x04

RADIO_CMD_GET_STATUS  = 0x01
RADIO_CMD_SET_MODE    = 0x02
RADIO_CMD_SET_CHAN    = 0x03
RADIO_CMD_SET_POWER   = 0x04
RADIO_CMD_SCAN_PEERS  = 0x05
RADIO_CMD_SET_ALIAS   = 0x06

MAX_PAYLOAD      = 250


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


class SerialEspNowTransport:
    def __init__(self, port: str = "/dev/ttyACM0", baudrate: int = 115200, on_packet_cb=None):
        self.port = port
        self.baudrate = baudrate
        self.on_packet_cb = on_packet_cb
        self.ser = None
        self.running = False
        self.thread = None
        self._ctrl_response_event = threading.Event()
        self._last_ctrl_response = None
        self.alias = "Desconocido"
        self.mac = ""
        self.channel = 1
        self.mode = "Normal"

    def start(self):
        try:
            self.ser = serial.Serial(self.port, self.baudrate, timeout=0.1)
            self.running = True
            self.thread = threading.Thread(target=self._reader_loop, daemon=True)
            self.thread.start()
            print(f"📻 [Dongle ESP-NOW] Conectado exitosamente en {self.port} @ {self.baudrate} bps")
            return True
        except Exception as e:
            print(f"⚠️ [Dongle ESP-NOW] No se pudo abrir el puerto {self.port}: {e}")
            return False

    def send_ctrl_cmd(self, cmd: int, data: bytes = b"", timeout: float = 1.0):
        """Envía un comando de control binario con garantía de ACK."""
        if not self.ser or not self.ser.is_open:
            return None
        self._ctrl_response_event.clear()
        self._last_ctrl_response = None

        payload = bytes([cmd]) + data
        header = bytes([
            FRAME_MAGIC_0,
            FRAME_MAGIC_1,
            DIR_CTRL_CMD,
            (len(payload) >> 8) & 0xFF,
            len(payload) & 0xFF
        ])
        crc = bytes([crc8_calc(payload)])
        frame = header + payload + crc

        try:
            self.ser.write(frame)
            self.ser.flush()
            if self._ctrl_response_event.wait(timeout=timeout):
                return self._last_ctrl_response
        except Exception as e:
            print(f"❌ Error enviando comando de control al Dongle: {e}")
        return None

    def set_radio_mode(self, mode: str = "lr") -> bool:
        """Configura el modo de radio (normal / lr) de forma garantizada mediante trama de control binaria."""
        mode_val = 0x02 if mode.strip().lower() in ("lr", "long_range") else 0x01
        resp = self.send_ctrl_cmd(RADIO_CMD_SET_MODE, bytes([mode_val]))
        if resp and len(resp) >= 2 and resp[1] == 0x00:
            cur_mode = "LR 🚀" if (len(resp) >= 3 and resp[2] == 0x02) else "NORMAL ⚡"
            print(f"📡 [Control Radio] Confirmado por Dongle: Modo {cur_mode}")
            return True
        else:
            print(f"⚠️ [Control Radio] No se recibió ACK del cambio de modo en el Dongle")
            return False

    def get_radio_status(self):
        """Consulta el estado del hardware de radio (MAC, modo, canal, potencia, alias)."""
        resp = self.send_ctrl_cmd(RADIO_CMD_GET_STATUS)
        if resp and len(resp) >= 8 and resp[1] == 0x00:
            mac_str = ":".join(f"{b:02X}" for b in resp[2:8])
            mode_str = "LR" if (len(resp) >= 9 and resp[8] == 0x02) else "Normal"
            chan = resp[9] if len(resp) >= 10 else 1
            pwr = resp[10] if len(resp) >= 11 else 84
            alias_str = resp[11:].decode("utf-8", errors="ignore") if len(resp) > 11 else "PoP"
            self.mac = mac_str
            self.mode = mode_str
            self.channel = chan
            self.alias = alias_str
            return {
                "alias": alias_str,
                "mac": mac_str,
                "mode": mode_str,
                "channel": chan,
                "power_dbm": pwr * 0.25
            }
        return None

    def set_node_alias(self, alias: str) -> bool:
        """Asigna un nuevo alias de nodo persistente en NVS (ej. 'PoP1a')."""
        resp = self.send_ctrl_cmd(RADIO_CMD_SET_ALIAS, alias.encode("utf-8"))
        if resp and len(resp) >= 2 and resp[1] == 0x00:
            self.alias = alias
            print(f"🏷️ [Control Radio] Alias actualizado a '{alias}' en {self.port}")
            return True
        return False

    def stop(self):
        self.running = False
        if self.ser and self.ser.is_open:
            self.ser.close()

    def send_packet(self, payload: bytes, msg_id: int = 1):
        """Enmarca y envía un paquete al Dongle para ser emitido por ESP-NOW."""
        if not self.ser or not self.ser.is_open or len(payload) == 0:
            return

        # Tamaño máximo de carga por fragmento ESP-NOW
        max_chunk = 240
        total_chunks = (len(payload) + max_chunk - 1) // max_chunk
        if total_chunks == 0:
            total_chunks = 1

        for idx in range(total_chunks):
            start = idx * max_chunk
            end = min(start + max_chunk, len(payload))
            chunk_data = payload[start:end]

            # MicroChunkHeader (2B): [idx:4 | total:4][msg_id:8]
            chunk_info = ((idx & 0x0F) << 4) | (total_chunks & 0x0F)
            micro_header = bytes([chunk_info, msg_id & 0xFF])
            espnow_frame = micro_header + chunk_data

            header = bytes([
                FRAME_MAGIC_0,
                FRAME_MAGIC_1,
                DIR_PC_TO_DONGLE,
                (len(espnow_frame) >> 8) & 0xFF,
                len(espnow_frame) & 0xFF
            ])
            crc = bytes([crc8_calc(espnow_frame)])
            frame = header + espnow_frame + crc

            try:
                self.ser.write(frame)
                self.ser.flush()
                time.sleep(0.005) # Pequeña pausa entre micro-chunks de radio
            except Exception as e:
                print(f"❌ Error enviando trama serial al Dongle: {e}")

    def _reader_loop(self):
        buf = bytearray()
        state = 0
        expected_len = 0
        dir_byte = 0

        while self.running:
            try:
                raw = self.ser.read(64)
                if not raw:
                    continue

                for b in raw:
                    if state == 0:
                        if b == FRAME_MAGIC_0:
                            state = 1
                    elif state == 1:
                        if b == FRAME_MAGIC_1:
                            state = 2
                        elif b == FRAME_MAGIC_0:
                            state = 1
                        else:
                            state = 0
                    elif state == 2:
                        dir_byte = b
                        state = 3
                    elif state == 3:
                        expected_len = b << 8
                        state = 4
                    elif state == 4:
                        expected_len |= b
                        buf.clear()
                        if expected_len == 0 or expected_len > MAX_PAYLOAD:
                            state = 0
                        else:
                            state = 5
                    elif state == 5:
                        buf.append(b)
                        if len(buf) >= expected_len:
                            state = 6
                    elif state == 6:
                        state = 0
                        if b == crc8_calc(buf):
                            if dir_byte == DIR_DONGLE_TO_PC and self.on_packet_cb:
                                if len(buf) >= 7:
                                    src_mac = bytes(buf[0:6])
                                    rssi = struct.unpack("b", bytes([buf[6]]))[0]
                                    payload = bytes(buf[7:])
                                    self.on_packet_cb(payload, self, src_mac, rssi)
                                else:
                                    self.on_packet_cb(bytes(buf), self, b"", 0)
                            elif dir_byte == DIR_CTRL_RESP:
                                self._last_ctrl_response = bytes(buf)
                                self._ctrl_response_event.set()

            except Exception as e:
                if self.running:
                    print(f"⚠️ Error en lectura serial del Dongle: {e}")
                    time.sleep(0.5)


def scan_all_dongles(baudrate: int = 115200) -> list:
    """Escanea todos los puertos USB/Serial disponibles para descubrir módems ESP32-C3 activos."""
    import glob
    candidates = sorted(glob.glob("/dev/ttyACM*") + glob.glob("/dev/ttyUSB*"))
    discovered = []

    for port in candidates:
        try:
            ser = serial.Serial(port, baudrate, timeout=0.3)
            # Enviar GET_STATUS
            payload = bytes([RADIO_CMD_GET_STATUS])
            hdr = bytes([FRAME_MAGIC_0, FRAME_MAGIC_1, DIR_CTRL_CMD, 0x00, len(payload)])
            crc = bytes([crc8_calc(payload)])
            ser.write(hdr + payload + crc)
            ser.flush()

            # Esperar respuesta
            time.sleep(0.2)
            raw = ser.read(64)
            ser.close()

            if len(raw) >= 12 and raw[0] == FRAME_MAGIC_0 and raw[1] == FRAME_MAGIC_1 and raw[2] == DIR_CTRL_RESP:
                payload_len = (raw[3] << 8) | raw[4]
                resp_payload = raw[5:5+payload_len]
                if len(resp_payload) >= 11 and resp_payload[0] == RADIO_CMD_GET_STATUS and resp_payload[1] == 0x00:
                    mac = ":".join(f"{b:02X}" for b in resp_payload[2:8])
                    mode = "LR" if resp_payload[8] == 2 else "Normal"
                    chan = resp_payload[9]
                    pwr = resp_payload[10] * 0.25
                    alias = resp_payload[11:].decode("utf-8", errors="ignore") if len(resp_payload) > 11 else "PoP"
                    discovered.append({
                        "port": port,
                        "alias": alias,
                        "mac": mac,
                        "mode": mode,
                        "channel": chan,
                        "power_dbm": pwr
                    })
        except Exception:
            continue

    return discovered


class MultiDongleManager:
    """Administrador multi-módem para soportar múltiples ESP32-C3 simultáneos en PC."""
    def __init__(self, on_packet_cb=None):
        self.on_packet_cb = on_packet_cb
        self.dongles = {} # alias -> SerialEspNowTransport

    def auto_discover_and_start(self) -> int:
        """Descubre automáticamente todos los módems C3 conectados e inicia sus transportes."""
        found = scan_all_dongles()
        for info in found:
            port = info["port"]
            alias = info["alias"]
            transport = SerialEspNowTransport(port=port, on_packet_cb=self.on_packet_cb)
            if transport.start():
                status = transport.get_radio_status()
                if status:
                    print(f"🛰️ [MultiMódem] '{status['alias']}' conectado en {port} | MAC: {status['mac']} | Ch: {status['channel']}")
                    self.dongles[status["alias"]] = transport
        return len(self.dongles)

    def broadcast_packet(self, payload: bytes, msg_id: int = 1):
        """Emite el paquete a través de todos los módems C3 conectados en paralelo."""
        for dongle in self.dongles.values():
            dongle.send_packet(payload, msg_id)

    def send_via_dongle(self, alias: str, payload: bytes, msg_id: int = 1) -> bool:
        """Emite a través de un módem específico (ej. 'PoP1a')."""
        if alias in self.dongles:
            self.dongles[alias].send_packet(payload, msg_id)
            return True
        return False

    def stop_all(self):
        for dongle in self.dongles.values():
            dongle.stop()
        self.dongles.clear()
