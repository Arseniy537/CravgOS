--lua
-- cwm-paint - drawing: damage, background, windows, overlays, cursor (module of cravgwm)
return function(W)
  local U = W.u
  W.logs = {}
  W.mx, W.my = 0, 0
  function W.damage(x, y, w, h)
    local x1, y1 = U.clamp(x, 0, W.sw), U.clamp(y, 0, W.sh)
    local x2, y2 = U.clamp(x + w, 0, W.sw), U.clamp(y + h, 0, W.sh)
    if x2 <= x1 or y2 <= y1 then return end
    if not W.dx then W.dx, W.dy, W.dx2, W.dy2 = x1, y1, x2, y2 return end
    W.dx, W.dy, W.dx2, W.dy2 = math.min(W.dx, x1), math.min(W.dy, y1), math.max(W.dx2, x2), math.max(W.dy2, y2)
  end
  function W.log(msg)
    W.logs[#W.logs + 1] = {msg, os.ms()}
    if #W.logs > 4 then table.remove(W.logs, 1) end
    if W.sw then W.damage(0, 0, W.sw, 16 * 5 + 8) end
  end
  local ARROW = {"X", "XX", "X.X", "X..X", "X...X", "X....X", "X.....X", "X......X", "X.......X", "X........X", "X.....XXXXX", "X..X..X",
    "X.X X..X", "XX  X..X", "X    X..X", "     X..X", "      X..X", "      X..X", "       XX"}
  function W.mkcursor()
    W.cur = gfx.surface(12, 19)
    W.cur_bg = gfx.surface(12, 19)
    W.cdrawn = false
    gfx.target(W.cur)
    for y, row in ipairs(ARROW) do
      for x = 1, #row do
        local c = row:sub(x, x)
        if c == "X" then gfx.raw(x - 1, y - 1, 1, 1, 0xFF000000) elseif c == "." then gfx.raw(x - 1, y - 1, 1, 1, 0xFFFFFFFF) end
      end
    end
    gfx.target(0)
  end
  function W.cursor_move(x, y)
    W.mx, W.my = x, y
  end
  function W.render()
    for _, w in ipairs(W.wins) do
      if W.visible(w) and (w.cdirty or w.fdirty) then
        if w.cdirty and w.c.draw and not w.shade then gfx.target(w.cs) gfx.clip() w.c:draw(w.cs, w.cw, w.ch) gfx.target(0) end
        w.cdirty = false
        if w.fs and w.cs then W.deco.draw(w) if W.comp then W.comp.after_draw(w) end end
        w.fdirty = false
        local x, y, fw, fh = W.frect(w)
        W.damage(x, y, fw, fh)
      end
    end
    if W.comp and W.comp.pending() then W.comp.step() end
    local mx, my = W.mx, W.my
    local has_cur = W.cur and W.cur_bg
    local need_cur = has_cur and (not W.cdrawn or mx ~= W.cmx or my ~= W.cmy)
    if not W.dx and not need_cur then return end
    gfx.target(0)
    if not W.dx then
      local ox, oy = W.cmx, W.cmy
      gfx.clip()
      if W.cdrawn then gfx.blit(W.cur_bg, ox, oy) end
      gfx.target(W.cur_bg) gfx.clip() gfx.blit(0, 0, 0, nil, mx, my, 12, 19) gfx.target(0)
      gfx.blit(W.cur, mx, my, 255)
      if W.cdrawn then
        if math.abs(ox - mx) < 12 and math.abs(oy - my) < 19 then
          local fx, fy = math.min(ox, mx), math.min(oy, my)
          gfx.flip(fx, fy, math.max(ox, mx) + 12 - fx, math.max(oy, my) + 19 - fy)
        else
          gfx.flip(ox, oy, 12, 19)
          gfx.flip(mx, my, 12, 19)
        end
      else gfx.flip(mx, my, 12, 19) end
      W.cmx, W.cmy, W.cdrawn = mx, my, true
      return
    end
    local x, y, w, h = W.dx, W.dy, W.dx2 - W.dx, W.dy2 - W.dy
    W.dx = nil
    local ox, oy = W.cmx, W.cmy
    local moved = W.cdrawn and (ox ~= mx or oy ~= my)
    local old_in = W.cdrawn and ox >= x and oy >= y and ox + 12 <= x + w and oy + 19 <= y + h
    local old_hit = W.cdrawn and ox + 12 > x and oy + 19 > y and ox < x + w and oy < y + h
    local touch_cur = has_cur and (not W.cdrawn or moved or old_hit)
    if W.cdrawn and touch_cur and not old_in then gfx.clip() gfx.blit(W.cur_bg, ox, oy) end
    gfx.clip(x, y, w, h)
    local has_bg = false
    for _, win in ipairs(W.wins) do
      if win.kind == "desktop" and W.visible(win) and win.fs and win.x <= x and win.y <= y and win.x + win.cw >= x + w and win.y + win.ch >= y + h then has_bg = true break end
    end
    if not has_bg then gfx.fill(x, y, w, h, W.cfg.theme.root) end
    for _, win in ipairs(W.wins) do
      if W.visible(win) and win.fs then
        local fx, fy, fw, fh = W.frect(win)
        if win.kind == "desktop" or U.inside(fx + fw // 2, fy + fh // 2, x - fw // 2 - 60, y - fh // 2 - 60, w + fw + 120, h + fh + 120) then
          if W.comp then W.comp.paint(win) else gfx.blit(win.fs, win.x, win.y) end
        end
      end
    end
    if W.comp then W.comp.ghosts() end
    if W.grab and W.grab.paint then W.grab:paint() end
    if #W.logs > 0 then
      local now, ly = os.ms(), 0
      for _, l in ipairs(W.logs) do
        if now - l[2] < 15000 then
          gfx.fill(0, ly, #l[1] * 8 + 8, 16, 0x400000, 220) gfx.text(4, ly, l[1], 0xFF8080)
          ly = ly + 16
        end
      end
    end
    gfx.clip()
    if touch_cur then
      gfx.target(W.cur_bg) gfx.clip() gfx.blit(0, 0, 0, nil, mx, my, 12, 19) gfx.target(0)
      gfx.blit(W.cur, mx, my, 255)
      W.cmx, W.cmy, W.cdrawn = mx, my, true
    end
    gfx.flip(x, y, w, h)
    if touch_cur then
      if moved and not old_in then gfx.flip(ox, oy, 12, 19) end
      if not (mx >= x and my >= y and mx + 12 <= x + w and my + 19 <= y + h) then gfx.flip(mx, my, 12, 19) end
    end
  end
end
