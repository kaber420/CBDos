import struct

FRAME_MAGIC_0    = 0xAA
FRAME_MAGIC_1    = 0x55
DIR_PC_TO_DONGLE = 0x01
DIR_DONGLE_TO_PC = 0x02
DIR_CTRL_CMD     = 0x03
DIR_CTRL_RESP    = 0x04

RADIO_CMD_GET_STATUS = 0x01
RADIO_CMD_SET_MODE   = 0x02
RADIO_CMD_SET_CHAN   = 0x03
RADIO_CMD_SET_POWER  = 0x04

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

def encode_serial_frame(direction: int, payload: bytes) -> bytes:
    frame = bytearray([
        FRAME_MAGIC_0,
        FRAME_MAGIC_1,
        direction,
        (len(payload) >> 8) & 0xFF,
        len(payload) & 0xFF
    ])
    frame.extend(payload)
    frame.append(crc8_calc(payload))
    return bytes(frame)
