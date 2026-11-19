--lua
--needs cravgwm
-- cravgbar - panel in the style of tint2: launchers, desktops, taskbar, clock. Config: /etc/cravgwm/cravgbar.xml
local M = {single = true}
function M.new(W, args)
  local UI, U = W.ui, W.u
  local root = UI.config("cravgbar")
  if not root then return nil end
  local a = root.a
  local h = U.num(a.height, 28)
  local c = {title = "cravgbar", kind = "dock", edge = a.position == "top" and "top" or "bottom", size = h, class = "cravgbar", w = W.sw, h = h,
    bg = U.color(a.bg, 0x1e1e2e), fg = U.color(a.fg, 0xdddddd), hl = U.color(a.highlight, 0x2a5caa), sc = U.num(a.scale, 1), items = {}, sig = "", hit = {}}
  for _, n in ipairs(root:child("items") and root:child("items").k or {}) do c.items[#c.items + 1] = n end
  local function text(self, i, t, x, w) gfx.text(x + 6, (h - 16 * self.sc) // 2, U.fit(t, w - 12, self.sc), self.fg, self.sc) end
  function c:layout()                                   -- вычислить вид и области нажатия
    local x, r, hits, draw = 4, W.sw - 4, {}, {}
    local sc = self.sc
    local function put(w, side, d, hit)
      local px
      if side == "right" then r = r - w px = r r = r - 4 else px = x x = x + w + 4 end
      draw[#draw + 1] = {px, w, d}
      if hit then hits[#hits + 1] = {px, w, hit} end
    end
    local tasks
    for _, n in ipairs(self.items) do
      local t, na = n.tag, n.a
      if t == "launcher" then
        local lab = na.label or na.command or "?"
        put(#lab * 8 * sc + 16, "left", function(px, w) gfx.fill(px, 3, w, h - 6, self.hl, 90) text(self, 0, lab, px, w) end, function() W.run(na.command) end)
      elseif t == "desktops" then
        for i = 1, W.cfg.desktops do
          local lab = W.cfg.dnames[i] or tostring(i)
          put(#lab * 8 * sc + 14, "left", function(px, w) if W.desk == i then gfx.fill(px, 3, w, h - 6, self.hl) end text(self, 0, lab, px, w) end, function() W.goto(i) end)
        end
      elseif t == "clock" then
        local s = U.clock(na.format or "%H:%M")
        put(#s * 8 * sc + 14, "right", function(px, w) text(self, 0, s, px, w) end)
      elseif t == "text" then
        local s = na.value or ""
        put(#s * 8 * sc + 14, "right", function(px, w) text(self, 0, s, px, w) end)
      elseif t == "exec" then
        local now = os.ms()
        if not n.last or now - n.last > U.num(na.interval, 10000) then
          n.last = now
          local o = os.exec(na.command or "")
          n.out = o and U.trim(o:sub(1, (o:find("\n", 1, true) or #o + 1) - 1)) or "(os.exec denied)"
        end
        local s = n.out or ""
        put(#s * 8 * sc + 14, "right", function(px, w) text(self, 0, s, px, w) end)
      elseif t == "taskbar" then tasks = true end
    end
    if tasks then
      local list = {}
      for _, w in ipairs(W.wins) do if not w.kind and (w.desk == 0 or w.desk == W.desk) then list[#list + 1] = w end end
      local room = r - x
      local bw = math.min(U.num(root.a.taskwidth, 170), #list > 0 and room // #list - 4 or room)
      for _, w in ipairs(list) do
        draw[#draw + 1] = {x, bw, function(px, ww) if W.focus == w and not w.icon then gfx.fill(px, 3, ww, h - 6, self.hl) else gfx.fill(px, 3, ww, h - 6, self.fg, 24) end
          text(self, 0, (w.icon and "- " or "") .. w.title, px, ww) end}
        hits[#hits + 1] = {x, bw, function() if W.focus == w and not w.icon then W.iconify(w) else if w.icon then W.restore(w) else W.raise(w) W.focus_win(w) end end end}
        x = x + bw + 4
      end
    end
    self.hit, self.drawl = hits, draw
  end
  function c:draw(s, w, hh)
    gfx.fill(0, 0, w, hh, self.bg)
    gfx.fill(0, self.edge == "top" and hh - 1 or 0, w, 1, self.hl)
    self:layout()
    for _, d in ipairs(self.drawl) do d[3](d[1], d[2]) end
  end
  function c:tick()
    local sig = {W.desk, W.focus and W.focus.id or 0, U.clock("%H%M%S")}
    for _, w in ipairs(W.wins) do if not w.kind then sig[#sig + 1] = w.title .. (w.icon and "i" or "") .. w.desk end end
    sig = table.concat(sig, "|")
    if sig ~= self.sig then self.sig = sig return true end
  end
  function c:mouse(ev)
    if ev.t ~= "down" or ev.btn ~= 1 then return false end
    for _, hh in ipairs(self.hit) do if ev.x >= hh[1] and ev.x < hh[1] + hh[2] then hh[3]() return true end end
    return false
  end
  return c
end
return M
