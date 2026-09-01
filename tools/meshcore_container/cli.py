#!/usr/bin/env python3
import sys
import time
import argparse
from mesh_node import MeshNodeEngine

def format_sig_bar(rssi: int) -> str:
    if rssi > -65: return "🟢"
    if rssi > -80: return "🟡"
    if rssi > -90: return "🔴"
    return "💀"

def main():
    parser = argparse.ArgumentParser(description="Terminal de Chat MeshCore sobre ESP-NOW (CBDos)")
    parser.add_argument("--port", default="/dev/ttyACM0", help="Puerto serie del Dongle ESP32-C3 (default: /dev/ttyACM0)")
    parser.add_argument("--name", default="Base-Laptop", help="Nombre del nodo en la malla")
    parser.add_argument("--id", type=lambda x: int(x, 0), default=0x00C3, help="Short ID (ej: 0x00C3)")
    parser.add_argument("--mode", default="lr", choices=["normal", "lr"], help="Modo de radio (normal / lr)")
    parser.add_argument("--channel", "-c", type=int, default=1, choices=range(1, 14), help="Canal Wi-Fi (1..13)")
    parser.add_argument("--test", action="store_true", help="Inicia prueba continua de alcance de inmediato")
    args = parser.parse_args()

    engine = MeshNodeEngine(port=args.port, name=args.name, short_id=args.id)

    # Callbacks de eventos
    def on_node(node_data):
        sig = format_sig_bar(node_data["rssi"])
        print(f"\n✨ [Nuevo Nodo Descubierto]: '{node_data['name']}' (ID: 0x{node_data['src_id']:04X} | MAC: {node_data['mac']} | Señal: {sig} {node_data['rssi']} dBm | Saltos: {node_data['hops']})")
        print("💬 > ", end="", flush=True)

    def on_chat(chat_data):
        sig = format_sig_bar(chat_data["rssi"])
        chan_str = "#general" if chat_data["channel_id"] == 0 else f"Canal {chat_data['channel_id']}"
        print(f"\n📩 [{chat_data['sender_name']} @ {chan_str} | Señal: {sig} {chat_data['rssi']} dBm | Hops: {chat_data['hops']} | {chat_data['timestamp']}]: {chat_data['text']}")
        print("💬 > ", end="", flush=True)

    engine.on_node_discovered = on_node
    engine.on_chat_received = on_chat

    if not engine.start():
        sys.exit(1)

    time.sleep(0.3)
    engine.set_radio_mode(args.mode)
    time.sleep(0.1)
    if args.channel != 1:
        engine.set_channel(args.channel)
    else:
        engine.send_beacon()


    if args.test:
        engine.range_test_active = True

    print("\n" + "=" * 68)
    print(" 🌐 TERMINAL INTERACTIVA MESHCORE (ESP-NOW 2.4 GHz + RANGE TEST)")
    print("=" * 68)
    print("  • Escribe texto y presiona [ENTER] para enviar a #general")
    print("  • /channel <1-13> - Cambia el canal de radio (ej: /channel 13)")
    print("  • /range [seg]  - Inicia prueba continua de alcance (ej: /range 3)")
    print("  • /stop         - Detiene la prueba continua de alcance")
    print("  • /echo on|off  - Activa/desactiva respuesta de eco con RSSI")
    print("  • /stats        - Muestra telemetría completa y calidad de señal")
    print("  • /nodes        - Lista los nodos descubiertos y su último RSSI")
    print("  • /beacon       - Emite manualmente una baliza de presencia")
    print("  • /mode lr|normal - Conmuta modo Long Range o Normal")
    print("  • /exit         - Salir")
    print("=" * 68 + "\n")

    try:
        while engine.running:
            try:
                line = input("💬 > ").strip()
            except EOFError:
                break

            if not line:
                continue

            cmd = line[1:].strip() if line.startswith("/") else line

            if cmd in ("exit", "quit", "q"):
                break
            elif cmd == "nodes":
                print("\n📋 Nodos activos en la red MeshCore:")
                if not engine.discovered_nodes:
                    print("   (Ningún vecino detectado aún. Esperando balizas...)")
                for sid, info in engine.discovered_nodes.items():
                    ago = int(time.time() - info["last_seen"])
                    sig = format_sig_bar(info["rssi"])
                    print(f"   • [{info['name']}] (ID: 0x{sid:04X}) | MAC: {info['mac']} | Señal: {sig} {info['rssi']} dBm | Visto hace {ago}s | Saltos: {info['hops']}")
                print()
            elif cmd.startswith("chan"):
                parts = cmd.split()
                if len(parts) >= 2 and parts[1].isdigit():
                    ch = int(parts[1])
                    engine.set_channel(ch)
                else:
                    print(f"📻 Canal activo actual: Canal {engine.channel} ({2412 + (engine.channel - 1) * 5} MHz)")
            elif cmd.startswith("range"):
                parts = cmd.split()
                if len(parts) >= 2 and parts[1].lower() in ("stop", "off", "0"):
                    engine.range_test_active = False
                    print("⏹️ Prueba de alcance (Range Test) DETENIDA.")
                else:
                    sec = 3
                    if len(parts) >= 2 and parts[1].isdigit():
                        sec = max(1, int(parts[1]))
                    engine.range_test_interval = sec
                    engine.range_test_active = True
                    print(f"📡 Prueba de alcance (Range Test) ACTIVADA (emitiendo cada {sec}s)...")
            elif cmd in ("stop", "range stop", "range off"):
                engine.range_test_active = False
                print("⏹️ Prueba de alcance (Range Test) DETENIDA.")
            elif cmd.startswith("echo"):
                parts = cmd.split()
                if len(parts) >= 2 and parts[1].lower() in ("off", "false", "0"):
                    engine.auto_echo = False
                    print("🔇 Auto-Echo DESACTIVADO.")
                else:
                    engine.auto_echo = True
                    print("🔊 Auto-Echo ACTIVADO.")
            elif cmd in ("stats", "status"):
                print("\n📊 TELEMETRÍA DEL NODO MESHCORE:")
                print(f"   • Canal Activo: Canal {engine.channel} ({2412 + (engine.channel - 1) * 5} MHz)")
                print(f"   • Mensajes TX:  {engine.stat_tx} | Mensajes RX: {engine.stat_rx}")
                print(f"   • Auto-Echo:    {'🟢 ACTIVO' if engine.auto_echo else '⚪ DESACTIVADO'}")
                print(f"   • Range Test:   {'🟢 ACTIVO (' + str(engine.range_test_interval) + 's)' if engine.range_test_active else '⚪ INACTIVO'}")
                print(f"   • Último RSSI:  {engine.last_rx_rssi} dBm")
                print()
            elif cmd == "beacon":
                engine.send_beacon()
                print("📡 Baliza emitida.")
            elif cmd.startswith("mode "):
                m = cmd.split(" ", 1)[1].strip()
                engine.set_radio_mode(m)
            elif cmd in ("help", "?"):
                print("\nComandos disponibles:")
                print("  /channel <1-13> - Cambia canal de radio")
                print("  /range [seg]    - Inicia prueba continua de alcance")
                print("  /stop           - Detiene la prueba de alcance")
                print("  /echo on|off    - Activa/desactiva bot de eco con RSSI")
                print("  /stats          - Muestra telemetría")
                print("  /nodes          - Lista nodos descubiertos")
                print("  /mode lr|normal - Cambia velocidad/alcance de radio")
                print("  /exit           - Salir\n")
            else:
                hora = time.strftime("%H:%M:%S")
                engine.send_chat(line)
                print(f"📤 [Tu -> #general | {hora}]: {line}")


    except KeyboardInterrupt:
        pass
    finally:
        engine.stop()

if __name__ == '__main__':
    main()
