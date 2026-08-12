package mesh

import (
	"bytes"
	"testing"
)

func TestParseHeaderDstOnly(t *testing.T) {
	// DST_ONLY (Control 0x08 = MeshCtrlDstOnly | SvcTlvglResponse)
	// Byte 0: 0x08, Bytes 1-2: 0x0001
	raw := []byte{0x08, 0x00, 0x01}
	hdr, err := ParseHeader(bytes.NewReader(raw))
	if err != nil {
		t.Fatalf("error parseando dst_only: %v", err)
	}

	if !hdr.IsDstOnly {
		t.Errorf("esperado IsDstOnly = true")
	}
	if hdr.Dst.NodeID != 1 {
		t.Errorf("esperado Dst.NodeID = 1, obtenido = %d", hdr.Dst.NodeID)
	}
	if hdr.HeaderLen != 3 {
		t.Errorf("esperado HeaderLen = 3, obtenido = %d", hdr.HeaderLen)
	}

	marshaled, err := hdr.MarshalBinary()
	if err != nil {
		t.Fatalf("error marshal: %v", err)
	}
	if !bytes.Equal(raw, marshaled) {
		t.Errorf("bytes desalineados: %v vs %v", raw, marshaled)
	}
}

func TestParseHeaderLocal9B(t *testing.T) {
	// Local 9B (Control 0x07 = SvcTlvglRequest, no flags)
	// DstID = 0xC0A84201, SrcID = 0xC0A842F8
	raw := []byte{
		0x07,
		0xC0, 0xA8, 0x42, 0x01, // Dst
		0xC0, 0xA8, 0x42, 0xF8, // Src
	}
	hdr, err := ParseHeader(bytes.NewReader(raw))
	if err != nil {
		t.Fatalf("error parseando local 9B: %v", err)
	}

	if hdr.ServiceID != SvcTlvglRequest {
		t.Errorf("esperado ServiceID = 0x07, obtenido = 0x%02X", hdr.ServiceID)
	}
	if hdr.Dst.NodeID != 0xC0A84201 {
		t.Errorf("esperado Dst.NodeID = 0xC0A84201, obtenido = 0x%08X", hdr.Dst.NodeID)
	}
	if hdr.Src.NodeID != 0xC0A842F8 {
		t.Errorf("esperado Src.NodeID = 0xC0A842F8, obtenido = 0x%08X", hdr.Src.NodeID)
	}
	if hdr.HeaderLen != 9 {
		t.Errorf("esperado HeaderLen = 9, obtenido = %d", hdr.HeaderLen)
	}

	marshaled, err := hdr.MarshalBinary()
	if err != nil {
		t.Fatalf("error marshal: %v", err)
	}
	if !bytes.Equal(raw, marshaled) {
		t.Errorf("bytes desalineados: %v vs %v", raw, marshaled)
	}
}
