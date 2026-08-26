#!/usr/bin/env python3
"""
Interfaz Base para Servicios de Red Mesh en el Gateway de CBDos.
Cada servicio (Hosting, Proxy, Routing, Chat, Telemetría) hereda de BaseService
y opera de manera desacoplada del medio físico (Radio ESP-NOW, TCP, LoRa).
"""

from abc import ABC, abstractmethod
from typing import Optional, Dict, Any, Tuple


class EventBus:
    """Bus de eventos liviano para comunicación desacoplada entre módulos de servicio."""
    def __init__(self):
        self._listeners: Dict[str, list] = {}

    def subscribe(self, event_name: str, callback):
        if event_name not in self._listeners:
            self._listeners[event_name] = []
        self._listeners[event_name].append(callback)

    def publish(self, event_name: str, data: Any = None):
        if event_name in self._listeners:
            for cb in self._listeners[event_name]:
                try:
                    cb(data)
                except Exception as e:
                    print(f"⚠️ [EventBus] Error en listener de '{event_name}': {e}")


class MeshContext:
    """Contexto de ejecución que provee acceso a la tabla Pseudo-ARP, EventBus y a la radio."""
    def __init__(self, arp_table, debug: bool = False, event_bus: Optional[EventBus] = None):
        self.arp_table = arp_table
        self.debug = debug
        self.event_bus = event_bus if event_bus is not None else EventBus()


class BaseService(ABC):
    def __init__(self, name: str, service_id: int):
        self.name = name
        self.service_id = service_id
        self.enabled = True

    def init_service(self, ctx: MeshContext):
        """Inicialización opcional y suscripción a eventos del bus."""
        pass

    @abstractmethod
    def handle_request(self, payload: bytes, client_entry: Optional[Dict[str, Any]], reply_short_id: int, ctx: MeshContext) -> Optional[Tuple[int, bytes]]:
        """
        Procesa una petición entrante para este servicio.
        Retorna: (service_id_respuesta, payload_binario_respuesta) o None si no hay respuesta.
        """
        pass
