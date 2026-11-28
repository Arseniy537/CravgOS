--lua
--needs cravgwm
-- cravgrun - launcher in the style of rofi: drun, window, run modes. Config: /etc/cravgwm/cravgrun.xml
local M = {single = true}
function M.new(W, args)
  local U, UI = W.u, W.ui
  local root = UI.config("cravgrun")
  local a = (root and root.a) or {}
  local lines, width = U.num(a.lines, 8), U.num(a.width, 520)
  local c = {title = "cravgrun", kind = "overlay", class = "cravgrun", w = width, h = 40 + lines * 20, px = (W.sw - width) // 2, py = W.sh // 5,
    bg = U.color(a.bg, 0x1e1e2e), fg = U.color(a.fg, 0xdddddd), sel = U.color(a.selbg, 0x2a5caa), bd = U.color(a.border, 0x2a5caa), sf = U.color(a.selfg, 0xffffff),
    modes = U.split(a.modes or "drun,window,run", ","), mi = 1, inp = UI.input(""), sel_i = 1, ent = {}, shown = {}}
  if root then
    for _, e in ipairs(root:list("entry")) do c.ent[#c.ent + 1] = {name = e.a.name or e.a.command, cmd = e.a.command} end
    for _, s in ipairs(root:list("scan")) do
      for _, f in ipairs(fs.list(s.a.dir or "/apps") or {}) do
        if f:sub(1, #(s.a.prefix or "")) == (s.a.prefix or "") and f:sub(-1) ~= "/" then c.ent[#c.ent + 1] = {name = f, cmd = f} end
      end
    end
  else
    for _, e in ipairs({{"Terminal", "cravgterm"}, {"Files", "cravgfm"}, {"Editor", "cravgedit"}, {"Paint", "cravgpaint"}}) do
      if fs.exists("/apps/" .. e[2]) then c.ent[#c.ent + 1] = {name = e[1], cmd = e[2]} end
    end
    for _, f in ipairs(fs.list("/apps") or {}) do
      if f:sub(1, 5) == "cravg" and f:sub(-1) ~= "/" then c.ent[#c.ent + 1] = {name = f, cmd = f} end
    end
  end
  function c:refresh()
    local mode, q, r = self.modes[self.mi], self.inp.t:lower(), {}
    if mode == "drun" then for _, e in ipairs(self.ent) do if q == "" or e.name:lower():find(q, 1, true) then r[#r + 1] = {label = e.name, fn = function() W.run(e.cmd) end} end end
    elseif mode == "window" then
      for _, w in ipairs(W.wins) do if not w.kind and (q == "" or w.title:lower():find(q, 1, true)) then r[#r + 1] = {label = w.title, fn = function() if w.icon then W.restore(w) else W.raise(w) W.focus_win(w) end if w.desk ~= 0 then W.goto(w.desk) end end} end end
    else
      if self.inp.t ~= "" then r[1] = {label = "run: " .. self.inp.t, fn = function() W.run(self.inp.t) end} end
    end
    self.shown, self.sel_i = r, math.min(self.sel_i, math.max(#r, 1))
  end
  function c:start() self:refresh() end
  function c:draw(s, w, h)
    UI.box(0, 0, w, h, self.bg, self.bd)
    gfx.text(8, 12, (a.prompt or self.modes[self.mi]) .. ":", self.sel)
    UI.draw_input(self.inp, 16 + #(a.prompt or self.modes[self.mi]) * 8 + 8, 12, w - 120, self.fg)
    gfx.fill(4, 34, w - 8, 1, self.bd)
    local top = math.max(0, self.sel_i - lines)
    for i = 1, lines do
      local it = self.shown[top + i]
      if it then
        if top + i == self.sel_i then gfx.fill(4, 38 + (i - 1) * 20, w - 8, 20, self.sel) end
        gfx.text(10, 40 + (i - 1) * 20, U.fit(it.label, w - 20), top + i == self.sel_i and self.sf or self.fg)
      end
    end
  end
  function c:tick() return (os.ms() // 500) % 2 ~= (self.blink or 0) and (function() self.blink = (os.ms() // 500) % 2 return true end)() end
  function c:key(k, m, ev)
    if k == 27 then W.close(self.win) return true end
    if k == 10 then local it = self.shown[self.sel_i] W.close(self.win) if it then it.fn() end return true end
    if k == 9 then self.mi = self.mi % #self.modes + 1 self:refresh() return true end
    if k == term.DOWN then self.sel_i = math.min(self.sel_i + 1, #self.shown) return true end
    if k == term.UP then self.sel_i = math.max(self.sel_i - 1, 1) return true end
    if UI.edit(self.inp, k, m, ev) then self.sel_i = 1 self:refresh() return true end
    return false
  end
  return c
end
return M
