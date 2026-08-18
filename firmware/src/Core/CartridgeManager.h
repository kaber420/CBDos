#pragma once
#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <cstddef>

#ifdef ARDUINO
#include <esp_partition.h>
#include <esp_ota_ops.h>
#else
typedef int esp_partition_subtype_t;
#define ESP_PARTITION_SUBTYPE_APP_OTA_0 0x10
#define ESP_PARTITION_SUBTYPE_APP_OTA_1 0x11
#define ESP_PARTITION_SUBTYPE_APP_OTA_2 0x12
#endif

struct CartridgeSlotInfo {
    bool isInstalled = false;
    std::string projectName = "Vacio";
    std::string version = "";
    std::string compileDate = "";
    std::string compileTime = "";
    size_t partitionSize = 0;
    esp_partition_subtype_t subtype;
};

class CartridgeManager {
public:
    // Obtener informacion de un slot (OTA_1 = Slot Grande 4MB, OTA_2 = Slot Pequeño 2MB)
    static CartridgeSlotInfo getSlotInfo(esp_partition_subtype_t subtype);

    // Verificar si un slot tiene un firmware valido instalado
    static bool isSlotInstalled(esp_partition_subtype_t subtype);

    // Reiniciar y arrancar el slot especificado
    static bool bootSlot(esp_partition_subtype_t subtype);

    // Listar todos los archivos .bin disponibles en la MicroSD
    static std::vector<std::string> listBinFilesOnSD(const std::string& directory = "/cartridges");

    // Flashear un archivo binario desde la SD a la particion OTA especificada
    static bool flashFromSD(const std::string& sdPath, 
                            esp_partition_subtype_t targetSlot, 
                            std::function<void(size_t written, size_t total)> progressCb = nullptr);
};
