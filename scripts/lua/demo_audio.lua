-- ==========================================
-- CBDos - Melodía y Test de Audio I2S
-- ==========================================

print("=== CBDos Lua - Reproductor de Notas ===")
cbdos.set_volume(85)
print("Volumen ajustado al: " .. cbdos.get_volume() .. "%")

-- Escala musical (Frecuencias en Hz)
local notas = {
    { nombre = "Do4 (C4)",  freq = 261.63, dur = 200 },
    { nombre = "Re4 (D4)",  freq = 293.66, dur = 200 },
    { nombre = "Mi4 (E4)",  freq = 329.63, dur = 200 },
    { nombre = "Fa4 (F4)",  freq = 349.23, dur = 200 },
    { nombre = "Sol4 (G4)", freq = 392.00, dur = 200 },
    { nombre = "La4 (A4)",  freq = 440.00, dur = 200 },
    { nombre = "Si4 (B4)",  freq = 493.88, dur = 200 },
    { nombre = "Do5 (C5)",  freq = 523.25, dur = 400 }
}

for i, n in ipairs(notas) do
    print("Nota [" .. i .. "/8]: " .. n.nombre .. " -> " .. n.freq .. " Hz")
    cbdos.beep(n.freq, n.dur)
    cbdos.delay(n.dur + 60)
end

print("=== Melodia Finalizada ===")
