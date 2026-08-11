# Especificación Técnica: Web Installer & Provisioner (WebSerial + eFuse MAC)

Este documento detalla la arquitectura del **Instalador y Aprovisionador Web** para el ecosistema **espOS32**, permitiendo flashear el firmware C++ y generar el archivo `.enc` cifrado directamente desde el navegador web mediante **WebSerial API** y **esptool-js**.

---

## 1. Arquitectura del Sistema

```
┌────────────────────────────────────────────────────────────────────────┐
│ Cliente Web (Chrome / Edge / Opera / Android USB-C OTG)                │
│ - Interfaz WebSerial en JavaScript (`esptool-js`).                      │
│ - Lee el eFuse MAC (64-bit) grabado en el silicio del ESP32-S3 por USB.  │
└──────────────────────────────────┬─────────────────────────────────────┘
                                   │
                                   │  HTTP POST /api/v1/provision/generate
                                   ▼
┌────────────────────────────────────────────────────────────────────────┐
│ Gateway Servidor (Python)                                              │
│ 1. Recibe el `eFuse MAC` validado por USB.                             │
│ 2. Deriva la clave de cifrado: Key = HMAC-SHA256(Secret, eFuseMAC).     │
│ 3. Genera la configuración `.enc` atada al hardware y la firma.       │
└──────────────────────────────────┬─────────────────────────────────────┘
                                   │
                                   │  Responde binario .enc cifrado
                                   ▼
┌────────────────────────────────────────────────────────────────────────┐
│ ESP32-S3 (Flasheo Directo WebSerial)                                  │
│ - Flashea `firmware.bin` en `0x10000`.                                │
│ - Escribe `tablehub.enc` cifrado en la partición LittleFS/NVS.        │
│ - Reinicio automático en modo aprovisionado listo para usar.           │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Flujo de Aprovisionamiento en 1 Solo Clic

1. **Conexión USB:** El usuario conecta el ESP32-S3 mediante cable USB-C a la PC o teléfono Android y abre `http://localhost:8765/installer`.
2. **Detección de Hardware:** El script `esptool-js` establece la sesión serie a 115200 baudios (o 921600 baudios para flasheo rápido) y consulta el `eFuse MAC` del microcontrolador.
3. **Petición de Certificado al Gateway:**
   ```json
   POST /api/v1/provision/generate
   {
     "efuse_mac": "A4C138E9F2B0",
     "device_model": "JC3248W535_S3",
     "user_token": "opcional_si_registrado"
   }
   ```
4. **Cifrado Hardware-Bound:** El servidor cifra el payload de configuración usando una clave simétrica AES derivando el `eFuse MAC`.
5. **Flasheo por WebSerial:** La web escribe el archivo binario del firmware y la partición `.enc` directamente a la memoria Flash externa de 16MB del ESP32-S3.

---

## 3. Endpoints REST Implementados en la Pasarela

| Método | Ruta | Parámetros | Descripción |
| :--- | :--- | :--- | :--- |
| `GET` | `/installer` | N/A | Sirve la interfaz gráfica WebSerial HTML5 |
| `POST` | `/api/v1/provision/generate` | `efuse_mac`, `user_pin` | Genera y retorna el archivo `.enc` cifrado por hardware |

---

## 4. Ventajas de Seguridad y UX

* **Cero instalación de software:** No requiere Python, PlatformIO ni controladores adicionales en la PC del usuario final.
* **Cero Clado de Credenciales:** El archivo `.enc` nunca viaja en texto plano y solo puede ser descifrado por el ESP32-S3 que posea ese eFuse MAC específico.
* **Compatibilidad Android:** Permite aprovisionar placas de forma portátil en campo conectando el ESP32 a un celular con cable USB OTG.
