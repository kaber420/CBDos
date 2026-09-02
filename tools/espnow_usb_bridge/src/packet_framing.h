#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define FRAME_MAGIC_0      0xAA
#define FRAME_MAGIC_1      0x55

#define DIR_PC_TO_DONGLE   0x01 // La PC envía al Dongle para emitir por radio
#define DIR_DONGLE_TO_PC   0x02 // El Dongle recibió por radio y envía a la PC
#define DIR_CTRL_CMD       0x03 // Comando de control de radio desde la PC hacia el Dongle
#define DIR_CTRL_RESP      0x04 // Respuesta de control y ACK desde el Dongle hacia la PC

// Códigos de comandos de control de radio
#define RADIO_CMD_GET_STATUS  0x01
#define RADIO_CMD_SET_MODE    0x02 // Payload: 0x01 = Normal, 0x02 = Long Range (LR)
#define RADIO_CMD_SET_CHAN    0x03 // Payload: Canal 1..13
#define RADIO_CMD_SET_POWER   0x04 // Payload: Potencia 1..84
#define RADIO_CMD_SCAN_PEERS  0x05
#define RADIO_CMD_SET_ALIAS   0x06 // Payload: String Alias (ej. "PoP1a")

// Códigos de estado / resultado
#define RADIO_STATUS_OK       0x00
#define RADIO_STATUS_ERR      0xFF

#define MAX_FRAME_PAYLOAD  260

static inline uint8_t crc8_update(uint8_t crc, const uint8_t* data, size_t len) {
    while (len--) {
        uint8_t extract = *data++;
        for (uint8_t tempI = 8; tempI; tempI--) {
            uint8_t sum = (crc ^ extract) & 0x01;
            crc >>= 1;
            if (sum) {
                crc ^= 0x8C;
            }
            extract >>= 1;
        }
    }
    return crc;
}

static inline uint8_t crc8_calc(const uint8_t* data, size_t len) {
    return crc8_update(0x00, data, len);
}

/**
 * @brief Estado de la máquina de decodificación de tramas seriales
 */
typedef enum {
    STATE_WAIT_MAGIC_0 = 0,
    STATE_WAIT_MAGIC_1,
    STATE_WAIT_DIR,
    STATE_WAIT_LEN_H,
    STATE_WAIT_LEN_L,
    STATE_READ_PAYLOAD,
    STATE_WAIT_CRC
} FrameParserState;

typedef struct {
    FrameParserState state;
    uint8_t dir;
    uint16_t expected_len;
    uint16_t payload_idx;
    uint8_t payload[MAX_FRAME_PAYLOAD];
} FrameParser;

static inline void frame_parser_init(FrameParser* p) {
    p->state = STATE_WAIT_MAGIC_0;
    p->dir = 0;
    p->expected_len = 0;
    p->payload_idx = 0;
}

static inline bool frame_parser_feed(FrameParser* p, uint8_t byte, uint8_t** out_payload, size_t* out_len, uint8_t* out_dir) {
    switch (p->state) {
        case STATE_WAIT_MAGIC_0:
            if (byte == FRAME_MAGIC_0) p->state = STATE_WAIT_MAGIC_1;
            break;

        case STATE_WAIT_MAGIC_1:
            if (byte == FRAME_MAGIC_1) p->state = STATE_WAIT_DIR;
            else if (byte == FRAME_MAGIC_0) p->state = STATE_WAIT_MAGIC_1;
            else p->state = STATE_WAIT_MAGIC_0;
            break;

        case STATE_WAIT_DIR:
            p->dir = byte;
            p->state = STATE_WAIT_LEN_H;
            break;

        case STATE_WAIT_LEN_H:
            p->expected_len = (uint16_t)byte << 8;
            p->state = STATE_WAIT_LEN_L;
            break;

        case STATE_WAIT_LEN_L:
            p->expected_len |= byte;
            p->payload_idx = 0;
            if (p->expected_len == 0 || p->expected_len > MAX_FRAME_PAYLOAD) {
                p->state = STATE_WAIT_MAGIC_0;
            } else {
                p->state = STATE_READ_PAYLOAD;
            }
            break;

        case STATE_READ_PAYLOAD:
            p->payload[p->payload_idx++] = byte;
            if (p->payload_idx >= p->expected_len) {
                p->state = STATE_WAIT_CRC;
            }
            break;

        case STATE_WAIT_CRC: {
            p->state = STATE_WAIT_MAGIC_0;
            uint8_t expected_crc = crc8_calc(p->payload, p->expected_len);
            if (byte == expected_crc) {
                *out_payload = p->payload;
                *out_len = p->expected_len;
                *out_dir = p->dir;
                return true;
            }
            break;
        }
    }
    return false;
}
