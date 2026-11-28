--lua
-- ce-buf - text buffer, selection, clipboard and file I/O for cravgedit
return function(W, c, UI, stitle, vrows, vcols)
  local U = W.u
  local B = {}
  local function split_ln(d)
    local ls, i = {}, 1
    while true do
      local j = d:find("\n", i, true)
      if not j then ls[#ls + 1] = d:sub(i) break end
      local s = d:sub(i, j - 1)
      if s:sub(-1) == "\r" then s = s:sub(1, -2) end
      ls[#ls + 1] = s i = j + 1
    end
    if #ls == 0 then ls[1] = "" end
    return ls
  end
  function B.load(p)
    p = U.trim(p or "")
    c.cx, c.cy, c.top, c.left, c.ay, c.ax, c.mod = 0, 1, 0, 0, nil, nil, false
    if p == "" then c.lines, c.path, c.msg = {""}, "", "new file" stitle() return end
    local d = fs.read(p)
    if not d then
      if fs.exists(p) then c.msg = "cannot read " .. p return end
      c.lines, c.path, c.msg = {""}, p, "new: " .. p stitle() return
    end
    c.lines, c.path, c.msg = split_ln(d), p, #d .. "B loaded" stitle()
  end
  function B.srange()
    if not c.ay or (c.ay == c.cy and c.ax == c.cx) then return nil end
    if c.ay < c.cy or (c.ay == c.cy and c.ax < c.cx) then return c.ay, c.ax, c.cy, c.cx end
    return c.cy, c.cx, c.ay, c.ax
  end
  function B.getsel()
    local y1, x1, y2, x2 = B.srange()
    if not y1 then return "" end
    if y1 == y2 then return c.lines[y1]:sub(x1 + 1, x2) end
    local out = {c.lines[y1]:sub(x1 + 1)}
    for y = y1 + 1, y2 - 1 do out[#out + 1] = c.lines[y] end
    out[#out + 1] = c.lines[y2]:sub(1, x2)
    return table.concat(out, "\n")
  end
  function B.delsel()
    local y1, x1, y2, x2 = B.srange()
    if not y1 then return false end
    c.lines[y1] = c.lines[y1]:sub(1, x1) .. c.lines[y2]:sub(x2 + 1)
    for _ = y1 + 1, y2 do table.remove(c.lines, y1 + 1) end
    c.cy, c.cx, c.ay, c.ax, c.mod = y1, x1, nil, nil, true
    return true
  end
  function B.insert(s)
    B.delsel()
    local ln, parts = c.lines[c.cy], split_ln(s)
    local pre, suf = ln:sub(1, c.cx), ln:sub(c.cx + 1)
    if #parts == 1 then
      c.lines[c.cy] = pre .. parts[1] .. suf
      c.cx = c.cx + #parts[1]
    else
      c.lines[c.cy] = pre .. parts[1]
      for k = 2, #parts - 1 do table.insert(c.lines, c.cy + k - 1, parts[k]) end
      local ny = c.cy + #parts - 1
      table.insert(c.lines, ny, parts[#parts] .. suf)
      c.cy, c.cx = ny, #parts[#parts]
    end
    c.mod = true stitle()
  end
  local function reload_cfg(p)
    if p == "/etc/cravgwm/rc.xml" or p == "/etc/cravgwm/menu.xml" or p == "/etc/cravgwm/cravgcomp.xml" then
      if W.menu and W.menu.reload then W.menu.reload() end
      W.reconf = true c.msg = "saved & reconfigured"
    elseif p == "/etc/cravgwm/cravgbg.xml" then
      W.run("cravgbg reload") c.msg = "saved & applied wallpaper"
    elseif p == "/etc/cravgwm/cravgterm.xml" then
      for _, w in ipairs(W.wins) do if w.c.app == "cravgterm" and w.c.reload then w.c:reload() w.cdirty = true end end
      c.msg = "saved & reloaded terminal"
    elseif p == "/etc/cravgwm/cravgbar.xml" then
      for _, w in ipairs(W.wins) do if w.c.app == "cravgbar" and w.c.reload then w.c:reload() w.cdirty = true end end
      c.msg = "saved & reloaded bar"
    else c.msg = "saved " .. p end
  end
  function B.save(p)
    p = U.trim(p or "")
    if p == "" then c.prompt = {kind = "save", label = "Save: ", inp = UI.input("/home/note.txt")} return end
    local data = table.concat(c.lines, "\n")
    if #data > 8192 then c.msg = "too large (" .. #data .. " > 8192)" return end
    local ok, err = fs.write(p, data)
    if not ok then c.msg = tostring(err or "write failed") return end
    c.path, c.mod = p, false stitle() reload_cfg(p)
  end
  function B.vis()
    local rows, cols = vrows(), vcols()
    if c.cy <= c.top then c.top = c.cy - 1 elseif c.cy > c.top + rows then c.top = c.cy - rows end
    c.top = U.clamp(c.top, 0, math.max(0, #c.lines - rows))
    if c.cx < c.left then c.left = c.cx elseif c.cx >= c.left + cols then c.left = c.cx - cols + 1 end
    c.left = math.max(0, c.left)
  end
  function B.mv(ny, nx, shift)
    if shift then if not c.ay then c.ay, c.ax = c.cy, c.cx end else c.ay, c.ax = nil, nil end
    c.cy = U.clamp(ny, 1, #c.lines)
    c.cx = U.clamp(nx, 0, #c.lines[c.cy])
    if c.ay == c.cy and c.ax == c.cx then c.ay, c.ax = nil, nil end
    B.vis()
  end
  function B.bs()
    if not B.delsel() then
      if c.cx > 0 then local ln = c.lines[c.cy] c.lines[c.cy] = ln:sub(1, c.cx - 1) .. ln:sub(c.cx + 1) c.cx, c.mod = c.cx - 1, true
      elseif c.cy > 1 then local ox = #c.lines[c.cy - 1] c.lines[c.cy - 1] = c.lines[c.cy - 1] .. c.lines[c.cy] table.remove(c.lines, c.cy) c.cy, c.cx, c.mod = c.cy - 1, ox, true end
    end
    B.vis() stitle()
  end
  function B.del()
    if not B.delsel() then
      local ln = c.lines[c.cy]
      if c.cx < #ln then c.lines[c.cy] = ln:sub(1, c.cx) .. ln:sub(c.cx + 2) c.mod = true
      elseif c.cy < #c.lines then c.lines[c.cy] = ln .. c.lines[c.cy + 1] table.remove(c.lines, c.cy + 1) c.mod = true end
    end
    B.vis() stitle()
  end
  return B
end
