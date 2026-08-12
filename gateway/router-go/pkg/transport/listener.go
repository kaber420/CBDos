package transport

import (
	"fmt"
	"net"
	"os"
)

// TransportType indica el tipo de medio de comunicacion
type TransportType string

const (
	TransportTCP  TransportType = "tcp"
	TransportUnix TransportType = "unix"
)

// MultiListener gestiona multiples escuchas de red (TCP, Unix Sockets, etc)
type MultiListener struct {
	listeners []net.Listener
}

func NewMultiListener() *MultiListener {
	return &MultiListener{
		listeners: make([]net.Listener, 0),
	}
}

// AddTCPListener agrega un puerto TCP para conexiones entrantes (ej: :8765)
func (m *MultiListener) AddTCPListener(addr string) error {
	l, err := net.Listen("tcp", addr)
	if err != nil {
		return fmt.Errorf("fallo al escuchar TCP en %s: %w", addr, err)
	}
	m.listeners = append(m.listeners, l)
	return nil
}

// AddUnixListener agrega un socket Unix de Linux para comunicacion IPC rapida con contenedores
func (m *MultiListener) AddUnixListener(sockPath string) error {
	// Eliminar el archivo de socket previo si existe
	_ = os.Remove(sockPath)

	l, err := net.Listen("unix", sockPath)
	if err != nil {
		return fmt.Errorf("fallo al escuchar Unix socket en %s: %w", sockPath, err)
	}
	m.listeners = append(m.listeners, l)
	return nil
}

// AcceptLoop inicia el bucle de aceptacion de conexiones en todos los transportes registrados
func (m *MultiListener) AcceptLoop(handleConn func(conn net.Conn, tType TransportType)) {
	for _, listener := range m.listeners {
		l := listener
		go func() {
			tType := TransportTCP
			if _, ok := l.Addr().(*net.UnixAddr); ok {
				tType = TransportUnix
			}

			for {
				conn, err := l.Accept()
				if err != nil {
					return
				}
				go handleConn(conn, tType)
			}
		}()
	}
}

// Close cierra todos los listeners activos
func (m *MultiListener) Close() {
	for _, l := range m.listeners {
		_ = l.Close()
	}
}
