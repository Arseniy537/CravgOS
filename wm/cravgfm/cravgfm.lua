--lua
--needs cravgwm
-- cravgfm - file manager in the style of pcmanfm: browse everywhere, edit/create/wallpaper. Config: /etc/cravgwm/cravgfm.xml
local M = {}
function M.new(W, args)
 local U, UI = W.u, W.ui
 local root = W.xml.load("/etc/cravgwm/cravgfm.xml") or {a = {}, list = function() return {} end}
 local a = root.a
 local c = {title = "files", class = "cravgfm", w = U.num(a.width, 580), h = U.num(a.height, 400), bg = U.color(a.bg, 0x1e1e2e), fg = U.color(a.fg, 0xdddddd),
  sel = U.color(a.selbg, 0x2a5caa), dir = U.color(a.dircolor, 0x66aaff), path = args ~= "" and args or (a.start or "/"), items = {}, i = 1, top = 0, pw = U.num(a.preview, 200), msg = ""}
 local assoc = {bmp = "cravgpaint %f"}
 for _, e in ipairs(U.split("txt,md,log,cfg,ini,xml,lua,sh,csv,json", ",")) do assoc[e] = "cravgedit %f" end
 for _, e in ipairs(root:list("assoc")) do assoc[(e.a.ext or ""):lower()] = e.a.command end
 local books = root:list("bookmark")
 if #books == 0 then books = {{a = {path = "/home"}}, {a = {path = "/lua"}}, {a = {path = "/etc/cravgwm"}}, {a = {path = "/docs"}}} end
 local function join(p, n) return p == "/" and ("/" .. n) or (p .. "/" .. n) end
 local function is_text(p)
  local d = fs.read(p) if not d then return false end
  for i = 1, math.min(#d, 256) do if d:byte(i) == 0 then return false end end
  return true
 end
 function c:load()
  local l, hid, d, f = fs.list(self.path) or {}, U.bool(a.hidden, false), {}, {}
  for _, n in ipairs(l) do if hid or n:sub(1, 1) ~= "." then if n:sub(-1) == "/" then d[#d + 1] = n else f[#f + 1] = n end end end
  table.sort(d) table.sort(f)
  self.items = {}
  if self.path ~= "/" then self.items[1] = {n = "..", dir = true} end
  for _, n in ipairs(d) do self.items[#self.items + 1] = {n = n:sub(1, -2), dir = true} end
  for _, n in ipairs(f) do local t = fs.read(join(self.path, n)) self.items[#self.items + 1] = {n = n, size = t and #t or 0} end
  self.i, self.top = math.min(self.i, math.max(#self.items, 1)), 0
  self.title = "files: " .. self.path
  if self.win then self.win.title = self.title self.win.fdirty = true end
 end
 c:load()
 function c:open(it, fedit)
  if it.dir then
   if it.n == ".." then
    local k = #self.path while k > 1 and self.path:sub(k, k) ~= "/" do k = k - 1 end
    local p = self.path:sub(1, k - 1) self.path = p == "" and "/" or p
   else self.path = join(self.path, it.n) end
   self.i = 1 self:load()
  else
   local p = join(self.path, it.n)
   if fedit then W.run("cravgedit " .. p) return end
   local k = #it.n while k > 0 and it.n:sub(k, k) ~= "." do k = k - 1 end
   local ext = k > 0 and it.n:sub(k + 1):lower() or ""
   local cmd = assoc[ext]
   if cmd then local m = cmd:find("%f", 1, true) W.run(m and (cmd:sub(1, m - 1) .. p .. cmd:sub(m + 2)) or (cmd .. " " .. p))
   elseif is_text(p) then W.run("cravgedit " .. p)
   else self.msg = "no association for ." .. ext end
  end
 end
 local function set_wall()
  local it = c.items[c.i]
  if not it or it.dir or it.n:sub(-4):lower() ~= ".bmp" then c.msg = "select a .bmp file" return end
  W.run("cravgbg " .. join(c.path, it.n)) c.msg = "wallpaper: " .. it.n
 end
 local function del_sel()
  local it = c.items[c.i]
  if not it or it.n == ".." then return end
  local ok, err = fs.remove(join(c.path, it.n))
  c.msg = ok and ("deleted " .. it.n) or tostring(err or "cannot delete") c:load()
 end
  local BTNS = {{"New", function() c.prompt = {kind = "new", label = "New file: ", inp = UI.input("note.txt")} end}, {"Edit", function() local it = c.items[c.i] if it and not it.dir then c:open(it, true) end end}, {"Mkdir", function() c.prompt = {kind = "mkdir", label = "Folder: ", inp = UI.input("dir1")} end}, {"Del", del_sel}, {"Wall", set_wall}, {"Reload", function() c:load() c.msg = "reloaded" end}}
 local function rows(self, h) return (h - 52) // 18 end
 function c:draw(s, w, h)
  gfx.fill(0, 0, w, h, self.bg) gfx.fill(0, 0, w, 24, self.fg, 20)
  local bx = w - 4 self.bhits = {}
  for idx = #BTNS, 1, -1 do
   local b = BTNS[idx] local bw = #b[1] * 8 + 10 bx = bx - bw
   gfx.fill(bx, 3, bw, 18, 0x32324c) gfx.text(bx + 5, 4, b[1], self.fg)
   self.bhits[#self.bhits + 1] = {bx, bw, b[2]} bx = bx - 4
  end
  gfx.text(6, 4, U.fit(self.path, math.max(40, bx - 10)), self.fg)
  local lw, n = w - self.pw, rows(self, h)
  if self.i <= self.top then self.top = self.i - 1 elseif self.i > self.top + n then self.top = self.i - n end
  for r = 1, n do
   local it = self.items[self.top + r]
   if it then
    local y = 26 + (r - 1) * 18
    if self.top + r == self.i then gfx.fill(2, y, lw - 4, 18, self.sel) end
    gfx.text(8, y + 1, it.dir and "[" or " ", self.dir) gfx.text(16, y + 1, U.fit(it.n, lw - 110), it.dir and self.dir or self.fg)
    if not it.dir then local sz = tostring(it.size) gfx.text(lw - 8 - #sz * 8, y + 1, sz, self.fg) end
   end
  end
  gfx.fill(lw, 24, 1, h - 48, self.fg, 60)
  local it = self.items[self.i]
  if it and not it.dir then
   local p = join(self.path, it.n)
   if it.n:sub(-4):lower() == ".bmp" then
    local bs = gfx.bmp(p)
    if bs then local iw, ih = gfx.sizeof(bs) local k = math.min((self.pw - 12) * 100 // iw, (h - 60) * 100 // ih, 400) gfx.scale(bs, lw + 6, 30, iw * k // 100, ih * k // 100) gfx.free(bs) end
   else
    local t, ln, y, pos = fs.read(p) or "", 0, 30, 1
    while y < h - 30 and pos <= #t and ln < 40 do
     local e = t:find("\n", pos, true) or #t + 1
     gfx.text(lw + 6, y, U.fit(t:sub(pos, e - 1), self.pw - 10), self.fg) pos, y, ln = e + 1, y + 16, ln + 1
    end
   end
  end
  gfx.fill(0, h - 24, w, 24, self.fg, 20)
  if self.prompt then
   gfx.text(6, h - 20, self.prompt.label, 0x88ccff)
   local lx = 6 + #self.prompt.label * 8 UI.draw_input(self.prompt.inp, lx, h - 20, w - lx - 8, self.fg)
  else gfx.text(6, h - 20, U.fit(self.msg ~= "" and self.msg or "Enter open  E edit  N new  W wall  F7 mkdir  Del rm  Alt+1..4", w - 12), self.fg) end
 end
 function c:key(k, m, ev)
  if self.prompt then
   if k == 27 then self.prompt = nil return true end
   if k == 10 then
    local pr = self.prompt self.prompt = nil local nm = U.trim(pr.inp.t)
    if nm ~= "" then
     local p = join(self.path, nm)
     if pr.kind == "new" then local ok, err = fs.write(p, "") self.msg = ok and ("created " .. nm) or tostring(err or "cannot create") self:load() if ok then W.run("cravgedit " .. p) end
     else self.msg = fs.mkdir(p) and ("created " .. nm) or "cannot create folder" self:load() end
    end
    return true
   end
   return UI.edit(self.prompt.inp, k, m, ev)
  end
  local it = self.items[self.i]
  local b = ev and ev.base or k if b >= 65 and b <= 90 then b = b + 32 end
  if k == term.UP then self.i = math.max(1, self.i - 1)
  elseif k == term.DOWN then self.i = math.min(#self.items, self.i + 1)
  elseif k == 10 and it then self:open(it)
  elseif k == 8 then self:open({n = "..", dir = true})
  elseif k == term.F1 + 4 then self:load() self.msg = "reloaded"
  elseif k == term.DEL then del_sel()
  elseif k == term.F1 + 6 then BTNS[3][2]()
  elseif (m & 14) == 0 and b == 101 and it and not it.dir then self:open(it, true)
  elseif (m & 14) == 0 and b == 110 then BTNS[1][2]()
  elseif (m & 14) == 0 and b == 119 then set_wall()
  elseif k >= 49 and k <= 57 and (m & 4) ~= 0 and books[k - 48] then self.path = books[k - 48].a.path or "/" self.i = 1 self:load()
  else return false end
  return true
 end
 function c:mouse(ev)
  if ev.t == "wheel" then self.i = U.clamp(self.i + (ev.wheel > 0 and -3 or 3), 1, math.max(#self.items, 1)) return true end
  if ev.t ~= "down" or ev.btn ~= 1 then return false end
  if ev.y < 24 then for _, h in ipairs(self.bhits or {}) do if ev.x >= h[1] and ev.x < h[1] + h[2] then h[3]() return true end end return true end
  local r = (ev.y - 26) // 18 + 1 local it = self.items[self.top + r]
  if ev.y >= 26 and it and ev.x < self.w - self.pw then
   local now = os.ms()
   if self.i == self.top + r and self.lc and now - self.lc < 400 then self:open(it) self.lc = nil else self.i = self.top + r self.lc = now end
   return true
  end
  return false
 end
 return c
end
return M
