-- ==========================================
-- CBDos - Test Grafico y Táctil en Vivo
-- ==========================================

print("=== CBDos Lua - Canvas Grafico 2D ===")
print("Dimensiones pantalla: " .. cbdos.gfx.width() .. " x " .. cbdos.gfx.height())

-- Limpiar fondo con color azul oscuro
local fondo = cbdos.gfx.rgb(15, 20, 30)
cbdos.gfx.clear(fondo)

-- Dibujar encabezado
local verde = cbdos.gfx.rgb(0, 230, 118)
local blanco = cbdos.gfx.rgb(255, 255, 255)
local naranja = cbdos.gfx.rgb(255, 140, 0)
local rojo = cbdos.gfx.rgb(255, 60, 60)

cbdos.gfx.draw_rect(10, 10, 300, 45, verde, false)
cbdos.gfx.draw_text(25, 22, "CBDos Lua Touch Paint", blanco, 2)

cbdos.gfx.draw_text(20, 70, "Toca la pantalla para dibujar", naranja, 1)
cbdos.gfx.draw_text(20, 85, "Presiona STOP en la UI para salir", blanco, 1)

print("Entrando en bucle de captura tactil...")

local contadorPuntos = 0

-- Bucle interactivo (se puede detener con el botón STOP de la UI)
while true do
    local t = cbdos.gfx.touch()
    if t.touched then
        contadorPuntos = contadorPuntos + 1
        -- Color dinámico según la coordenada X
        local r = math.floor((t.x / 320) * 255)
        local g = math.floor((t.y / 480) * 255)
        local b = 180
        local colorPunto = cbdos.gfx.rgb(r, g, b)

        cbdos.gfx.draw_circle(t.x, t.y, 7, colorPunto, true)
        
        if contadorPuntos % 15 == 0 then
            cbdos.beep(1200, 10)
            print("Punto dibujado en (" .. t.x .. ", " .. t.y .. ")")
        end
    end
    cbdos.delay(15) -- 60 FPS aprox, cede tiempo a FreeRTOS
end
