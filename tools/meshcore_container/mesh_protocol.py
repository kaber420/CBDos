import struct
import time
from typing import Optional, Tuple, Dict, Any

MESHCORE_MAGIC = 0x4D43 # 'MC'
PKT_BEACON     = 0x01
PKT_CHAT       = 0x02
PKT_ACK        = 0x03

def generate_msg_id() -> int:
    """Genera un ID único de mensaje basado en milisegundos para evitar descartes por deduplicación."""
    return int(time.time() * 1000) & 0x7FFFFFFF

def pack_beacon(short_id: int, node_name: str) -> bytes:
    name_bytes = node_name.encode('utf-8')[:32]
    # Magic(2B) + Type(1B) + Hops(1B) + SrcID(2B) + DstID(2B: 0xFFFF) + Spare(2B) + NameLen(1B) + Name
    header = struct.pack("<HBBHHHB", MESHCORE_MAGIC, PKT_BEACON, 0, short_id, 0xFFFF, 0, len(name_bytes))
    return header + name_bytes

def pack_chat(src_id: int, target_id: int, channel_id: int, text: str, msg_id: Optional[int] = None) -> Tuple[int, bytes]:
    if msg_id is None:
        msg_id = generate_msg_id()
    text_bytes = text.encode('utf-8')[:180]
    # Magic(2) + Type(1) + Hops(1) + SrcId(2) + DstId(2) + ChanId(2) + MsgId(4) + Flags(1) + PayLen(1) + Text
    header = struct.pack("<HBBHHHIBB", MESHCORE_MAGIC, PKT_CHAT, 0, src_id, target_id, channel_id, msg_id, 0x00, len(text_bytes))
    return msg_id, header + text_bytes

def unpack_packet(data: bytes) -> Optional[Dict[str, Any]]:
    if len(data) < 11:
        return None

    magic, = struct.unpack_from("<H", data, 0)
    if magic != MESHCORE_MAGIC:
        return None

    pkt_type = data[2]
    hops = data[3]
    src_id, dst_id = struct.unpack_from("<HH", data, 4)

    if pkt_type == PKT_BEACON:
        name_len = data[10] if len(data) > 10 else 0
        name_str = data[11:11 + name_len].decode('utf-8', errors='ignore') if name_len > 0 else f"Nodo_{src_id:04X}"
        return {
            "type": "BEACON",
            "hops": hops,
            "src_id": src_id,
            "dst_id": dst_id,
            "name": name_str
        }

    elif pkt_type == PKT_CHAT:
        if len(data) < 16:
            return None
        chan_id, msg_id, flags, pay_len = struct.unpack_from("<HIBB", data, 8)
        text = data[16:16 + pay_len].decode('utf-8', errors='ignore')
        return {
            "type": "CHAT",
            "hops": hops,
            "src_id": src_id,
            "dst_id": dst_id,
            "channel_id": chan_id,
            "msg_id": msg_id,
            "flags": flags,
            "text": text
        }

    return None
