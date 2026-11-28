--lua
--needs cravgwm
-- cravgedit - multi-line GUI text viewer and editor with clipboard. Config: none
local M = {}
function M.new(W, args)
  local U, UI = W.u, W.ui
  local c = {title = "cravgedit", class = "cravgedit", w = 620, h = 420, minw = 320, minh = 200,
    bg = 0x161622, gutter = 0x1e1e2e, bar = 0x242438, fg = 0xdddddd, dim = 0x7a849e, sel = 0x2a5caa,
    lines = {""}, cx = 0, cy = 1, top = 0, left = 0, path = "", mod = false, msg = ""}
  local GW, TH, BH, SB = 42, 24, 22, 10
  local function stitle()
    c.title = (c.mod and "* " or "") .. (c.path ~= "" and c.path or "untitled") .. " - cravgedit"
    if c.win then c.win.title = c.title c.win.fdirty = true end
  end
  local function vrows() return math.max(1, (c.h - TH - BH) // 16) end
  local function vcols() return math.max(4, (c.w - GW - SB - 8) // 8) end
  local B = dofile("/apps/ce-buf")(W, c, UI, stitle, vrows, vcols)
  local BTNS = {
    {"New", function() B.load("") end},
    {"Open", function() c.prompt = {kind = "open", label = "Open: ", inp = UI.input(c.path ~= "" and c.path or "/home/")} end},
    {"Save", function() B.save(c.path) end},
    {"SaveAs", function() c.prompt = {kind = "save", label = "SaveAs: ", inp = UI.input(c.path ~= "" and c.path or "/home/note.txt")} end},
    {"Cut", function() local s = B.getsel() if s ~= "" then if os.setclip then os.setclip(s) end B.delsel() stitle() B.vis() c.msg = "cut" end end},
    {"Copy", function() local s = B.getsel() if s ~= "" and os.setclip then os.setclip(s) c.msg = "copied " .. #s .. "B" end end},
    {"Paste", function() local s = os.getclip and os.getclip() or "" if s ~= "" then B.insert(s) B.vis() c.msg = "pasted" end end},
  }
  B.load(args)
  function c:resize(w, h) self.w, self.h = w, h B.vis() end
  function c:tick() local b = (os.ms() // 500) % 2 if b ~= self.blink then self.blink = b return true end end
  function c:draw(s, w, h)
    gfx.fill(0, 0, w, h, self.bg)
    gfx.fill(0, 0, w, TH, self.bar)
    local bx = 4 self.bhits = {}
    for _, b in ipairs(BTNS) do
      local bw = #b[1] * 8 + 12
      gfx.fill(bx, 3, bw, 18, 0x32324c)
      gfx.text(bx + 6, 4, b[1], self.fg)
      self.bhits[#self.bhits + 1] = {bx, bw, b[2]}
      bx = bx + bw + 4
    end
    local rows, cols = vrows(), vcols()
    local y1, x1, y2, x2 = B.srange()
    gfx.fill(0, TH, GW, h - TH - BH, self.gutter)
    for r = 1, rows do
      local ln, y = self.top + r, TH + (r - 1) * 16
      if ln <= #self.lines then
        gfx.text(4, y, string.format("%4d", ln), ln == self.cy and self.fg or self.dim)
        local txt = self.lines[ln]
        if y1 and ln >= y1 and ln <= y2 then
          local va = math.max(ln == y1 and x1 or 0, self.left)
          local vb = math.min(ln == y2 and x2 or #txt + 1, self.left + cols)
          if vb > va then gfx.fill(GW + 4 + (va - self.left) * 8, y, (vb - va) * 8, 16, self.sel) end
        end
        gfx.text(GW + 4, y, txt:sub(self.left + 1, self.left + cols), self.fg)
        if ln == self.cy and not self.prompt and (os.ms() // 500) % 2 == 0 then
          local cxp = GW + 4 + (self.cx - self.left) * 8
          if cxp >= GW + 4 and cxp < w - SB - 4 then gfx.fill(cxp, y, 2, 16, 0x88ccff) end
        end
      end
    end
    local eh = h - TH - BH
    gfx.fill(w - SB, TH, SB, eh, self.gutter)
    if #self.lines > rows then
      local th = math.max(16, eh * rows // #self.lines)
      gfx.fill(w - SB + 2, TH + (eh - th) * self.top // math.max(1, #self.lines - rows), SB - 4, th, self.sel)
    end
    gfx.fill(0, h - BH, w, BH, self.bar)
    if self.prompt then
      gfx.text(6, h - BH + 3, self.prompt.label, 0x88ccff)
      local lx = 6 + #self.prompt.label * 8
      UI.draw_input(self.prompt.inp, lx, h - BH + 3, w - lx - 8, self.fg)
    else
      gfx.text(6, h - BH + 3, U.fit(string.format("Ln %d:%d  %dL  %s", self.cy, self.cx + 1, #self.lines, self.msg), w - 12), self.fg)
    end
  end
  function c:key(k, m, ev)
    m = m or 0
    if self.prompt then
      if k == 27 then self.prompt = nil return true end
      if k == 10 then
        local p = self.prompt self.prompt = nil
        if p.kind == "open" then B.load(p.inp.t) else B.save(p.inp.t) end
        return true
      end
      return UI.edit(self.prompt.inp, k, m, ev)
    end
    local ctrl, shift = (m & 2) ~= 0, (m & 1) ~= 0
    local b = ev and ev.base or k
    if b >= 65 and b <= 90 then b = b + 32 end
    if ctrl and (m & 12) == 0 then
      if b == 115 or k == 19 then B.save(self.path) return true
      elseif b == 111 or k == 15 then BTNS[2][2]() return true
      elseif b == 110 or k == 14 then BTNS[1][2]() return true
      elseif b == 97 or k == 1 then self.ay, self.ax, self.cy = 1, 0, #self.lines self.cx = #self.lines[self.cy] B.vis() return true
      elseif b == 99 or k == 3 then BTNS[6][2]() return true
      elseif b == 120 or k == 24 then BTNS[5][2]() return true
      elseif b == 118 or k == 22 then BTNS[7][2]() return true end
      return false
    end
    if k == term.LEFT then
      local y1, x1 = B.srange()
      if not shift and y1 then B.mv(y1, x1, false)
      elseif self.cx > 0 then B.mv(self.cy, self.cx - 1, shift) elseif self.cy > 1 then B.mv(self.cy - 1, #self.lines[self.cy - 1], shift)
      elseif not shift then self.ay, self.ax = nil, nil end
    elseif k == term.RIGHT then
      local _, _, y2, x2 = B.srange()
      if not shift and y2 then B.mv(y2, x2, false)
      elseif self.cx < #self.lines[self.cy] then B.mv(self.cy, self.cx + 1, shift) elseif self.cy < #self.lines then B.mv(self.cy + 1, 0, shift)
      elseif not shift then self.ay, self.ax = nil, nil end
    elseif k == term.UP then B.mv(self.cy - 1, self.cx, shift)
    elseif k == term.DOWN then B.mv(self.cy + 1, self.cx, shift)
    elseif k == term.HOME then B.mv(self.cy, 0, shift)
    elseif k == term.END then B.mv(self.cy, #self.lines[self.cy], shift)
    elseif k == term.PGUP then B.mv(self.cy - vrows(), self.cx, shift)
    elseif k == term.PGDN then B.mv(self.cy + vrows(), self.cx, shift)
    elseif k == 10 then B.insert("\n") B.vis()
    elseif k == 9 then B.insert("  ") B.vis()
    elseif k == 8 then B.bs()
    elseif k == term.DEL then B.del()
    elseif k >= 32 and k < 256 and (m & 14) == 0 then B.insert(string.char(k)) B.vis()
    else return false end
    return true
  end
  function c:mouse(ev)
    if ev.t == "wheel" then self.top = U.clamp(self.top + (ev.wheel > 0 and -3 or 3), 0, math.max(0, #self.lines - vrows())) return true end
    local function pos(mx, my) local ny = U.clamp(self.top + (my - TH) // 16 + 1, 1, #self.lines) return ny, U.clamp(self.left + (mx - GW) // 8, 0, #self.lines[ny]) end
    if ev.t == "down" and ev.btn == 1 then
      if ev.y < TH then for _, h in ipairs(self.bhits or {}) do if ev.x >= h[1] and ev.x < h[1] + h[2] then h[3]() return true end end return true end
      if ev.y >= TH and ev.y < self.h - BH then
        if ev.x >= self.w - SB then local r = vrows() self.top = U.clamp((ev.y - TH) * math.max(0, #self.lines - r) // math.max(1, self.h - TH - BH), 0, math.max(0, #self.lines - r)) return true end
        local ny, nx = pos(ev.x, ev.y) B.mv(ny, nx, (ev.mods & 1) ~= 0) self.drag = true return true
      end
    elseif ev.t == "move" and self.drag then local ny, nx = pos(ev.x, ev.y) B.mv(ny, nx, true) return true
    elseif ev.t == "up" and ev.btn == 1 then self.drag = false return true end
    return false
  end
  return c
end
return M
