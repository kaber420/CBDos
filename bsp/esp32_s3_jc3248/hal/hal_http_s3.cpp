#include "cbdos/http.hpp"
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClient.h>

namespace cbdos {
namespace bsp {

class ArduinoHttpClient : public http::IHttpClient {
public:
    http::HttpResponse get(const std::string& url, uint32_t timeoutMs) override {
        http::HttpResponse response;
        response.statusCode = 0;
        response.success = false;

        WiFiClient client;
        client.setTimeout(timeoutMs);

        HTTPClient http;
        http.setTimeout(timeoutMs);
        http.setUserAgent("CBDos-Http/1.0 (ESP32-S3)");
        http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

        if (http.begin(client, url.c_str())) {
            int httpCode = http.GET();
            response.statusCode = httpCode;
            if (httpCode == HTTP_CODE_OK || (httpCode >= 200 && httpCode < 300)) {
                response.body = http.getString().c_str();
                response.success = true;
            }
            http.end();
        }

        return response;
    }

    http::HttpResponse post(const std::string& url, const std::string& payload, const std::string& contentType, uint32_t timeoutMs) override {
        http::HttpResponse response;
        response.statusCode = 0;
        response.success = false;

        WiFiClient client;
        client.setTimeout(timeoutMs);

        HTTPClient http;
        http.setTimeout(timeoutMs);
        http.setUserAgent("CBDos-Http/1.0 (ESP32-S3)");

        if (http.begin(client, url.c_str())) {
            http.addHeader("Content-Type", contentType.c_str());
            int httpCode = http.POST((uint8_t*)payload.c_str(), payload.length());
            response.statusCode = httpCode;
            if (httpCode >= 200 && httpCode < 300) {
                response.body = http.getString().c_str();
                response.success = true;
            }
            http.end();
        }

        return response;
    }
};

static ArduinoHttpClient s_s3HttpClient;

void initHttpClientS3() {
    http::setClient(&s_s3HttpClient);
}

} // namespace bsp
} // namespace cbdos
