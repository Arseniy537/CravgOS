--lua
-- cwm-conf - reads /etc/cravgwm/rc.xml (module of cravgwm)
return function(W)
  local U, X = W.u, W.xml
  local C = {path = "/etc/cravgwm/"}
  W.conf = C
  local KEYS = {Tab = 9, Return = 10, Enter = 10, Escape = 27, space = 32, BackSpace = 8, Delete = "DEL", Home = "HOME", End = "END",
    Prior = "PGUP", Next = "PGDN", Insert = "INS", Left = "LEFT", Right = "RIGHT", Up = "UP", Down = "DOWN"}
  local function mods(spec)
    local m, i = 0, 1
    while true do
      local p = spec:sub(i, i + 1)
      if p == "S-" then m = m | 1 elseif p == "C-" then m = m | 2 elseif p == "A-" or p == "M-" then m = m | 4 elseif p == "W-" then m = m | 8 else break end
      i = i + 2
    end
    return m, spec:sub(i)
  end
  -- "A-Tab" -> {k = key code, m = modifiers: 1 shift, 2 ctrl, 4 alt, 8 super}
  function C.key(spec)
    local m, name = mods(spec)
    local k = KEYS[name]
    if type(k) == "string" then k = term[k] end
    local fn = name:sub(1, 1) == "F" and tonumber(name:sub(2))
    if not k and fn then k = term.F1 + fn - 1 end
    if not k and #name == 1 then k = name:lower():byte() end
    if k then return {k = k, m = m} end
  end
  local BTN = {Left = 1, Right = 2, Middle = 4, Up = "up", Down = "down"}
  function C.btn(spec) local m, name = mods(spec) return m, BTN[name] end
  -- <action name="X"><arg>v</arg></action> -> {name = "X", a = {arg = "v"}}
  local function acts(node)
    local r = {}
    for _, a in ipairs(node:list("action")) do
      local t = {name = a.a.name or "", a = {}}
      for k, v in pairs(a.a) do t.a[k] = v end
      for _, c in ipairs(a.k) do t.a[c.tag] = c.t end
      r[#r + 1] = t
    end
    return r
  end
  C.acts = acts
  local function defaults()
    return {desktops = 1, dnames = {}, focus = {model = "click", raise = true}, place = "center", keys = {}, mouse = {}, rules = {},
      autostart = {}, menu = C.path .. "menu.xml", wheel = 1,
      theme = {title = 22, border = 2, scale = 1, radius = 0, align = "left", bsize = 14, bgap = 4,
        a = {t1 = 0x2a5caa, t2 = 0x2a5caa, tx = 0xFFFFFF, bd = 0x2a5caa}, i = {t1 = 0x555555, t2 = 0x555555, tx = 0xCCCCCC, bd = 0x555555},
        close = 0xCC3333, max = 0x33AA33, min = 0xCCAA33, btx = 0xFFFFFF, root = 0x101820,
        menu = {bg = 0x222222, fg = 0xDDDDDD, sel = 0x2a5caa, selfg = 0xFFFFFF, bd = 0x555555, sc = 1}}}
  end
  function C.load()
    local cfg = defaults()
    W.cfg = cfg
    local root, err = X.load(C.path .. "rc.xml")
    if not root then W.log("rc.xml: " .. err) return cfg end
    local th = cfg.theme
    for _, n in ipairs(root.k) do
      local t, a = n.tag, n.a
      if t == "desktops" then
        cfg.desktops = U.clamp(U.num(a.number, 1), 1, 9)
        for i, nm in ipairs(n:list("name")) do cfg.dnames[i] = nm.t end
      elseif t == "focus" then cfg.focus.model = a.model or "click" cfg.focus.raise = U.bool(a.raise, true)
      elseif t == "placement" then cfg.place = a.policy or "center"
      elseif t == "autostart" then for _, r in ipairs(n:list("run")) do cfg.autostart[#cfg.autostart + 1] = r.t end
      elseif t == "menu" then cfg.menu = a.file or cfg.menu
      elseif t == "theme" then
        for _, c in ipairs(n.k) do
          local ca = c.a
          if c.tag == "title" then th.title = U.num(ca.height, th.title) th.scale = U.num(ca.scale, 1) th.align = ca.align or th.align
          elseif c.tag == "border" then th.border = U.num(ca.width, th.border) th.radius = U.num(ca.radius, 0)
          elseif c.tag == "root" then th.root = U.color(ca.color, th.root)
          elseif c.tag == "buttons" then
            th.bsize = U.num(ca.size, th.bsize) th.bgap = U.num(ca.gap, th.bgap) th.close = U.color(ca.close, th.close)
            th.max = U.color(ca.max, th.max) th.min = U.color(ca.min, th.min) th.btx = U.color(ca.text, th.btx)
          elseif c.tag == "active" or c.tag == "inactive" then
            local s = c.tag == "active" and th.a or th.i
            s.t1 = U.color(ca.title, s.t1) s.t2 = U.color(ca.title2, ca.title and U.color(ca.title, s.t1) or s.t2)
            s.tx = U.color(ca.text, s.tx) s.bd = U.color(ca.border, s.bd)
          elseif c.tag == "menu" then
            local m = th.menu
            m.bg = U.color(ca.bg, m.bg) m.fg = U.color(ca.fg, m.fg) m.sel = U.color(ca.sel, m.sel) m.selfg = U.color(ca.selfg, m.selfg)
            m.bd = U.color(ca.border, m.bd) m.sc = U.num(ca.scale, 1)
          end
        end
      elseif t == "keyboard" then
        for _, kb in ipairs(n:list("keybind")) do
          local k = C.key(kb.a.key or "")
          if k then k.acts = acts(kb) cfg.keys[#cfg.keys + 1] = k else W.log("rc.xml: bad key " .. tostring(kb.a.key)) end
        end
      elseif t == "mouse" then
        cfg.wheel = a.wheel == "inverted" and -1 or 1
        for _, cx in ipairs(n:list("context")) do
          local l = cfg.mouse[cx.a.name or ""] or {}
          cfg.mouse[cx.a.name or ""] = l
          for _, mb in ipairs(cx:list("mousebind")) do
            local m, b = C.btn(mb.a.button or "")
            if b then l[#l + 1] = {m = m, b = b, how = mb.a.action or "Press", acts = acts(mb)} else W.log("rc.xml: bad button " .. tostring(mb.a.button)) end
          end
        end
      elseif t == "applications" then
        for _, ap in ipairs(n:list("application")) do
          local r = {class = ap.a.class or "*"}
          for _, c in ipairs(ap.k) do r[c.tag] = c.t for k, v in pairs(c.a) do r[c.tag .. "." .. k] = v end end
          cfg.rules[#cfg.rules + 1] = r
        end
      end
    end
    return cfg
  end
end
