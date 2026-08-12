package mesh

import (
	"encoding/binary"
	"fmt"
	"io"
)

const (
	TYPE_REQ_URL          byte = 0x01
	TYPE_REQ_INPUT_SUBMIT byte = 0x20
	TYPE_REQ_LINK_CLICK   byte = 0x21
	TYPE_END              byte = 0xFE
)

// ReadPayload lee el payload TLV completo desde un io.Reader de forma delimitada.
func ReadPayload(r io.Reader) ([]byte, error) {
	var payload []byte

	// 1. Leer primer byte (Tag o Magic 'P')
	var firstByte [1]byte
	if _, err := io.ReadFull(r, firstByte[:]); err != nil {
		return nil, err
	}
	payload = append(payload, firstByte[0])

	// Caso A: Trama Uplink de petición única (REQ_URL, LINK_CLICK, INPUT_SUBMIT)
	if firstByte[0] == TYPE_REQ_URL || firstByte[0] == TYPE_REQ_LINK_CLICK || firstByte[0] == TYPE_REQ_INPUT_SUBMIT {
		var lenBuf [2]byte
		if _, err := io.ReadFull(r, lenBuf[:]); err != nil {
			return nil, err
		}
		payload = append(payload, lenBuf[:]...)
		valLen := binary.BigEndian.Uint16(lenBuf[:])
		if valLen > 0 {
			valBuf := make([]byte, valLen)
			if _, err := io.ReadFull(r, valBuf); err != nil {
				return nil, err
			}
			payload = append(payload, valBuf...)
		}
		return payload, nil
	}

	// Caso B: Trama Downlink ("PH" + nodos TLV)
	if firstByte[0] == 'P' {
		var secondByte [1]byte
		if _, err := io.ReadFull(r, secondByte[:]); err != nil {
			return nil, err
		}
		payload = append(payload, secondByte[0])

		if secondByte[0] == 'H' {
			// Es magic "PH", continuar leyendo nodos TLV hasta TYPE_END (0xFE)
			for {
				var tagBuf [1]byte
				if _, err := io.ReadFull(r, tagBuf[:]); err != nil {
					return payload, nil
				}
				tag := tagBuf[0]
				payload = append(payload, tag)

				if tag == TYPE_END {
					break
				}

				var lenBuf [2]byte
				if _, err := io.ReadFull(r, lenBuf[:]); err != nil {
					return nil, fmt.Errorf("error leyendo TLV length para tag 0x%02X: %w", tag, err)
				}
				payload = append(payload, lenBuf[:]...)

				valLen := binary.BigEndian.Uint16(lenBuf[:])
				if valLen > 0 {
					valBuf := make([]byte, valLen)
					if _, err := io.ReadFull(r, valBuf); err != nil {
						return nil, fmt.Errorf("error leyendo TLV value para tag 0x%02X: %w", tag, err)
					}
					payload = append(payload, valBuf...)
				}
			}
			return payload, nil
		} else {
			// 'P' no era magic "PH", sino tag 0x50 con secondByte como Byte Alto de longitud
			var thirdByte [1]byte
			if _, err := io.ReadFull(r, thirdByte[:]); err != nil {
				return nil, err
			}
			payload = append(payload, thirdByte[0])
			valLen := binary.BigEndian.Uint16([]byte{secondByte[0], thirdByte[0]})
			if valLen > 0 {
				valBuf := make([]byte, valLen)
				if _, err := io.ReadFull(r, valBuf); err != nil {
					return nil, err
				}
				payload = append(payload, valBuf...)
			}
			return payload, nil
		}
	}

	// Caso C: Secuencia directa de nodos TLV (empezando por un Tag TLV como 0x10 PAGE)
	// Primer byte ya leído en payload[0] como Tag
	tag := firstByte[0]
	if tag == TYPE_END {
		return payload, nil
	}

	var lenBuf [2]byte
	if _, err := io.ReadFull(r, lenBuf[:]); err != nil {
		return nil, fmt.Errorf("error leyendo TLV length para tag inicial 0x%02X: %w", tag, err)
	}
	payload = append(payload, lenBuf[:]...)
	valLen := binary.BigEndian.Uint16(lenBuf[:])
	if valLen > 0 {
		valBuf := make([]byte, valLen)
		if _, err := io.ReadFull(r, valBuf); err != nil {
			return nil, fmt.Errorf("error leyendo TLV value para tag inicial 0x%02X: %w", tag, err)
		}
		payload = append(payload, valBuf...)
	}

	// Si hay más nodos TLV en esta trama (hasta TYPE_END 0xFE)
	for {
		var tagBuf [1]byte
		if _, err := io.ReadFull(r, tagBuf[:]); err != nil {
			return payload, nil
		}
		nextTag := tagBuf[0]
		payload = append(payload, nextTag)

		if nextTag == TYPE_END {
			break
		}

		var lBuf [2]byte
		if _, err := io.ReadFull(r, lBuf[:]); err != nil {
			return nil, fmt.Errorf("error leyendo TLV length para tag 0x%02X: %w", nextTag, err)
		}
		payload = append(payload, lBuf[:]...)

		vLen := binary.BigEndian.Uint16(lBuf[:])
		if vLen > 0 {
			vBuf := make([]byte, vLen)
			if _, err := io.ReadFull(r, vBuf); err != nil {
				return nil, fmt.Errorf("error leyendo TLV value para tag 0x%02X: %w", nextTag, err)
			}
			payload = append(payload, vBuf...)
		}
	}

	return payload, nil
}
