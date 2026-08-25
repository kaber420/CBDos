#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>
#include "mesh_types.hpp"

namespace cbdos {
namespace mesh {

using MeshRawRecvCallback = std::function<void(const uint8_t* src_mac, const uint8_t* data, size_t len, int8_t rssi)>;

/**
 * @brief Interfaz abstracta para la capa de transporte físico de radio (ESP-NOW, LoRa, FLRC, Serial)
 * Permite que MeshEngine sea 100% agnóstico de la plataforma y del hardware de radio.
 */
class IMeshTransport {
public:
    virtual ~IMeshTransport() = default;

    /**
     * @brief Inicializa el hardware de radio en canal especificado
     */
    virtual bool init(uint8_t channel = 1) = 0;

    /**
     * @brief Detiene el hardware de radio o entra en bajo consumo
     */
    virtual void stop() = 0;

    /**
     * @brief Comprueba si el transporte está listo para transmitir/recibir
     */
    virtual bool isReady() const = 0;

    /**
     * @brief Envía una trama cruda (máx 250 bytes en ESP-NOW) a una dirección MAC o Broadcast
     * @param dest_mac Puntero a 6 bytes de MAC (o nullptr para broadcast FF:FF:FF:FF:FF:FF)
     * @param data Datos crudos
     * @param len Longitud en bytes
     */
    virtual bool sendRaw(const uint8_t* dest_mac, const uint8_t* data, size_t len) = 0;

    /**
     * @brief Registra el callback invocado cuando la radio recibe una trama del aire
     */
    virtual void setRecvCallback(MeshRawRecvCallback cb) = 0;

    /**
     * @brief Obtiene la dirección MAC propia del hardware
     */
    virtual bool getMacAddress(uint8_t out_mac[6]) = 0;

    /**
     * @brief Obtiene el canal de radio actual
     */
    virtual uint8_t getChannel() = 0;

    /**
     * @brief Cambia el canal de radio activo
     */
    virtual bool setChannel(uint8_t channel) = 0;

    /**
     * @brief Configura el modo de modulación de radio (Normal, LR, etc.)
     */
    virtual bool setRadioMode(RadioMode mode) { return true; }
    virtual RadioMode getRadioMode() const { return RadioMode::Auto; }
};

} // namespace mesh
} // namespace cbdos
