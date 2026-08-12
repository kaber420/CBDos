package router

import (
	"net"
	"testing"
)

func TestRoutingTableLookup(t *testing.T) {
	table := NewRoutingTable()

	// Mock Connection
	c1, _ := net.Pipe()
	defer c1.Close()

	// Registrar Nodo ESP32 (0xC0A842F8)
	table.RegisterRoute(&RouteEntry{
		NodeID:     0xC0A842F8,
		Conn:       c1,
		RemoteAddr: "192.168.66.248:8765",
	})

	// Registrar Servicio de Hosting (0x07 = TLVGL Request)
	c2, _ := net.Pipe()
	defer c2.Close()

	table.RegisterRoute(&RouteEntry{
		ServiceID:  0x07,
		IsService:  true,
		Conn:       c2,
		RemoteAddr: "hosting-container:8766",
	})

	// 1. Buscar Nodo directo
	entry, found := table.Lookup(0xC0A842F8, 0x00)
	if !found || entry.RemoteAddr != "192.168.66.248:8765" {
		t.Fatalf("fallo al buscar nodo directo ESP32")
	}

	// 2. Buscar por Servicio
	entrySvc, foundSvc := table.Lookup(0x00, 0x07)
	if !foundSvc || entrySvc.RemoteAddr != "hosting-container:8766" {
		t.Fatalf("fallo al buscar servicio de hosting")
	}
}
