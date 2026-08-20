#include "cbdos/network.hpp"
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <nvs_flash.h>
#include <esp_log.h>
#include <lwip/inet.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>


static const char* TAG = "CBDOS_NET_P4";

namespace cbdos {
namespace network {

static NetStatus s_status = NetStatus::Disconnected;
static std::string s_ip = "0.0.0.0";
static int8_t s_rssi = -127;
static esp_netif_t* s_sta_netif = nullptr;
static bool s_initialized = false;

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi Station iniciada");
                break;
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "WiFi conectado al AP, esperando IP...");
                s_status = NetStatus::Connecting;
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGW(TAG, "WiFi desconectado");
                s_status = NetStatus::Disconnected;
                s_ip = "0.0.0.0";
                s_rssi = -127;
                break;
            default:
                break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
            char ip_str[IP4ADDR_STRLEN_MAX];
            esp_ip4addr_ntoa(&event->ip_info.ip, ip_str, sizeof(ip_str));
            s_ip = ip_str;
            s_status = NetStatus::Connected;
            ESP_LOGI(TAG, "WiFi conectado con éxito. Dirección IP: %s", s_ip.c_str());
        }
    }
}

bool init() {
    if (s_initialized) {
        return true;
    }

    ESP_LOGI(TAG, "Inicializando subsistema de red (ESP-Hosted SDIO)...");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fallo nvs_flash_init: %s", esp_err_to_name(ret));
    }

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Fallo esp_netif_init: %s", esp_err_to_name(err));
        s_status = NetStatus::Error;
        return false;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Fallo esp_event_loop_create_default: %s", esp_err_to_name(err));
    }

    if (!s_sta_netif) {
        s_sta_netif = esp_netif_create_default_wifi_sta();
    }

    // Energizar carril C6 (GPIO 36 -> R44 -> ESP_3V3) y Reset (GPIO 54)
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << GPIO_NUM_36) | (1ULL << GPIO_NUM_54);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    // Secuencia de encendido y reset del C6
    gpio_set_level(GPIO_NUM_36, 1); // Energizar ESP_3V3
    gpio_set_level(GPIO_NUM_54, 0); // Pulso de Reset
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(GPIO_NUM_54, 1); // Liberar Reset
    vTaskDelay(pdMS_TO_TICKS(500)); // Esperar que el C6 arranque su firmware esclavo

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Fallo esp_wifi_init: %s", esp_err_to_name(err));
        s_status = NetStatus::Error;
        return false;
    }

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, nullptr, &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, nullptr, &instance_got_ip);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    s_sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");

    s_initialized = true;
    s_status = NetStatus::Disconnected;
    return true;
}



bool connectWifi(const char* ssid, const char* password) {
    ESP_LOGI(TAG, "Solicitando conexion Wi-Fi -> SSID: '%s', Pass: '%s'", ssid ? ssid : "(null)", (password && strlen(password) > 0) ? "******" : "(vacio)");
    if (!ssid || strlen(ssid) == 0) {
        ESP_LOGW(TAG, "SSID nulo o vacio, cancelando conexion");
        return false;
    }
    if (!s_initialized && !init()) {
        ESP_LOGE(TAG, "Fallo al inicializar subsistema de red");
        return false;
    }

    // Asegurar DHCP habilitado
    if (s_sta_netif) {
        esp_netif_dhcpc_start(s_sta_netif);
    }

    wifi_config_t wifi_config;
    std::memset(&wifi_config, 0, sizeof(wifi_config));
    std::strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    if (password && strlen(password) > 0) {
        std::strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    }

    s_status = NetStatus::Connecting;
    s_ip = "0.0.0.0";

    ESP_LOGI(TAG, "Iniciando esp_wifi_connect()...");
    esp_wifi_disconnect();
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Fallo esp_wifi_connect: %s", esp_err_to_name(err));
        s_status = NetStatus::Error;
        return false;
    }

    ESP_LOGI(TAG, "esp_wifi_connect() enviado con exito al C6");
    return true;
}


bool connectWifiStatic(const char* ssid, const char* password, const char* ip, const char* gateway, const char* subnet, const char* dns) {
    if (!ssid || strlen(ssid) == 0) return false;
    if (!s_initialized && !init()) return false;

    if (s_sta_netif && ip && gateway) {
        esp_netif_dhcpc_stop(s_sta_netif);

        esp_netif_ip_info_t ip_info;
        std::memset(&ip_info, 0, sizeof(ip_info));
        ip_info.ip.addr = esp_ip4addr_aton(ip);
        ip_info.gw.addr = esp_ip4addr_aton(gateway);
        ip_info.netmask.addr = esp_ip4addr_aton(subnet ? subnet : "255.255.255.0");

        esp_netif_set_ip_info(s_sta_netif, &ip_info);

        if (dns && strlen(dns) > 0) {
            esp_netif_dns_info_t dns_info;
            dns_info.ip.u_addr.ip4.addr = esp_ip4addr_aton(dns);
            dns_info.ip.type = ESP_IPADDR_TYPE_V4;
            esp_netif_set_dns_info(s_sta_netif, ESP_NETIF_DNS_MAIN, &dns_info);
        }
    }

    return connectWifi(ssid, password);
}

void disconnectWifi() {
    if (s_initialized) {
        esp_wifi_disconnect();
    }
    s_status = NetStatus::Disconnected;
    s_ip = "0.0.0.0";
    s_rssi = -127;
}

NetStatus getStatus() {
    return s_status;
}

bool isConnected() {
    return (s_status == NetStatus::Connected);
}

std::string getIpAddress() {
    return s_ip;
}

int8_t getRssi() {
    if (s_status == NetStatus::Connected) {
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            s_rssi = ap_info.rssi;
            return s_rssi;
        }
    }
    return -127;
}

} // namespace network
} // namespace cbdos

