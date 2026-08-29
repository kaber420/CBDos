-- ============================================================================
-- CBDos - BadUSB Lua Demo (Linux / Ubuntu)
-- ============================================================================

print("[LUA HID] Iniciando script demo HID...")
cbdos.sleep(1000)

-- 1. Abrir terminal en Linux / Ubuntu (Ctrl + Alt + T)
print("[LUA HID] Lanzando terminal de Linux (Ctrl+Alt+T)...")
hid.press_combo({"CTRL", "ALT", "t"})
hid.delay(1000)

-- 2. Escribir mensaje en Bash puro
print("[LUA HID] Escribiendo en Bash...")
hid.type("echo '========================================'\n", 10)
hid.type("echo '  Hola Mundo desde CBDos USB HID Engine!  '\n", 10)
hid.type("echo '========================================'\n", 10)
hid.delay(500)

-- 3. Demostración de movimiento de ratón
print("[LUA HID] Demostración de ratón/cursor...")
for i = 1, 15 do
    hid.mouse_move(10, 5, 0)
    cbdos.sleep(30)
end

print("[LUA HID] Script finalizado con éxito.")
