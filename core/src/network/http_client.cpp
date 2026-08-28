#include "cbdos/http.hpp"

namespace cbdos {
namespace http {

static IHttpClient* s_httpClient = nullptr;

void setClient(IHttpClient* client) {
    s_httpClient = client;
}

IHttpClient* getClient() {
    return s_httpClient;
}

HttpResponse get(const std::string& url, uint32_t timeoutMs) {
    if (s_httpClient) {
        return s_httpClient->get(url, timeoutMs);
    }
    HttpResponse res;
    res.statusCode = -1;
    res.success = false;
    return res;
}

HttpResponse post(const std::string& url, const std::string& payload, const std::string& contentType, uint32_t timeoutMs) {
    if (s_httpClient) {
        return s_httpClient->post(url, payload, contentType, timeoutMs);
    }
    HttpResponse res;
    res.statusCode = -1;
    res.success = false;
    return res;
}

} // namespace http
} // namespace cbdos
