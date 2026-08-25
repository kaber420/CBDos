#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "packet_framing.h"

#define LED_PIN 8 // LED integrado en ESP32-C3 SuperMini
#define DEFAULT_WIFI_CHANNEL 1
#define MAX_PEERS_TABLE 16

struct PeerRecord {
    uint8_t mac[6];
    int8_t rssi;
    uint32_t last_seen_ms;
    uint32_t rx_pkts;
    uint16_t short_id;
    char last_url[32];
};

static uint8_t s_broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static FrameParser s_parser;
static PeerRecord s_peers[MAX_PEERS_TABLE];
static size_t s_peer_count = 0;

static uint8_t s_channel = DEFAULT_WIFI_CHANNEL;
static bool s_is_lr_mode = false;
static uint8_t s_tx_power = 84; // 20 dBm
static bool s_sniffer_active = false;
static uint32_t s_stat_rx_pkts = 0;
static uint32_t s_stat_tx_pkts = 0;
static uint32_t s_stat_crc_errors = 0;

static char s_cli_line[128];
static size_t s_cli_len = 0;

// Actualizar tabla de nodos descubiertos
static void update_peer(const uint8_t* mac, int8_t rssi, const uint8_t* data, int len) {
    uint32_t now = millis();
    s_stat_rx_pkts++;

    PeerRecord* p = nullptr;
    for (size_t i = 0; i < s_peer_count; i++) {
        if (memcmp(s_peers[i].mac, mac, 6) == 0) {
            p = &s_peers[i];
            break;
        }
    }

    if (!p) {
        if (s_peer_count < MAX_PEERS_TABLE) {
            p = &s_peers[s_peer_count++];
            memcpy(p->mac, mac, 6);
            p->rx_pkts = 0;
            p->short_id = 0;
            p->last_url[0] = '\0';
        } else {
            // Reemplazar el más antiguo
            p = &s_peers[0];
            for (size_t i = 1; i < MAX_PEERS_TABLE; i++) {
                if (s_peers[i].last_seen_ms < p->last_seen_ms) {
                    p = &s_peers[i];
                }
            }
            memcpy(p->mac, mac, 6);
            p->rx_pkts = 0;
            p->short_id = 0;
            p->last_url[0] = '\0';
        }
    }

    p->rssi = rssi;
    p->last_seen_ms = now;
    p->rx_pkts++;

    // Intentar extraer ShortID o URL del paquete
    if (len >= 5) {
        // [MicroChunk: 2B][MeshHeader: 3B -> Ctrl 1B, DstID 2B]
        uint8_t ctrl = data[2];
        uint16_t dst_id = (data[3] << 8) | data[4];
        p->short_id = dst_id;

        // Si es TLVGL Request y trae URL (Tag 0x01)
        if (len >= 8 && data[5] == 0x01) {
            uint16_t ulen = (data[6] << 8) | data[7];
            size_t copy_len = ulen < 31 ? ulen : 31;
            if (len >= 8 + copy_len) {
                memcpy(p->last_url, data + 8, copy_len);
                p->last_url[copy_len] = '\0';
            }
        }
    }
}

// Callback de recepción ESP-NOW (del aire hacia la PC)
static void onDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if (!data || len <= 0 || len > MAX_FRAME_PAYLOAD || !info) return;

    digitalWrite(LED_PIN, LOW); // Enciende LED en recepción
    int8_t rssi = info->rx_ctrl ? info->rx_ctrl->rssi : -50;

    update_peer(info->src_addr, rssi, data, len);

    if (s_sniffer_active) {
        Serial.printf("[SNIFF][RSSI:%ddBm] MAC:%02X:%02X:%02X:%02X:%02X:%02X | Len:%dB\r\n",
                      rssi, info->src_addr[0], info->src_addr[1], info->src_addr[2],
                      info->src_addr[3], info->src_addr[4], info->src_addr[5], len);
    }

    // Auto-responder a Sondeo de Torres (Probe Request = Tag 0x01 en Servicio 0x0F)
    if (len >= 6 && (data[2] & 0x0F) == 0x0F && data[5] == 0x01) {
        const char* tower_name = "Dongle Gateway USB";
        uint8_t name_len = (uint8_t)strlen(tower_name);
        uint8_t resp[64];
        resp[0] = 0x01; // chunk 0 of 1 (MicroChunk)
        resp[1] = 0x99; // msg id
        resp[2] = 0x4F; // Service: RoutingControl (MESH_CTRL_DST_ONLY 0x40 | 0x0F)
        resp[3] = data[3]; // dst short id MSB
        resp[4] = data[4]; // dst short id LSB
        resp[5] = 0x02; // Tag Probe Response
        resp[6] = 0x00; // Tower ID MSB (0x0001)
        resp[7] = 0x01; // Tower ID LSB
        resp[8] = s_channel;
        resp[9] = s_is_lr_mode ? 0x02 : 0x01; // Modo soportado
        resp[10] = name_len;
        memcpy(resp + 11, tower_name, name_len);
        size_t resp_len = 11 + name_len;

        if (!esp_now_is_peer_exist(info->src_addr)) {
            esp_now_peer_info_t peer = {};
            memcpy(peer.peer_addr, info->src_addr, 6);
            peer.channel = 0; // 0 = Seguir dinámicamente el canal Wi-Fi activo
            peer.ifidx = WIFI_IF_STA;
            peer.encrypt = false;
            esp_now_add_peer(&peer);
        }
        esp_now_send(info->src_addr, resp, resp_len);
        s_stat_tx_pkts++;
    }

    // Enmarcar paquete hacia la PC: [0xAA][0x55][DIR=0x02][LEN_H][LEN_L][SRC_MAC 6B][RSSI 1B][PAYLOAD][CRC8]
    size_t total_payload_len = (size_t)len + 7;
    uint8_t header[5];
    header[0] = FRAME_MAGIC_0;
    header[1] = FRAME_MAGIC_1;
    header[2] = DIR_DONGLE_TO_PC;
    header[3] = (total_payload_len >> 8) & 0xFF;
    header[4] = total_payload_len & 0xFF;

    uint8_t crc = 0x00;
    crc = crc8_update(crc, info->src_addr, 6);
    crc = crc8_update(crc, (const uint8_t*)&rssi, 1);
    crc = crc8_update(crc, data, (size_t)len);

    Serial.write(header, sizeof(header));
    Serial.write(info->src_addr, 6);
    Serial.write((const uint8_t*)&rssi, 1);
    Serial.write(data, len);
    Serial.write(&crc, 1);
    Serial.flush();

    digitalWrite(LED_PIN, HIGH);
}

static void apply_radio_config() {
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(s_channel, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);

    if (s_is_lr_mode) {
        esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR);
    } else {
        esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
    }
    esp_wifi_set_max_tx_power(s_tx_power);
}

// Ejecutar comandos CLI interactivos
static void process_cli_command(const char* line) {
    while (*line == ' ') line++;
    if (*line == '\0') return;

    if (strcmp(line, "help") == 0 || strcmp(line, "?") == 0) {
        Serial.println("\r\n======================================================");
        Serial.println("       🛰️ CBDos ESP-NOW Gateway & Auditor v1.0");
        Serial.println("======================================================");
        Serial.println("Comandos disponibles:");
        Serial.println("  status             - Muestra estado de radio y estadisticas");
        Serial.println("  peers              - Muestra tabla de nodos descubiertos");
        Serial.println("  mode [normal|lr]   - Cambia entre Normal y Long Range (LR)");
        Serial.println("  channel <1-13>     - Cambia canal Wi-Fi");
        Serial.println("  power <1-84>       - Ajusta potencia TX (84 = +20dBm)");
        Serial.println("  sniff [on|off]     - Activa/desactiva monitor de trafico en vivo");
        Serial.println("  ping               - Emite un pulso broadcast de sondeo");
        Serial.println("  clear              - Limpia la tabla de peers");
        Serial.println("  reboot             - Reinicia el Dongle");
        Serial.println("======================================================\r\n");
    } else if (strcmp(line, "status") == 0) {
        uint8_t mac[6];
        esp_wifi_get_mac(WIFI_IF_STA, mac);
        Serial.println("\r\n--- ESTADO DEL DONGLE GATEWAY ---");
        Serial.printf("Modo Radio:      %s\r\n", s_is_lr_mode ? "ESP-NOW LONG RANGE (LR) 🚀" : "ESP-NOW ESTÁNDAR (802.11 b/g/n) ⚡");
        Serial.printf("Canal Activo:    Canal %u (2412 + %d MHz)\r\n", s_channel, (s_channel - 1) * 5);
        Serial.printf("Potencia TX:     %.2f dBm (%u/84)\r\n", s_tx_power * 0.25f, s_tx_power);
        Serial.printf("MAC Propia:      %02X:%02X:%02X:%02X:%02X:%02X\r\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        Serial.printf("Sniffer en Vivo: %s\r\n", s_sniffer_active ? "HABILITADO" : "DESHABILITADO");
        Serial.printf("Estadísticas:    RX: %u paq | TX: %u paq | Nodos vistos: %u\r\n", 
                      (unsigned int)s_stat_rx_pkts, (unsigned int)s_stat_tx_pkts, (unsigned int)s_peer_count);
        Serial.printf("Tiempo Activo:   %lu seg\r\n\r\n", (unsigned long)(millis() / 1000));
    } else if (strcmp(line, "peers") == 0) {
        Serial.println("\r\n┌──────────┬───────────────────┬──────────────┬──────────────┬─────────────┬──────────────────────────┐");
        Serial.println("│ Short ID │    Direccion MAC  │ RSSI (Senal) │ Ultimo Visto │ Total Paq   │ Ultima Peticion / URL    │");
        Serial.println("├──────────┼───────────────────┼──────────────┼──────────────┼─────────────┼──────────────────────────┤");
        if (s_peer_count == 0) {
            Serial.println("│                                  (No hay dispositivos detectados aun)                                    │");
        } else {
            uint32_t now = millis();
            for (size_t i = 0; i < s_peer_count; i++) {
                uint32_t ago = (now - s_peers[i].last_seen_ms) / 1000;
                const char* sig = (s_peers[i].rssi > -60) ? "🟢 Excelente" : ((s_peers[i].rssi > -80) ? "🟡 Buena" : "🔴 Debil");
                Serial.printf("│  0x%04X  │ %02X:%02X:%02X:%02X:%02X:%02X │ %4ddBm %-4s │   hace %3us │ %7u pkts │ %-24s │\r\n",
                              s_peers[i].short_id,
                              s_peers[i].mac[0], s_peers[i].mac[1], s_peers[i].mac[2],
                              s_peers[i].mac[3], s_peers[i].mac[4], s_peers[i].mac[5],
                              s_peers[i].rssi, sig, (unsigned int)ago,
                              (unsigned int)s_peers[i].rx_pkts,
                              s_peers[i].last_url[0] ? s_peers[i].last_url : "(sin url)");
            }
        }
        Serial.println("└──────────┴───────────────────┴──────────────┴──────────────┴─────────────┴──────────────────────────┘\r\n");
    } else if (strncmp(line, "mode", 4) == 0) {
        if (strstr(line, "lr") || strstr(line, "LR")) {
            s_is_lr_mode = true;
            apply_radio_config();
            Serial.println("✅ Modo cambiado a: ESP-NOW LONG RANGE (LR)");
        } else if (strstr(line, "normal") || strstr(line, "std")) {
            s_is_lr_mode = false;
            apply_radio_config();
            Serial.println("✅ Modo cambiado a: ESP-NOW ESTANDAR (802.11 b/g/n)");
        } else {
            Serial.printf("Modo actual: %s. Usa: 'mode normal' o 'mode lr'\r\n", s_is_lr_mode ? "LR" : "Normal");
        }
    } else if (strncmp(line, "channel", 7) == 0) {
        int ch = atoi(line + 7);
        if (ch >= 1 && ch <= 13) {
            s_channel = (uint8_t)ch;
            apply_radio_config();
            Serial.printf("✅ Canal cambiado a: %u\r\n", s_channel);
        } else {
            Serial.println("❌ Canal invalido (debe ser 1 a 13)");
        }
    } else if (strncmp(line, "power", 5) == 0) {
        int pwr = atoi(line + 5);
        if (pwr >= 1 && pwr <= 84) {
            s_tx_power = (uint8_t)pwr;
            apply_radio_config();
            Serial.printf("✅ Potencia TX cambiada a: %.2f dBm (%u/84)\r\n", s_tx_power * 0.25f, s_tx_power);
        } else {
            Serial.println("❌ Potencia invalida (debe ser 1 a 84, donde 84 = 20dBm)");
        }
    } else if (strncmp(line, "sniff", 5) == 0) {
        if (strstr(line, "on") || strstr(line, "1")) {
            s_sniffer_active = true;
            Serial.println("📡 Sniffer de trafico en vivo: ACTIVADO");
        } else {
            s_sniffer_active = false;
            Serial.println("🔇 Sniffer de trafico en vivo: DESACTIVADO");
        }
    } else if (strcmp(line, "ping") == 0) {
        uint8_t ping_pkt[4] = {0x01, 0x00, 0x00, 0xFE}; // Trama de sondeo ligera
        esp_now_send(s_broadcast_mac, ping_pkt, sizeof(ping_pkt));
        s_stat_tx_pkts++;
        Serial.println("📡 Pulso de ping broadcast transmitido por radio");
    } else if (strcmp(line, "clear") == 0) {
        s_peer_count = 0;
        Serial.println("🧹 Tabla de peers limpiada");
    } else if (strcmp(line, "reboot") == 0) {
        Serial.println("🔄 Reiniciando Dongle...");
        delay(100);
        ESP.restart();
    } else {
        Serial.printf("Comando desconocido: '%s'. Escribe 'help' para ver opciones.\r\n", line);
    }
}

void setup() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH); // Apagado

    Serial.begin(115200);

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    apply_radio_config();

    if (esp_now_init() != ESP_OK) {
        Serial.println("❌ Fallo inicializando ESP-NOW");
        return;
    }

    esp_now_register_recv_cb(onDataRecv);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, s_broadcast_mac, 6);
    peerInfo.channel = s_channel;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);

    frame_parser_init(&s_parser);
    s_cli_len = 0;

    Serial.println("\r\n======================================================");
    Serial.println("       🛰️ CBDos ESP-NOW Gateway & Auditor LISTO");
    Serial.println("======================================================");
    Serial.println("Escribe 'help' en la terminal para administrar la radio.");
    Serial.println("======================================================\r\n");
}

void loop() {
    while (Serial.available() > 0) {
        uint8_t byte = Serial.read();

        // 1. Intentar decodificar como trama binaria enmarcada (desde el servidor Python)
        uint8_t* payload = nullptr;
        size_t len = 0;
        uint8_t dir = 0;

        if (frame_parser_feed(&s_parser, byte, &payload, &len, &dir)) {
            if (dir == DIR_PC_TO_DONGLE && payload && len > 0) {
                digitalWrite(LED_PIN, LOW); // Enciende LED en TX
                esp_now_send(s_broadcast_mac, payload, len);
                s_stat_tx_pkts++;
                digitalWrite(LED_PIN, HIGH);
            }
            s_cli_len = 0; // Si fue binario, resetear CLI
            continue;
        }

        // 2. Si es entrada de texto interactiva para la CLI (humano en terminal)
        if (byte == '\r' || byte == '\n') {
            if (s_cli_len > 0) {
                s_cli_line[s_cli_len] = '\0';
                process_cli_command(s_cli_line);
                s_cli_len = 0;
            }
        } else if (byte == '\b' || byte == 127) {
            if (s_cli_len > 0) s_cli_len--;
        } else if (byte >= 32 && byte <= 126) {
            if (s_cli_len < sizeof(s_cli_line) - 1) {
                s_cli_line[s_cli_len++] = (char)byte;
            }
        }
    }
}
