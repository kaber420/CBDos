#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace cbdos {
namespace http {

struct HttpResponse {
    int statusCode = 0;
    std::string body;
    bool success = false;
};

class IHttpClient {
public:
    virtual ~IHttpClient() = default;
    virtual HttpResponse get(const std::string& url, uint32_t timeoutMs = 5000) = 0;
    virtual HttpResponse post(const std::string& url, const std::string& payload, const std::string& contentType = "application/json", uint32_t timeoutMs = 5000) = 0;
};

void setClient(IHttpClient* client);
IHttpClient* getClient();

// Funciones de conveniencia
HttpResponse get(const std::string& url, uint32_t timeoutMs = 5000);
HttpResponse post(const std::string& url, const std::string& payload, const std::string& contentType = "application/json", uint32_t timeoutMs = 5000);

} // namespace http
} // namespace cbdos
