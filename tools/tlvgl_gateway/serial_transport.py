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
    def __init__(self, port: str = "/dev/ttyACM1", baudrate: int = 115200, on_packet_cb=None):
        self.port = port
        self.baudrate = baudrate
        self.on_packet_cb = on_packet_cb
        self.ser = None
        self.running = False
        self.thread = None

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
                                self.on_packet_cb(bytes(buf), self)

            except Exception as e:
                if self.running:
                    print(f"⚠️ Error en lectura serial del Dongle: {e}")
                    time.sleep(0.5)
