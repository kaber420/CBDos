pico-8 cartridge // http://www.pico-8.com
version 16
__lua__
-- pong retro p8 para cbdos
-- motor lua 5.4 & pico-8

function _init()
    p1 = {y=50, h=18, w=3, spd=2, score=0}
    p2 = {y=50, h=18, w=3, spd=1.5, score=0}
    ball = {x=64, y=64, r=2, vx=1.5, vy=1.0}
    t = 0
end

function _update60()
    t += 1

    -- Control Jugador 1 (D-Pad Arriba / Abajo)
    if btn(2) and p1.y > 10 then p1.y -= p1.spd end
    if btn(3) and p1.y + p1.h < 118 then p1.y += p1.spd end

    -- IA Oponente (Jugador 2)
    if p2.y + p2.h/2 < ball.y - 2 and p2.y + p2.h < 118 then
        p2.y += p2.spd
    elseif p2.y + p2.h/2 > ball.y + 2 and p2.y > 10 then
        p2.y -= p2.spd
    end

    -- Mover Pelota
    ball.x += ball.vx
    ball.y += ball.vy

    -- Rebote Superior e Inferior
    if ball.y <= 12 or ball.y >= 118 then
        ball.vy = -ball.vy
        sfx(0)
    end

    -- Rebote Paleta Jugador 1
    if ball.x <= 14 and ball.x >= 8 and ball.y >= p1.y and ball.y <= p1.y + p1.h then
        ball.vx = abs(ball.vx) * 1.05
        ball.vy = (ball.y - (p1.y + p1.h/2)) * 0.2
        sfx(1)
    end

    -- Rebote Paleta Jugador 2
    if ball.x >= 114 and ball.x <= 120 and ball.y >= p2.y and ball.y <= p2.y + p2.h then
        ball.vx = -abs(ball.vx) * 1.05
        ball.vy = (ball.y - (p2.y + p2.h/2)) * 0.2
        sfx(1)
    end

    -- Punto Jugador 2
    if ball.x < 0 then
        p2.score += 1
        reset_ball(1)
    end

    -- Punto Jugador 1
    if ball.x > 128 then
        p1.score += 1
        reset_ball(-1)
    end
end

function reset_ball(dir)
    ball.x = 64
    ball.y = 64
    ball.vx = dir * 1.5
    ball.vy = (rnd(2) - 1) * 1.5
end

function _draw()
    cls(0)

    -- Lineas de campo y red
    rect(0, 10, 127, 120, 5)
    for y=12, 116, 6 do
        rectfill(63, y, 64, y+2, 5)
    end

    -- Marcador
    print(p1.score, 45, 14, 7)
    print(p2.score, 78, 14, 7)

    -- Paleta 1 (Verde)
    rectfill(8, p1.y, 8 + p1.w, p1.y + p1.h, 11)

    -- Paleta 2 (Rosa)
    rectfill(116, p2.y, 116 + p2.w, p2.y + p2.h, 14)

    -- Pelota (Amarilla)
    circfill(ball.x, ball.y, ball.r, 10)
end
