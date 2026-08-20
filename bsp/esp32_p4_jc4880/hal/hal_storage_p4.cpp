#include "cbdos/storage.hpp"
#include <esp_flash.h>
#include <esp_partition.h>
#include <esp_log.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>
#include <driver/sdmmc_host.h>
#include <driver/sdmmc_types.h>
#include <esp_ldo_regulator.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <cstring>

static const char* TAG = "HAL_Storage_P4";
static const char* MOUNT_POINT = "/sdcard";

#define BOARD_DISP_SD_LDO_CH 4

namespace cbdos {
namespace storage {

static bool s_sdMounted = false;
static sdmmc_card_t* s_cardHandle = nullptr;
static esp_ldo_channel_handle_t s_sdLdoHandle = nullptr;

static void ensureLdoPower() {
    if (!s_sdLdoHandle) {
        esp_ldo_channel_config_t sd_ldo_cfg = {};
        sd_ldo_cfg.chan_id = BOARD_DISP_SD_LDO_CH;
        sd_ldo_cfg.voltage_mv = 3300;
        esp_err_t ret = esp_ldo_acquire_channel(&sd_ldo_cfg, &s_sdLdoHandle);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "LDO VO4 (3.3V) para MicroSD energizado correctamente");
        } else {
            ESP_LOGW(TAG, "Aviso: no se pudo adquirir canal LDO %d (puede estar activo): %s", 
                     BOARD_DISP_SD_LDO_CH, esp_err_to_name(ret));
        }
    }
}

bool mountSd() {
    if (s_sdMounted) {
        ESP_LOGI(TAG, "MicroSD ya se encuentra montada en %s", MOUNT_POINT);
        return true;
    }

    ensureLdoPower();

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 8,
        .allocation_unit_size = 16 * 1024,
        .disk_status_check_enable = false
    };

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_DEFAULT; // 20 MHz estándar para compatibilidad amplia

    // Slot 0 nativo en ESP32-P4
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 4;
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    ESP_LOGI(TAG, "Intentando montar MicroSD (Slot 0, 4-bit) en %s...", MOUNT_POINT);
    esp_err_t ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &s_cardHandle);

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Fallo montaje 4-bit (%s), reintentando en modo 1-bit...", esp_err_to_name(ret));
        slot_config.width = 1;
        ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &s_cardHandle);
    }

    if (ret == ESP_OK && s_cardHandle != nullptr) {
        s_sdMounted = true;
        ESP_LOGI(TAG, "MicroSD montada exitosamente en %s!", MOUNT_POINT);
        sdmmc_card_print_info(stdout, s_cardHandle);
        return true;
    } else {
        s_sdMounted = false;
        s_cardHandle = nullptr;
        ESP_LOGW(TAG, "No se pudo montar la tarjeta MicroSD: %s", esp_err_to_name(ret));
        return false;
    }
}

bool unmountSd() {
    if (!s_sdMounted) {
        return true;
    }

    ESP_LOGI(TAG, "Desmontando MicroSD de %s...", MOUNT_POINT);
    esp_err_t ret = esp_vfs_fat_sdcard_unmount(MOUNT_POINT, s_cardHandle);
    s_sdMounted = false;
    s_cardHandle = nullptr;

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "MicroSD desmontada de forma segura.");
        return true;
    } else {
        ESP_LOGE(TAG, "Error al desmontar MicroSD: %s", esp_err_to_name(ret));
        return false;
    }
}

bool init() {
    ESP_LOGI(TAG, "Inicializando subsistema de almacenamiento...");
    return mountSd();
}

bool isSdMounted() {
    return s_sdMounted;
}

StorageStats getFlashStats() {
    StorageStats stats = { true, 0, 0, 0, "Flash Interna (SPI NOR)" };
    uint32_t flashSize = 0;
    if (esp_flash_get_size(esp_flash_default_chip, &flashSize) == ESP_OK) {
        stats.totalBytes = flashSize;
    } else {
        stats.totalBytes = 16 * 1024 * 1024; // 16MB por defecto
    }

    // Estimación razonable según particiones y firmware
    stats.usedBytes = 4718592; // 4.5 MB
    stats.freeBytes = (stats.totalBytes > stats.usedBytes) ? (stats.totalBytes - stats.usedBytes) : 0;
    return stats;
}

StorageStats getSdCardStats() {
    StorageStats stats = { s_sdMounted, 0, 0, 0, "Tarjeta MicroSD" };
    if (!s_sdMounted) {
        return stats;
    }

    uint64_t totalBytes = 0;
    uint64_t freeBytes = 0;
    esp_err_t ret = esp_vfs_fat_info(MOUNT_POINT, &totalBytes, &freeBytes);
    if (ret == ESP_OK) {
        stats.totalBytes = totalBytes;
        stats.freeBytes = freeBytes;
        stats.usedBytes = (totalBytes > freeBytes) ? (totalBytes - freeBytes) : 0;
    } else if (s_cardHandle != nullptr) {
        // Fallback usando capacidad del descriptor del driver SDMMC
        stats.totalBytes = (uint64_t)s_cardHandle->csd.capacity * s_cardHandle->csd.sector_size;
        stats.freeBytes = stats.totalBytes;
        stats.usedBytes = 0;
    }

    return stats;
}

StorageStats getUsbStats() {
    return StorageStats{ false, 0, 0, 0, "Puerto USB (OTG / HS)" };
}

static std::string normalizePath(const char* path) {
    if (!path || strlen(path) == 0) return MOUNT_POINT;
    std::string p = path;
    if (p.rfind("S:/", 0) == 0 || p.rfind("A:/", 0) == 0) {
        p = std::string(MOUNT_POINT) + p.substr(2);
    } else if (p[0] != '/') {
        p = std::string(MOUNT_POINT) + "/" + p;
    }
    return p;
}

std::vector<FileEntry> listDir(const char* path) {
    std::vector<FileEntry> result;
    if (!s_sdMounted) {
        return result;
    }

    std::string fullPath = normalizePath(path);
    DIR* dir = opendir(fullPath.c_str());
    if (!dir) {
        ESP_LOGW(TAG, "No se pudo abrir directorio: %s", fullPath.c_str());
        return result;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        FileEntry item;
        item.name = entry->d_name;
        item.size = 0;
        item.isDirectory = (entry->d_type == DT_DIR);

        std::string itemPath = fullPath;
        if (itemPath.back() != '/') itemPath += '/';
        itemPath += entry->d_name;

        struct stat st;
        if (stat(itemPath.c_str(), &st) == 0) {
            item.size = st.st_size;
            if (S_ISDIR(st.st_mode)) {
                item.isDirectory = true;
            }
        }

        result.push_back(item);
    }

    closedir(dir);
    return result;
}

bool fileExists(const char* path) {
    if (!s_sdMounted) return false;
    std::string fullPath = normalizePath(path);
    struct stat st;
    return (stat(fullPath.c_str(), &st) == 0);
}

size_t getFreeBytes(StorageType type) {
    if (type == StorageType::InternalFlash) return (size_t)getFlashStats().freeBytes;
    if (type == StorageType::SdCard) return (size_t)getSdCardStats().freeBytes;
    return 0;
}

size_t getTotalBytes(StorageType type) {
    if (type == StorageType::InternalFlash) return (size_t)getFlashStats().totalBytes;
    if (type == StorageType::SdCard) return (size_t)getSdCardStats().totalBytes;
    return 0;
}

} // namespace storage
} // namespace cbdos
