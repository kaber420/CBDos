-- ==========================================
-- CBDos - Super Mario Bros Theme en Lua
-- ==========================================

print(" === Super Mario Bros Theme (CBDos Lua) ===")
cbdos.set_volume(90)
print("Volumen ajustado al: " .. cbdos.get_volume() .. "%")

local notas = {
    
    
    { nombre = "Do5 (C5)",  freq = 523.25, dur = 200 },
    { nombre = "Sol4 (G4)", freq = 392.00, dur = 200 },
    { nombre = "Mi4 (E4)",  freq = 329.63, dur = 200 },
    { nombre = "La4 (A4)",  freq = 440.00, dur = 180 },
    { nombre = "Si4 (B4)",  freq = 493.88, dur = 180 },
    { nombre = "La#4 (A#4)",freq = 466.16, dur = 160 },
    { nombre = "La4 (A4)",  freq = 440.00, dur = 200 },
    
    { nombre = "Sol4 (G4)", freq = 392.00, dur = 160 },
    { nombre = "Mi5 (E5)",  freq = 659.25, dur = 160 },
    { nombre = "Sol5 (G5)", freq = 783.99, dur = 160 },
    { nombre = "La5 (A5)",  freq = 880.00, dur = 200 },
    { nombre = "Fa5 (F5)",  freq = 698.46, dur = 160 },
    { nombre = "Sol5 (G5)", freq = 783.99, dur = 160 },
    { nombre = "Mi5 (E5)",  freq = 659.25, dur = 200 },
    { nombre = "Do5 (C5)",  freq = 523.25, dur = 160 },
    { nombre = "Re5 (D5)",  freq = 587.33, dur = 160 },
    { nombre = "Si4 (B4)",  freq = 493.88, dur = 250 },

    -- Cierre
    { nombre = "Do5 (C5)",  freq = 523.25, dur = 200 },
    { nombre = "Sol4 (G4)", freq = 392.00, dur = 200 },
    { nombre = "Mi4 (E4)",  freq = 329.63, dur = 200 },
    { nombre = "La4 (A4)",  freq = 440.00, dur = 180 },
    { nombre = "Si4 (B4)",  freq = 493.88, dur = 180 },
    { nombre = "La#4 (A#4)",freq = 466.16, dur = 160 },
    { nombre = "La4 (A4)",  freq = 440.00, dur = 200 },
    { nombre = "Sol4 (G4)", freq = 392.00, dur = 160 },
    { nombre = "Mi5 (E5)",  freq = 659.25, dur = 160 },
    { nombre = "Sol5 (G5)", freq = 783.99, dur = 160 },
    { nombre = "La5 (A5)",  freq = 880.00, dur = 220 },
    { nombre = "Do5 (C5)",  freq = 523.25, dur = 350 }
}

print("Tocando Super Mario Bros (" .. #notas .. " notas)...")

for i, n in ipairs(notas) do
    print("Nota [" .. i .. "/" .. #notas .. "]: " .. n.nombre .. " -> " .. n.freq .. " Hz")
    cbdos.beep(n.freq, n.dur)
    cbdos.delay(n.dur + 50)
end

print(" === Melodía de Mario Finalizada con Éxito ===")
