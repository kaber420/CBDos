import os
import sys
import struct
from PIL import Image, ImageOps

TARGET_W = 320
TARGET_H = 480
OUTPUT_C_FILE = "firmware/src/UI/Assets/default_wallpaper.c"

def rgb888_to_rgb565(r, g, b):
    # LVGL standard 16-bit RGB565 little-endian
    val = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
    # Little endian byte order (low byte first, then high byte)
    low_byte = val & 0xFF
    high_byte = (val >> 8) & 0xFF
    return low_byte, high_byte

def generate_default_wallpaper_c(image_path):
    if not os.path.exists(image_path):
        print(f"Error: {image_path} does not exist!")
        return False

    print(f"Reading image: {image_path}")
    img = Image.open(image_path).convert("RGB")
    img_resized = ImageOps.fit(img, (TARGET_W, TARGET_H), method=Image.Resampling.LANCZOS)
    
    bytes_data = bytearray()
    for y in range(TARGET_H):
        for x in range(TARGET_W):
            r, g, b = img_resized.getpixel((x, y))
            low, high = rgb888_to_rgb565(r, g, b)
            bytes_data.append(low)
            bytes_data.append(high)

    print(f"Generated {len(bytes_data)} bytes of RGB565 data.")

    with open(OUTPUT_C_FILE, "w", encoding="utf-8") as f:
        f.write('#include "default_wallpaper.h"\n\n')
        f.write('#ifndef LV_ATTRIBUTE_MEM_ALIGN\n#define LV_ATTRIBUTE_MEM_ALIGN\n#endif\n\n')
        f.write('#ifndef LV_ATTRIBUTE_IMAGE_DEFAULT_WALLPAPER\n#define LV_ATTRIBUTE_IMAGE_DEFAULT_WALLPAPER\n#endif\n\n')
        f.write('const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMAGE_DEFAULT_WALLPAPER uint8_t default_wallpaper_map[] = {\n')
        
        # Write bytes in chunks of 16 per line
        chunk_size = 16
        for i in range(0, len(bytes_data), chunk_size):
            chunk = bytes_data[i:i+chunk_size]
            line_str = "    " + ", ".join(f"0x{b:02x}" for b in chunk)
            if i + chunk_size < len(bytes_data):
                line_str += ","
            f.write(line_str + "\n")
            
        f.write('};\n\n')
        f.write('const lv_image_dsc_t default_wallpaper = {\n')
        f.write('    .header = {\n')
        f.write('        .magic = LV_IMAGE_HEADER_MAGIC,\n')
        f.write('        .cf = LV_COLOR_FORMAT_RGB565,\n')
        f.write('        .flags = 0,\n')
        f.write(f'        .w = {TARGET_W},\n')
        f.write(f'        .h = {TARGET_H},\n')
        f.write(f'        .stride = {TARGET_W * 2},\n')
        f.write('        .reserved_2 = 0\n')
        f.write('    },\n')
        f.write(f'    .data_size = {len(bytes_data)},\n')
        f.write('    .data = default_wallpaper_map,\n')
        f.write('    .reserved = NULL\n')
        f.write('};\n')

    print(f"Successfully wrote new {OUTPUT_C_FILE}!")
    return True

if __name__ == "__main__":
    if len(sys.argv) > 1:
        generate_default_wallpaper_c(sys.argv[1])
    else:
        print("Usage: python3 scripts/set_default_wallpaper.py <image_path>")
