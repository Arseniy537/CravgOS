--lua
--needs cravgwm
-- cravgpaint - simple 16-color indexed BMP Paint program. Config: none
local M = {}
function M.new(W, args)
  local U, UI = W.u, W.ui
  local CW, CH, SC = 128, 80, 4
  local CX0, CY0, TH, PH, BH = 14, 52, 24, 24, 22
  local c = {title = "cravgpaint", class = "cravgpaint", w = CX0 * 2 + CW * SC, h = CY0 + CH * SC + BH + 8,
    bg = 0x1a1a28, bar = 0x242438, fg = 0xdddddd, sel = 0x2a5caa,
    tool = "pen", col = 0, bsz = 1, path = "", mod = false, msg = "ready"}
  c.img = gfx.surface(CW, CH)
  local function stitle()
    c.title = (c.mod and "* " or "") .. (c.path ~= "" and c.path or "art1.bmp") .. " - cravgpaint"
    if c.win then c.win.title = c.title c.win.fdirty = true end
  end
  local P = dofile("/apps/cp-bmp")(W, c, CW, CH, stitle)
  local PAL = P.PAL
  local TOOLS = {{"Pen", "pen"}, {"Ers", "ers"}, {"Line", "line"}, {"Rect", "rect"}, {"Fill", "fill"}}
  local BTNS = {
    {"1x", function() c.bsz = 1 end, 1}, {"2x", function() c.bsz = 2 end, 2}, {"3x", function() c.bsz = 3 end, 3},
    {"Clr", function() P.clr(1) c.mod = true stitle() c.msg = "cleared" end},
    {"Open", function() c.prompt = {kind = "open", label = "Open: ", inp = UI.input(c.path ~= "" and c.path or "/home/art1.bmp")} end},
    {"Save", function() if c.path == "" then c.prompt = {kind = "save", label = "Save: ", inp = UI.input("/home/art1.bmp")} else P.save_bmp(c.path) end end},
    {"Wall", P.set_wall}
  }
  P.clr(1)
  if args ~= "" then P.load_bmp(args) else stitle() end
  function c:close() if self.img then gfx.free(self.img) self.img = nil end end
  function c:draw(s, w, h)
    gfx.fill(0, 0, w, h, self.bg)
    gfx.fill(0, 0, w, TH, self.bar)
    local bx = 4 self.thits = {}
    for _, t in ipairs(TOOLS) do
      local bw = #t[1] * 8 + 10
      gfx.fill(bx, 3, bw, 18, self.tool == t[2] and self.sel or 0x32324c)
      gfx.text(bx + 5, 4, t[1], self.fg)
      self.thits[#self.thits + 1] = {bx, bw, function() self.tool = t[2] end}
      bx = bx + bw + 3
    end
    bx = bx + 6
    for _, b in ipairs(BTNS) do
      local bw = #b[1] * 8 + 10
      gfx.fill(bx, 3, bw, 18, (b[3] and self.bsz == b[3]) and self.sel or 0x32324c)
      gfx.text(bx + 5, 4, b[1], self.fg)
      self.thits[#self.thits + 1] = {bx, bw, b[2]}
      bx = bx + bw + 3
    end
    gfx.fill(0, TH, w, PH, 0x141420)
    for i, rgb in ipairs(PAL) do
      local px = 14 + (i - 1) * 28
      if i - 1 == self.col then gfx.fill(px - 2, TH + 2, 28, 20, 0xFFFFFF) end
      gfx.fill(px, TH + 4, 24, 16, rgb)
    end
    gfx.fill(CX0 - 1, CY0 - 1, CW * SC + 2, CH * SC + 2, 0x555577)
    if self.img then gfx.scale(self.img, CX0, CY0, CW * SC, CH * SC) end
    if self.drag and (self.tool == "line" or self.tool == "rect") then
      local fn = function(x, y, ci) if x >= 0 and y >= 0 and x < CW and y < CH then gfx.fill(CX0 + x * SC, CY0 + y * SC, SC, SC, PAL[ci + 1]) end end
      if self.tool == "line" then P.line(self.sx, self.sy, self.lx, self.ly, self.col, fn) else P.rect(self.sx, self.sy, self.lx, self.ly, self.col, fn) end
    end
    gfx.fill(0, h - BH, w, BH, self.bar)
    if self.prompt then
      gfx.text(6, h - BH + 3, self.prompt.label, 0x88ccff)
      local lx = 6 + #self.prompt.label * 8
      UI.draw_input(self.prompt.inp, lx, h - BH + 3, w - lx - 8, self.fg)
    else
      gfx.text(6, h - BH + 3, U.fit(self.tool .. " " .. self.bsz .. "px | " .. self.msg, w - 12), self.fg)
    end
  end
  function c:key(k, m, ev)
    if self.prompt then
      if k == 27 then self.prompt = nil return true end
      if k == 10 then
        local p = self.prompt self.prompt = nil
        if p.kind == "open" then P.load_bmp(p.inp.t) else P.save_bmp(p.inp.t) end
        return true
      end
      return UI.edit(self.prompt.inp, k, m, ev)
    end
    if (m & 2) ~= 0 then
      local b = ev and ev.base or k
      if b == 115 or k == 19 then BTNS[6][2]() return true end
      if b == 111 or k == 15 then BTNS[5][2]() return true end
    end
    return false
  end
  function c:mouse(ev)
    local cx, cy = U.clamp((ev.x - CX0) // SC, 0, CW - 1), U.clamp((ev.y - CY0) // SC, 0, CH - 1)
    if ev.t == "down" and ev.btn == 1 then
      if ev.y < TH then for _, h in ipairs(self.thits or {}) do if ev.x >= h[1] and ev.x < h[1] + h[2] then h[3]() return true end end return true
      elseif ev.y < TH + PH then local idx = (ev.x - 14) // 28 if idx >= 0 and idx < 16 then self.col = idx end return true
      elseif U.inside(ev.x, ev.y, CX0, CY0, CW * SC, CH * SC) then
        if self.tool == "fill" then P.flood(cx, cy, self.col) self.mod = true stitle() return true end
        self.drag, self.sx, self.sy, self.lx, self.ly = true, cx, cy, cx, cy
        if self.tool == "pen" or self.tool == "ers" then gfx.target(self.img) P.plot(cx, cy, self.tool == "ers" and 1 or self.col) gfx.target(0) self.mod = true stitle() end
        return true
      end
    elseif ev.t == "move" and self.drag then
      if self.tool == "pen" or self.tool == "ers" then gfx.target(self.img) P.line(self.lx, self.ly, cx, cy, self.tool == "ers" and 1 or self.col, P.plot) gfx.target(0) end
      self.lx, self.ly = cx, cy return true
    elseif ev.t == "up" and ev.btn == 1 and self.drag then
      if self.tool == "line" or self.tool == "rect" then
        gfx.target(self.img)
        if self.tool == "line" then P.line(self.sx, self.sy, cx, cy, self.col, P.plot) else P.rect(self.sx, self.sy, cx, cy, self.col, P.plot) end
        gfx.target(0) self.mod = true stitle()
      end
      self.drag = false return true
    end
    return false
  end
  return c
end
return M
