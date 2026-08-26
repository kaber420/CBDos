#!/usr/bin/env python3
"""
Interfaz Base para Servicios de Red Mesh en el Gateway de CBDos.
Cada servicio (Hosting, Proxy, Routing, Chat, Telemetría) hereda de BaseService
y opera de manera desacoplada del medio físico (Radio ESP-NOW, TCP, LoRa).
"""

from abc import ABC, abstractmethod
from typing import Optional, Dict, Any, Tuple


class MeshContext:
    """Contexto de ejecución que provee acceso a la tabla Pseudo-ARP y a la radio."""
    def __init__(self, arp_table, debug: bool = False):
        self.arp_table = arp_table
        self.debug = debug


class BaseService(ABC):
    def __init__(self, name: str, service_id: int):
        self.name = name
        self.service_id = service_id
        self.enabled = True

    @abstractmethod
    def handle_request(self, payload: bytes, client_entry: Optional[Dict[str, Any]], reply_short_id: int, ctx: MeshContext) -> Optional[Tuple[int, bytes]]:
        """
        Procesa una petición entrante para este servicio.
        Retorna: (service_id_respuesta, payload_binario_respuesta) o None si no hay respuesta.
        """
        pass
