--lua
-- cwm-win - window model: list, stacking, focus, geometry (module of cravgwm)
return function(W)
  local U = W.u
  W.wins, W.mru, W.desk = {}, {}, 1
  local seq = 0
  local LAYER = {desktop = 0, below = 1, normal = 2, above = 3, dock = 4, overlay = 5}
  function W.mets(w)                         -- рамка: толщина, высота заголовка
    if w.nodecor or w.full or w.kind then return 0, 0 end
    return W.cfg.theme.border, W.cfg.theme.title
  end
  function W.fsize(w)
    local b, t = W.mets(w)
    return w.cw + 2 * b, (w.shade and 0 or w.ch) + t + 2 * b
  end
  function W.visible(w) return not w.icon and (w.desk == 0 or w.desk == W.desk) end
  function W.frect(w) local fw, fh = W.fsize(w) return w.x, w.y, fw, fh end
  function W.dmg(w) local x, y, fw, fh = W.frect(w) W.damage(x - 40, y - 40, fw + 80, fh + 100) end   -- запас под тень
  local function relayer(w)
    for i, v in ipairs(W.wins) do if v == w then table.remove(W.wins, i) break end end
    local at = #W.wins + 1
    for i, v in ipairs(W.wins) do if v.layer > w.layer then at = i break end end
    table.insert(W.wins, at, w)
  end
  function W.raise(w) relayer(w) W.dmg(w) end
  function W.lower(w)
    for i, v in ipairs(W.wins) do if v == w then table.remove(W.wins, i) break end end
    local at = 1
    for i, v in ipairs(W.wins) do if v.layer < w.layer then at = i + 1 end end
    table.insert(W.wins, at, w)
    W.dmg(w)
  end
  local function surfaces(w)
    local fw, fh = W.fsize(w)
    if w.cs then gfx.free(w.cs) end
    if w.fs then gfx.free(w.fs) end
    w.cs = gfx.surface(w.cw, w.ch)
    w.fs = gfx.surface(fw, fh)
    w.cdirty, w.fdirty = true, true
    if not w.cs or not w.fs then W.log("out of memory for window surfaces") end
  end
  W.surfaces = surfaces
  W.relayer = relayer
  function W.work()                          -- рабочая область без панелей
    local x, y, r, b = 0, 0, W.sw, W.sh
    for _, w in ipairs(W.wins) do
      if w.strut and W.visible(w) then
        if w.strut.edge == "top" then y = math.max(y, w.y + w.strut.size) elseif w.strut.edge == "bottom" then b = math.min(b, w.y)
        elseif w.strut.edge == "left" then x = math.max(x, w.x + w.strut.size) else r = math.min(r, w.x) end
      end
    end
    return x, y, r - x, b - y
  end
  local function rule(w)
    for _, r in ipairs(W.cfg.rules) do
      if r.class == "*" or r.class == w.class then
        if r.desktop then w.desk = tonumber(r.desktop) or w.desk end
        if r.decor then w.nodecor = not U.bool(r.decor, true) end
        if r["position.x"] then w.px, w.py = tonumber(r["position.x"]), tonumber(r["position.y"]) end
        if r["size.w"] then w.cw, w.ch = tonumber(r["size.w"]), tonumber(r["size.h"]) or w.ch end
        if r.maximized then w.wantmax = U.bool(r.maximized, false) end
        if r.layer then w.layer = LAYER[r.layer] or w.layer end
        if r.focus then w.nofocus = not U.bool(r.focus, true) end
        if r.alpha then w.alpha = tonumber(r.alpha) end
        if r.omnipresent then if U.bool(r.omnipresent, false) then w.desk = 0 end end
      end
    end
  end
  -- c - клиентский объект приложения (title, w, h, kind...), возвращает окно
  function W.add(c)
    seq = seq + 1
    local w = {id = seq, c = c, title = c.title or "?", class = c.class or "", kind = c.kind, x = 0, y = 0, cw = c.w or 400, ch = c.h or 300, desk = W.desk, layer = LAYER.normal}
    if c.kind == "dock" then w.layer, w.desk, w.strut = LAYER.dock, 0, {edge = c.edge or "bottom", size = c.size or 28}
    elseif c.kind == "desktop" then w.layer, w.desk = LAYER.desktop, 0
    elseif c.kind == "overlay" then w.layer = LAYER.overlay end
    c.win = w
    if c.px then w.px, w.py = c.px, c.py end
    rule(w)
    w.cw, w.ch = U.clamp(w.cw, c.minw or 80, W.sw), U.clamp(w.ch, c.minh or 40, W.sh)
    if w.kind == "dock" then
      local e = w.strut.edge
      if e == "top" or e == "bottom" then w.cw, w.ch = W.sw, w.strut.size else w.cw, w.ch = w.strut.size, W.sh end
      w.x = e == "right" and W.sw - w.cw or 0
      w.y = e == "bottom" and W.sh - w.ch or 0
    elseif w.kind == "desktop" then w.cw, w.ch = W.sw, W.sh
    else
      local wx, wy, ww, wh = W.work()
      local fw, fh = W.fsize(w)
      if w.px then w.x, w.y = w.px, w.py
      elseif W.cfg.place == "cascade" then local n = #W.wins % 8 w.x, w.y = wx + 30 + n * 26, wy + 30 + n * 26
      elseif W.cfg.place == "mouse" then local mx, my = gfx.mouse() w.x, w.y = mx - fw // 2, my - 10
      else w.x, w.y = wx + (ww - fw) // 2, wy + (wh - fh) // 2 end
      w.x, w.y = U.clamp(w.x, 0, math.max(0, W.sw - fw)), U.clamp(w.y, wy, math.max(wy, W.sh - fh))
    end
    surfaces(w)
    relayer(w)
    if w.kind == "overlay" or w.kind == "dock" or w.kind == "desktop" then
      if c.start then c:start(w) end
      if w.kind == "overlay" then W.focus_win(w) end
    else
      if w.wantmax then W.maximize(w, true) end
      if not w.nofocus then W.focus_win(w) end
    end
    if c.kind == "desktop" then W.damage(0, 0, W.sw, W.sh) else W.dmg(w) end
    if W.comp then W.comp.opened(w) end
    return w
  end
  function W.focus_win(w)
    local old = W.focus
    if old == w then return end
    W.focus = w
    if old then old.fdirty = true if old.c.focus then old.c:focus(false) end W.dmg(old) end
    if w then
      for i, v in ipairs(W.mru) do if v == w then table.remove(W.mru, i) break end end
      table.insert(W.mru, 1, w)
      w.fdirty = true
      if w.c.focus then w.c:focus(true) end
      W.dmg(w)
    end
  end
end
