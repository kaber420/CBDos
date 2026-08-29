pico-8 cartridge // http://www.pico-8.com
version 16
__lua__
-- space invaders p8 para cbdos
-- motor lua 5.4 & pico-8

function _init()
    p = {x=60, y=110, spd=2, w=8, h=8}
    bullets = {}
    enemies = {}
    particles = {}
    score = 0
    lives = 3
    game_over = false
    t = 0
    dir = 1
    enemy_step_t = 0
    spawn_enemies()
end

function spawn_enemies()
    enemies = {}
    for ey=0,2 do
        for ex=0,5 do
            add(enemies, {
                x = 16 + ex * 16,
                y = 15 + ey * 12,
                alive = true,
                type = ey + 1
            })
        end
    end
end

function _update60()
    if game_over then
        if btnp(4) or btnp(5) then _init() end
        return
    end

    t += 1

    -- Control del jugador (D-Pad y botones O/X)
    if btn(0) and p.x > 4 then p.x -= p.spd end
    if btn(1) and p.x < 116 then p.x += p.spd end

    -- Disparo (Boton O o X)
    if (btnp(4) or btnp(5)) and #bullets < 3 then
        add(bullets, {x=p.x+3, y=p.y-2, spd=3})
        sfx(0)
    end

    -- Actualizar balas
    for b in all(bullets) do
        b.y -= b.spd
        if b.y < 0 then del(bullets, b) end
    end

    -- Movimiento de invasores
    enemy_step_t += 1
    if enemy_step_t > 20 then
        enemy_step_t = 0
        local shift_down = false
        for e in all(enemies) do
            if e.alive then
                if (dir == 1 and e.x > 108) or (dir == -1 and e.x < 8) then
                    shift_down = true
                end
            end
        end
        if shift_down then
            dir = -dir
            for e in all(enemies) do
                e.y += 4
                if e.alive and e.y >= p.y - 8 then
                    lives = 0
                    game_over = true
                end
            end
        else
            for e in all(enemies) do
                e.x += dir * 4
            end
        end
    end

    -- Colisiones bala vs invasor
    for b in all(bullets) do
        for e in all(enemies) do
            if e.alive and b.x >= e.x and b.x <= e.x+8 and b.y >= e.y and b.y <= e.y+8 then
                e.alive = false
                del(bullets, b)
                score += 100
                sfx(1)
                -- Particulas de explosion
                for i=1,8 do
                    add(particles, {x=e.x+4, y=e.y+4, vx=(rnd(2)-1)*1.5, vy=(rnd(2)-1)*1.5, life=15, col=8+flr(rnd(3))})
                end
                break
            end
        end
    end

    -- Actualizar particulas
    for pt in all(particles) do
        pt.x += pt.vx
        pt.y += pt.vy
        pt.life -= 1
        if pt.life <= 0 then del(particles, pt) end
    end

    -- Victoria: reaparecer oleada
    local any_alive = false
    for e in all(enemies) do
        if e.alive then any_alive = true break end
    end
    if not any_alive then
        spawn_enemies()
    end
end

function _draw()
    cls(0)

    -- Fondo de estrellas parpadeantes
    for i=0,20 do
        local sx = (i * 37 + t/2) % 128
        local sy = (i * 53) % 128
        local col = (i%3 == 0) and 7 or 5
        pset(sx, sy, col)
    end

    -- HUD
    print("SCORE:"..score, 4, 4, 7)
    print("LIVES:"..lives, 88, 4, 11)
    line(0, 12, 127, 12, 5)

    -- Dibujar jugador (Nave)
    rectfill(p.x+2, p.y, p.x+5, p.y+2, 11)
    rectfill(p.x, p.y+3, p.x+7, p.y+6, 3)
    pset(p.x+1, p.y+2, 7)
    pset(p.x+6, p.y+2, 7)

    -- Dibujar balas
    for b in all(bullets) do
        rectfill(b.x, b.y, b.x+1, b.y+3, 10)
    end

    -- Dibujar invasores
    for e in all(enemies) do
        if e.alive then
            local c = (e.type == 1) and 8 or ((e.type == 2) and 9 or 12)
            local anim = (flr(t/15)%2 == 0) and 0 or 1
            rectfill(e.x+1, e.y+1, e.x+6, e.y+6, c)
            pset(e.x+2, e.y+3, 7)
            pset(e.x+5, e.y+3, 7)
            if anim == 0 then
                pset(e.x, e.y+6, c)
                pset(e.x+7, e.y+6, c)
            else
                pset(e.x, e.y+1, c)
                pset(e.x+7, e.y+1, c)
            end
        end
    end

    -- Dibujar particulas
    for pt in all(particles) do
        pset(pt.x, pt.y, pt.col)
    end

    -- Game Over
    if game_over then
        rectfill(20, 48, 108, 76, 0)
        rect(20, 48, 108, 76, 8)
        print("GAME OVER", 44, 54, 8)
        print("PULSA (O) / (X)", 32, 64, 7)
    end
end
