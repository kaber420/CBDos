#include "AssetManager.h"
#include <SD.h>
#include <lvgl.h>
#include "ConfigManager.h"
#include <algorithm>

#ifdef ARDUINO
#include <HTTPClient.h>
#include <WiFi.h>
#endif

#include <unordered_map>
#include <unordered_set>

static std::unordered_map<std::string, std::string> s_assetCache;
static std::unordered_set<std::string> s_notFound;

void AssetManager::init() {
#ifdef ARDUINO
    if (!SD.exists("/assets")) {
        SD.mkdir("/assets");
    }
#endif
    s_assetCache.clear();
    s_notFound.clear();
}

#include "../Core/LVFS_Driver.h"

std::string AssetManager::getAssetPath(const std::string& hash, int width, int height) {
    if (hash.empty()) return "";

    std::string key = hash + "_" + std::to_string(width) + "x" + std::to_string(height);

    auto it = s_assetCache.find(key);
    if (it != s_assetCache.end()) {
        return it->second;
    }

    if (s_notFound.count(key)) {
        return "";
    }

#ifdef ARDUINO
    String path = "/assets/" + String(key.c_str()) + ".bin";
    lv_fs_spi_lock();
    bool exists = SD.exists(path);
    lv_fs_spi_unlock();

    if (exists) {
        std::string fullPath = "A:" + std::string(path.c_str());
        s_assetCache[key] = fullPath;
        return fullPath;
    }

    s_notFound.insert(key);
#endif
    return "";
}

bool AssetManager::isAssetCached(const std::string& hash, int width, int height) {
    return !getAssetPath(hash, width, height).empty();
}

void AssetManager::queueDownload(const std::string& hash, int width, int height) {
    if (hash.empty()) return;
    for (const auto& task : downloadQueue) {
        if (task.hash == hash && task.width == width && task.height == height) {
            return;
        }
    }
    downloadQueue.push_back({hash, width, height});
}

void AssetManager::processQueue() {
    if (downloadQueue.empty() || isDownloading) {
        return;
    }

    uint32_t now = millis();
    if (now - lastProcessTime < 1000) {
        return; // Esperar 1 segundo entre descargas
    }
    lastProcessTime = now;

    DownloadTask task = downloadQueue.front();
    isDownloading = true;

    bool success = downloadAsset(task);
    
    if (success) {
        pendingRefresh = true;
    }

    downloadQueue.erase(downloadQueue.begin());
    isDownloading = false;
}

bool AssetManager::downloadAsset(const DownloadTask& task) {
#ifdef ARDUINO
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    GatewayConfig gateway;
    if (!ConfigManager::getInstance().loadActiveGateway(gateway)) return false;
    if (gateway.address.length() == 0) return false;

    String filename = String(task.hash.c_str()) + "_" + String(task.width) + "x" + String(task.height) + ".bin";
    String url = "http://" + gateway.address + ":8080/api/cdn/image?hash=" + String(task.hash.c_str()) + "&w=" + String(task.width) + "&h=" + String(task.height);
    String path = "/assets/" + filename;

    Serial.printf("[CDN] Descargando asset: %s\n", url.c_str());

    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        File f = SD.open(path, FILE_WRITE);
        if (f) {
            http.writeToStream(&f);
            f.close();
            std::string key = task.hash + "_" + std::to_string(task.width) + "x" + std::to_string(task.height);
            s_notFound.erase(key);
            s_assetCache[key] = "A:" + std::string(path.c_str());
            http.end();
            return true;
        } else {
            Serial.println("[CDN] Error al abrir archivo en SD para escritura");
        }
    } else {
        Serial.printf("[CDN] Error HTTP %d al descargar asset\n", httpCode);
    }
    http.end();
#endif
    return false;
}
