package router

import (
	"net"
	"sync"
)

// RouteEntry representa un destino conocido en la red (Nodo ESP32 o Servicio)
type RouteEntry struct {
	NodeID     uint32
	ServiceID  byte
	IsService  bool
	Zone       uint16
	Conn       net.Conn
	RemoteAddr string
}

// RoutingTable gestiona las rutas activas de forma concurrente
type RoutingTable struct {
	mu           sync.RWMutex
	nodeRoutes   map[uint32]*RouteEntry
	serviceMap   map[byte]*RouteEntry
}

func NewRoutingTable() *RoutingTable {
	return &RoutingTable{
		nodeRoutes: make(map[uint32]*RouteEntry),
		serviceMap: make(map[byte]*RouteEntry),
	}
}

// RegisterRoute registra o actualiza un destino en la tabla de ruteo
func (rt *RoutingTable) RegisterRoute(entry *RouteEntry) {
	rt.mu.Lock()
	defer rt.mu.Unlock()

	if entry.NodeID != 0 {
		rt.nodeRoutes[entry.NodeID] = entry
		// Registrar también los 16-bits inferiores como ShortID
		shortID := entry.NodeID & 0xFFFF
		if shortID != 0 {
			rt.nodeRoutes[shortID] = entry
		}
	}
	if entry.IsService && entry.ServiceID != 0 {
		rt.serviceMap[entry.ServiceID] = entry
	}
}

// UnregisterRoute elimina una conexion de la tabla de ruteo
func (rt *RoutingTable) UnregisterRoute(conn net.Conn) {
	rt.mu.Lock()
	defer rt.mu.Unlock()

	for id, entry := range rt.nodeRoutes {
		if entry.Conn == conn {
			delete(rt.nodeRoutes, id)
		}
	}
	for svc, entry := range rt.serviceMap {
		if entry.Conn == conn {
			delete(rt.serviceMap, svc)
		}
	}
}

// Lookup busca la mejor ruta disponible distinguiendo entre peticiones de servicio y respuestas a nodos
func (rt *RoutingTable) Lookup(nodeID uint32, serviceID byte) (*RouteEntry, bool) {
	rt.mu.RLock()
	defer rt.mu.RUnlock()

	// 1. Si es un servicio de petición (REQ_URL, Proxy), buscar exclusivamente en el backend registrado
	if serviceID == 0x07 || serviceID == 0x05 { // SvcTlvglRequest (0x07), SvcProxy (0x05)
		if entry, found := rt.serviceMap[serviceID]; found {
			return entry, true
		}
		return nil, false
	}

	// 2. Para respuestas (0x08 SvcTlvglResponse) o tráfico directo entre nodos, buscar por NodeID / ShortID
	if nodeID != 0 {
		if entry, found := rt.nodeRoutes[nodeID]; found {
			return entry, true
		}
		// Buscar por ShortID (16-bits)
		if entry, found := rt.nodeRoutes[nodeID&0xFFFF]; found {
			return entry, true
		}
	}

	// 3. Fallback: Buscar por ServiceID en serviceMap
	if serviceID != 0 {
		if entry, found := rt.serviceMap[serviceID]; found {
			return entry, true
		}
	}

	return nil, false
}




// IsServiceConn comprueba si una conexion pertenece a un servicio backend registrado
func (rt *RoutingTable) IsServiceConn(conn net.Conn) bool {
	rt.mu.RLock()
	defer rt.mu.RUnlock()
	for _, entry := range rt.serviceMap {
		if entry.Conn == conn {
			return true
		}
	}
	return false
}

// Size devuelve la cantidad de rutas activas
func (rt *RoutingTable) Size() (nodes int, services int) {
	rt.mu.RLock()
	defer rt.mu.RUnlock()
	return len(rt.nodeRoutes), len(rt.serviceMap)
}

