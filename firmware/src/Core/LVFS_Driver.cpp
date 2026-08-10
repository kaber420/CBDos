#include "LVFS_Driver.h"
#include <Arduino.h>
#include <SD.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// ═══════════════════════════════════════════════════════════════════════
// Mutex SPI compartido — protege el acceso al bus SPI de la MicroSD.
// ═══════════════════════════════════════════════════════════════════════
static SemaphoreHandle_t s_spiMutex = NULL;

void lv_fs_set_spi_mutex(void* mutex) {
    s_spiMutex = (SemaphoreHandle_t)mutex;
}

static inline void spi_lock() {
    if (s_spiMutex) xSemaphoreTake(s_spiMutex, portMAX_DELAY);
}

static inline void spi_unlock() {
    if (s_spiMutex) xSemaphoreGive(s_spiMutex);
}

void lv_fs_spi_lock() {
    spi_lock();
}

void lv_fs_spi_unlock() {
    spi_unlock();
}

static void * fs_open(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode) {
    Serial.printf("[LVFS] Intentando abrir archivo: %s\n", path);
    String fullPath = String(path);
    if (!fullPath.startsWith("/")) {
        fullPath = "/" + fullPath;
    }
    Serial.printf("[LVFS] Ruta formateada para SD: %s\n", fullPath.c_str());
    const char * arduinoMode;
    if (mode == LV_FS_MODE_WR) {
        arduinoMode = FILE_WRITE;
    } else if (mode == LV_FS_MODE_RD) {
        arduinoMode = FILE_READ;
    } else if (mode == (LV_FS_MODE_WR | LV_FS_MODE_RD)) {
        arduinoMode = FILE_WRITE;
    } else {
        Serial.println("[LVFS] Modo de apertura invalido");
        return NULL;
    }
    
    spi_lock();
    File f = SD.open(fullPath, arduinoMode);
    if (!f) {
        spi_unlock();
        Serial.printf("[LVFS] ERROR: No se pudo abrir el archivo %s en SD\n", fullPath.c_str());
        return NULL;
    }
    
    File * fPtr = new File(f);
    spi_unlock();
    Serial.println("[LVFS] Archivo abierto exitosamente");
    return (void *)fPtr;
}

static lv_fs_res_t fs_close(lv_fs_drv_t * drv, void * file_p) {
    File * fPtr = (File *)file_p;
    if (fPtr) {
        spi_lock();
        fPtr->close();
        delete fPtr;
        spi_unlock();
    }
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_read(lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br) {
    File * fPtr = (File *)file_p;
    if (!fPtr) return LV_FS_RES_UNKNOWN;
    spi_lock();
    *br = fPtr->read((uint8_t *)buf, btr);
    spi_unlock();
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_write(lv_fs_drv_t * drv, void * file_p, const void * buf, uint32_t btw, uint32_t * bw) {
    File * fPtr = (File *)file_p;
    if (!fPtr) return LV_FS_RES_UNKNOWN;
    spi_lock();
    *bw = fPtr->write((const uint8_t *)buf, btw);
    spi_unlock();
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_seek(lv_fs_drv_t * drv, void * file_p, uint32_t pos, lv_fs_whence_t whence) {
    File * fPtr = (File *)file_p;
    if (!fPtr) return LV_FS_RES_UNKNOWN;
    
    spi_lock();
    bool ok = false;
    if (whence == LV_FS_SEEK_SET) {
        ok = fPtr->seek(pos, SeekSet);
    } else if (whence == LV_FS_SEEK_CUR) {
        ok = fPtr->seek(fPtr->position() + (int32_t)pos, SeekSet);
    } else if (whence == LV_FS_SEEK_END) {
        ok = fPtr->seek(fPtr->size() + (int32_t)pos, SeekSet);
    }
    spi_unlock();
    return ok ? LV_FS_RES_OK : LV_FS_RES_UNKNOWN;
}

static lv_fs_res_t fs_tell(lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p) {
    File * fPtr = (File *)file_p;
    if (!fPtr) return LV_FS_RES_UNKNOWN;
    spi_lock();
    *pos_p = fPtr->position();
    spi_unlock();
    return LV_FS_RES_OK;
}

void lv_fs_if_init() {
    static lv_fs_drv_t fs_drv;
    lv_fs_drv_init(&fs_drv);
    
    fs_drv.letter = 'A';
    fs_drv.open_cb = fs_open;
    fs_drv.close_cb = fs_close;
    fs_drv.read_cb = fs_read;
    fs_drv.write_cb = fs_write;
    fs_drv.seek_cb = fs_seek;
    fs_drv.tell_cb = fs_tell;
    
    lv_fs_drv_register(&fs_drv);
    Serial.println("[LVFS] Driver de File System VFS inicializado para SD (Unidad A:)");
}
