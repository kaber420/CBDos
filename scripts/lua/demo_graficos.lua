-- ===============================================
-- CBDos - Test Completo de Gráficos y Touch Lua
-- ===============================================

print("=== CBDos Lua - Canvas Grafico 2D ===")
print("Dimensiones pantalla: " .. cbdos.gfx.width() .. " x " .. cbdos.gfx.height())

-- Pausar la interfaz de LVGL por 30 segundos (o hasta tocar [SALIR])
-- Esto da control exclusivo del display AMOLED y del Touch a Lua
cbdos.gfx.pause_ui(30)

local W = cbdos.gfx.width()   -- 320
local H = cbdos.gfx.height()  -- 480

-- 1. Paleta de Colores
local C_FONDO   = cbdos.gfx.rgb(10, 15, 26)
local C_AZUL    = cbdos.gfx.rgb(33, 150, 243)
local C_CIAN    = cbdos.gfx.rgb(0, 229, 255)
local C_VERDE   = cbdos.gfx.rgb(0, 230, 118)
local C_AMARILLO= cbdos.gfx.rgb(255, 214, 0)
local C_ROJO    = cbdos.gfx.rgb(255, 82, 82)
local C_BLANCO  = cbdos.gfx.rgb(255, 255, 255)
local C_GRIS    = cbdos.gfx.rgb(60, 70, 85)

-- 2. Limpiar fondo con color oscuro
cbdos.gfx.clear(C_FONDO)

-- 3. Encabezado / Banner Superior
cbdos.gfx.draw_rect(0, 0, W, 45, C_AZUL, true)
cbdos.gfx.draw_text(15, 12, "CBDos Graphics Engine", C_BLANCO, 2)

-- 4. Primitivas Geométricas
cbdos.gfx.draw_rect(10, 55, 300, 115, C_GRIS, false)
cbdos.gfx.draw_text(20, 65, "Primitivas 2D en Lua:", C_AMARILLO, 1)

-- Rectángulos (relleno y borde)
cbdos.gfx.draw_rect(20, 85, 45, 35, C_VERDE, true)
cbdos.gfx.draw_rect(75, 85, 45, 35, C_CIAN, false)

-- Círculos concéntricos
cbdos.gfx.draw_circle(155, 102, 18, C_ROJO, true)
cbdos.gfx.draw_circle(155, 102, 9, C_AMARILLO, true)
cbdos.gfx.draw_circle(195, 102, 18, C_CIAN, false)

-- Líneas cruzadas
cbdos.gfx.draw_line(230, 85, 280, 120, C_BLANCO)
cbdos.gfx.draw_line(230, 120, 280, 85, C_AMARILLO)

-- 5. Área de Lienzo Táctil Interactivo
cbdos.gfx.draw_rect(10, 180, 300, 235, C_CIAN, false)
cbdos.gfx.draw_text(20, 190, "Lienzo Tactil (Dibuja aqui):", C_BLANCO, 1)

-- 6. Botón de Salida en Pantalla (Zona Inferior)
cbdos.gfx.draw_rect(10, 425, 300, 45, C_ROJO, true)
cbdos.gfx.draw_text(105, 438, "[ SALIR ]", C_BLANCO, 2)

-- Tono de inicio
cbdos.beep(1000, 60)
print("Canvas listo. Toca la pantalla para dibujar o presiona [SALIR].")

local contadorPuntos = 0
local activo = true

-- 7. Bucle Interactivo
while activo do
    local t = cbdos.gfx.touch()
    if t.touched then
        -- Si presiona el botón inferior [SALIR]
        if t.y >= 425 and t.y <= 470 and t.x >= 10 and t.x <= 310 then
            cbdos.beep(600, 100)
            activo = false
        -- Si dibuja en la zona de lienzo
        elseif t.y >= 180 and t.y <= 415 and t.x >= 10 and t.x <= 310 then
            contadorPuntos = contadorPuntos + 1
            local r = math.floor((t.x / 320) * 255)
            local g = math.floor((t.y / 480) * 255)
            local colorPunto = cbdos.gfx.rgb(r, g, 220)
            cbdos.gfx.draw_circle(t.x, t.y, 6, colorPunto, true)
            
            if contadorPuntos % 15 == 0 then
                cbdos.beep(1200, 10)
            end
        end
    end
    cbdos.delay(10) -- ~100 FPS
end

-- Limpiar antes de salir y reactivar la interfaz LVGL
cbdos.gfx.clear(C_FONDO)
cbdos.gfx.draw_text(70, 220, "Regresando a CBDos...", C_VERDE, 2)
cbdos.delay(400)

-- Reanudar LVGL para redibujar el sistema inmediatamente
cbdos.gfx.resume_ui()
print("Test de graficos completado.")

