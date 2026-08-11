#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*tlv_packet_recv_cb_t)(const uint8_t* packet, size_t len);

#ifdef __cplusplus
}
#endif

class TlvNetworkClient {
public:
    static void init(const char* gateway_ip = "192.168.66.254", uint16_t port = 8765);
    static void setGatewayConfig(const char* ip, uint16_t port);
    
    static bool sendRequest(const uint8_t* packet, size_t len);
    static void setPacketRecvCallback(tlv_packet_recv_cb_t cb);
    
    static void loop();
    static bool isConnected();

private:
    static char serverIp[64];
    static uint16_t serverPort;
    static tlv_packet_recv_cb_t recvCb;
};
