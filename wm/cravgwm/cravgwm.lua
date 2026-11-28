--lua
--needs cravgbar cravgbg cravgrun cravgterm cravgfm cravgedit cravgpaint cwm-doc
-- cravgwm - floating window manager in the style of Openbox (configuration: /etc/cravgwm/*.xml)
if CWM then return CWM end
local W = {}
CWM = W
local MODS = {"cwm-util", "cwm-xml", "cwm-xmlf", "cwm-conf", "cwm-win", "cwm-wops", "cwm-deco", "cwm-act", "cwm-input", "cwm-paint", "cwm-ui"}
local sw, sh = gfx.init()
if not sw then print("cravgwm: " .. tostring(sh)) return end
W.sw, W.sh = sw, sh
W.apps = {}
function W.log(m) end                                   -- до загрузки cwm-paint
for _, m in ipairs(MODS) do dofile("/apps/" .. m)(W) end
-- запуск программы по имени из /apps; возвращает окно
function W.spawn(name, args)
  local mod = W.apps[name]
  if not mod then
    if not fs.exists("/apps/" .. name) then W.log("not installed: " .. name .. " (stas install " .. name .. ")") return end
    local ok, m = pcall(dofile, "/apps/" .. name)
    if not ok or type(m) ~= "table" or not m.new then W.log(name .. ": " .. (ok and "is not a window program" or tostring(m))) return end
    W.apps[name], mod = m, m
  end
  if mod.single then
    for _, w in ipairs(W.wins) do if w.c.app == name then W.raise(w) W.focus_win(w) return w end end
  end
  local ok, c = pcall(mod.new, W, args or "")
  if not ok or not c then W.log(name .. ": " .. tostring(c)) return end
  c.app = name
  c.class = c.class or name
  return W.add(c)
end
function W.damage_all()
  W.damage(0, 0, sw, sh)
  for _, w in ipairs(W.wins) do w.fdirty = true w.cdirty = true end
end
local function start()
  W.conf.load()
  if fs.exists("/etc/cravgwm/cravgcomp.xml") then dofile("/apps/cwm-comp")(W) W.comp.load() end
  for _, cmd in ipairs(W.cfg.autostart) do W.run(cmd) end
  W.desk = math.min(W.desk, W.cfg.desktops)
  W.damage_all()
end
local function tick(dt)
  for _, w in ipairs(W.wins) do
    if w.c.tick and W.visible(w) and w.c:tick(dt) then w.cdirty = true end
  end
  if W.comp then W.comp.tick(dt) end
  if #W.logs > 0 and os.ms() - W.logs[1][2] > 15000 then table.remove(W.logs, 1) W.damage(0, 0, sw, 90) end
end
W.mkcursor()
W.mx, W.my = gfx.mouse()
W.cfg = nil
local ok, err = pcall(start)
if not ok then W.log("startup: " .. tostring(err)) end
if not W.cfg then W.cfg = {} end
local last = os.ms()
while not W.quit do
  local ev, n = gfx.poll(), 0
  while ev and n < 64 do
    local ok2, e2 = pcall(ev.type == "key" and W.input.key or W.input.mouse, ev)
    if not ok2 then W.log("input: " .. tostring(e2)) end
    n = n + 1
    ev = gfx.poll()
  end
  local now = os.ms()
  local ok3, e3 = pcall(tick, now - last)
  if not ok3 then W.log("tick: " .. tostring(e3)) end
  last = now
  if W.reconf then
    W.reconf = false
    local ok4, e4 = pcall(function() W.conf.load() W.damage_all() end)
    if not ok4 then W.log("reconfigure: " .. tostring(e4)) end
  end
  local ok5, e5 = pcall(W.render)
  if not ok5 then W.log("render: " .. tostring(e5)) W.dx = nil end
  if n == 0 then os.sleep(W.comp and W.comp.pending() and 4 or 2) end
end
gfx.close()
CWM = nil
