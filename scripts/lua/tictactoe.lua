-- ===============================================
-- CBDos - Juego 3 en Raya (Tic-Tac-Toe) en Lua
-- Archivo: /sd/scripts/tictactoe.lua
-- ===============================================

print("=== Iniciando Tic-Tac-Toe en CBDos ===")

-- Pausar la interfaz de LVGL para modo juego exclusivo
cbdos.gfx.pause_ui(0)

local W = cbdos.gfx.width()   -- 320
local H = cbdos.gfx.height()  -- 480

-- 1. Paleta de Colores
local C_FONDO   = cbdos.gfx.rgb(12, 16, 28)
local C_PANEL   = cbdos.gfx.rgb(20, 27, 45)
local C_BORDE   = cbdos.gfx.rgb(40, 52, 80)
local C_AZUL    = cbdos.gfx.rgb(33, 150, 243)
local C_CIAN    = cbdos.gfx.rgb(0, 229, 255)   -- Color para 'X'
local C_CORAL   = cbdos.gfx.rgb(255, 82, 82)   -- Color para 'O'
local C_VERDE   = cbdos.gfx.rgb(0, 230, 118)
local C_AMARILLO= cbdos.gfx.rgb(255, 214, 0)
local C_BLANCO  = cbdos.gfx.rgb(255, 255, 255)
local C_GRIS    = cbdos.gfx.rgb(120, 135, 160)

-- 2. Variables de Juego
local board = {
    0, 0, 0,
    0, 0, 0,
    0, 0, 0
} -- 0 = vacio, 1 = Jugador (X), 2 = CPU (O)

local scorePlayer = 0
local scoreCPU = 0
local scoreTies = 0

local gameState = "PLAYER_TURN" -- PLAYER_TURN, CPU_TURN, GAME_OVER
local winner = 0 -- 0=ninguno, 1=Jugador, 2=CPU, 3=Empate
local winningLine = nil -- {c1, c2, c3}

-- Dimensiones del Tablero
local GRID_X = 25
local GRID_Y = 110
local CELL_SIZE = 80
local CELL_GAP = 10

-- 3. Funciones de Dibujo

local function getCellRect(col, row)
    local x = GRID_X + (col - 1) * (CELL_SIZE + CELL_GAP)
    local y = GRID_Y + (row - 1) * (CELL_SIZE + CELL_GAP)
    return x, y
end

local function drawX(cx, cy)
    local pad = 18
    local x1, y1 = cx + pad, cy + pad
    local x2, y2 = cx + CELL_SIZE - pad, cy + CELL_SIZE - pad
    
    -- Dibujar cruz con grosor
    for offset = -2, 2 do
        cbdos.gfx.draw_line(x1 + offset, y1, x2 + offset, y2, C_CIAN)
        cbdos.gfx.draw_line(x1, y1 + offset, x2, y2 + offset, C_CIAN)
        cbdos.gfx.draw_line(x1 + offset, y2, x2 + offset, y1, C_CIAN)
        cbdos.gfx.draw_line(x1, y2 + offset, x2, y1 + offset, C_CIAN)
    end
end

local function drawO(cx, cy)
    local center_x = cx + math.floor(CELL_SIZE / 2)
    local center_y = cy + math.floor(CELL_SIZE / 2)
    local r = 24
    
    -- Dibujar anillo
    cbdos.gfx.draw_circle(center_x, center_y, r, C_CORAL, true)
    cbdos.gfx.draw_circle(center_x, center_y, r - 5, C_PANEL, true)
end

local function drawHeader()
    -- Banner superior
    cbdos.gfx.draw_rect(0, 0, W, 42, C_PANEL, true)
    cbdos.gfx.draw_line(0, 42, W, 42, C_BORDE)
    cbdos.gfx.draw_text(60, 12, "TIC - TAC - TOE", C_BLANCO, 2)

    -- Barra de Marcador
    cbdos.gfx.draw_rect(15, 52, 290, 40, C_PANEL, true)
    cbdos.gfx.draw_rect(15, 52, 290, 40, C_BORDE, false)
    
    local txtScore = string.format("TU(X): %d  |  CPU(O): %d  |  EMP: %d", scorePlayer, scoreCPU, scoreTies)
    cbdos.gfx.draw_text(25, 65, txtScore, C_AMARILLO, 1)
end

local function drawBoard()
    for row = 1, 3 do
        for col = 1, 3 do
            local idx = (row - 1) * 3 + col
            local x, y = getCellRect(col, row)
            
            -- Fondo de celda
            cbdos.gfx.draw_rect(x, y, CELL_SIZE, CELL_SIZE, C_PANEL, true)
            cbdos.gfx.draw_rect(x, y, CELL_SIZE, CELL_SIZE, C_BORDE, false)
            
            if board[idx] == 1 then
                drawX(x, y)
            elseif board[idx] == 2 then
                drawO(x, y)
            end
        end
    end
end

local function drawStatus()
    -- Limpiar área de mensaje
    cbdos.gfx.draw_rect(0, 380, W, 35, C_FONDO, true)
    
    if gameState == "PLAYER_TURN" then
        cbdos.gfx.draw_text(85, 390, "Tu turno (Toca una celda)", C_CIAN, 1)
    elseif gameState == "CPU_TURN" then
        cbdos.gfx.draw_text(105, 390, "CPU pensando...", C_AMARILLO, 1)
    elseif gameState == "GAME_OVER" then
        if winner == 1 then
            cbdos.gfx.draw_text(80, 390, " VICTORIA! GANASTE! ", C_VERDE, 1)
        elseif winner == 2 then
            cbdos.gfx.draw_text(90, 390, "GANA LA CPU!", C_CORAL, 1)
        else
            cbdos.gfx.draw_text(115, 390, "EMPATE!", C_AMARILLO, 1)
        end
    end
end

local function drawButtons()
    -- Botón Nueva Partida
    cbdos.gfx.draw_rect(15, 425, 135, 45, C_AZUL, true)
    cbdos.gfx.draw_rect(15, 425, 135, 45, C_BLANCO, false)
    cbdos.gfx.draw_text(30, 440, "REINICIAR", C_BLANCO, 1)

    -- Botón Salir
    cbdos.gfx.draw_rect(170, 425, 135, 45, C_CORAL, true)
    cbdos.gfx.draw_rect(170, 425, 135, 45, C_BLANCO, false)
    cbdos.gfx.draw_text(210, 440, "SALIR", C_BLANCO, 1)
end

local function renderAll()
    cbdos.gfx.clear(C_FONDO)
    drawHeader()
    drawBoard()
    drawStatus()
    drawButtons()
end

-- 4. Lógica de Victoria e IA

local WIN_COMBOS = {
    {1, 2, 3}, {4, 5, 6}, {7, 8, 9}, -- Filas
    {1, 4, 7}, {2, 5, 8}, {3, 6, 9}, -- Columnas
    {1, 5, 9}, {3, 5, 7}             -- Diagonales
}

local function checkWinner()
    for _, combo in ipairs(WIN_COMBOS) do
        local a, b, c = combo[1], combo[2], combo[3]
        if board[a] ~= 0 and board[a] == board[b] and board[b] == board[c] then
            winningLine = combo
            return board[a] -- 1 o 2
        end
    end
    
    -- Comprobar si quedan casillas vacías
    local full = true
    for i = 1, 9 do
        if board[i] == 0 then full = false break end
    end
    if full then return 3 end -- Empate
    
    return 0 -- Juego sigue
end

local function resetGame()
    for i = 1, 9 do board[i] = 0 end
    winner = 0
    winningLine = nil
    gameState = "PLAYER_TURN"
    renderAll()
    cbdos.beep(1000, 50)
end

-- IA de la CPU (Gana si puede, bloquea si es necesario, o elige la mejor casilla)
local function cpuMove()
    -- 1. ¿Puede ganar en este turno?
    for _, combo in ipairs(WIN_COMBOS) do
        local a, b, c = combo[1], combo[2], combo[3]
        if board[a] == 2 and board[b] == 2 and board[c] == 0 then return c end
        if board[a] == 2 and board[c] == 2 and board[b] == 0 then return b end
        if board[b] == 2 and board[c] == 2 and board[a] == 0 then return a end
    end
    
    -- 2. ¿Necesita bloquear al jugador?
    for _, combo in ipairs(WIN_COMBOS) do
        local a, b, c = combo[1], combo[2], combo[3]
        if board[a] == 1 and board[b] == 1 and board[c] == 0 then return c end
        if board[a] == 1 and board[c] == 1 and board[b] == 0 then return b end
        if board[b] == 1 and board[c] == 1 and board[a] == 0 then return a end
    end
    
    -- 3. Tomar el centro si está libre
    if board[5] == 0 then return 5 end
    
    -- 4. Tomar esquinas libres
    local corners = {1, 3, 7, 9}
    local freeCorners = {}
    for _, idx in ipairs(corners) do
        if board[idx] == 0 then table.insert(freeCorners, idx) end
    end
    if #freeCorners > 0 then
        return freeCorners[math.random(1, #freeCorners)]
    end
    
    -- 5. Tomar cualquier casilla libre
    local freeCells = {}
    for i = 1, 9 do
        if board[i] == 0 then table.insert(freeCells, i) end
    end
    if #freeCells > 0 then
        return freeCells[math.random(1, #freeCells)]
    end
    
    return nil
end

-- 5. Bucle Principal y Manejo Táctil
renderAll()

local wasTouched = false
local running = true

while running do
    local t = cbdos.gfx.touch()
    
    if t.touched and not wasTouched then
        wasTouched = true
        
        -- A. Comprobar Botón Salir
        if t.y >= 425 and t.y <= 470 and t.x >= 170 and t.x <= 305 then
            cbdos.beep(500, 80)
            running = false
            
        -- B. Comprobar Botón Reiniciar
        elseif t.y >= 425 and t.y <= 470 and t.x >= 15 and t.x <= 150 then
            resetGame()
            
        -- C. Comprobar Toque en Tablero
        elseif gameState == "PLAYER_TURN" then
            for row = 1, 3 do
                for col = 1, 3 do
                    local cx, cy = getCellRect(col, row)
                    if t.x >= cx and t.x <= cx + CELL_SIZE and t.y >= cy and t.y <= cy + CELL_SIZE then
                        local idx = (row - 1) * 3 + col
                        if board[idx] == 0 then
                            board[idx] = 1
                            drawBoard()
                            cbdos.beep(1200, 30)
                            
                            winner = checkWinner()
                            if winner ~= 0 then
                                gameState = "GAME_OVER"
                                if winner == 1 then
                                    scorePlayer = scorePlayer + 1
                                    drawHeader()
                                    drawStatus()
                                    cbdos.beep(900, 80) cbdos.delay(90)
                                    cbdos.beep(1200, 80) cbdos.delay(90)
                                    cbdos.beep(1600, 150)
                                elseif winner == 3 then
                                    scoreTies = scoreTies + 1
                                    drawHeader()
                                    drawStatus()
                                    cbdos.beep(600, 120)
                                end
                            else
                                gameState = "CPU_TURN"
                                drawStatus()
                            end
                        end
                    end
                end
            end
        end
    elseif not t.touched then
        wasTouched = false
    end
    
    -- Turno de la CPU
    if gameState == "CPU_TURN" and running then
        cbdos.delay(350) -- Simular pensamiento
        local cpuIdx = cpuMove()
        if cpuIdx then
            board[cpuIdx] = 2
            drawBoard()
            cbdos.beep(750, 40)
            
            winner = checkWinner()
            if winner ~= 0 then
                gameState = "GAME_OVER"
                if winner == 2 then
                    scoreCPU = scoreCPU + 1
                    drawHeader()
                    drawStatus()
                    cbdos.beep(600, 100) cbdos.delay(110)
                    cbdos.beep(400, 180)
                elseif winner == 3 then
                    scoreTies = scoreTies + 1
                    drawHeader()
                    drawStatus()
                    cbdos.beep(600, 120)
                end
            else
                gameState = "PLAYER_TURN"
                drawStatus()
            end
        end
    end
    
    cbdos.delay(15) -- ~60 FPS
end

-- Limpiar y volver a CBDos
cbdos.gfx.clear(C_FONDO)
cbdos.gfx.draw_text(70, 220, "Regresando a CBDos...", C_VERDE, 2)
cbdos.delay(300)

cbdos.gfx.resume_ui()
print("Partida de Tic-Tac-Toe terminada.")
