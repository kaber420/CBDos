#include "cbdos/cartridge.hpp"
#include "cbdos/storage.hpp"
#include "cbdos/system.hpp"

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <dirent.h>
#include <sys/stat.h>

#ifdef ESP_PLATFORM
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#endif

static const char* TAG = "CartridgeMgr";

namespace cbdos {
namespace cartridge {

CartridgeSlotInfo CartridgeManager::getSlotInfo(esp_partition_subtype_t subtype) {
    CartridgeSlotInfo info;
    info.subtype = subtype;
    info.isInstalled = false;

#ifdef ESP_PLATFORM
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
        info.projectName = "Ranura Vacía";
    }
#else
    info.isInstalled = (subtype == ESP_PARTITION_SUBTYPE_APP_OTA_1);
    info.projectName = (subtype == ESP_PARTITION_SUBTYPE_APP_OTA_1) ? "DOOM Classic" : "Ranura Vacía";
    info.version = "1.0.0";
    info.partitionSize = 4 * 1024 * 1024;
#endif

    return info;
}

bool CartridgeManager::isSlotInstalled(esp_partition_subtype_t subtype) {
    return getSlotInfo(subtype).isInstalled;
}

bool CartridgeManager::bootSlot(esp_partition_subtype_t subtype) {
#ifdef ESP_PLATFORM
    const esp_partition_t* part = esp_partition_find_first(ESP_PARTITION_TYPE_APP, subtype, NULL);
    if (!part) {
        cbdos::system::log(cbdos::system::LogLevel::Error, TAG, "Particion 0x%x no encontrada", subtype);
        return false;
    }

    esp_app_desc_t app_desc;
    if (esp_ota_get_partition_description(part, &app_desc) != ESP_OK || app_desc.magic_word != ESP_APP_DESC_MAGIC_WORD) {
        cbdos::system::log(cbdos::system::LogLevel::Warn, TAG, "Particion vacia o invalida en 0x%x", subtype);
        return false;
    }

    cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "Arrancando %s v%s...", app_desc.project_name, app_desc.version);
    esp_err_t err = esp_ota_set_boot_partition(part);
    if (err == ESP_OK) {
        cbdos::system::sleepMs(300);
        cbdos::system::restart();
        return true;
    } else {
        cbdos::system::log(cbdos::system::LogLevel::Error, TAG, "Error al configurar arranque: %s", esp_err_to_name(err));
        return false;
    }
#else
    cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "Simulación: Boot slot 0x%x", subtype);
    return true;
#endif
}

std::vector<std::string> CartridgeManager::listBinFilesOnSD(const std::string& directory) {
    std::vector<std::string> list;

    // Directorios a escanear en orden de preferencia
    std::vector<std::string> searchDirs;
    if (!directory.empty()) {
        searchDirs.push_back(directory);
    }
    searchDirs.push_back("/sdcard/cartridges");
    searchDirs.push_back("/sdcard/cartuchos");
    searchDirs.push_back("/sdcard");
    searchDirs.push_back("/cartridges");
    searchDirs.push_back("/cartuchos");
    searchDirs.push_back("/sd/cartridges");
    searchDirs.push_back("/sd");

    for (const auto& dir : searchDirs) {
        auto entries = cbdos::storage::listDir(dir.c_str());
        for (const auto& entry : entries) {
            if (entry.isDirectory) continue;
            std::string name = entry.name;
            std::string lowerName = name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
            if (lowerName.length() > 4 && lowerName.substr(lowerName.length() - 4) == ".bin") {
                std::string fullPath = dir;
                if (fullPath.empty() || fullPath.back() != '/') fullPath += "/";
                fullPath += name;
                
                // Evitar duplicados
                if (std::find(list.begin(), list.end(), fullPath) == list.end()) {
                    list.push_back(fullPath);
                }
            }
        }
        // Si encontramos archivos en la carpeta de cartuchos específica, priorizamos esa
        if (!list.empty() && (dir.find("cartridges") != std::string::npos || dir.find("cartuchos") != std::string::npos)) {
            break;
        }
    }

    return list;
}

bool CartridgeManager::flashFromSD(const std::string& sdPath, 
                                   esp_partition_subtype_t targetSlot, 
                                   std::function<void(size_t written, size_t total)> progressCb) {
#ifdef ESP_PLATFORM
    const esp_partition_t* part = esp_partition_find_first(ESP_PARTITION_TYPE_APP, targetSlot, NULL);
    if (!part) {
        cbdos::system::log(cbdos::system::LogLevel::Error, TAG, "Particion de destino no encontrada en tabla");
        return false;
    }

    FILE* f = fopen(sdPath.c_str(), "rb");
    if (!f) {
        // Intentar con prefijo /sdcard si no lo tiene
        std::string altPath = sdPath;
        if (altPath.rfind("/sdcard", 0) != 0) {
            altPath = (altPath[0] == '/') ? ("/sdcard" + altPath) : ("/sdcard/" + altPath);
            f = fopen(altPath.c_str(), "rb");
        }
    }

    if (!f) {
        cbdos::system::log(cbdos::system::LogLevel::Error, TAG, "Error al abrir %s", sdPath.c_str());
        return false;
    }

    struct stat st;
    if (stat(sdPath.c_str(), &st) != 0 || st.st_size == 0) {
        fclose(f);
        cbdos::system::log(cbdos::system::LogLevel::Error, TAG, "Archivo invalido o vacio: %s", sdPath.c_str());
        return false;
    }

    size_t totalBytes = st.st_size;
    if (totalBytes > part->size) {
        fclose(f);
        cbdos::system::log(cbdos::system::LogLevel::Error, TAG, "Tamano (%u bytes) excede particion (%u bytes)", 
                           (unsigned)totalBytes, (unsigned)part->size);
        return false;
    }

    cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "Iniciando OTA en 0x%06lx (%u bytes)...", 
                       (unsigned long)part->address, (unsigned)totalBytes);

    esp_ota_handle_t ota_handle = 0;
    esp_err_t err = esp_ota_begin(part, totalBytes, &ota_handle);
    if (err != ESP_OK) {
        fclose(f);
        cbdos::system::log(cbdos::system::LogLevel::Error, TAG, "esp_ota_begin fallo: %s", esp_err_to_name(err));
        return false;
    }

    const size_t CHUNK_SIZE = 4096;
    uint8_t* buffer = (uint8_t*)malloc(CHUNK_SIZE);
    if (!buffer) {
        esp_ota_abort(ota_handle);
        fclose(f);
        cbdos::system::log(cbdos::system::LogLevel::Error, TAG, "Sin memoria RAM para buffer de flasheo");
        return false;
    }

    size_t bytesWritten = 0;
    bool success = true;

    while (bytesWritten < totalBytes) {
        size_t toRead = std::min((size_t)(totalBytes - bytesWritten), CHUNK_SIZE);
        size_t bytesRead = fread(buffer, 1, toRead, f);
        if (bytesRead == 0) {
            success = false;
            break;
        }

        err = esp_ota_write(ota_handle, (const void*)buffer, bytesRead);
        if (err != ESP_OK) {
            cbdos::system::log(cbdos::system::LogLevel::Error, TAG, "esp_ota_write fallo: %s", esp_err_to_name(err));
            success = false;
            break;
        }

        bytesWritten += bytesRead;
        if (progressCb) {
            progressCb(bytesWritten, totalBytes);
        }

        // Ceder tiempo al planificador y al bus DMA de pantalla (MIPI-DPI a 60 FPS)
        cbdos::system::sleepMs(2);
    }

    free(buffer);
    fclose(f);

    if (success && bytesWritten == totalBytes) {
        err = esp_ota_end(ota_handle);
        if (err != ESP_OK) {
            cbdos::system::log(cbdos::system::LogLevel::Error, TAG, "esp_ota_end fallo: %s", esp_err_to_name(err));
            return false;
        }
        cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "Flasheo completado y validado (%u bytes)", (unsigned)bytesWritten);
        return true;
    } else {
        esp_ota_abort(ota_handle);
        return false;
    }
#else
    cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "Simulación: Flasheo desde SD %s a slot 0x%x", sdPath.c_str(), targetSlot);
    return true;
#endif
}

} // namespace cartridge
} // namespace cbdos
