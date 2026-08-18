#include "CartridgeManager.h"
#include "StorageManager.h"
#include "LVFS_Driver.h"
#include <cstdio>
#include <algorithm>

#ifdef ARDUINO
#include <Arduino.h>
#include <SD.h>
#endif

CartridgeSlotInfo CartridgeManager::getSlotInfo(esp_partition_subtype_t subtype) {
    CartridgeSlotInfo info;
    info.subtype = subtype;
    info.isInstalled = false;

#ifdef ARDUINO
    const esp_partition_t* part = esp_partition_find_first(ESP_PARTITION_TYPE_APP, subtype, NULL);
    if (!part) {
        info.projectName = "No existe";
        return info;
    }
    info.partitionSize = part->size;

    esp_app_desc_t app_desc;
    esp_err_t err = esp_ota_get_partition_description(part, &app_desc);
    if (err == ESP_OK && app_desc.magic_word == ESP_APP_DESC_MAGIC_WORD) {
        info.isInstalled = true;
        info.projectName = app_desc.project_name;
        info.version = app_desc.version;
        info.compileDate = app_desc.date;
        info.compileTime = app_desc.time;
    } else {
        info.projectName = "Ranura Vacia";
    }
#else
    info.isInstalled = (subtype == ESP_PARTITION_SUBTYPE_APP_OTA_1);
    info.projectName = (subtype == ESP_PARTITION_SUBTYPE_APP_OTA_1) ? "DOOM Classic" : "Ranura Vacia";
    info.version = "1.0.0";
    info.partitionSize = (subtype == ESP_PARTITION_SUBTYPE_APP_OTA_1) ? (4 * 1024 * 1024) : (2 * 1024 * 1024);
#endif

    return info;
}

bool CartridgeManager::isSlotInstalled(esp_partition_subtype_t subtype) {
    return getSlotInfo(subtype).isInstalled;
}

bool CartridgeManager::bootSlot(esp_partition_subtype_t subtype) {
#ifdef ARDUINO
    const esp_partition_t* part = esp_partition_find_first(ESP_PARTITION_TYPE_APP, subtype, NULL);
    if (!part) {
        Serial.printf("[CartridgeManager] Particion 0x%x no encontrada\n", subtype);
        return false;
    }

    esp_app_desc_t app_desc;
    if (esp_ota_get_partition_description(part, &app_desc) != ESP_OK || app_desc.magic_word != ESP_APP_DESC_MAGIC_WORD) {
        Serial.printf("[CartridgeManager] Particion vacia o invalida en 0x%x\n", subtype);
        return false;
    }

    Serial.printf("[CartridgeManager] Arrancando %s v%s...\n", app_desc.project_name, app_desc.version);
    esp_err_t err = esp_ota_set_boot_partition(part);
    if (err == ESP_OK) {
        delay(300);
        esp_restart();
        return true;
    } else {
        Serial.printf("[CartridgeManager] Error al configurar arranque: %s\n", esp_err_to_name(err));
        return false;
    }
#else
    return true;
#endif
}

std::vector<std::string> CartridgeManager::listBinFilesOnSD(const std::string& directory) {
    std::vector<std::string> list;
#ifdef ARDUINO
    if (!StorageManager::isSdAvailable()) return list;

    lv_fs_spi_lock();
    if (!SD.exists(directory.c_str())) {
        SD.mkdir(directory.c_str());
    }

    File dir = SD.open(directory.c_str());
    if (dir && dir.isDirectory()) {
        File entry = dir.openNextFile();
        while (entry) {
            if (!entry.isDirectory()) {
                std::string name = entry.name();
                size_t slashIdx = name.rfind('/');
                if (slashIdx != std::string::npos) {
                    name = name.substr(slashIdx + 1);
                }
                
                std::string lowerName = name;
                std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                if (lowerName.length() > 4 && lowerName.substr(lowerName.length() - 4) == ".bin") {
                    std::string fullPath = directory;
                    if (fullPath.empty() || fullPath.back() != '/') fullPath += "/";
                    fullPath += name;
                    list.push_back(fullPath);
                }
            }
            entry.close();
            entry = dir.openNextFile();
        }
        dir.close();
    }
    lv_fs_spi_unlock();
#endif
    return list;
}

bool CartridgeManager::flashFromSD(const std::string& sdPath, 
                                   esp_partition_subtype_t targetSlot, 
                                   std::function<void(size_t written, size_t total)> progressCb) {
#ifdef ARDUINO
    if (!StorageManager::isSdAvailable()) {
        Serial.println("[CartridgeManager] Error: MicroSD no disponible para flasheo");
        return false;
    }

    const esp_partition_t* part = esp_partition_find_first(ESP_PARTITION_TYPE_APP, targetSlot, NULL);
    if (!part) {
        Serial.println("[CartridgeManager] Error: Particion de destino no encontrada en tabla");
        return false;
    }

    lv_fs_spi_lock();
    File f = SD.open(sdPath.c_str(), FILE_READ);
    if (!f) {
        lv_fs_spi_unlock();
        Serial.printf("[CartridgeManager] Error al abrir %s\n", sdPath.c_str());
        return false;
    }

    size_t totalBytes = f.size();
    if (totalBytes > part->size || totalBytes == 0) {
        f.close();
        lv_fs_spi_unlock();
        Serial.printf("[CartridgeManager] Error: Tamaño de archivo (%u) excede particion (%u)\n", 
                      (uint32_t)totalBytes, (uint32_t)part->size);
        return false;
    }

    Serial.printf("[CartridgeManager] Iniciando OTA en particion 0x%06lx (Tamaño: %lu bytes)...\n", 
                  (unsigned long)part->address, (unsigned long)totalBytes);

    esp_ota_handle_t ota_handle = 0;
    esp_err_t err = esp_ota_begin(part, totalBytes, &ota_handle);
    if (err != ESP_OK) {
        f.close();
        lv_fs_spi_unlock();
        Serial.printf("[CartridgeManager] esp_ota_begin fallo: %s\n", esp_err_to_name(err));
        return false;
    }

    const size_t CHUNK_SIZE = 4096;
    uint8_t* buffer = (uint8_t*)malloc(CHUNK_SIZE);
    if (!buffer) {
        esp_ota_abort(ota_handle);
        f.close();
        lv_fs_spi_unlock();
        Serial.println("[CartridgeManager] Error: No hay memoria RAM para buffer de flasheo");
        return false;
    }

    size_t bytesWritten = 0;
    bool success = true;

    while (bytesWritten < totalBytes) {
        size_t toRead = std::min((size_t)(totalBytes - bytesWritten), CHUNK_SIZE);
        size_t bytesRead = f.read(buffer, toRead);
        if (bytesRead == 0) {
            success = false;
            break;
        }

        err = esp_ota_write(ota_handle, (const void*)buffer, bytesRead);
        if (err != ESP_OK) {
            Serial.printf("[CartridgeManager] esp_ota_write fallo en byte %lu: %s\n", 
                          (unsigned long)bytesWritten, esp_err_to_name(err));
            success = false;
            break;
        }

        bytesWritten += bytesRead;
        if (progressCb) {
            progressCb(bytesWritten, totalBytes);
        }
    }

    free(buffer);
    f.close();
    lv_fs_spi_unlock();

    if (success && bytesWritten == totalBytes) {
        err = esp_ota_end(ota_handle);
        if (err != ESP_OK) {
            Serial.printf("[CartridgeManager] esp_ota_end fallo: %s\n", esp_err_to_name(err));
            return false;
        }
        Serial.printf("[CartridgeManager] Flasheo completo y validado exitosamente (%lu bytes)\n", (unsigned long)bytesWritten);
        return true;
    } else {
        esp_ota_abort(ota_handle);
        return false;
    }
#else
    return true;
#endif
}
