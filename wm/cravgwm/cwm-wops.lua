--lua
-- cwm-wops - window operations: resize, maximize, iconify, close, desktops (module of cravgwm)
return function(W)
  local U = W.u
  local surfaces = W.surfaces
  function W.move(w, x, y) W.dmg(w) w.x, w.y = x, y W.dmg(w) end
  function W.resize(w, cw, ch)
    cw, ch = U.clamp(cw, w.c.minw or 80, W.sw * 2), U.clamp(ch, w.c.minh or 40, W.sh * 2)
    if cw == w.cw and ch == w.ch then return end
    W.dmg(w)
    w.cw, w.ch = cw, ch
    surfaces(w)
    if w.c.resize then w.c:resize(cw, ch) end
    W.dmg(w)
  end
  function W.maximize(w, on)
    if w.kind then return end
    if on and not w.max then
      w.rest = {w.x, w.y, w.cw, w.ch}
      w.max = true
      local x, y, ww, wh = W.work()
      local b, t = W.mets(w)
      W.dmg(w)
      w.x, w.y = x, y
      W.resize(w, ww - 2 * b, wh - t - 2 * b)
      w.fdirty = true
      W.dmg(w)
    elseif not on and w.max then
      w.max = false
      W.dmg(w)
      local r = w.rest
      w.x, w.y = r[1], r[2]
      W.resize(w, r[3], r[4])
      w.fdirty = true
      W.dmg(w)
    end
  end
  function W.iconify(w)
    if w.kind then return end
    W.dmg(w) w.icon = true
    if W.focus == w then W.focus = nil W.next_focus() end
  end
  function W.restore(w) w.icon = false w.fdirty = true W.raise(w) W.focus_win(w) end
  function W.shade(w, on) if w.kind or w.shade == on then return end W.dmg(w) w.shade = on surfaces(w) W.dmg(w) end
  function W.fullscreen(w, on)
    if w.kind or (w.full or false) == on then return end
    W.dmg(w)
    if on then w.frest = {w.x, w.y, w.cw, w.ch} w.full = true w.x, w.y = 0, 0 W.resize(w, W.sw, W.sh)
    else w.full = false local r = w.frest w.x, w.y = r[1], r[2] W.resize(w, r[3], r[4]) end
    surfaces(w) W.dmg(w) W.damage(0, 0, W.sw, W.sh)
  end
  function W.nodecor(w, on) if w.kind then return end W.dmg(w) local ox, oy = w.x, w.y w.nodecor = on surfaces(w) W.dmg(w) end
  function W.next_focus()                    -- сфокусировать верхнее видимое обычное окно
    for i = #W.wins, 1, -1 do
      local w = W.wins[i]
      if not w.kind and W.visible(w) and not w.nofocus then W.focus_win(w) return end
    end
    W.focus_win(nil)
  end
  function W.close(w)
    if w.c.close and w.c:close() == false then return end
    if W.focus == w then W.focus = nil end
    for i, v in ipairs(W.wins) do if v == w then table.remove(W.wins, i) break end end
    for i, v in ipairs(W.mru) do if v == w then table.remove(W.mru, i) break end end
    W.dmg(w)
    if w.kind == "desktop" then W.damage(0, 0, W.sw, W.sh) end
    if W.comp and W.comp.closed(w) then w.fs = nil else if w.fs then gfx.free(w.fs) end end
    if w.cs then gfx.free(w.cs) end
    if not W.focus then W.next_focus() end
  end
  function W.goto(n)
    n = U.clamp(n, 1, W.cfg.desktops)
    if n == W.desk then return end
    W.desk = n
    W.damage(0, 0, W.sw, W.sh)
    if W.focus and not W.visible(W.focus) then W.focus = nil end
    if not W.focus then W.next_focus() end
  end
  function W.send(w, n) if w.kind then return end w.desk = U.clamp(n, 1, W.cfg.desktops) W.damage(0, 0, W.sw, W.sh) if not W.visible(w) then if W.focus == w then W.focus = nil W.next_focus() end end end
  function W.cycle(dir)                      -- Alt+Tab по порядку использования
    local list = {}
    for _, w in ipairs(W.mru) do if W.visible(w) and not w.kind then list[#list + 1] = w end end
    if #list < 2 then return end
    local nw = dir > 0 and list[2] or list[#list]
    W.raise(nw) W.focus_win(nw)
  end
  function W.pick(px, py)                    -- окно под точкой и часть окна
    for i = #W.wins, 1, -1 do
      local w = W.wins[i]
      if W.visible(w) and w.kind ~= "desktop" then
        local x, y, fw, fh = W.frect(w)
        if U.inside(px, py, x, y, fw, fh) then return w, W.deco.hit(w, px - x, py - y), px - x, py - y end
      end
    end
    return nil, "Root", px, py
  end
end
