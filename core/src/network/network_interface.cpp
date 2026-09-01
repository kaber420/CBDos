#include "cbdos/network_interface.hpp"

namespace cbdos {
namespace network {

NetworkInterfaceManager& NetworkInterfaceManager::getInstance() {
    static NetworkInterfaceManager instance;
    return instance;
}

bool NetworkInterfaceManager::registerInterface(uint8_t slot, INetworkInterface* iface) {
    if (slot >= MAX_NETWORK_SLOTS) {
        return false;
    }
    m_slots[slot] = iface;
    return true;
}

INetworkInterface* NetworkInterfaceManager::getInterface(uint8_t slot) {
    if (slot >= MAX_NETWORK_SLOTS) {
        return nullptr;
    }
    return m_slots[slot];
}

uint8_t NetworkInterfaceManager::getInterfaceCount() const {
    uint8_t count = 0;
    for (size_t i = 0; i < MAX_NETWORK_SLOTS; ++i) {
        if (m_slots[i] != nullptr) {
            count++;
        }
    }
    return count;
}

void NetworkInterfaceManager::setAllOffline() {
    for (size_t i = 0; i < MAX_NETWORK_SLOTS; ++i) {
        if (m_slots[i] != nullptr) {
            m_slots[i]->setMode(InterfaceMode::Off);
        }
    }
}

bool NetworkInterfaceManager::isAnyInterfaceActive() const {
    for (size_t i = 0; i < MAX_NETWORK_SLOTS; ++i) {
        if (m_slots[i] != nullptr && m_slots[i]->getMode() != InterfaceMode::Off && m_slots[i]->isReady()) {
            return true;
        }
    }
    return false;
}

} // namespace network
} // namespace cbdos
