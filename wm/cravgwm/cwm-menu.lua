--lua
-- cwm-menu - Openbox-style menus from /etc/cravgwm/menu.xml (module of cravgwm)
return function(W)
  local U, M = W.u, {}
  W.menu = M
  local defs
  local n = 0
  function M.reload() defs = nil end
  local function scan(node)                    -- id -> <menu>; вложенные меню получают собственные id
    for _, c in ipairs(node.k) do
      if c.tag == "menu" then
        if not c.a.id then n = n + 1 c.a.id = "menu-" .. n end
        if #c.k > 0 then defs[c.a.id] = c scan(c) end
      end
    end
  end
  local function tolist(id, ctx)
    local l = {}
    if id == "client-list-menu" then
      for _, w in ipairs(W.wins) do
        if not w.kind then l[#l + 1] = {label = (w.icon and "[" or "") .. w.title .. (w.icon and "]" or ""), fn = function() if w.icon then W.restore(w) else W.raise(w) W.focus_win(w) end if w.desk ~= 0 then W.goto(w.desk) end end} end
      end
      return l
    elseif id == "tpl-menu" then
      local seen = {}
      for _, f in ipairs(fs.list("/etc/cravgwm/tpl") or {}) do
        if f:sub(-4) == ".xml" then
          local nm = f:sub(1, -5)
          seen[nm] = true
          l[#l + 1] = {label = nm, acts = {{name = "ApplyTemplate", a = {template = nm, file = "/etc/cravgwm/tpl/" .. f}}}}
        end
      end
      for _, nm in ipairs(W.tpls or {"default", "dark", "compact", "classic"}) do
        if not seen[nm] then l[#l + 1] = {label = nm, acts = {{name = "ApplyTemplate", a = {template = nm}}}} end
      end
      return l
    elseif id == "client-menu" then
      local w = ctx.w
      local function a(name, to) return function() W.do_acts({{name = name, a = {to = to}}}, {w = w}) end end
      return {{label = "Maximize / Restore", fn = a("ToggleMaximize")}, {label = "Iconify", fn = a("Iconify")}, {label = "Shade", fn = a("ToggleShade")},
        {label = "Always on top", fn = a("ToggleAlwaysOnTop")}, {label = "All desktops", fn = a("ToggleOmnipresent")}, {sep = true}, {label = "Close", fn = a("Close")}}
    end
    local node = defs[id]
    if not node then
      if id == "root-menu" then
        local function act(nm, cmd) return {{name = nm, a = cmd and {command = cmd} or {}}} end
        local it = {}
        if fs.exists("/apps/cravgterm") then it[#it + 1] = {label = "Terminal", acts = act("Execute", "cravgterm")} end
        if fs.exists("/apps/cravgfm") then it[#it + 1] = {label = "Files", acts = act("Execute", "cravgfm")} end
        if fs.exists("/apps/cravgedit") then it[#it + 1] = {label = "Editor", acts = act("Execute", "cravgedit")} end
        if fs.exists("/apps/cravgpaint") then it[#it + 1] = {label = "Paint", acts = act("Execute", "cravgpaint")} end
        if fs.exists("/apps/cravgrun") then it[#it + 1] = {label = "Launcher", acts = act("Execute", "cravgrun")} end
        if #it > 0 then it[#it + 1] = {sep = true} end
        it[#it + 1] = {label = "Templates  >", sub = "tpl-menu"}
        it[#it + 1] = {label = "Windows  >", sub = "client-list-menu"}
        it[#it + 1] = {sep = true}
        it[#it + 1] = {label = "Exit", acts = act("Exit")}
        return it
      end
      l[1] = {label = "(no menu " .. id .. ")"}
      return l
    end
    for _, c in ipairs(node.k) do
      if c.tag == "item" then l[#l + 1] = {label = c.a.label or "?", acts = W.conf.acts(c)}
      elseif c.tag == "separator" then l[#l + 1] = {sep = true}
      elseif c.tag == "menu" then l[#l + 1] = {label = (c.a.label or c.a.id) .. "  >", sub = c.a.id} end
    end
    return l
  end
  local function size(lv)
    local m = W.cfg.theme.menu
    local mw = 0
    for _, it in ipairs(lv.items) do if it.label then mw = math.max(mw, #it.label) end end
    lv.w, lv.h = mw * 8 * m.sc + 28, #lv.items * 18 * m.sc + 4
  end
  local function close()
    local o = M.obj
    if not o then return end
    W.grab = nil M.obj = nil
    for _, lv in ipairs(o.lv) do W.damage(lv.x - 2, lv.y - 2, lv.w + 8, lv.h + 8) end
  end
  local function open(o, id, x, y)
    local lv = {items = tolist(id, o.ctx), sel = 0}
    size(lv)
    lv.x, lv.y = U.clamp(x, 0, W.sw - lv.w), U.clamp(y, 0, W.sh - lv.h)
    o.lv[#o.lv + 1] = lv
    W.damage(lv.x - 2, lv.y - 2, lv.w + 8, lv.h + 8)
  end
  local function trim(o, depth) while #o.lv > depth do local lv = table.remove(o.lv) W.damage(lv.x - 2, lv.y - 2, lv.w + 8, lv.h + 8) end end
  local function activate(o, it)
    if it.sub then return end
    close()
    if it.fn then it.fn() elseif it.acts then W.do_acts(it.acts, o.ctx) end
  end
  local function at(lv, px, py)
    local m = W.cfg.theme.menu
    if not U.inside(px, py, lv.x, lv.y, lv.w, lv.h) then return nil end
    return (py - lv.y - 2) // (18 * m.sc) + 1
  end
  function M.show(id, ctx)
    if not defs then
      defs = {}
      if fs.exists(W.cfg.menu) then
        local root, err = W.xml.load(W.cfg.menu)
        if root then scan(root) end
      end
    end
    if M.obj then close() end
    local o = {lv = {}, ctx = {w = ctx.w, ev = ctx.ev}}
    local ev = ctx.ev
    open(o, id, ev and ev.x or W.mx, ev and ev.y or W.my)
    M.obj = o
    W.grab = o
    function o:paint()
      local m = W.cfg.theme.menu
      for _, lv in ipairs(self.lv) do
        gfx.shadow(lv.x, lv.y, lv.w, lv.h, 6, 90, 0, 0)
        gfx.fill(lv.x, lv.y, lv.w, lv.h, m.bd) gfx.fill(lv.x + 1, lv.y + 1, lv.w - 2, lv.h - 2, m.bg)
        for i, it in ipairs(lv.items) do
          local y = lv.y + 2 + (i - 1) * 18 * m.sc
          if it.sep then gfx.fill(lv.x + 4, y + 8 * m.sc, lv.w - 8, 1, m.bd)
          else
            if lv.sel == i then gfx.fill(lv.x + 2, y, lv.w - 4, 18 * m.sc, m.sel) end
            gfx.text(lv.x + 10, y + 1, it.label, lv.sel == i and m.selfg or m.fg, m.sc)
          end
        end
      end
    end
    function o:mouse(ev)
      for d = #self.lv, 1, -1 do
        local lv = self.lv[d]
        local i = at(lv, ev.x, ev.y)
        if i and lv.items[i] then
          local it = lv.items[i]
          if not it.sep and lv.sel ~= i then
            lv.sel = i
            trim(self, d)
            if it.sub then open(self, it.sub, lv.x + lv.w - 4, lv.y + 2 + (i - 1) * 18 * W.cfg.theme.menu.sc) end
            W.damage(lv.x, lv.y, lv.w, lv.h)
          end
          if ev.down & 1 ~= 0 and not it.sep then activate(self, it) end
          return true
        end
      end
      if ev.down ~= 0 then close() return true end
      return true
    end
    function o:key(ev)
      local lv = self.lv[#self.lv]
      local k, c = ev.key, #lv.items
      if k == 27 then close() return true end
      if k == term.UP or k == term.DOWN then
        local d, i = k == term.DOWN and 1 or -1, lv.sel
        for _ = 1, c do i = (i - 1 + d) % c + 1 if not lv.items[i].sep then break end end
        lv.sel = i W.damage(lv.x, lv.y, lv.w, lv.h)
      elseif k == term.RIGHT or k == 10 then
        local it = lv.items[lv.sel]
        if it and it.sub then open(self, it.sub, lv.x + lv.w - 4, lv.y + 2 + (lv.sel - 1) * 18 * W.cfg.theme.menu.sc) self.lv[#self.lv].sel = 1
        elseif it and k == 10 then activate(self, it) end
      elseif k == term.LEFT and #self.lv > 1 then trim(self, #self.lv - 1) end
      return true
    end
  end
end
