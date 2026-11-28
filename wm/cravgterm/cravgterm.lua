--lua
--needs cravgwm
-- cravgterm - terminal window: shell commands, interactive TTY apps, GUI launchers, clipboard. Config: cravgterm.xml
local M = {}
function M.new(W, args)
  local U, UI = W.u, W.ui
  local c = {title = "terminal", class = "cravgterm", w = 640, h = 400, fg = 0xcccccc, bg = 0x101010,
    prompt = "$ ", lines = {}, inp = UI.input(""), hist = {}, hi = 0, off = 0, alias = {}, sc = 1, maxl = 200}
  local TH = 20
  function c:reload()
    local root = W.xml.load("/etc/cravgwm/cravgterm.xml") or {a = {}, list = function() return {} end}
    local a = root.a
    self.sc = U.num(a.scale, 1)
    if not self.win then self.w, self.h = U.num(a.width, 640), U.num(a.height, 400) end
    self.fg = U.color(a.fg, 0xcccccc)
    self.bg = U.color(a.bg, 0x101010)
    self.prompt = a.prompt or "$ "
    self.maxl = U.num(a.scrollback, 200)
    self.alias = {}
    for _, e in ipairs(root:list("alias")) do self.alias[e.a.name or ""] = e.a.value end
  end
  c:reload()
  function c:print(s)
    local sc = self.sc
    for _, l in ipairs(U.split(s, "\n")) do
      local cols = math.max(8, self.w // (8 * sc))
      repeat
        self.lines[#self.lines + 1] = l:sub(1, cols)
        l = l:sub(cols + 1)
      until #l == 0
    end
    while #self.lines > self.maxl do table.remove(self.lines, 1) end
    self.off = 0
  end
  local GUI_MAP = {fm = "cravgfm", edit = "cravgedit", paint = "cravgpaint", run = "cravgrun", bg = "cravgbg"}
  local function paste_clip()
    local s = os.getclip and os.getclip() or ""
    if s == "" then return end
    local nl = s:find("\n", 1, true)
    if nl then s = s:sub(1, nl - 1) end
    if s:sub(-1) == "\r" then s = s:sub(1, -2) end
    if s ~= "" then
      c.inp.t = c.inp.t:sub(1, c.inp.p) .. s .. c.inp.t:sub(c.inp.p + 1)
      c.inp.p = c.inp.p + #s
      c.inp.s = nil
    end
  end
  local function copy_buf()
    if not os.setclip or #c.lines == 0 then return end
    os.setclip(table.concat(c.lines, "\n", math.max(1, #c.lines - 24), #c.lines))
  end
  function c:exec(line)
    self:print(self.prompt .. line)
    line = U.trim(line)
    if line == "" then return end
    self.hist[#self.hist + 1] = line self.hi = 0
    local sp = line:find(" ", 1, true)
    local cmd, rest = line:sub(1, (sp or #line + 1) - 1), sp and U.trim(line:sub(sp + 1)) or ""
    if self.alias[cmd] then
      line = self.alias[cmd] .. (rest ~= "" and (" " .. rest) or "")
      sp = line:find(" ", 1, true)
      cmd, rest = line:sub(1, (sp or #line + 1) - 1), sp and U.trim(line:sub(sp + 1)) or ""
    end
    if cmd == "clear" then self.lines = {}
    elseif cmd == "exit" then W.close(self.win)
    elseif cmd == "help" then
      self:print("builtins: clear exit help lua EXPR termconf reconfig rctpl [NAME]\ngui: fm edit paint run bg (or /apps/<name>)\ntty/shell: web luaide tetris calendar snake cwm-doc stas ls cat ...")
    elseif cmd == "termconf" or cmd == "config" then
      if fs.exists("/apps/cravgedit") then W.run("cravgedit /etc/cravgwm/cravgterm.xml") else self:print("cravgedit not installed") end
    elseif cmd == "reconfig" then
      self:reload() W.reconf = true self:print("reconfigured")
    elseif cmd == "rctpl" then
      if rest == "" then
        local l = fs.list("/etc/cravgwm/tpl") or {}
        self:print("templates: default dark compact classic" .. (#l > 0 and (" | /etc/cravgwm/tpl: " .. table.concat(l, " ")) or ""))
      else
        W.act.ApplyTemplate(self, {template = rest})
        self:print("applied template: " .. rest)
      end
    elseif cmd == "lua" then
      local f, e = load("return " .. rest, "=term")
      if not f then f, e = load(rest, "=term") end
      if not f then self:print(e) else local r = {pcall(f)} if not r[1] then self:print(tostring(r[2])) else local o = {} for i = 2, #r do o[#o + 1] = tostring(r[i]) end self:print(table.concat(o, "  ")) end end
    elseif GUI_MAP[cmd] and fs.exists("/apps/" .. GUI_MAP[cmd]) then
      W.run(GUI_MAP[cmd] .. (rest ~= "" and (" " .. rest) or ""))
    else
      local ap = "/apps/" .. cmd
      local src = (not W.apps[cmd] and fs.exists(ap)) and fs.read(ap) or nil
      local is_gui = W.apps[cmd] ~= nil or (src ~= nil and src:sub(1, 5) == "--lua" and src:find("M.new", 1, true) ~= nil)
      if is_gui then
        W.run(line)
      elseif (src and src:sub(1, 5) == "--lua") or (cmd == "stas" and (rest:sub(1, 7) == "install" or rest:sub(1, 6) == "update" or rest:sub(1, 3) == "run")) then
        if os.run_tty then
          local ok, err = os.run_tty(line)
          W.damage_all()
          if not ok and err then self:print(tostring(err)) end
        else self:print("tty runner unavailable") end
      else
        local o, e = os.exec(line)
        if o then
          if #o > 0 then self:print(o:sub(1, #o - (o:sub(-1) == "\n" and 1 or 0))) end
        elseif os.run_tty and not (e and e:find("denied", 1, true)) then
          local ok, err = os.run_tty(line)
          W.damage_all()
          if not ok and err then self:print(tostring(err)) end
        else
          self:print(tostring(e))
        end
      end
    end
  end
  local BTNS = {
    {"Copy", copy_buf},
    {"Paste", paste_clip},
    {"Conf", function() if fs.exists("/apps/cravgedit") then W.run("cravgedit /etc/cravgwm/cravgterm.xml") end end}
  }
  c:print("cravgterm: type help")
  function c:resize(w, h) self.w, self.h = w, h end
  function c:draw(s, w, h)
    local sc = self.sc
    gfx.fill(0, 0, w, h, self.bg)
    gfx.fill(0, 0, w, TH, 0x1e1e2e)
    gfx.text(6, 2, "cravgterm  Ctrl+Shift+C copy  Ctrl+V paste", 0x8892b0)
    local bx = w - 4
    self.bhits = {}
    for i = #BTNS, 1, -1 do
      local b = BTNS[i]
      local bw = #b[1] * 8 + 10
      bx = bx - bw
      gfx.fill(bx, 2, bw, 16, 0x32324c)
      gfx.text(bx + 5, 2, b[1], 0xdddddd)
      self.bhits[#self.bhits + 1] = {bx, bw, b[2]}
      bx = bx - 4
    end
    local lh = 16 * sc
    local rows = math.max(1, ((h - TH) // lh) - 1)
    local first = math.max(1, #self.lines - rows + 1 - self.off)
    for i = 0, rows - 1 do
      local l = self.lines[first + i]
      if l then gfx.text(4, TH + i * lh, l, self.fg, sc) end
    end
    local y = TH + rows * lh
    gfx.text(4, y, self.prompt, 0x66dd66, sc)
    UI.draw_input(self.inp, 4 + #self.prompt * 8 * sc, y, w - 8 - #self.prompt * 8 * sc, self.fg, sc)
  end
  function c:tick() local b = (os.ms() // 500) % 2 if b ~= self.blink then self.blink = b return true end end
  function c:key(k, m, ev)
    m = m or 0
    local b = ev and ev.base or k
    if b >= 65 and b <= 90 then b = b + 32 end
    if (m & 3) == 3 and (m & 12) == 0 and b == 99 then copy_buf() return true end
    if (m & 3) == 3 and (m & 12) == 0 and b == 118 then paste_clip() return true end
    if k == 10 then local l = self.inp.t self.inp = UI.input("") self:exec(l) return true end
    if k == term.UP then if #self.hist > 0 then self.hi = math.min(self.hi + 1, #self.hist) self.inp = UI.input(self.hist[#self.hist - self.hi + 1]) end return true end
    if k == term.DOWN then self.hi = math.max(self.hi - 1, 0) self.inp = UI.input(self.hi > 0 and self.hist[#self.hist - self.hi + 1] or "") return true end
    if k == term.PGUP then self.off = math.min(self.off + 5, #self.lines) return true end
    if k == term.PGDN then self.off = math.max(self.off - 5, 0) return true end
    return UI.edit(self.inp, k, m, ev)
  end
  function c:mouse(ev)
    if ev.t == "wheel" then self.off = U.clamp(self.off + (ev.wheel > 0 and 3 or -3), 0, #self.lines) return true end
    if ev.t == "down" and ev.btn == 1 and ev.y < TH then
      for _, h in ipairs(self.bhits or {}) do if ev.x >= h[1] and ev.x < h[1] + h[2] then h[3]() return true end end
      return true
    end
  end
  return c
end
return M
