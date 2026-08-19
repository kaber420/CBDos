-- ==========================================
-- CBDos - Script de Diagnóstico del Sistema
-- ==========================================

print("=== CBDos Lua 5.4 - Diagnostico ===")
print("Tiempo activo : " .. cbdos.millis() .. " ms")
print("PSRAM libre   : " .. cbdos.free_psram() .. " bytes (" .. string.format("%.2f", cbdos.free_psram() / (1024 * 1024)) .. " MB)")
print("SRAM libre    : " .. cbdos.free_heap() .. " bytes (" .. string.format("%.2f", cbdos.free_heap() / 1024) .. " KB)")
print("Nivel bateria : " .. cbdos.get_battery() .. "%")
print("WiFi conectado: " .. tostring(cbdos.wifi_status()))
print("IP asignada   : " .. cbdos.get_ip())
print("------------------------------------")

-- Conteo en bucle con delay
for i = 1, 5 do
    print("Contador: " .. i .. " / 5 | Tick: " .. cbdos.millis() .. " ms")
    cbdos.beep(600 + (i * 100), 80)
    cbdos.delay(500)
end

print("=== Test de Sistema Completado con Exito ===")
