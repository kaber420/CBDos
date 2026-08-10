# Plan de Implementación: Eventos de Interacción en formato TLV

El gateway/proxy se encarga de aislar la sesión real con WebDriver BiDi y Firefox. El cliente (MicroPythonOS) debe operar de manera completamente agnóstica al transporte (LoRa o WiFi) y comunicarse exclusivamente a través de tramas binarias **TLV** (Type-Length-Value) para minimizar el ancho de banda.

## Diseño del Protocolo TLV (Cliente -> Servidor/Gateway)

Al igual que el gateway envía la pantalla usando etiquetas (ej. `0x13` para Inputs, `0x12` para Links), el navegador debe empaquetar las acciones del usuario en tramas TLV hacia el gateway, en lugar de URLs en texto plano.

### 1. Nuevas etiquetas TLV propuestas para eventos (Upstream)
Proponemos los siguientes tipos para estructurar el mensaje binario que enviará `app.py`:

*   **`TYPE_REQ_INPUT_SUBMIT = 0x20`** (El usuario dio "Enter" en un campo de texto)
    *   **Estructura del Value:** 
        *   `ID_Length` (1 byte)
        *   `Element_ID` (String de longitud `ID_Length` extraído del nodo `0x13` original)
        *   `Text_Value` (String con el texto introducido, el resto de la longitud)

*   **`TYPE_REQ_LINK_CLICK = 0x21`** (El usuario hizo clic en un botón/enlace)
    *   **Estructura del Value:**
        *   `Link_ID` (1 byte, que coincide con el ID recibido en `TYPE_ABS_LINK`)

### 2. Desacoplamiento de la Capa de Transporte (WiFi / LoRa)
Actualmente `app.py` tiene código directo de `socket.socket(...)` acoplado a una IP (comportamiento WiFi).
*   **Cambio propuesto:** Extraer el envío a una función o clase abstracta encargada del transporte (ej. `mpos.net.GatewayTransport`).
*   Esta capa decidirá internamente si usa Sockets TCP (si hay WiFi activo) o si manda el payload binario a través de LoRa, permitiendo que el navegador funcione en ambos escenarios.

### 3. Modificaciones en `app.py` (Flujo de la UI)
1. **Mapeo de IDs:** Analizar el TLV recibido para asociar cada `lv.textarea` a su respectivo `Element_ID`.
2. **Captura del Enter:** Al interceptar `lv.EVENT.READY` en el teclado, `app.py` leerá el texto del input activo.
3. **Empaquetado TLV:** Se construirá un `bytearray` usando la estructura `0x20` + Longitud + ID + Texto.
4. **Transmisión:** Se pasará el `bytearray` al transporte genérico.
5. **Recepción:** Se esperará la respuesta del gateway (el nuevo DOM en TLV) y se llamará de nuevo a `tlv_browser.render()`.

---

**PREGUNTAS PARA CONTINUAR:**
1. ¿Ya tienes definidos en el gateway los códigos (Tags Hexadecimales) para cuando el cliente le manda los eventos "Submit Text" y "Click Link"? (Si es así, dime cuáles son para no inventar `0x20` y `0x21`).
2. ¿Existe ya una clase de transporte para LoRa en este proyecto, o quieres que la llamada desde `app.py` sea una interfaz genérica que deje un "hueco" preparado para LoRa?
