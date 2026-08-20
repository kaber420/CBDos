import sys
import esptool

def main():
    print("=== Iniciando Flasheo del ESP32-C6 (15 intentos continuos) ===")
    args = [
        "--chip", "esp32c6",
        "-p", "/dev/ttyACM0",
        "-b", "115200",
        "--connect-attempts", "20",
        "--before", "no_reset",
        "--after", "no_reset",
        "write_flash",
        "0x0", "/home/kaber420/Documentos/proyectos/esp-hosted-mcu/slave/build/network_adapter.bin"
    ]
    esptool.main(args)

if __name__ == "__main__":
    main()
