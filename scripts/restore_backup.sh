#!/usr/bin/env bash
# ==============================================================================
# Script de Restauración del Firmware de Fábrica para JC4880P443C (ESP32-P4)
# ==============================================================================

set -e

# Directorios y rutas
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BACKUP_DIR="$PROJECT_ROOT/firmware/backup"
FULL_IMAGE="$BACKUP_DIR/JC4880P443C_full_16MB.bin"

# Detección automática del puerto serial
PORT="${1:-/dev/ttyACM0}"
if [ ! -e "$PORT" ]; then
    # Intentar buscar un puerto ttyACM o ttyUSB disponible
    DETECTED_PORT=$(ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null | head -n 1 || true)
    if [ -n "$DETECTED_PORT" ]; then
        PORT="$DETECTED_PORT"
    fi
fi

# Buscar ejecutable de esptool
ESPTOOL="esptool"
if ! command -v esptool &> /dev/null; then
    if [ -f "$HOME/.local/bin/esptool" ]; then
        ESPTOOL="$HOME/.local/bin/esptool"
    elif command -v esptool.py &> /dev/null; then
        ESPTOOL="esptool.py"
    else
        echo "❌ Error: No se encontró 'esptool'. Instálalo con: pip install esptool"
        exit 1
    fi
fi

echo "=================================================================="
echo " 🔄 RESTAURACIÓN DE FIRMWARE DE FÁBRICA (JC4880P443C_I_W)"
echo "=================================================================="
echo " Puerto serial seleccionado : $PORT"
echo " Imagen de respaldo         : $FULL_IMAGE"
echo " Herramienta de flasheo     : $ESPTOOL"
echo "=================================================================="

# Validar que el archivo de respaldo existe
if [ ! -f "$FULL_IMAGE" ]; then
    echo "❌ Error: No se encontró el archivo $FULL_IMAGE"
    exit 1
fi

# Validar que el dispositivo esté conectado
if [ ! -e "$PORT" ]; then
    echo "❌ Error: El puerto serial $PORT no existe o no está conectado."
    echo "👉 Conecta tu placa ESP32-P4 por el cable USB y verifica con 'ls /dev/ttyACM*'"
    exit 1
fi

echo ""
echo "⚠️  ADVERTENCIA: Esto sobreescribirá la memoria Flash completa de 16MB con el firmware de fábrica original."
read -p "¿Deseas continuar con el flasheo? [s/N]: " CONFIRM
if [[ ! "$CONFIRM" =~ ^[sS]$ ]]; then
    echo "Operación cancelada por el usuario."
    exit 0
fi

echo ""
echo "🚀 Iniciando flasheo de la imagen completa de 16 MB a través de $PORT..."
echo ""

$ESPTOOL -p "$PORT" write-flash 0x0 "$FULL_IMAGE"

echo ""
echo "=================================================================="
echo " ✅ ¡Restauración completada con éxito!"
echo " Tu placa JC4880P443C_I_W ha vuelto a su estado original de fábrica."
echo "=================================================================="
