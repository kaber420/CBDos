#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <functional>
#include <string>

#include "kerberos_core/u2f.h"
#include "kerberos_core/ctap2.h"
#include "kerberos_core/ctaphid_dispatch.h"
#include "kerberos_crypto.h"
#include "kerberos_attestation.h"

namespace cbdos {
namespace security {

class KerberosManager {
public:
    using UsbSendCallback = std::function<void(const uint8_t packet[64])>;
    using PresenceRequestCallback = std::function<void(const std::string& appName, bool isRegistration)>;

    static KerberosManager& instance() {
        static KerberosManager s_instance;
        return s_instance;
    }

    void init();
    void reset();

    void setUsbSendCallback(UsbSendCallback cb);
    void setPresenceRequestCallback(PresenceRequestCallback cb);

    struct PresenceRequest {
        std::string rpId;
        bool isRegistration = false;
    };

    void handleIncomingUsbReport(const uint8_t pkt[64]);

    // Gestión de presencia de usuario (aprobación/rechazo interactivo UI)
    void approvePresence();
    void denyPresence();
    bool isPresencePending() const { return m_pendingPresence; }
    bool isRegistration() const { return m_isRegistration; }
    std::string getPendingRpId() const { return m_pendingRpId; }
    PresenceRequest getPresenceRequest() const { return {m_pendingRpId, m_isRegistration}; }

    uint32_t getSignatureCounter() const { return m_counter; }
    uint32_t getPacketsReceivedCount() const { return m_packetsReceived; }
    uint8_t getLastCommand() const { return m_lastCommand; }

private:
    KerberosManager();
    ~KerberosManager() = default;

    static void sinkThunk(const uint8_t pkt[64], void* ctx);
    static uint16_t u2fThunk(const uint8_t* req, uint16_t rl, uint8_t* resp, uint16_t cap, void* ctx);
    static uint16_t ctap2Thunk(const uint8_t* req, uint16_t rl, uint8_t* resp, uint16_t cap, void* ctx);
    static bool userPresentThunk(void* ctx);

    bool m_initialized = false;
    uint8_t m_devKey[32];
    uint32_t m_counter = 0;
    
    ctaphid_ctx_t m_ctaphidCtx;
    u2f_cfg_t m_u2fCfg;
    ctap2_cfg_t m_ctap2Cfg;
    ctap2_pin_rt m_pinRt;

    UsbSendCallback m_sendCb = nullptr;
    PresenceRequestCallback m_presenceCb = nullptr;

    volatile bool m_pendingPresence = false;
    volatile bool m_presenceApproved = false;
    volatile bool m_presenceDenied = false;
    bool m_isRegistration = false;
    std::string m_pendingRpId;
    uint32_t m_currentCid = 0xFFFFFFFF;
    uint32_t m_packetsReceived = 0;
    uint8_t m_lastCommand = 0;
};

} // namespace security
} // namespace cbdos
