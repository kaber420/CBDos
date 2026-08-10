#include "DiscoveryService.h"

#ifdef ARDUINO
#include <ArduinoJson.h>

void DiscoveryService::startDiscovery(const GatewayConfig& gw) {
    hubIp = "";
    retryCount = 0;
    mqttPort = gw.mqttPort;

    if (gw.discoveryMethod == "static") {
        targetStaticIp = gw.address;
        currentState = DiscoveryState::TRY_STATIC_IP;
        Serial.println("[Discovery] Iniciando Nivel 1 (Estática): " + targetStaticIp);
    } else if (gw.discoveryMethod == "mdns") {
        currentState = DiscoveryState::TRY_MDNS;
        targetStaticIp = gw.domain;
        Serial.println("[Discovery] Iniciando Nivel 2 (mDNS): " + targetStaticIp);
    } else {
        currentState = DiscoveryState::TRY_UDP_BROADCAST;
        stateStartTime = millis();
        udp.begin(9999);
        Serial.println("[Discovery] Iniciando Nivel 3 (UDP Broadcast)...");
    }
}

DiscoveryState DiscoveryService::update() {
    switch (currentState) {
        case DiscoveryState::IDLE:
        case DiscoveryState::SUCCESS:
        case DiscoveryState::FAILED:
            return currentState;

        case DiscoveryState::TRY_STATIC_IP: {
            // Verificar si el servidor HTTP del Hub responde en la IP guardada
            IPAddress ip;
            if (ip.fromString(targetStaticIp)) {
                hubIp = targetStaticIp;
                mqttPort = 1883;
                currentState = DiscoveryState::SUCCESS;
                Serial.println("[Discovery] Nivel 1 Exitoso: Usando IP guardada " + hubIp);
                return currentState;
            }
            // Fallback a Nivel 2
            currentState = DiscoveryState::TRY_MDNS;
            stateStartTime = millis();
            Serial.println("[Discovery] Nivel 1 Falló. Pasando a Nivel 2: mDNS...");
            break;
        }

        case DiscoveryState::TRY_MDNS: {
            if (!MDNS.begin("esp32-tablehub")) {
                Serial.println("[Discovery] Error iniciando mDNS client");
            }
            int n = MDNS.queryService("tablehub", "tcp");
            if (n > 0) {
                hubIp = MDNS.address(0).toString();
                mqttPort = MDNS.port(0);
                currentState = DiscoveryState::SUCCESS;
                Serial.println("[Discovery] Nivel 2 Exitoso: mDNS encontró Hub en " + hubIp + ":" + String(mqttPort));
                return currentState;
            }

            if (millis() - stateStartTime > 2000) { // Timeout mDNS 2s
                currentState = DiscoveryState::TRY_UDP_BROADCAST;
                stateStartTime = millis();
                retryCount = 0;
                udp.begin(9999);
                Serial.println("[Discovery] Nivel 2 Timeout. Pasando a Nivel 3: UDP Broadcast...");
            }
            break;
        }

        case DiscoveryState::TRY_UDP_BROADCAST: {
            if (retryCount == 0 || millis() - stateStartTime > 1000) {
                if (retryCount >= 3) {
                    currentState = DiscoveryState::FAILED;
                    udp.stop();
                    Serial.println("[Discovery] Nivel 3 Falló tras 3 reintentos. Cascading FAILED.");
                    return currentState;
                }

                sendUdpBroadcast();
                retryCount++;
                stateStartTime = millis();
            }

            if (checkUdpResponse()) {
                currentState = DiscoveryState::SUCCESS;
                udp.stop();
                Serial.println("[Discovery] Nivel 3 Exitoso: UDP Broadcast descubrió Hub en " + hubIp + ":" + String(mqttPort));
                return currentState;
            }
            break;
        }
    }

    return currentState;
}

bool DiscoveryService::sendUdpBroadcast() {
    IPAddress broadcastIp(255, 255, 255, 255);
    udp.beginPacket(broadcastIp, 9999);
    
    JsonDocument doc;
    doc["cmd"] = "DISCOVER_HUB";
    doc["device_mac"] = WiFi.macAddress();

    serializeJson(doc, udp);
    udp.endPacket();
    Serial.println("[Discovery] Paquete UDP Broadcast enviado.");
    return true;
}

bool DiscoveryService::checkUdpResponse() {
    int packetSize = udp.parsePacket();
    if (packetSize > 0) {
        char buffer[256];
        int len = udp.read(buffer, sizeof(buffer) - 1);
        if (len > 0) {
            buffer[len] = 0;
        }

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, buffer);
        if (!error) {
            const char* status = doc["status"];
            if (status && strcmp(status, "IAM_HUB") == 0) {
                hubIp = doc["hub_ip"].as<String>();
                mqttPort = doc["mqtt_port"] | 1883;
                return true;
            }
        }
    }
    return false;
}

#else

// Mock para entorno Native Emulator
void DiscoveryService::startDiscovery(const GatewayConfig& gw) {
    hubIp = "127.0.0.1";
    mqttPort = 1883;
    currentState = DiscoveryState::SUCCESS;
}

DiscoveryState DiscoveryService::update() {
    return currentState;
}

#endif
