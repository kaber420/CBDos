#include "cbdos/storage.hpp"
#include <esp_flash.h>
#include <esp_partition.h>
#include <esp_spiffs.h>
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
static const char* SD_MOUNT_POINT = "/sdcard";
static const char* SPIFFS_MOUNT_POINT = "/spiffs";
static const char* SPIFFS_PARTITION_LABEL = "spiffs";

#define BOARD_DISP_SD_LDO_CH 4

namespace cbdos {
namespace storage {

static bool s_sdMounted = false;
static bool s_spiffsMounted = false;
static sdmmc_card_t* s_cardHandle = nullptr;
static esp_ldo_channel_handle_t s_sdLdoHandle = nullptr;

static void ensureLdoPower() {
    if (s_sdLdoHandle) {
        esp_ldo_release_channel(s_sdLdoHandle);
        s_sdLdoHandle = nullptr;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    esp_ldo_channel_config_t sd_ldo_cfg = {};
    sd_ldo_cfg.chan_id = BOARD_DISP_SD_LDO_CH;
    sd_ldo_cfg.voltage_mv = 3300;
    esp_err_t ret = esp_ldo_acquire_channel(&sd_ldo_cfg, &s_sdLdoHandle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "LDO VO4 (3.3V) para MicroSD energizado (Power-Cycle OK)");
    } else {
        ESP_LOGW(TAG, "Aviso LDO VO4: %s", esp_err_to_name(ret));
    }
    vTaskDelay(pdMS_TO_TICKS(100));
}

static bool mountSpiffs() {
    if (s_spiffsMounted) {
        return true;
    }

    ESP_LOGI(TAG, "Montando particion Flash interna SPIFFS ('%s') en %s...", SPIFFS_PARTITION_LABEL, SPIFFS_MOUNT_POINT);

    esp_vfs_spiffs_conf_t conf = {
        .base_path = SPIFFS_MOUNT_POINT,
        .partition_label = SPIFFS_PARTITION_LABEL,
        .max_files = 8,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Fallo al montar o formatear SPIFFS");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "No se encontro la particion SPIFFS ('%s')", SPIFFS_PARTITION_LABEL);
        } else {
            ESP_LOGE(TAG, "Error SPIFFS: %s", esp_err_to_name(ret));
        }
        s_spiffsMounted = false;
        return false;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(SPIFFS_PARTITION_LABEL, &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "SPIFFS montado OK. Tamano: %d KB, Usado: %d KB", (int)(total / 1024), (int)(used / 1024));
    } else {
        ESP_LOGI(TAG, "SPIFFS montado OK.");
    }

    s_spiffsMounted = true;
    return true;
}

bool mountSd() {
    if (s_sdMounted) {
        ESP_LOGI(TAG, "MicroSD ya se encuentra montada en %s", SD_MOUNT_POINT);
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
    host.slot = SDMMC_HOST_SLOT_0;
    host.max_freq_khz = SDMMC_FREQ_DEFAULT; // 20 MHz estándar

    // Slot 0 nativo en ESP32-P4 (GPIO 39-44)
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 4;
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
    slot_config.clk = GPIO_NUM_43;
    slot_config.cmd = GPIO_NUM_44;
    slot_config.d0 = GPIO_NUM_39;
    slot_config.d1 = GPIO_NUM_40;
    slot_config.d2 = GPIO_NUM_41;
    slot_config.d3 = GPIO_NUM_42;

    ESP_LOGI(TAG, "Intentando montar MicroSD (Slot 0, 4-bit) en %s...", SD_MOUNT_POINT);
    esp_err_t ret = esp_vfs_fat_sdmmc_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &s_cardHandle);

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Fallo montaje 4-bit (%s), reintentando en modo 1-bit...", esp_err_to_name(ret));
        slot_config.width = 1;
        ret = esp_vfs_fat_sdmmc_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &s_cardHandle);
    }

    if (ret == ESP_OK && s_cardHandle != nullptr) {
        s_sdMounted = true;
        ESP_LOGI(TAG, "MicroSD montada exitosamente en %s!", SD_MOUNT_POINT);
        sdmmc_card_print_info(stdout, s_cardHandle);
        return true;
    } else {
        s_sdMounted = false;
        s_cardHandle = nullptr;
        ESP_LOGW(TAG, "MicroSD no detectada o no montada: %s", esp_err_to_name(ret));
        return false;
    }
}

bool unmountSd() {
    if (!s_sdMounted) {
        return true;
    }

    ESP_LOGI(TAG, "Desmontando MicroSD de %s...", SD_MOUNT_POINT);
    esp_err_t ret = esp_vfs_fat_sdcard_unmount(SD_MOUNT_POINT, s_cardHandle);
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
    ESP_LOGI(TAG, "Inicializando subsistema de almacenamiento (Flash SPIFFS + MicroSD)...");
    bool spiffsOk = mountSpiffs();
    mountSd(); // Intento no bloqueante de montaje MicroSD
    return spiffsOk || s_sdMounted;
}

bool isSdMounted() {
    return s_sdMounted;
}

bool isFlashMounted() {
    return s_spiffsMounted;
}

StorageStats getFlashStats() {
    StorageStats stats = { s_spiffsMounted, 0, 0, 0, "Flash Interna (SPIFFS)" };
    if (s_spiffsMounted) {
        size_t total = 0, used = 0;
        if (esp_spiffs_info(SPIFFS_PARTITION_LABEL, &total, &used) == ESP_OK) {
            stats.totalBytes = total;
            stats.usedBytes = used;
            stats.freeBytes = (total > used) ? (total - used) : 0;
            return stats;
        }
    }

    // Fallback general del chip
    uint32_t flashSize = 0;
    if (esp_flash_get_size(esp_flash_default_chip, &flashSize) == ESP_OK) {
        stats.totalBytes = flashSize;
    } else {
        stats.totalBytes = 16 * 1024 * 1024;
    }
    stats.usedBytes = 4 * 1024 * 1024;
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
    esp_err_t ret = esp_vfs_fat_info(SD_MOUNT_POINT, &totalBytes, &freeBytes);
    if (ret == ESP_OK) {
        stats.totalBytes = totalBytes;
        stats.freeBytes = freeBytes;
        stats.usedBytes = (totalBytes > freeBytes) ? (totalBytes - freeBytes) : 0;
    } else if (s_cardHandle != nullptr) {
        stats.totalBytes = (uint64_t)s_cardHandle->csd.capacity * s_cardHandle->csd.sector_size;
        stats.freeBytes = stats.totalBytes;
        stats.usedBytes = 0;
    }

    return stats;
}

StorageStats getUsbStats() {
    return StorageStats{ false, 0, 0, 0, "Puerto USB (OTG / HS)" };
}

static std::string normalizePath(const char* path, bool& isSdTarget) {
    if (!path || strlen(path) == 0) {
        isSdTarget = false;
        return SPIFFS_MOUNT_POINT;
    }

    std::string p = path;

    // Rutas con alias de MicroSD
    if (p.rfind("S:/", 0) == 0 || p.rfind("A:/", 0) == 0) {
        isSdTarget = true;
        return std::string(SD_MOUNT_POINT) + p.substr(2);
    }
    if (p.rfind("/sdcard", 0) == 0) {
        isSdTarget = true;
        return p;
    }
    if (p.rfind("/sd", 0) == 0 && (p.length() == 3 || p[3] == '/')) {
        isSdTarget = true;
        return std::string(SD_MOUNT_POINT) + p.substr(3);
    }

    // Rutas con prefijo explícito de Flash SPIFFS
    if (p.rfind("/spiffs", 0) == 0) {
        isSdTarget = false;
        return p;
    }
    if (p.rfind("/flash", 0) == 0 && (p.length() == 6 || p[6] == '/')) {
        isSdTarget = false;
        return std::string(SPIFFS_MOUNT_POINT) + p.substr(6);
    }

    // Rutas relativas o sin prefijo conocido -> Enrutamiento predeterminado a Flash Interna SPIFFS
    isSdTarget = false;
    if (p[0] == '/') {
        return std::string(SPIFFS_MOUNT_POINT) + p;
    }
    return std::string(SPIFFS_MOUNT_POINT) + "/" + p;
}

std::vector<FileEntry> listDir(const char* path) {
    std::vector<FileEntry> result;
    bool isSd = false;
    std::string fullPath = normalizePath(path, isSd);

    if (isSd && !s_sdMounted) {
        return result;
    }
    if (!isSd && !s_spiffsMounted) {
        if (!mountSpiffs()) return result;
    }

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
    bool isSd = false;
    std::string fullPath = normalizePath(path, isSd);

    if (isSd && !s_sdMounted) return false;
    if (!isSd && !s_spiffsMounted) return false;

    struct stat st;
    return (stat(fullPath.c_str(), &st) == 0);
}

std::string readFile(const char* path) {
    std::string content = "";
    bool isSd = false;
    std::string fullPath = normalizePath(path, isSd);

    if (isSd) {
        if (!s_sdMounted && !mountSd()) return content;
    } else {
        if (!s_spiffsMounted && !mountSpiffs()) return content;
    }

    FILE* f = fopen(fullPath.c_str(), "rb");
    if (!f) {
        ESP_LOGW(TAG, "readFile: No se pudo abrir %s", fullPath.c_str());
        return content;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz > 0) {
        content.resize(sz);
        size_t bytesRead = fread(&content[0], 1, sz, f);
        content.resize(bytesRead);
    }
    fclose(f);
    return content;
}

bool makeDir(const char* path) {
    bool isSd = false;
    std::string fullPath = normalizePath(path, isSd);

    if (isSd) {
        if (!s_sdMounted && !mountSd()) return false;
    } else {
        if (!s_spiffsMounted && !mountSpiffs()) return false;
        return true;
    }

    struct stat st;
    if (stat(fullPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        return true;
    }
    return (mkdir(fullPath.c_str(), 0777) == 0);
}

bool writeFile(const char* path, const std::string& content) {
    bool isSd = false;
    std::string fullPath = normalizePath(path, isSd);

    if (isSd) {
        if (!s_sdMounted && !mountSd()) return false;

        // Asegurar directorios padres en FAT
        size_t lastSlash = fullPath.rfind('/');
        if (lastSlash != std::string::npos && lastSlash > 0) {
            for (size_t i = 1; i <= lastSlash; i++) {
                if (fullPath[i] == '/' || i == lastSlash) {
                    std::string currentDir = fullPath.substr(0, i);
                    struct stat st;
                    if (stat(currentDir.c_str(), &st) != 0) {
                        mkdir(currentDir.c_str(), 0777);
                    }
                }
            }
        }
    } else {
        if (!s_spiffsMounted && !mountSpiffs()) return false;
    }

    FILE* f = fopen(fullPath.c_str(), "wb");
    if (!f) {
        ESP_LOGE(TAG, "writeFile: Error al crear %s", fullPath.c_str());
        return false;
    }

    bool ok = true;
    if (!content.empty()) {
        size_t written = fwrite(content.data(), 1, content.size(), f);
        ok = (written == content.size());
    }
    fclose(f);
    return ok;
}

bool deleteFile(const char* path) {
    bool isSd = false;
    std::string fullPath = normalizePath(path, isSd);

    if (isSd && !s_sdMounted) return false;
    if (!isSd && !s_spiffsMounted) return false;

    return (remove(fullPath.c_str()) == 0);
}

bool copyFile(const char* srcPath, const char* dstPath) {
    std::string data = readFile(srcPath);
    if (data.empty() && !fileExists(srcPath)) {
        ESP_LOGE(TAG, "copyFile: Origen no encontrado: %s", srcPath);
        return false;
    }
    return writeFile(dstPath, data);
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
