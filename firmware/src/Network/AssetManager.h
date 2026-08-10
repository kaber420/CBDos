#pragma once

#include <Arduino.h>
#include <vector>
#include <string>

class AssetManager {
public:
    static AssetManager& getInstance() {
        static AssetManager instance;
        return instance;
    }

    // Inicializa la tarea de fondo de descargas
    void init();

    // Comprueba si un asset está cacheado con el tamaño solicitado
    bool isAssetCached(const std::string& hash, int width, int height);

    // Obtiene la ruta completa "A:/..." del asset en SD independientemente de la variante de nombre
    std::string getAssetPath(const std::string& hash, int width, int height);

    // Encola una descarga para un hash y tamaño si no está en progreso
    void queueDownload(const std::string& hash, int width, int height);

    // Procesa la cola de descargas en segundo plano (Core 0)
    void processQueue();

    // Consulta y limpia la bandera de refresco de UI de forma segura (Core 1)
    bool checkAndClearRefreshFlag() {
        if (pendingRefresh) {
            pendingRefresh = false;
            return true;
        }
        return false;
    }

private:
    AssetManager() = default;
    ~AssetManager() = default;

    volatile bool pendingRefresh = false;

    struct DownloadTask {
        std::string hash;
        int width;
        int height;
    };

    std::vector<DownloadTask> downloadQueue;
    bool isDownloading = false;
    uint32_t lastProcessTime = 0;

    bool downloadAsset(const DownloadTask& task);
};
