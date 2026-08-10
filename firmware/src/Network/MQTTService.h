#pragma once
#include <vector>
#include <map>

#ifdef ARDUINO
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#else
#include <string>
#endif

struct OrderItem {
#ifdef ARDUINO
    String id;
    String name;
    String status;
    std::vector<String> options;
#else
    std::string id;
    std::string name;
    std::string status;
    std::vector<std::string> options;
#endif
    int progress;
};

class MQTTService {
public:
    static MQTTService& getInstance() {
        static MQTTService instance;
        return instance;
    }

#ifdef ARDUINO
    void init(const String& hubIp, int mqttPort, const String& mac);
    void publishCommand(const String& cmd);
#else
    void init(const std::string& hubIp, int mqttPort, const std::string& mac);
    void publishCommand(const std::string& cmd);
#endif

    void update();
    
    typedef void (*OrdersCallback)(const std::vector<OrderItem>& orders);
    void setOrdersCallback(OrdersCallback cb) { ordersCb = cb; }

    typedef void (*BillStateCallback)(float subtotal, float tax, float total);
    void setBillStateCallback(BillStateCallback cb) { billStateCb = cb; }

    void publishBillRequest();

    bool isConnected() {
#ifdef ARDUINO
        return client.connected();
#else
        return true;
#endif
    }

private:
    MQTTService() = default;
    ~MQTTService() = default;

#ifdef ARDUINO
    String hubIp;
    String macAddress;
    String pendingCmd;
    unsigned long lastReconnectAttempt = 0;
    
    WiFiClient espClient;
    PubSubClient client;
    
    void reconnect();
    void mqttCallback(char* topic, byte* payload, unsigned int length);
    void processSync(byte* payload, unsigned int length);
#else
    std::string hubIp;
    std::string macAddress;
#endif

    int mqttPort;
    OrdersCallback ordersCb = nullptr;
    BillStateCallback billStateCb = nullptr;

#ifdef ARDUINO
    std::map<String, std::vector<OrderItem>> activeOrders;
#else
    std::map<std::string, std::vector<OrderItem>> activeOrders;
#endif
};
