#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PORT="${1:-/dev/ttyACM1}"
NODE_NAME="${2:-Laptop-Cyberdeck}"

echo "🐳 Construyendo imagen aislada Podman/Docker (cbdos-meshcore)..."
podman build --no-cache -t cbdos-meshcore "$SCRIPT_DIR"


echo "🚀 Iniciando Nodo MeshCore interactivo sobre $PORT..."
podman run -it --rm \
    --group-add keep-groups \
    --device="$PORT:$PORT:rwm" \
    cbdos-meshcore --port "$PORT" --name "$NODE_NAME" "${@:3}"


