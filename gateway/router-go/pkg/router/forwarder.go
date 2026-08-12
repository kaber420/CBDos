package router

import (
	"bytes"
	"fmt"
	"io"
	"log"
	"net"

	"espos32/router/pkg/mesh"
	"espos32/router/pkg/transport"
)

// RouterEngine coordina la tabla de ruteo, listeners y el reenvio de tramas
type RouterEngine struct {
	Table     *RoutingTable
	Listeners *transport.MultiListener
}

func NewRouterEngine() *RouterEngine {
	return &RouterEngine{
		Table:     NewRoutingTable(),
		Listeners: transport.NewMultiListener(),
	}
}

// HandleConn bucle principal que procesa la conexion de cada cliente o servicio
func (re *RouterEngine) HandleConn(conn net.Conn, tType transport.TransportType) {
	defer func() {
		re.Table.UnregisterRoute(conn)
		_ = conn.Close()
		log.Printf("[Router] Conexión cerrada desde %s (%s)", conn.RemoteAddr(), tType)
	}()

	log.Printf("[Router] Nueva conexión aceptada desde %s (%s)", conn.RemoteAddr(), tType)

	// Bucle de lectura de tramas MeshHeader
	for {
		hdr, err := mesh.ParseHeader(conn)
		if err != nil {
			if err != io.EOF {
				log.Printf("[Router] [%s] Error decodificando MeshHeader: %v", conn.RemoteAddr(), err)
			}
			return
		}

		// Leer el payload TLV completo (delimitado por magic PH y/o tag TYPE_END 0xFE)
		payload, err := mesh.ReadPayload(conn)
		if err != nil && len(payload) == 0 {
			if err != io.EOF {
				log.Printf("[Router] [%s] Error leyendo payload TLV: %v", conn.RemoteAddr(), err)
			}
			return
		}


		// Auto-registrar la ruta de retorno SOLO si la conexion viene de un cliente (no de un servicio backend)
		if !re.Table.IsServiceConn(conn) {
			if hdr.Src.NodeID != 0 {
				re.Table.RegisterRoute(&RouteEntry{
					NodeID:     hdr.Src.NodeID,
					Conn:       conn,
					RemoteAddr: conn.RemoteAddr().String(),
				})
			} else {
				// Si es DST_ONLY (Src.NodeID == 0) desde un cliente, registrar la conexion
				// bajo el ID de cliente por defecto (0x0001) para recibir respuestas
				re.Table.RegisterRoute(&RouteEntry{
					NodeID:     0x0001,
					Conn:       conn,
					RemoteAddr: conn.RemoteAddr().String(),
				})
			}
		}



		// Buscar destino en la tabla de ruteo
		targetRoute, found := re.Table.Lookup(hdr.Dst.NodeID, hdr.ServiceID)
		if !found && (hdr.ServiceID == mesh.SvcTlvglRequest || hdr.ServiceID == mesh.SvcTlvglResponse) {
			// Intentar reconectar automáticamente con el backend de Hosting si la conexión cayó
			_ = re.ConnectBackendService(mesh.SvcTlvglRequest, "127.0.0.1:8766")
			targetRoute, found = re.Table.Lookup(hdr.Dst.NodeID, hdr.ServiceID)
		}

		if !found {
			log.Printf("[Router] ⚠ No se encontro ruta para DstID=0x%08X ServiceID=0x%02X (Trama descartada)",
				hdr.Dst.NodeID, hdr.ServiceID)
			continue
		}

		// Reenviar trama binaria al destino (Header + Payload)
		hdrBytes, err := hdr.MarshalBinary()
		if err != nil {
			log.Printf("[Router] Error serializando header para reenvío: %v", err)
			continue
		}

		fullFrame := append(hdrBytes, payload...)
		_, err = targetRoute.Conn.Write(fullFrame)
		if err != nil {
			log.Printf("[Router] Error reenviando trama a %s: %v. Reintentando reconexión...", targetRoute.RemoteAddr, err)
			re.Table.UnregisterRoute(targetRoute.Conn)
			_ = re.ConnectBackendService(mesh.SvcTlvglRequest, "127.0.0.1:8766")
			if newRoute, ok := re.Table.Lookup(hdr.Dst.NodeID, hdr.ServiceID); ok {
				_, _ = newRoute.Conn.Write(fullFrame)
			}
			continue
		}


		log.Printf("[Router] 🔀 Trama de %dB reenviada con éxito a DstID=0x%08X (Servicio=0x%02X)",
			len(fullFrame), hdr.Dst.NodeID, hdr.ServiceID)
	}
}

// ForwardDirect reenvia un buffer crudo directamente a un destino registrado
func (re *RouterEngine) ForwardDirect(dstNode uint32, serviceID byte, data []byte) error {
	targetRoute, found := re.Table.Lookup(dstNode, serviceID)
	if !found {
		return fmt.Errorf("no existe ruta para DstNode=0x%08X Service=0x%02X", dstNode, serviceID)
	}

	_, err := targetRoute.Conn.Write(data)
	return err
}

// ConnectBackendService conecta el router a un servicio backend (ej: Hosting en :8766) y lo registra
func (re *RouterEngine) ConnectBackendService(serviceID byte, addr string) error {
	conn, err := net.Dial("tcp", addr)
	if err != nil {
		return fmt.Errorf("error conectando al servicio 0x%02X en %s: %w", serviceID, addr, err)
	}

	re.Table.RegisterRoute(&RouteEntry{
		ServiceID:  serviceID,
		IsService:  true,
		Conn:       conn,
		RemoteAddr: addr,
	})

	log.Printf("[Init] Servicio 0x%02X conectado exitosamente a backend en %s", serviceID, addr)
	go re.HandleConn(conn, transport.TransportTCP)
	return nil
}


// Helper para convertir slice a io.Reader
func bytesReader(b []byte) io.Reader {
	return bytes.NewReader(b)
}
