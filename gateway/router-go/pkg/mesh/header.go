package mesh

import (
	"encoding/binary"
	"errors"
	"fmt"
	"io"
)

// Control Byte Flags (espejo exacto de mesh_header.h y mesh_proto.py)
const (
	MeshCtrlGlobalBit byte = 1 << 7 // 0x80 - 21B inter-ASN header
	MeshCtrlSignalBit byte = 1 << 6 // 0x40 - Señalización
	MeshCtrlIntraZone byte = 1 << 5 // 0x20 - Intra-zona OSPF
	MeshCtrlShortID   byte = 1 << 4 // 0x10 - IDs de 2 bytes (short)
	MeshCtrlDstOnly   byte = 1 << 3 // 0x08 - 3B ultra-ligera
)

// Servicios conocidos
const (
	SvcChat          byte = 0x01
	SvcProxy         byte = 0x05
	SvcTlvglRequest  byte = 0x07
	SvcTlvglResponse byte = 0x08
)

// Address representa una dirección global en la red (10B)
type Address struct {
	ASN    uint16
	Zone   uint16
	Tower  uint16
	NodeID uint32
}

// MeshHeader representa la cabecera binaria decodificada
type MeshHeader struct {
	Control    byte
	ServiceID  byte
	IsDstOnly  bool
	IsShortID  bool
	IsGlobal   bool
	IsIntra    bool
	IsSignal   bool
	Src        Address
	Dst        Address
	HeaderLen  int
}

var ErrInvalidHeader = errors.New("cabecera mesh invalida o incompleta")

// ParseHeader lee la cabecera MeshHeader desde un io.Reader
func ParseHeader(r io.Reader) (*MeshHeader, error) {
	var ctrlBuf [1]byte
	if _, err := io.ReadFull(r, ctrlBuf[:]); err != nil {
		return nil, err
	}

	ctrl := ctrlBuf[0]
	hdr := &MeshHeader{
		Control:   ctrl,
		IsGlobal:  (ctrl & MeshCtrlGlobalBit) != 0,
		IsSignal:  (ctrl & MeshCtrlSignalBit) != 0,
		IsIntra:   (ctrl & MeshCtrlIntraZone) != 0,
		IsShortID: (ctrl & MeshCtrlShortID) != 0,
		IsDstOnly: (ctrl & MeshCtrlDstOnly) != 0,
	}

	// Extraer ServiceID (0x08 es SvcTlvglResponse, 0x07/0x05/0x01 usan bits 2..0)
	if (ctrl & 0x0F) == SvcTlvglResponse {
		hdr.ServiceID = SvcTlvglResponse
	} else {
		hdr.ServiceID = ctrl & 0x07
	}


	// Level 1: DST_ONLY (3 bytes total -> 1B control + 2B DstShortID)
	if hdr.IsDstOnly {
		var buf [2]byte
		if _, err := io.ReadFull(r, buf[:]); err != nil {
			return nil, fmt.Errorf("error leyendo dst_only id: %w", err)
		}
		hdr.Dst.NodeID = uint32(binary.BigEndian.Uint16(buf[:]))
		hdr.IsShortID = true
		hdr.HeaderLen = 3
		return hdr, nil
	}

	// Level 4: GLOBAL 21B (1B control + 10B SrcAddr + 10B DstAddr)
	if hdr.IsGlobal {
		var buf [20]byte
		if _, err := io.ReadFull(r, buf[:]); err != nil {
			return nil, fmt.Errorf("error leyendo cabecera global: %w", err)
		}
		hdr.Src = unpackAddress(buf[0:10])
		hdr.Dst = unpackAddress(buf[10:20])
		hdr.HeaderLen = 21
		return hdr, nil
	}

	// Level 2 & 3: INTRA_ZONE (9B o 13B)
	if hdr.IsIntra {
		if hdr.IsShortID {
			// 9B: 1B control + 2B SrcTower + 2B DstTower + 2B SrcID + 2B DstID
			var buf [8]byte
			if _, err := io.ReadFull(r, buf[:]); err != nil {
				return nil, fmt.Errorf("error leyendo intra short header: %w", err)
			}
			hdr.Src.Tower = binary.BigEndian.Uint16(buf[0:2])
			hdr.Dst.Tower = binary.BigEndian.Uint16(buf[2:4])
			hdr.Src.NodeID = uint32(binary.BigEndian.Uint16(buf[4:6]))
			hdr.Dst.NodeID = uint32(binary.BigEndian.Uint16(buf[6:8]))
			hdr.HeaderLen = 9
			return hdr, nil
		}
		// 13B: 1B control + 2B SrcTower + 2B DstTower + 4B SrcID + 4B DstID
		var buf [12]byte
		if _, err := io.ReadFull(r, buf[:]); err != nil {
			return nil, fmt.Errorf("error leyendo intra uuid header: %w", err)
		}
		hdr.Src.Tower = binary.BigEndian.Uint16(buf[0:2])
		hdr.Dst.Tower = binary.BigEndian.Uint16(buf[2:4])
		hdr.Src.NodeID = binary.BigEndian.Uint32(buf[4:8])
		hdr.Dst.NodeID = binary.BigEndian.Uint32(buf[8:12])
		hdr.HeaderLen = 13
		return hdr, nil
	}

	// Level 2 estándar local (9B -> 1B control + 4B DstID + 4B SrcID)
	var buf [8]byte
	if _, err := io.ReadFull(r, buf[:]); err != nil {
		return nil, fmt.Errorf("error leyendo local header: %w", err)
	}
	hdr.Dst.NodeID = binary.BigEndian.Uint32(buf[0:4])
	hdr.Src.NodeID = binary.BigEndian.Uint32(buf[4:8])
	hdr.HeaderLen = 9
	return hdr, nil
}

// MarshalBinary serializa la estructura MeshHeader a su representacion binaria exact
func (h *MeshHeader) MarshalBinary() ([]byte, error) {
	if h.IsDstOnly {
		buf := make([]byte, 3)
		buf[0] = h.Control
		binary.BigEndian.PutUint16(buf[1:3], uint16(h.Dst.NodeID))
		return buf, nil
	}

	if h.IsGlobal {
		buf := make([]byte, 21)
		buf[0] = h.Control
		copy(buf[1:11], packAddress(h.Src))
		copy(buf[11:21], packAddress(h.Dst))
		return buf, nil
	}

	if h.IsIntra {
		if h.IsShortID {
			buf := make([]byte, 9)
			buf[0] = h.Control
			binary.BigEndian.PutUint16(buf[1:3], h.Src.Tower)
			binary.BigEndian.PutUint16(buf[3:5], h.Dst.Tower)
			binary.BigEndian.PutUint16(buf[5:7], uint16(h.Src.NodeID))
			binary.BigEndian.PutUint16(buf[7:9], uint16(h.Dst.NodeID))
			return buf, nil
		}
		buf := make([]byte, 13)
		buf[0] = h.Control
		binary.BigEndian.PutUint16(buf[1:3], h.Src.Tower)
		binary.BigEndian.PutUint16(buf[3:5], h.Dst.Tower)
		binary.BigEndian.PutUint32(buf[5:9], h.Src.NodeID)
		binary.BigEndian.PutUint32(buf[9:13], h.Dst.NodeID)
		return buf, nil
	}

	// Local 9B
	buf := make([]byte, 9)
	buf[0] = h.Control
	binary.BigEndian.PutUint32(buf[1:5], h.Dst.NodeID)
	binary.BigEndian.PutUint32(buf[5:9], h.Src.NodeID)
	return buf, nil
}

func unpackAddress(buf []byte) Address {
	return Address{
		ASN:    binary.BigEndian.Uint16(buf[0:2]),
		Zone:   binary.BigEndian.Uint16(buf[2:4]),
		Tower:  binary.BigEndian.Uint16(buf[4:6]),
		NodeID: binary.BigEndian.Uint32(buf[6:10]),
	}
}

func packAddress(addr Address) []byte {
	buf := make([]byte, 10)
	binary.BigEndian.PutUint16(buf[0:2], addr.ASN)
	binary.BigEndian.PutUint16(buf[2:4], addr.Zone)
	binary.BigEndian.PutUint16(buf[4:6], addr.Tower)
	binary.BigEndian.PutUint32(buf[6:10], addr.NodeID)
	return buf
}

// BuildDstOnlyHeader utilitario para construir tramas DST_ONLY de 3B
func BuildDstOnlyHeader(control byte, dstID uint16) []byte {
	buf := make([]byte, 3)
	buf[0] = control
	binary.BigEndian.PutUint16(buf[1:3], dstID)
	return buf
}
