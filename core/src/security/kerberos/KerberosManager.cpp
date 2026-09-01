#include "KerberosManager.hpp"
#include <string.h>

#ifdef ARDUINO
#include <Arduino.h>
#include <Preferences.h>
#else
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static inline uint32_t millis() {
    return (uint32_t)(esp_timer_get_time() / 1000);
}
static inline void delay(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}
#endif

#include <lvgl.h>
#include "cbdos/ui.hpp"
#include "cbdos/input.hpp"

extern cred_store* cred_store_nvs(void);
extern pin_store* pin_store_nvs(void);

namespace cbdos {
namespace security {

static const uint8_t KERB_AAGUID[16] = {
    0x4B, 0x45, 0x52, 0x42, 0x45, 0x52, 0x4F, 0x53,
    0x50, 0x4F, 0x53, 0x45, 0x49, 0x44, 0x4F, 0x4E
};

KerberosManager::KerberosManager() {
    memset(m_devKey, 0, sizeof(m_devKey));
    memset(&m_ctaphidCtx, 0, sizeof(m_ctaphidCtx));
    memset(&m_u2fCfg, 0, sizeof(m_u2fCfg));
    memset(&m_ctap2Cfg, 0, sizeof(m_ctap2Cfg));
    memset(&m_pinRt, 0, sizeof(m_pinRt));
}

void KerberosManager::init() {
    if (m_initialized) return;

    const kerb_crypto_t* cy = kerb_mbedtls_crypto();

#ifdef ARDUINO
    Preferences prefs;
    prefs.begin("kerberos", false);
    if (prefs.getBytesLength("devkey") == 32) {
        prefs.getBytes("devkey", m_devKey, 32);
    } else {
        if (cy && cy->rand) {
            cy->rand(m_devKey, sizeof(m_devKey), nullptr);
        }
        prefs.putBytes("devkey", m_devKey, 32);
    }
    m_counter = prefs.getUInt("ctr", 0);
    prefs.end();
#else
    if (cy && cy->rand) {
        cy->rand(m_devKey, sizeof(m_devKey), nullptr);
    }
#endif

    // Configuración U2F / CTAP1
    m_u2fCfg.cy = cy;
    m_u2fCfg.devkey = m_devKey;
    m_u2fCfg.att_cert = KERB_ATT_CERT;
    m_u2fCfg.att_cert_len = KERB_ATT_CERT_LEN;
    m_u2fCfg.att_priv = KERB_ATT_PRIV;
    m_u2fCfg.counter = &m_counter;
    m_u2fCfg.user_present = userPresentThunk;
    m_u2fCfg.ui = this;

    // Configuración CTAP2 / WebAuthn
    m_ctap2Cfg.cy = cy;
    m_ctap2Cfg.devkey = m_devKey;
    m_ctap2Cfg.aaguid = KERB_AAGUID;
    m_ctap2Cfg.att_cert = KERB_ATT_CERT;
    m_ctap2Cfg.att_cert_len = KERB_ATT_CERT_LEN;
    m_ctap2Cfg.att_priv = KERB_ATT_PRIV;
    m_ctap2Cfg.user_present = userPresentThunk;
    m_ctap2Cfg.ui = this;
    m_ctap2Cfg.counter = &m_counter;
    m_ctap2Cfg.store = cred_store_nvs(); // Resident credentials en NVS
    
    memset(&m_pinRt, 0, sizeof(m_pinRt));
    m_pinRt.boot_ms = millis();
    m_ctap2Cfg.pin = pin_store_nvs();     // ClientPIN store en NVS
    m_ctap2Cfg.pin_rt = &m_pinRt;

    // Inicializar despachador CTAPHID
    ctaphid_ctx_init(&m_ctaphidCtx, sinkThunk, this, u2fThunk, &m_u2fCfg);
    ctaphid_set_cbor(&m_ctaphidCtx, ctap2Thunk, &m_ctap2Cfg);

    m_initialized = true;
}

void KerberosManager::reset() {
    m_initialized = false;
    m_counter = 0;
    m_pendingPresence = false;
    m_presenceApproved = false;
    m_presenceDenied = false;
    m_pendingRpId.clear();
    init();
}

void KerberosManager::setUsbSendCallback(UsbSendCallback cb) {
    m_sendCb = cb;
}

void KerberosManager::setPresenceRequestCallback(PresenceRequestCallback cb) {
    m_presenceCb = cb;
}

void KerberosManager::handleIncomingUsbReport(const uint8_t pkt[64]) {
    if (!m_initialized) init();
    m_packetsReceived++;
    if (pkt && (pkt[4] & 0x80)) {
        m_lastCommand = pkt[4] & ~0x80;
    }
    m_currentCid = ctaphid_dispatch(&m_ctaphidCtx, pkt);
}

void KerberosManager::approvePresence() {
    if (m_pendingPresence) {
        m_presenceApproved = true;
    }
}

void KerberosManager::denyPresence() {
    if (m_pendingPresence) {
        m_presenceDenied = true;
    }
}

void KerberosManager::sinkThunk(const uint8_t pkt[64], void* ctx) {
    auto* self = static_cast<KerberosManager*>(ctx);
    if (self && self->m_sendCb) {
        self->m_sendCb(pkt);
    }
}

uint16_t KerberosManager::u2fThunk(const uint8_t* req, uint16_t rl, uint8_t* resp, uint16_t cap, void* ctx) {
    auto* cfg = static_cast<const u2f_cfg_t*>(ctx);
    if (cfg && cfg->ui) {
        auto* self = static_cast<KerberosManager*>(cfg->ui);
        self->m_isRegistration = (req && rl > 1 && req[1] == 0x01); // U2F_REGISTER
        self->m_pendingRpId = "U2F Security Key";
    }
    return u2f_handle(cfg, req, rl, resp, cap);
}

uint16_t KerberosManager::ctap2Thunk(const uint8_t* req, uint16_t rl, uint8_t* resp, uint16_t cap, void* ctx) {
    auto* cfg = static_cast<ctap2_cfg_t*>(ctx);
    if (cfg) {
        cfg->now_ms = millis();
        if (cfg->ui) {
            auto* self = static_cast<KerberosManager*>(cfg->ui);
            self->m_isRegistration = (req && rl > 0 && req[0] == 0x01); // CTAP2_MAKE_CRED
            self->m_pendingRpId = "FIDO2 / WebAuthn";
        }
    }
    return ctap2_handle(cfg, req, rl, resp, cap);
}

bool KerberosManager::userPresentThunk(void* ctx) {
    auto* self = static_cast<KerberosManager*>(ctx);
    if (!self) return false;

    self->m_pendingPresence = true;
    self->m_presenceApproved = false;
    self->m_presenceDenied = false;

    if (self->m_presenceCb) {
        self->m_presenceCb(self->m_pendingRpId, self->m_isRegistration);
    }

    // Esperar respuesta de usuario enviando keepalives (status 2 = user presence needed)
    uint32_t startMs = millis();
    uint32_t lastKeepalive = 0;
    while (self->m_pendingPresence) {
        // 1. Detección directa por hardware táctil (idéntico al input_poll de Poseidon)
        cbdos::input::TouchPoint tp;
        if (cbdos::input::getTouch(tp) && tp.isPressed) {
            self->m_pendingPresence = false;
            return true;
        }

        // 2. Detección por botón en UI
        if (self->m_presenceApproved) {
            self->m_pendingPresence = false;
            return true;
        }
        if (self->m_presenceDenied) {
            self->m_pendingPresence = false;
            return false;
        }

        // Timeout a los 30 segundos
        if (millis() - startMs > 30000) {
            self->m_pendingPresence = false;
            return false;
        }

        if (millis() - lastKeepalive >= 90) {
            if (self->m_currentCid != 0xFFFFFFFF) {
                ctaphid_keepalive(&self->m_ctaphidCtx, self->m_currentCid, 2);
            }
            lastKeepalive = millis();
        }

        // Bombeo del motor gráfico y táctil
        lv_timer_handler();
        cbdos::ui::update();
        delay(10);
    }

    return false;
}

} // namespace security
} // namespace cbdos
