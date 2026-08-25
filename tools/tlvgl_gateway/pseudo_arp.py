#!/usr/bin/env python3
"""
Módulo de Tabla Pseudo-ARP / Pseudo-NAT para el Gateway-Router de CBDos.
Mapea direcciones IPv4 Mesh (10.MAC[3].MAC[4].MAC[5]), Short IDs de 2 Bytes con DAD,
y permisos de Proxy de Internet con persistencia en clients.json.
"""

import json
import time
from pathlib import Path
from typing import Optional, Dict, Any


class PseudoArpTable:
    def __init__(self, storage_path: str = "clients.json"):
        self.storage_path = Path(storage_path)
        # Tablas en memoria (indexadas para búsqueda en O(1))
        self.by_mac: Dict[str, Dict[str, Any]] = {}
        self.by_short_id: Dict[int, Dict[str, Any]] = {}
        self.by_ipv4: Dict[str, Dict[str, Any]] = {}
        
        # Rango del pool dinámico para resolución de colisiones
        self.POOL_START = 0x0002
        self.POOL_END   = 0x01FF

        self.load()

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

    def load(self):
        """Carga la base de datos de clientes desde disco si existe."""
        if not self.storage_path.exists():
            return
        try:
            with open(self.storage_path, "r", encoding="utf-8") as f:
                data = json.load(f)
                for item in data:
                    mac_str = item.get("mac")
                    short_id = item.get("short_id")
                    ipv4 = item.get("ipv4")
                    if mac_str and short_id is not None and ipv4:
                        self.by_mac[mac_str] = item
                        self.by_short_id[short_id] = item
                        self.by_ipv4[ipv4] = item
            print(f"🗂️ [Pseudo-ARP] Cargados {len(self.by_mac)} clientes desde {self.storage_path.name}")
        except Exception as e:
            print(f"⚠️ [Pseudo-ARP] Error cargando {self.storage_path}: {e}")

    def save(self):
        """Guarda la base de datos de clientes en formato JSON."""
        try:
            with open(self.storage_path, "w", encoding="utf-8") as f:
                json.dump(list(self.by_mac.values()), f, indent=2)
        except Exception as e:
            print(f"⚠️ [Pseudo-ARP] Error guardando {self.storage_path}: {e}")

    def _find_free_short_id(self, candidate: int) -> int:
        """Encuentra un Short ID libre en el pool aplicando DAD."""
        if candidate not in self.by_short_id and candidate != 0x0000 and candidate != 0x0001 and candidate != 0xFFFF:
            return candidate

        # Buscar el primer slot disponible en el pool
        for sid in range(self.POOL_START, self.POOL_END + 1):
            if sid not in self.by_short_id:
                return sid
        
        # Fallback al candidato si el pool se satura
        return candidate

    def register_or_update(self, mac_bytes: bytes, candidate_short_id: Optional[int] = None, hostname: str = "CBDos Node", rssi: int = 0) -> Dict[str, Any]:
        """
        Registra un nuevo nodo o actualiza su estado aplicando DAD.
        Retorna el registro completo del cliente.
        """
        mac_str = self.format_mac(mac_bytes)
        ipv4_str = self.calc_ipv4(mac_bytes)

        # Si ya está registrado con esta MAC, retornar registro existente
        if mac_str in self.by_mac:
            entry = self.by_mac[mac_str]
            entry["last_seen"] = time.time()
            entry["rssi"] = rssi
            if hostname and entry.get("hostname") != hostname:
                entry["hostname"] = hostname
                self.save()
            return entry

        # Resolver colisión DAD para nuevo cliente
        if candidate_short_id is None or candidate_short_id == 0 or candidate_short_id == 0xFFFF:
            candidate_short_id = self.calc_short_id_candidate(mac_bytes)

        assigned_short_id = self._find_free_short_id(candidate_short_id)

        entry = {
            "mac": mac_str,
            "ipv4": ipv4_str,
            "short_id": assigned_short_id,
            "hostname": hostname,
            "proxy_acl": True, # Salida a Internet autorizada por defecto
            "created_at": time.time(),
            "last_seen": time.time(),
            "rssi": rssi,
            "is_collision_resolved": (assigned_short_id != candidate_short_id)
        }

        self.by_mac[mac_str] = entry
        self.by_short_id[assigned_short_id] = entry
        self.by_ipv4[ipv4_str] = entry
        self.save()

        status_txt = f" (DAD Reasignado de 0x{candidate_short_id:04X})" if entry["is_collision_resolved"] else ""
        print(f"✨ [Pseudo-ARP] Nuevo nodo registrado: IP={ipv4_str} | MAC={mac_str} | ShortID=0x{assigned_short_id:04X}{status_txt} | Proxy=HABILITADO")

        return entry

    def get_by_mac(self, mac_bytes: bytes) -> Optional[Dict[str, Any]]:
        return self.by_mac.get(self.format_mac(mac_bytes))

    def get_by_short_id(self, short_id: int) -> Optional[Dict[str, Any]]:
        return self.by_short_id.get(short_id)

    def get_by_ipv4(self, ipv4_str: str) -> Optional[Dict[str, Any]]:
        return self.by_ipv4.get(ipv4_str)

    def is_proxy_allowed(self, identifier: Any) -> bool:
        """Verifica si el cliente (por MAC, Short ID o IPv4) tiene permiso de Proxy Web."""
        entry = None
        if isinstance(identifier, bytes):
            entry = self.get_by_mac(identifier)
        elif isinstance(identifier, int):
            entry = self.get_by_short_id(identifier)
        elif isinstance(identifier, str):
            entry = self.get_by_ipv4(identifier)

        return bool(entry and entry.get("proxy_acl", False))
