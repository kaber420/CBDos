#include "cbdos/socket.hpp"

namespace cbdos {
namespace network {

static ISocketFactory* s_socketFactory = nullptr;

void setSocketFactory(ISocketFactory* factory) {
    s_socketFactory = factory;
}

ISocketFactory* getSocketFactory() {
    return s_socketFactory;
}

std::unique_ptr<ISocketStream> createSocket(SocketType type) {
    if (s_socketFactory) {
        return s_socketFactory->createSocket(type);
    }
    return nullptr;
}

} // namespace network
} // namespace cbdos
