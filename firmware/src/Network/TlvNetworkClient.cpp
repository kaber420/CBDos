#include "TlvNetworkClient.h"
#include <WiFiClient.h>
#include <WiFi.h>
#include <cstring>
#include <Arduino.h>

char TlvNetworkClient::serverIp[64] = "192.168.66.254";
uint16_t TlvNetworkClient::serverPort = 8765;
tlv_packet_recv_cb_t TlvNetworkClient::recvCb = nullptr;

static WiFiClient g_wifi_client;
static uint8_t g_rx_buffer[1024];

void TlvNetworkClient::init(const char* gateway_ip, uint16_t port) {
    setGatewayConfig(gateway_ip, port);
}

void TlvNetworkClient::setGatewayConfig(const char* ip, uint16_t port) {
    if (ip && strlen(ip) > 0) {
        strncpy(serverIp, ip, sizeof(serverIp) - 1);
        serverIp[sizeof(serverIp) - 1] = '\0';
    }
    serverPort = port;
}

void TlvNetworkClient::setPacketRecvCallback(tlv_packet_recv_cb_t cb) {
    recvCb = cb;
}

bool TlvNetworkClient::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

bool TlvNetworkClient::sendRequest(const uint8_t* packet, size_t len) {
    if (!packet || len == 0) return false;
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[TLV Client] WiFi no conectado. No se pudo enviar peticion.");
        return false;
    }
    
    if (!g_wifi_client.connected()) {
        Serial.printf("[TLV Client] Conectando a Pasarela %s:%u...\n", serverIp, serverPort);
        if (!g_wifi_client.connect(serverIp, serverPort, 3000)) {
            Serial.println("[TLV Client] Fallo al conectar con la Pasarela.");
            return false;
        }
    }
    
    // Transmitir trama binaria completa (MeshHeader + TLV Payload)
    size_t written = g_wifi_client.write(packet, len);
    g_wifi_client.flush();
    Serial.printf("[TLV Client] Enviados %u bytes a la pasarela.\n", (unsigned int)written);
    return written == len;
}

void TlvNetworkClient::loop() {
    if (!g_wifi_client.connected()) return;
    
    if (g_wifi_client.available() > 0) {
        size_t bytesRead = g_wifi_client.read(g_rx_buffer, sizeof(g_rx_buffer));
        if (bytesRead > 0) {
            Serial.printf("[TLV Client] Recibidos %u bytes desde la pasarela.\n", (unsigned int)bytesRead);
            if (recvCb) {
                recvCb(g_rx_buffer, bytesRead);
            }
        }
    }
}
