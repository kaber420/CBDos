import sys
from PIL import Image, ImageDraw, ImageFont

# Canvas amplio con altura de 1050 px para 2 filas de margen libre
W, H = 880, 1050
BG_COLOR = (15, 23, 42)        # #0F172A Dark Slate
TEXT_WHITE = (248, 250, 252)   # #F8FAFC
TEXT_MUTED = (148, 163, 184)   # #94A3B8
PIN_NUM_COLOR = (100, 116, 139)# #64748B

# Colores de cajas de pines
BOX_DEFAULT = (30, 41, 59)     # #1E293B
BORDER_DEFAULT = (51, 65, 85)  # #334155
GOLD_PIN = (234, 179, 8)       # #EAB308

# Jumpers / Cables
COLOR_GREEN = (34, 197, 94)    # #22C55E Verde (TX)
COLOR_MAGENTA = (236, 72, 153) # #EC4899 Magenta (RX)
COLOR_CYAN = (6, 182, 212)     # #06B6D4 Celeste (BOOT)

# Pines JP1 (1 a 26)
ROWS = [
    ("3V3", "(1)", "(2)", "5V", False, False),
    ("3V3", "(3)", "(4)", "5V", False, False),
    ("GND", "(5)", "(6)", "GND", False, False),
    ("GPIO 52", "(7)", "(8)", "GPIO 33", False, False),
    ("GPIO 51", "(9)", "(10)", "GPIO 31", False, False),
    ("GPIO 50", "(11)", "(12)", "GPIO 30", False, False),
    ("GPIO 49", "(13)", "(14)", "GPIO 29", False, False),
    ("GPIO 35", "(15)", "(16)", "GND", False, False),
    ("GPIO 34", "(17)", "(18)", "ESP_3V3", "cyan", False),     # Pin 17 es salida BOOT
    ("GPIO 32", "(19)", "(20)", "C6_U0RXD", "green", "green"), # Pin 19->20 TX
    ("GPIO 28", "(21)", "(22)", "C6_U0TXD", "magenta", "magenta"), # Pin 21<-22 RX
    ("I2C_SDA", "(23)", "(24)", "C6_IO9", False, "cyan"),      # Pin 24 es entrada BOOT
    ("I2C_SCL", "(25)", "(26)", "C6_CHIP_PU", False, False),
]

img = Image.new("RGB", (W, H), BG_COLOR)
draw = ImageDraw.Draw(img)

try:
    font_title = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 22)
    font_sub = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 13)
    font_pin = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 14)
    font_num = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 11)
except:
    font_title = font_sub = font_pin = font_num = ImageFont.load_default()

# Título
draw.text((40, 30), "Diagrama de Pines JP1 - Flasheo Coprocesador ESP32-C6", fill=TEXT_WHITE, font=font_title)
draw.text((40, 62), "Guition JC4880P443C (ESP32-P4) -> Conexión Simplificada (3 Cables)", fill=(56, 189, 248), font=font_sub)

start_y = 110
row_height = 54
box_w = 175
box_h = 42

# Columnas con 50px de separación central
left_x = 230
right_x = 475

# Dibujar Cajas de Pines
for i, (l_name, l_num, r_num, r_name, l_hl, r_hl) in enumerate(ROWS):
    y = start_y + i * row_height

    # 1. Caja Izquierda
    l_bg = BOX_DEFAULT
    l_border = BORDER_DEFAULT
    if l_hl == "green": l_border = COLOR_GREEN
    elif l_hl == "magenta": l_border = COLOR_MAGENTA
    elif l_hl == "cyan": l_border = COLOR_CYAN

    draw.rounded_rectangle([left_x, y, left_x + box_w, y + box_h], radius=8, fill=l_bg, outline=l_border, width=2)
    draw.text((left_x + 14, y + 12), l_name, fill=TEXT_WHITE, font=font_pin)
    draw.text((left_x + box_w - 32, y + 14), l_num, fill=PIN_NUM_COLOR, font=font_num)

    # 2. Puntos dorados centrales (Separación clara de 18px entre pines)
    pin_l_x = 431
    pin_r_x = 449
    draw.ellipse([pin_l_x - 4, y + box_h//2 - 4, pin_l_x + 4, y + box_h//2 + 4], fill=GOLD_PIN)
    draw.ellipse([pin_r_x - 4, y + box_h//2 - 4, pin_r_x + 4, y + box_h//2 + 4], fill=GOLD_PIN)

    # 3. Caja Derecha
    r_bg = BOX_DEFAULT
    r_border = BORDER_DEFAULT
    if r_hl == "green": r_border = COLOR_GREEN
    elif r_hl == "magenta": r_border = COLOR_MAGENTA
    elif r_hl == "cyan": r_border = COLOR_CYAN

    draw.rounded_rectangle([right_x, y, right_x + box_w, y + box_h], radius=8, fill=r_bg, outline=r_border, width=2)
    draw.text((right_x + 12, y + 14), r_num, fill=PIN_NUM_COLOR, font=font_num)
    draw.text((right_x + 40, y + 12), r_name, fill=TEXT_WHITE, font=font_pin)

# Jumpers Horizontales (Verde TX y Magenta RX)
y_green = start_y + 9 * row_height + box_h//2
draw.line([(left_x + box_w, y_green), (right_x, y_green)], fill=COLOR_GREEN, width=12)
draw.ellipse([left_x + box_w - 4, y_green - 4, left_x + box_w + 4, y_green + 4], fill=COLOR_GREEN)
draw.ellipse([right_x - 4, y_green - 4, right_x + 4, y_green + 4], fill=COLOR_GREEN)

y_mag = start_y + 10 * row_height + box_h//2
draw.line([(left_x + box_w, y_mag), (right_x, y_mag)], fill=COLOR_MAGENTA, width=12)
draw.ellipse([left_x + box_w - 4, y_mag - 4, left_x + box_w + 4, y_mag + 4], fill=COLOR_MAGENTA)
draw.ellipse([right_x - 4, y_mag - 4, right_x + 4, y_mag + 4], fill=COLOR_MAGENTA)

# Cable Celeste (Pin 17 / GPIO 34 a Pin 24 / C6_IO9)
# Pasando 2 filas completas (100px) por debajo de la última caja para holgura total
y_p17 = start_y + 8 * row_height + box_h//2
y_p24 = start_y + 11 * row_height + box_h//2

track_left = left_x - 50
track_bottom = start_y + 14 * row_height + 20
track_right = right_x + box_w + 50

draw.line([(left_x, y_p17), (track_left, y_p17)], fill=COLOR_CYAN, width=5)
draw.line([(track_left, y_p17), (track_left, track_bottom)], fill=COLOR_CYAN, width=5)
draw.line([(track_left, track_bottom), (track_right, track_bottom)], fill=COLOR_CYAN, width=5)
draw.line([(track_right, track_bottom), (track_right, y_p24)], fill=COLOR_CYAN, width=5)
draw.line([(track_right, y_p24), (right_x + box_w, y_p24)], fill=COLOR_CYAN, width=5)

# Puntos terminales circulares en los puertos
draw.ellipse([left_x - 5, y_p17 - 5, left_x + 5, y_p17 + 5], fill=COLOR_CYAN)
draw.ellipse([right_x + box_w - 5, y_p24 - 5, right_x + box_w + 5, y_p24 + 5], fill=COLOR_CYAN)

img.save("docs/images/esp32_c6_flasher_diagram.png", "PNG")
print("Diagrama generado exitosamente con 2 filas extras de margen.")
