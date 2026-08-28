#include "cbdos/http.hpp"
#include <esp_http_client.h>
#include <esp_log.h>
#include <cstring>

static const char* TAG_HTTP = "ESP_HTTP_P4";

namespace cbdos {
namespace bsp {

class EspIdfHttpClient : public http::IHttpClient {
public:
    http::HttpResponse get(const std::string& url, uint32_t timeoutMs) override {
        http::HttpResponse response;
        response.statusCode = 0;
        response.success = false;

        esp_http_client_config_t config = {};
        config.url = url.c_str();
        config.timeout_ms = timeoutMs;
        config.disable_auto_redirect = false;
        config.max_redirection_count = 3;
        config.user_agent = "CBDos-Http/1.0 (ESP32-P4)";
        config.buffer_size = 2048;
        config.buffer_size_tx = 512;

        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (!client) return response;

        esp_http_client_set_header(client, "Connection", "close");

        esp_err_t err = esp_http_client_open(client, 0);
        if (err != ESP_OK) {
            ESP_LOGE(TAG_HTTP, "Error conexion HTTP: %s", esp_err_to_name(err));
            esp_http_client_cleanup(client);
            return response;
        }

        int contentLength = esp_http_client_fetch_headers(client);
        int statusCode = esp_http_client_get_status_code(client);
        response.statusCode = statusCode;

        if (statusCode >= 200 && statusCode < 300) {
            if (contentLength > 0 && contentLength < 128 * 1024) {
                response.body.resize(contentLength);
                int totalRead = 0;
                while (totalRead < contentLength) {
                    int r = esp_http_client_read(client, &response.body[totalRead], contentLength - totalRead);
                    if (r <= 0) break;
                    totalRead += r;
                }
                response.body.resize(totalRead);
            } else {
                char buf[1024];
                int r = 0;
                while ((r = esp_http_client_read(client, buf, sizeof(buf) - 1)) > 0) {
                    buf[r] = '\0';
                    response.body.append(buf, r);
                    if (esp_http_client_is_complete_data_received(client)) break;
                    if (response.body.size() > 128 * 1024) break;
                }
            }
            response.success = true;
        }

        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return response;
    }

    http::HttpResponse post(const std::string& url, const std::string& payload, const std::string& contentType, uint32_t timeoutMs) override {
        http::HttpResponse response;
        response.statusCode = 0;
        response.success = false;

        esp_http_client_config_t config = {};
        config.url = url.c_str();
        config.timeout_ms = timeoutMs;
        config.method = HTTP_METHOD_POST;
        config.user_agent = "CBDos-Http/1.0 (ESP32-P4)";

        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (!client) return response;

        esp_http_client_set_header(client, "Content-Type", contentType.c_str());
        esp_http_client_set_post_field(client, payload.c_str(), payload.length());

        esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK) {
            response.statusCode = esp_http_client_get_status_code(client);
            response.success = (response.statusCode >= 200 && response.statusCode < 300);
        }

        esp_http_client_cleanup(client);
        return response;
    }
};

static EspIdfHttpClient s_p4HttpClient;

void initHttpClientP4() {
    http::setClient(&s_p4HttpClient);
}

} // namespace bsp
} // namespace cbdos
