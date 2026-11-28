--lua
-- cwm-comp - compositor (cravgcomp): shadows, opacity, fading, rounded corners. Config: /etc/cravgwm/cravgcomp.xml
return function(W)
  local U = W.u
  local C = {ghost = {}, anim = false}
  W.comp = C
  local cf = {shadow = false, sr = 14, sa = 110, sx = 0, sy = 5, sc = 0, fade = false, fin = 150, fout = 110, oa = 100, oi = 100, cr = 0, rules = {}}
  C.cf = cf
  function C.load()
    local root, err = W.xml.load("/etc/cravgwm/cravgcomp.xml")
    if not root then W.log("cravgcomp: " .. err) return end
    for _, n in ipairs(root.k) do
      local a = n.a
      if n.tag == "shadow" then
        cf.shadow = U.bool(a.enabled, true) cf.sr = U.num(a.radius, cf.sr) cf.sa = U.clamp(U.num(a.opacity, cf.sa), 0, 255)
        cf.sx, cf.sy, cf.sc = U.num(a.x, 0), U.num(a.y, cf.sy), U.color(a.color, 0)
      elseif n.tag == "fade" then cf.fade = U.bool(a.enabled, true) cf.fin = U.num(a["in"], cf.fin) cf.fout = U.num(a.out, cf.fout)
      elseif n.tag == "opacity" then cf.oa = U.clamp(U.num(a.active, 100), 0, 100) cf.oi = U.clamp(U.num(a.inactive, 100), 0, 100)
      elseif n.tag == "corners" then cf.cr = U.num(a.radius, 0)
      elseif n.tag == "rule" then cf.rules[a.class or "*"] = a end
    end
  end
  local function rule(w) return cf.rules[w.class] or cf.rules["*"] or {} end
  local function opts(w)                       -- эффективные параметры окна
    local r = rule(w)
    local plain = w.kind == "desktop"
    local sh = not plain and not w.max and not w.full and U.bool(r.shadow, cf.shadow)
    local op = r.opacity and U.num(r.opacity, 100) or ((W.focus == w or w.kind) and cf.oa or cf.oi)
    if w.alpha then op = w.alpha end
    return sh, U.clamp(op, 0, 100) * 255 // 100, (plain or w.kind == "dock" or w.max or w.full) and 0 or U.num(r.corners, cf.cr)
  end
  function C.opened(w)
    w.cp = {fade = 255}
    if cf.fade and w.kind ~= "desktop" then w.cp.fade, w.cp.t0, w.cp.dur, w.cp.to = 0, os.ms(), cf.fin, 255 C.anim = true end
  end
  function C.after_draw(w)                     -- скруглить углы кадра (прозрачные пиксели)
    local _, _, r = opts(w)
    if r < 1 or not w.fs then return end
    local fw, fh = W.fsize(w)
    r = math.min(r, fw // 2, fh // 2)
    gfx.target(w.fs)
    for j = 0, r - 1 do
      local run = r - math.floor(math.sqrt(r * r - (r - j) * (r - j)))
      if run > 0 then
        gfx.raw(0, j, run, 1, 0) gfx.raw(fw - run, j, run, 1, 0) gfx.raw(0, fh - 1 - j, run, 1, 0) gfx.raw(fw - run, fh - 1 - j, run, 1, 0)
      end
    end
    gfx.target(0)
  end
  -- тень с учётом смещения: кадр окна вырезается из неё точно (под окном тени нет)
  local function shadow(w, fw, fh)
    local cp = w.cp
    if cp.sh and cp.shw == fw and cp.shh == fh then return cp.sh end
    if cp.sh then gfx.free(cp.sh) cp.sh = nil end
    local mx, my = cf.sr + math.abs(cf.sx), cf.sr + math.abs(cf.sy)
    local s = gfx.surface(fw + 2 * mx, fh + 2 * my)
    if not s then return nil end
    gfx.target(s)
    gfx.shadow(mx + cf.sx, my + cf.sy, fw, fh, cf.sr, cf.sa, cf.sc, 2)
    gfx.raw(mx, my, fw, fh, 0)
    gfx.target(0)
    cp.sh, cp.shw, cp.shh, cp.mx, cp.my = s, fw, fh, mx, my
    return s
  end
  function C.paint(w)
    local cp = w.cp
    if not cp then C.opened(w) cp = w.cp end
    local sh, op, cr = opts(w)
    local fw, fh = W.fsize(w)
    local f = cp.fade
    if sh and cf.sr > 0 then
      local s = shadow(w, fw, fh)
      if s then gfx.blit(s, w.x - cp.mx, w.y - cp.my, f) end
    end
    if op * f >= 255 * 255 and cr == 0 then gfx.blit(w.fs, w.x, w.y) else gfx.blit(w.fs, w.x, w.y, op * f // 255) end
  end
  function C.closed(w)                         -- окно закрыто: оставить кадр на время затухания
    local cp = w.cp
    if cp and cp.sh and not (cf.fade and w.fs) then gfx.free(cp.sh) end
    if not (cf.fade and w.fs and cf.fout > 0) then return false end
    local _, op = opts(w)
    local fw, fh = W.fsize(w)
    local g = {fs = w.fs, sh = cp and cp.sh, x = w.x, y = w.y, fw = fw, fh = fh, t0 = os.ms(), dur = cf.fout, op = op, mx = cp and cp.mx or 0, my = cp and cp.my or 0}
    local sh = opts(w)
    g.noshadow = not sh
    C.ghost[#C.ghost + 1] = g
    C.anim = true
    return true
  end
  function C.ghosts()
    for _, g in ipairs(C.ghost) do
      local k = 255 - U.clamp((os.ms() - g.t0) * 255 // g.dur, 0, 255)
      if g.sh and not g.noshadow then gfx.blit(g.sh, g.x - g.mx, g.y - g.my, k) end
      gfx.blit(g.fs, g.x, g.y, g.op * k // 255)
    end
  end
  function C.pending() return C.anim end
  function C.step()                            -- шаг анимации: перерисовать затронутые окна
    local now, busy = os.ms(), false
    for _, w in ipairs(W.wins) do
      local cp = w.cp
      if cp and cp.t0 then
        local k = U.clamp((now - cp.t0) * 255 // math.max(cp.dur, 1), 0, 255)
        cp.fade = cp.to == 255 and k or 255 - k
        if k >= 255 then cp.t0 = nil else busy = true end
        W.dmg(w)
      end
    end
    for i = #C.ghost, 1, -1 do
      local g = C.ghost[i]
      W.damage(g.x - cf.sr - 10, g.y - cf.sr - 10, g.fw + 2 * cf.sr + 30, g.fh + 2 * cf.sr + 30)
      if now - g.t0 >= g.dur then
        gfx.free(g.fs) if g.sh then gfx.free(g.sh) end
        table.remove(C.ghost, i)
      else busy = true end
    end
    C.anim = busy
  end
  function C.tick(dt)
    if W.focus ~= C.lastfocus then
      local old = C.lastfocus
      C.lastfocus = W.focus
      if cf.oa ~= cf.oi then
        if old and old.cp and not old.kind then W.dmg(old) end
        if W.focus and W.focus.cp and not W.focus.kind then W.dmg(W.focus) end
      end
    end
  end
end
