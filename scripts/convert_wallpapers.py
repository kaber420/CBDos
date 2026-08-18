import os
import struct
from PIL import Image, ImageOps

TARGET_W = 320
TARGET_H = 480

OUTPUT_DIR = "wallpapers"
os.makedirs(OUTPUT_DIR, exist_ok=True)

IMAGES = [
    ("EchinopsisOxygona.jpg", "echinopsis"),
    ("SmallCactuses.jpg", "small_cactuses"),
    ("retro-console-cartoon-illustration-free-vector.jpg", "retro_console"),
    ("retrojoy.jpg", "retrojoy")
]

def rgb888_to_rgb565(r, g, b):
    val = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
    return val

# LVGL 9.5 image header (12 bytes total):
LVGL_HEADER = struct.pack("<BBHHHHH", 0x19, 0x12, 0, TARGET_W, TARGET_H, TARGET_W * 2, 0)

for src_file, base_name in IMAGES:
    source_path = src_file
    if not os.path.exists(source_path):
        # Fallback to existing jpg in wallpapers folder
        source_path = os.path.join(OUTPUT_DIR, f"{base_name}.jpg")
    
    if not os.path.exists(source_path):
        print(f"Warning: {src_file} and {source_path} not found!")
        continue

    img = Image.open(source_path).convert("RGB")
    img_resized = ImageOps.fit(img, (TARGET_W, TARGET_H), method=Image.Resampling.LANCZOS)
    
    # 1. Save as resized JPG
    jpg_path = os.path.join(OUTPUT_DIR, f"{base_name}.jpg")
    img_resized.save(jpg_path, "JPEG", quality=95)

    # 2. Save as BMP (24-bit RGB)
    bmp_path = os.path.join(OUTPUT_DIR, f"{base_name}.bmp")
    img_resized.save(bmp_path, "BMP")

    # 3. Save as PNG
    png_path = os.path.join(OUTPUT_DIR, f"{base_name}.png")
    img_resized.save(png_path, "PNG")

    # 4. Save as LVGL 9 Binary (.bin with lv_image_header_t + RGB565)
    bin_path = os.path.join(OUTPUT_DIR, f"{base_name}.bin")
    raw_bytes = bytearray(LVGL_HEADER)
    for y in range(TARGET_H):
        for x in range(TARGET_W):
            r, g, b = img_resized.getpixel((x, y))
            val = rgb888_to_rgb565(r, g, b)
            raw_bytes.extend(struct.pack("<H", val))
            
    with open(bin_path, "wb") as f:
        f.write(raw_bytes)
    print(f"Generated {base_name}: JPG, BMP, PNG, BIN ({len(raw_bytes)} bytes)")

print("\nDone generating all formats in:", OUTPUT_DIR)
