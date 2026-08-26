#!/usr/bin/env python3
"""
Módulo de Tabla Pseudo-ARP / Pseudo-NAT en SQLite3 para el Gateway-Router de CBDos.
Mapea direcciones IPv4 Mesh (10.MAC[3].MAC[4].MAC[5]), Short IDs de 2 Bytes con DAD,
y permisos de Proxy de Internet con persistencia transaccional y concurrent-safe en clients.db.
"""

import sqlite3
import time
from pathlib import Path
from typing import Optional, Dict, Any


class PseudoArpTable:
    def __init__(self, db_path: str = "data/router.db"):
        if Path(db_path).is_absolute():
            self.db_path = Path(db_path)
        else:
            self.db_path = (Path(__file__).parent / db_path).resolve()
        
        self.db_path.parent.mkdir(parents=True, exist_ok=True)
        
        # Rango del pool dinámico para resolución de colisiones DAD
        self.POOL_START = 0x0002
        self.POOL_END   = 0x01FF

        self._init_db()

    def _get_conn(self) -> sqlite3.Connection:
        """Obtiene una conexión a la base de datos con modo WAL para alta concurrencia."""
        conn = sqlite3.connect(str(self.db_path), timeout=5.0)
        conn.row_factory = sqlite3.Row
        conn.execute("PRAGMA journal_mode=WAL;")
        conn.execute("PRAGMA synchronous=NORMAL;")
        return conn

    def _init_db(self):
        """Crea la estructura de tablas e índices si no existen."""
        with self._get_conn() as conn:
            conn.execute("""
                CREATE TABLE IF NOT EXISTS clients (
                    mac TEXT PRIMARY KEY,
                    ipv4 TEXT UNIQUE NOT NULL,
                    short_id INTEGER UNIQUE NOT NULL,
                    hostname TEXT DEFAULT 'CBDos Node',
                    proxy_acl INTEGER DEFAULT 1,
                    created_at REAL NOT NULL,
                    last_seen REAL NOT NULL,
                    rssi INTEGER DEFAULT 0,
                    is_collision_resolved INTEGER DEFAULT 0
                );
            """)
            conn.execute("CREATE INDEX IF NOT EXISTS idx_short_id ON clients(short_id);")
            conn.execute("CREATE INDEX IF NOT EXISTS idx_ipv4 ON clients(ipv4);")
            conn.commit()

        # Migrar desde clients.json antiguo si existe
        old_json = self.db_path.parent / "clients.json"
        if old_json.exists():
            try:
                import json
                with open(old_json, "r", encoding="utf-8") as f:
                    entries = json.load(f)
                with self._get_conn() as conn:
                    for item in entries:
                        conn.execute("""
                            INSERT OR IGNORE INTO clients (mac, ipv4, short_id, hostname, proxy_acl, created_at, last_seen, rssi)
                            VALUES (?, ?, ?, ?, ?, ?, ?, ?);
                        """, (
                            item.get("mac"), item.get("ipv4"), item.get("short_id"),
                            item.get("hostname", "CBDos Node"), 1 if item.get("proxy_acl", True) else 0,
                            item.get("created_at", time.time()), item.get("last_seen", time.time()),
                            item.get("rssi", 0)
                        ))
                    conn.commit()
                print(f"🗂️ [Pseudo-ARP] Migrados registros desde {old_json.name} hacia {self.db_path.name}")
            except Exception as e:
                print(f"⚠️ [Pseudo-ARP] Error en migración JSON: {e}")

    @staticmethod
    def format_mac(mac_bytes: bytes) -> str:
        """Formatea 6 bytes en string MAC legible (ej: 9C:CC:01:7C:0C:94)."""
        return ":".join(f"{b:02X}" for b in mac_bytes)

    @staticmethod
    def calc_ipv4(mac_bytes: bytes) -> str:
        """
        Calcula la IP Privada Mesh (RFC 1918) a partir de los eFuses del ESP32:
        Formato: 10.MAC[3].MAC[4].MAC[5] (ej: 10.124.12.148)
        """
        if len(mac_bytes) < 6:
            return "10.0.0.1"
        return f"10.{mac_bytes[3]}.{mac_bytes[4]}.{mac_bytes[5]}"

    @staticmethod
    def calc_short_id_candidate(mac_bytes: bytes) -> int:
        """Calcula el Short ID candidato natural de 2 Bytes (MAC[4] << 8 | MAC[5])."""
        if len(mac_bytes) < 6:
            return 0x0002
        return ((mac_bytes[4] & 0xFF) << 8) | (mac_bytes[5] & 0xFF)

    def _find_free_short_id(self, conn: sqlite3.Connection, candidate: int) -> int:
        """Encuentra un Short ID libre en la base de datos aplicando DAD."""
        cur = conn.cursor()
        cur.execute("SELECT 1 FROM clients WHERE short_id = ? LIMIT 1;", (candidate,))
        if cur.fetchone() is None and candidate not in (0x0000, 0x0001, 0xFFFF):
            return candidate

        # Buscar el primer slot disponible en el pool
        cur.execute("SELECT short_id FROM clients WHERE short_id BETWEEN ? AND ?;", (self.POOL_START, self.POOL_END))
        used_ids = set(row[0] for row in cur.fetchall())

        for sid in range(self.POOL_START, self.POOL_END + 1):
            if sid not in used_ids:
                return sid

        return candidate

    def register_or_update(self, mac_bytes: bytes, candidate_short_id: Optional[int] = None, hostname: str = "CBDos Node", rssi: int = 0) -> Dict[str, Any]:
        """
        Registra un nuevo nodo o actualiza su estado aplicando DAD en SQLite.
        Retorna el registro completo del cliente.
        """
        mac_str = self.format_mac(mac_bytes)
        ipv4_str = self.calc_ipv4(mac_bytes)
        now = time.time()

        with self._get_conn() as conn:
            cur = conn.cursor()
            cur.execute("SELECT * FROM clients WHERE mac = ? LIMIT 1;", (mac_str,))
            row = cur.fetchone()

            if row:
                # Nodo existente: actualizar last_seen y rssi
                cur.execute("UPDATE clients SET last_seen = ?, rssi = ? WHERE mac = ?;", (now, rssi, mac_str))
                conn.commit()
                entry = dict(row)
                entry["last_seen"] = now
                entry["rssi"] = rssi
                entry["proxy_acl"] = bool(entry.get("proxy_acl", 1))
                return entry

            # Nuevo nodo: resolver DAD
            if candidate_short_id is None or candidate_short_id in (0x0000, 0xFFFF):
                candidate_short_id = self.calc_short_id_candidate(mac_bytes)

            assigned_short_id = self._find_free_short_id(conn, candidate_short_id)
            is_resolved = 1 if (assigned_short_id != candidate_short_id) else 0

            cur.execute("""
                INSERT INTO clients (mac, ipv4, short_id, hostname, proxy_acl, created_at, last_seen, rssi, is_collision_resolved)
                VALUES (?, ?, ?, ?, 1, ?, ?, ?, ?);
            """, (mac_str, ipv4_str, assigned_short_id, hostname, now, now, rssi, is_resolved))
            conn.commit()

            entry = {
                "mac": mac_str,
                "ipv4": ipv4_str,
                "short_id": assigned_short_id,
                "hostname": hostname,
                "proxy_acl": True,
                "created_at": now,
                "last_seen": now,
                "rssi": rssi,
                "is_collision_resolved": bool(is_resolved)
            }

            status_txt = f" (DAD Reasignado de 0x{candidate_short_id:04X})" if is_resolved else ""
            print(f"✨ [Pseudo-ARP SQLite] Nuevo nodo registrado: IP={ipv4_str} | MAC={mac_str} | ShortID=0x{assigned_short_id:04X}{status_txt} | Proxy=HABILITADO")

            return entry

    def get_by_mac(self, mac_bytes: bytes) -> Optional[Dict[str, Any]]:
        mac_str = self.format_mac(mac_bytes)
        with self._get_conn() as conn:
            cur = conn.cursor()
            cur.execute("SELECT * FROM clients WHERE mac = ? LIMIT 1;", (mac_str,))
            row = cur.fetchone()
            if row:
                d = dict(row)
                d["proxy_acl"] = bool(d.get("proxy_acl", 1))
                return d
        return None

    def get_by_short_id(self, short_id: int) -> Optional[Dict[str, Any]]:
        with self._get_conn() as conn:
            cur = conn.cursor()
            cur.execute("SELECT * FROM clients WHERE short_id = ? LIMIT 1;", (short_id,))
            row = cur.fetchone()
            if row:
                d = dict(row)
                d["proxy_acl"] = bool(d.get("proxy_acl", 1))
                return d
        return None

    def get_by_ipv4(self, ipv4_str: str) -> Optional[Dict[str, Any]]:
        with self._get_conn() as conn:
            cur = conn.cursor()
            cur.execute("SELECT * FROM clients WHERE ipv4 = ? LIMIT 1;", (ipv4_str,))
            row = cur.fetchone()
            if row:
                d = dict(row)
                d["proxy_acl"] = bool(d.get("proxy_acl", 1))
                return d
        return None

    def is_proxy_allowed(self, identifier: Any) -> bool:
        """Verifica si el cliente tiene permiso de Proxy Web en SQLite."""
        entry = None
        if isinstance(identifier, bytes):
            entry = self.get_by_mac(identifier)
        elif isinstance(identifier, int):
            entry = self.get_by_short_id(identifier)
        elif isinstance(identifier, str):
            entry = self.get_by_ipv4(identifier)

        return bool(entry and entry.get("proxy_acl", False))
