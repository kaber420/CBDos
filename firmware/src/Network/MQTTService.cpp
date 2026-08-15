#include "MQTTService.h"
#include "ConfigManager.h"
#include "../UI/Views/MenuView.h"
#include "../Core/LVFS_Driver.h"

#ifdef ARDUINO
#include <WiFi.h>
#include <SD.h>

void MQTTService::init(const GatewayConfig& gw, const String& mac) {
    this->hubIp = gw.address;
    this->mqttPort = gw.mqttPort;
    this->macAddress = mac;
    
    espClient.setTimeout(50); // Timeout corto de 50ms para jamás congelar la UI si el Hub está offline
    client.setClient(espClient);
    client.setServer(this->hubIp.c_str(), this->mqttPort);
    
    client.setCallback([this](char* topic, byte* payload, unsigned int length) {
        this->mqttCallback(topic, payload, length);
    });
}

void MQTTService::reconnectTo(const GatewayConfig& gw) {
    if (client.connected()) {
        client.disconnect();
    }
    this->hubIp = gw.address;
    this->mqttPort = gw.mqttPort;
    client.setServer(this->hubIp.c_str(), this->mqttPort);
    lastReconnectAttempt = 0;
}

void MQTTService::update() {
    if (WiFi.status() != WL_CONNECTED || hubIp.length() == 0) return;

    if (!client.connected()) {
        long now = millis();
        if (lastReconnectAttempt == 0 || now - lastReconnectAttempt > 5000) {
            lastReconnectAttempt = now;
            if (client.connect(macAddress.c_str())) {
                lastReconnectAttempt = 0;
                String baseTopic = "tablehub/device/" + macAddress;
                client.subscribe((baseTopic + "/state").c_str());
                client.subscribe((baseTopic + "/config").c_str());
                client.subscribe((baseTopic + "/branding").c_str());
                client.subscribe((baseTopic + "/sync").c_str());
                client.subscribe((baseTopic + "/bill/state").c_str());
                client.subscribe((baseTopic + "/order/+/state").c_str());
                Serial.println("[MQTT] Conectado y suscrito a la jerarquía de tópicos para: " + macAddress);

                GatewayConfig gwConfig;
                if (ConfigManager::getInstance().loadActiveGateway(gwConfig)) {
                    // 1. Enviar paquete de registro de aprovisionamiento con auth_token
                    if (gwConfig.authToken.length() > 0) {
                        JsonDocument provDoc;
                        provDoc["mac"] = macAddress;
                        provDoc["gateway_name"] = gwConfig.name;
                        provDoc["auth_token"] = gwConfig.authToken;
                        provDoc["ip"] = WiFi.localIP().toString();
                        provDoc["type"] = "CBDos";

                        char provBuf[256];
                        serializeJson(provDoc, provBuf);
                        client.publish("cbdos/device/provision", provBuf);
                        Serial.println("[MQTT] Paquete de Aprovisionamiento enviado a cbdos/device/provision");
                    }

                    // 2. Enviar telemetría inicial de estado
                    JsonDocument statusDoc;
                    statusDoc["mac"] = macAddress;
                    statusDoc["gateway"] = gwConfig.name;
                    statusDoc["ip"] = WiFi.localIP().toString();
                    statusDoc["battery"] = 100;
                    statusDoc["wifi"] = WiFi.RSSI();
                    statusDoc["status"] = "active";

                    char statusBuf[256];
                    serializeJson(statusDoc, statusBuf);
                    String statusTopic = "cbdos/device/" + macAddress + "/status";
                    client.publish(statusTopic.c_str(), statusBuf);
                }
            }
        }
    } else {
        client.loop();

        if (pendingCmd.length() > 0) {
            String topic = "tablehub/device/" + macAddress + "/command";
            JsonDocument doc;
            doc["cmd"] = pendingCmd;
            doc["table_id"] = macAddress;
            
            char buffer[200];
            serializeJson(doc, buffer);
            client.publish(topic.c_str(), buffer);
            Serial.println("[MQTT] Comando publicado (diferido): " + String(buffer));
            pendingCmd = "";
        }
    }
}

void MQTTService::publishCommand(const String& cmd) {
    pendingCmd = cmd;
}

void MQTTService::publishBillRequest() {
    if (client.connected()) {
        String topic = "tablehub/device/" + macAddress + "/bill/request";
        client.publish(topic.c_str(), "{}");
        Serial.println("[MQTT] Solicitud de cuenta enviada a " + topic);
    }
}

void MQTTService::mqttCallback(char* topic, byte* payload, unsigned int length) {
    String topicStr = String(topic);
    String baseTopic = "tablehub/device/" + macAddress;

    if (topicStr == baseTopic + "/state") {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload, length);
        if (!error) {
            if (doc["orders"].is<JsonArray>() && ordersCb != nullptr) {
                activeOrders.clear();
                std::vector<OrderItem> ordersList;
                JsonArray arr = doc["orders"].as<JsonArray>();
                for (JsonVariant v : arr) {
                    OrderItem item;
                    item.id = v["id"] | "";
                    item.name = v["name"].as<String>();
                    item.status = v["status"].as<String>();
                    item.progress = v["progress"] | 0;
                    ordersList.push_back(item);
                }
                activeOrders["legacy"] = ordersList;
                ordersCb(ordersList);
            }
        }
    } else if (topicStr == baseTopic + "/config") {
        // config processing
    } else if (topicStr == baseTopic + "/sync") {
        processSync(payload, length);
    } else if (topicStr == baseTopic + "/bill/state") {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload, length);
        if (!error && billStateCb) {
            float subtotal = doc["subtotal"] | 0.0f;
            float tax = doc["tax"] | 0.0f;
            float total = doc["total"] | 0.0f;
            billStateCb(subtotal, tax, total);
        }
    } else if (topicStr.startsWith(baseTopic + "/order/")) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload, length);
        if (!error) {
            String orderId = doc["order_id"].as<String>();
            if (orderId.length() == 0) {
                int orderIdx = topicStr.indexOf("/order/") + 7;
                int stateIdx = topicStr.indexOf("/state", orderIdx);
                if (stateIdx > orderIdx) {
                    orderId = topicStr.substring(orderIdx, stateIdx);
                }
            }

            int progressPct = doc["order_progress_pct"] | 0;

            if (progressPct >= 100) {
                activeOrders.erase(orderId);
            } else if (doc["items"].is<JsonArray>()) {
                std::vector<OrderItem> itemsList;
                JsonArray arr = doc["items"].as<JsonArray>();
                for (JsonVariant v : arr) {
                    OrderItem item;
                    item.id = v["id"] | "";
                    item.name = v["name"].as<String>();
                    item.status = v["status_label"].as<String>();
                    
                    if (v["options"].is<JsonArray>()) {
                        JsonArray optArr = v["options"].as<JsonArray>();
                        for (JsonVariant opt : optArr) {
                            item.options.push_back(opt.as<String>());
                        }
                    }
                    
                    String statusLower = item.status;
                    statusLower.toLowerCase();
                    if (statusLower.indexOf("listo") >= 0 || statusLower.indexOf("servido") >= 0 || statusLower.indexOf("ready") >= 0 || statusLower.indexOf("completed") >= 0) {
                        item.progress = 100;
                    } else if (statusLower.indexOf("cocina") >= 0 || statusLower.indexOf("preparando") >= 0 || statusLower.indexOf("cooking") >= 0) {
                        item.progress = 50;
                    } else {
                        item.progress = 0;
                    }
                    itemsList.push_back(item);
                }
                activeOrders[orderId] = itemsList;
            }

            std::vector<OrderItem> allOrders;
            for (const auto& pair : activeOrders) {
                for (const auto& item : pair.second) {
                    allOrders.push_back(item);
                }
            }

            if (ordersCb != nullptr) {
                ordersCb(allOrders);
            }
        }
    }
}

void MQTTService::processSync(byte* payload, unsigned int length) {
    JsonDocument doc;
    DeserializationError error = deserializeMsgPack(doc, payload, length);
    if (error) {
        Serial.println("[SYNC] Error parseando catalogo MsgPack recibido");
        return;
    }
    
    std::string newVersion = doc["catalog_version"] | "";
    std::string currentVersion = MenuView::getCurrentCatalogVersion();
    
    if (newVersion == currentVersion && !newVersion.empty()) {
        Serial.printf("[SYNC] Catalogo ya actualizado (Version: %s). Ignorando.\n", newVersion.c_str());
        return;
    }

    Serial.printf("[SYNC] Nueva version de catalogo detectada: %s\n", newVersion.c_str());
    
    lv_fs_spi_lock();
    File file = SD.open("/catalog_manifest.msgpack", FILE_WRITE);
    if (file) {
        size_t written = file.write(payload, length);
        file.close();
        if (written == length) {
            Serial.println("[SYNC] Nuevo manifesto MsgPack guardado exitosamente.");
            MenuView::setCurrentCatalogVersion(newVersion);
        } else {
            Serial.printf("[SYNC] ERROR: Solo se escribieron %d de %d bytes en la SD\n", written, length);
        }
    } else {
        Serial.println("[SYNC] ERROR: No se pudo abrir la SD para guardar el catalogo");
    }
    lv_fs_spi_unlock();
    
    if (!SD.exists("/assets")) {
        SD.mkdir("/assets");
    }
    
    std::vector<String> validFiles;
    JsonArray items = doc["items"].as<JsonArray>();
    for (JsonVariant v : items) {
        String hash = v["image_hash"].as<String>();
        if (hash.length() > 0) {
            validFiles.push_back(hash);
        }
    }
    
    File dir = SD.open("/assets");
    if (dir && dir.isDirectory()) {
        File entry = dir.openNextFile();
        while (entry) {
            String fileName = String(entry.name());
            String filePath = String(entry.path());
            if (filePath.length() == 0) {
                 filePath = "/assets/" + fileName;
            }
            
            bool isValid = false;
            for (const String& validPrefix : validFiles) {
                if (fileName.indexOf(validPrefix) >= 0) {
                    isValid = true;
                    break;
                }
            }
            
            entry.close();
            
            if (!isValid) {
                Serial.println("[SYNC] Purgando asset obsoleto: " + filePath);
                SD.remove(filePath.c_str());
            }
            
            entry = dir.openNextFile();
        }
        dir.close();
    }
    
    Serial.println("[SYNC] Sincronizacion logica completada.");
}

#else

// Mock implementation for emulator
#include <iostream>

void MQTTService::init(const GatewayConfig& gw, const std::string& mac) {
    this->hubIp = gw.address;
    this->mqttPort = gw.mqttPort;
    this->macAddress = mac;
    std::cout << "[MQTT Mock] Inicializado hacia " << gw.address << ":" << gw.mqttPort << std::endl;
}

void MQTTService::reconnectTo(const GatewayConfig& gw) {
    this->hubIp = gw.address;
    this->mqttPort = gw.mqttPort;
    std::cout << "[MQTT Mock] Reconectado hacia " << gw.address << ":" << gw.mqttPort << std::endl;
}

void MQTTService::update() {
    static bool fired = false;
    if (!fired && ordersCb != nullptr) {
        std::vector<OrderItem> mockOrders = {
            {"item_1", "Mock Burger", "En Cocina", 50},
            {"item_2", "Mock Fries", "En Espera", 10}
        };
        ordersCb(mockOrders);
        fired = true;
    }
}

void MQTTService::publishCommand(const std::string& cmd) {
    std::cout << "[MQTT Mock] Comando publicado: " << cmd << std::endl;
}

void MQTTService::publishBillRequest() {
    std::cout << "[MQTT Mock] Solicitud de cuenta enviada" << std::endl;
    if (billStateCb) {
        billStateCb(450.00f, 72.00f, 522.00f);
    }
}

#endif
