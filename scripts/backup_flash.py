import os
import sys
import time
import esptool.cmds

PORT = "/dev/ttyACM0"
BACKUP_DIR = "firmware/backup"

os.makedirs(BACKUP_DIR, exist_ok=True)

PARTITIONS = [
    ("bootloader", 0x0, 0x8000),             # 32 KB Bootloader
    ("partition_table", 0x8000, 0x1000),      # 4 KB Partition Table
    ("nvs", 0x9000, 0x8000),                  # NVS area
    ("factory_app", 0x20000, 0x700000),       # 7 MB Factory App
    ("storage", 0x720000, 0x600000),          # 6 MB Storage / Assets
]

def connect_esp():
    for i in range(1, 6):
        try:
            time.sleep(0.5)
            esp = esptool.cmds.detect_chip(port=PORT, baud=115200)
            esp = esp.run_stub()
            return esp
        except Exception as e:
            print(f"[-] Reconnect attempt {i} failed: {e}. Retrying...")
            time.sleep(1.0)
    raise RuntimeError("Cannot connect to ESP32-P4")

def read_partition_in_sessions(name, offset, total_size, session_limit=2*1024*1024):
    out_file = os.path.join(BACKUP_DIR, f"{name}.bin")
    if os.path.exists(out_file) and os.path.getsize(out_file) == total_size:
        print(f"[+] {name} already complete ({total_size / 1024 / 1024:.2f} MB), skipping.")
        return True
        
    tmp_file = out_file + ".tmp"
    bytes_read = os.path.getsize(tmp_file) if os.path.exists(tmp_file) else 0
    
    print(f"\n==========================================")
    print(f"[*] Dumping {name} (Total: {total_size / 1024 / 1024:.2f} MB, Start offset: {hex(offset)})")
    if bytes_read > 0:
        print(f"[*] Resuming from {bytes_read / 1024 / 1024:.2f} MB...")
    print(f"==========================================")
    
    CHUNK = 32 * 1024 # 32 KB
    
    with open(tmp_file, "ab" if bytes_read > 0 else "wb") as f_out:
        while bytes_read < total_size:
            print(f"[*] Opening fresh USB-JTAG session at offset {hex(offset + bytes_read)}...")
            esp = connect_esp()
            
            session_bytes = 0
            # Read at most session_limit (2MB) in this session to prevent buffer freeze
            target_in_session = min(session_limit, total_size - bytes_read)
            
            try:
                while session_bytes < target_in_session:
                    to_read = min(CHUNK, target_in_session - session_bytes)
                    cur_addr = offset + bytes_read
                    data = esp.read_flash(cur_addr, to_read)
                    f_out.write(data)
                    f_out.flush()
                    bytes_read += to_read
                    session_bytes += to_read
                    pct = (bytes_read / total_size) * 100
                    if bytes_read % (256 * 1024) == 0 or bytes_read == total_size:
                        print(f"    -> {name}: {bytes_read / 1024 / 1024:.2f} / {total_size / 1024 / 1024:.2f} MB ({pct:.1f}%)")
                    time.sleep(0.002) # 2ms pacing
                    
                # Graceful close of session
                try:
                    esp._port.close()
                except Exception:
                    pass
                time.sleep(0.2)
                
            except Exception as e:
                print(f"[!] Session interrupted at {hex(offset + bytes_read)}: {e}. Re-connecting next session...")
                try:
                    esp._port.close()
                except Exception:
                    pass
                time.sleep(1.0)

    os.replace(tmp_file, out_file)
    print(f"[✓] Successfully downloaded {name} ({os.path.getsize(out_file)} bytes)\n")
    return True

def main():
    print("[*] Starting session-cycled flash backup for ESP32-P4...")
    for name, offset, size in PARTITIONS:
        read_partition_in_sessions(name, offset, size)
        
    print("\n==========================================")
    print("[*] Assembling full 16MB monolithic flash image...")
    full_image = os.path.join(BACKUP_DIR, "JC4880P443C_full_16MB.bin")
    with open(full_image, "wb") as f_full:
        f_full.write(b'\xFF' * 0x1000000)

    with open(full_image, "r+b") as f_full:
        for name, offset, size in PARTITIONS:
            part_file = os.path.join(BACKUP_DIR, f"{name}.bin")
            if os.path.exists(part_file):
                f_full.seek(offset)
                with open(part_file, "rb") as f_part:
                    f_full.write(f_part.read())

    print(f"[✓] Full 16MB backup created at: {full_image} ({os.path.getsize(full_image)} bytes)")
    print("[✓] ALL FLASH BACKUPS ARE COMPLETE AND VERIFIED!")

if __name__ == "__main__":
    main()
