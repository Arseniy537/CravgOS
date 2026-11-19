--lua
-- cwm-input - keyboard and mouse: bindings, focus, move and resize (module of cravgwm)
return function(W)
  local U = W.u
  local I = {press = {}}
  W.input = I
  local EDGE = {Top = {0, -1}, Bottom = {0, 1}, Left = {-1, 0}, Right = {1, 0}, TLCorner = {-1, -1}, TRCorner = {1, -1}, BLCorner = {-1, 1}, BRCorner = {1, 1}}
  local BITS = {1, 2, 4}
  function I.start(kind, w, ev, ctx)
    local x, y = ev and ev.x or W.mx, ev and ev.y or W.my
    if kind == "move" then I.drag = {k = "move", w = w, ox = x - w.x, oy = y - w.y} return end
    local e = EDGE[ctx or ""]
    if not e then
      local fx, fy, fw, fh = W.frect(w)
      e = {x < fx + fw // 3 and -1 or x > fx + 2 * fw // 3 and 1 or 0, y < fy + fh // 3 and -1 or y > fy + 2 * fh // 3 and 1 or 0}
      if e[1] == 0 and e[2] == 0 then e = {1, 1} end
    end
    I.drag = {k = "resize", w = w, e = e, x0 = x, y0 = y, fx = w.x, fy = w.y, cw = w.cw, ch = w.ch}
  end
  local function fire(names, how, btn, mods, c)
    local hit = false
    for _, n in ipairs(names) do
      for _, b in ipairs(W.cfg.mouse[n] or {}) do
        if b.b == btn and b.how == how and b.m == mods then W.do_acts(b.acts, c) hit = true end
      end
    end
    return hit
  end
  local function names(w, ctx) if not w then return {"Root"} end return {ctx, "Frame"} end
  function I.key(ev)
    local k, m = ev.base or ev.key, ev.mods
    if k == 8 and m == 6 then W.quit = true return end                     -- Ctrl+Alt+BackSpace: аварийный выход
    if W.grab and W.grab.key and W.grab:key(ev) then return end
    for _, kb in ipairs(W.cfg.keys) do
      if kb.k == k and kb.m == m then W.do_acts(kb.acts, {w = W.focus, ev = ev}) return end
    end
    local f = W.focus
    if f and f.c.key and f.c:key(ev.key, m, ev) then f.cdirty = true end
  end
  local function client(w, ev, t, b)
    if not w.c.mouse then return end
    local bd, th = W.mets(w)
    if w.c:mouse({t = t, x = ev.x - w.x - bd, y = ev.y - w.y - bd - th, btn = b or 0, mods = ev.mods, wheel = ev.wheel}) then w.cdirty = true end
  end
  local function drag(ev)
    local d = I.drag
    local w = d.w
    if d.k == "move" then
      local wx, wy, ww, wh = W.work()
      local fw, fh = W.fsize(w)
      local nx, ny = ev.x - d.ox, ev.y - d.oy
      if math.abs(nx - wx) < 10 then nx = wx elseif math.abs(nx + fw - wx - ww) < 10 then nx = wx + ww - fw end
      if math.abs(ny - wy) < 10 then ny = wy elseif math.abs(ny + fh - wy - wh) < 10 then ny = wy + wh - fh end
      W.move(w, nx, math.max(0, ny))
    else
      local dx, dy = ev.x - d.x0, ev.y - d.y0
      local ncw, nch = d.cw + d.e[1] * dx, d.ch + d.e[2] * dy
      if d.e[1] == 0 then ncw = d.cw end
      if d.e[2] == 0 then nch = d.ch end
      W.resize(w, ncw, nch)
      W.move(w, d.e[1] < 0 and d.fx + d.cw - w.cw or d.fx, d.e[2] < 0 and d.fy + d.ch - w.ch or d.fy)
    end
  end
  function I.mouse(ev)
    W.cursor_move(ev.x, ev.y)
    if W.grab and W.grab.mouse and W.grab:mouse(ev) then return end
    if I.drag then
      if ev.up ~= 0 or ev.btn == 0 then I.drag = nil I.press = {} else drag(ev) end
      return
    end
    local mods = ev.mods
    local w, ctx = W.pick(ev.x, ev.y)
    local c = {w = w, ev = ev, ctx = ctx}
    if (ev.dx ~= 0 or ev.dy ~= 0) then
      if W.cfg.focus.model == "follow" and w and not w.kind and w ~= W.focus then if W.cfg.focus.raise then W.raise(w) end W.focus_win(w) end
      for b, p in pairs(I.press) do
        if not p.drag and (math.abs(ev.x - p.x) > 3 or math.abs(ev.y - p.y) > 3) then
          p.drag = true
          fire(names(p.w, p.ctx), "Drag", b, p.mods, {w = p.w, ev = p, ctx = p.ctx})
        end
      end
      if I.drag then drag(ev) return end
      local t = I.cap or (ctx == "Client" and w)
      if t then client(t, ev, "move", 0) end
    end
    for _, b in ipairs(BITS) do
      if ev.down & b ~= 0 then
        local fo = W.focus
        if fo and fo.kind == "overlay" and w ~= fo then W.close(fo) w, ctx = W.pick(ev.x, ev.y) c.w, c.ctx = w, ctx end
        if w and W.cfg.focus.model ~= "none" and not w.nofocus then
          if W.cfg.focus.raise or ctx ~= "Client" then W.raise(w) end
          if not w.kind or w.kind == "overlay" then W.focus_win(w) end
        end
        I.press[b] = {x = ev.x, y = ev.y, w = w, ctx = ctx, mods = mods, drag = false}
        local used = fire(names(w, ctx), "Press", b, mods, c)
        if w and ctx == "Client" and not used then I.cap = w client(w, ev, "down", b) end
      elseif ev.up & b ~= 0 then
        local p = I.press[b]
        I.press[b] = nil
        if p and not p.drag then
          fire(names(p.w, p.ctx), "Click", b, p.mods, {w = p.w, ev = ev, ctx = p.ctx})
          local l, now = I.last, os.ms()
          if l and l.b == b and l.ctx == p.ctx and l.w == p.w and now - l.t < 400 and math.abs(l.x - ev.x) < 6 and math.abs(l.y - ev.y) < 6 then
            fire(names(p.w, p.ctx), "DoubleClick", b, p.mods, {w = p.w, ev = ev, ctx = p.ctx})
            I.last = nil
          else I.last = {b = b, ctx = p.ctx, w = p.w, t = now, x = ev.x, y = ev.y} end
        end
        fire(names(w, ctx), "Release", b, mods, c)
        if I.cap then client(I.cap, ev, "up", b) I.cap = nil end
      end
    end
    if ev.wheel ~= 0 then
      local b = ev.wheel * W.cfg.wheel > 0 and "up" or "down"
      if not fire(names(w, ctx), "Press", b, mods, c) and w and ctx == "Client" then client(w, ev, "wheel", 0) end
    end
  end
end
