--lua
-- cp-bmp - drawing primitives and 4-bit BMP open/save for cravgpaint (uses c.img surface directly)
return function(W, c, CW, CH, stitle)
  local U = W.u
  local PAL = {0x000000, 0xFFFFFF, 0x808080, 0xC0C0C0, 0xCC2222, 0xFF7722, 0xEECC22, 0x33AA33,
    0x22CCAA, 0x2A5CAA, 0x5588FF, 0x8833CC, 0xFF66AA, 0x774422, 0x0B1D3A, 0x1E1E2E}
  local PIDX = {}
  for i, rgb in ipairs(PAL) do PIDX[rgb] = i - 1 end
  local P = {PAL = PAL}
  function P.plot(x, y, ci)
    local r = c.bsz - 1
    local x0, y0 = math.max(0, x - r), math.max(0, y - r)
    local x1, y1 = math.min(CW - 1, x + r), math.min(CH - 1, y + r)
    if x1 >= x0 and y1 >= y0 then gfx.fill(x0, y0, x1 - x0 + 1, y1 - y0 + 1, PAL[ci + 1]) end
  end
  function P.line(x0, y0, x1, y1, ci, fn)
    local dx, dy = math.abs(x1 - x0), -math.abs(y1 - y0)
    local sx, sy, err = x0 < x1 and 1 or -1, y0 < y1 and 1 or -1, dx + dy
    while true do
      fn(x0, y0, ci)
      if x0 == x1 and y0 == y1 then break end
      local e2 = 2 * err
      if e2 >= dy then err = err + dy x0 = x0 + sx end
      if e2 <= dx then err = err + dx y0 = y0 + sy end
    end
  end
  function P.rect(x0, y0, x1, y1, ci, fn)
    if x0 > x1 then x0, x1 = x1, x0 end
    if y0 > y1 then y0, y1 = y1, y0 end
    for x = x0, x1 do fn(x, y0, ci) fn(x, y1, ci) end
    for y = y0, y1 do fn(x0, y, ci) fn(x1, y, ci) end
  end
  function P.flood(sx, sy, ci)
    if sx < 0 or sy < 0 or sx >= CW or sy >= CH then return end
    gfx.target(c.img)
    local old = gfx.px(sx, sy) & 0xFFFFFF
    local nw = PAL[ci + 1]
    if old == nw then gfx.target(0) return end
    local st = {{sx, sy}}
    while #st > 0 do
      local pt = table.remove(st)
      local x, y = pt[1], pt[2]
      if (gfx.px(x, y) & 0xFFFFFF) == old then
        local lx, rx = x, x
        while lx > 0 and (gfx.px(lx - 1, y) & 0xFFFFFF) == old do lx = lx - 1 end
        while rx + 1 < CW and (gfx.px(rx + 1, y) & 0xFFFFFF) == old do rx = rx + 1 end
        gfx.fill(lx, y, rx - lx + 1, 1, nw)
        for nx = lx, rx do
          if y > 0 and (gfx.px(nx, y - 1) & 0xFFFFFF) == old then st[#st + 1] = {nx, y - 1} end
          if y + 1 < CH and (gfx.px(nx, y + 1) & 0xFFFFFF) == old then st[#st + 1] = {nx, y + 1} end
        end
      end
    end
    gfx.target(0)
  end
  function P.clr(ci)
    gfx.target(c.img) gfx.fill(0, 0, CW, CH, PAL[(ci or 1) + 1]) gfx.target(0)
  end
  local function near(rgb)
    rgb = rgb & 0xFFFFFF
    local hit = PIDX[rgb]
    if hit then return hit end
    local r, g, b, best, bd = (rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255, 0, 1000000
    for i, p in ipairs(PAL) do
      local dr, dg, db = r - ((p >> 16) & 255), g - ((p >> 8) & 255), b - (p & 255)
      local d = dr * dr + dg * dg + db * db
      if d < bd then bd, best = d, i - 1 end
    end
    return best
  end
  function P.load_bmp(p)
    p = U.trim(p or "")
    if p == "" then return end
    local s, err = gfx.bmp(p)
    if not s then c.msg = tostring(err or "load failed") return end
    gfx.target(c.img)
    gfx.scale(s, 0, 0, CW, CH)
    gfx.free(s)
    for y = 0, CH - 1 do
      for x = 0, CW - 1 do
        local rgb = gfx.px(x, y) & 0xFFFFFF
        if not PIDX[rgb] then gfx.fill(x, y, 1, 1, PAL[near(rgb) + 1]) end
      end
    end
    gfx.target(0)
    c.path, c.mod, c.msg = p, false, "opened " .. p
    stitle()
  end
  local function le32(v) return string.char(v & 255, (v >> 8) & 255, (v >> 16) & 255, (v >> 24) & 255) end
  function P.save_bmp(p)
    p = U.trim(p or "")
    if p == "" then p = "/home/art1.bmp" end
    if p:sub(1, 6) == "/home/" and not fs.exists("/home") then fs.mkdir("/home") end
    local stride = (CW * 4 + 31) // 32 * 4
    local sz = 118 + stride * CH
    local out = {"BM", le32(sz), "\0\0\0\0", le32(118), le32(40), le32(CW), le32(CH), "\1\0\4\0", le32(0), le32(stride * CH), le32(2835), le32(2835), le32(16), le32(0)}
    for _, rgb in ipairs(PAL) do out[#out + 1] = string.char(rgb & 255, (rgb >> 8) & 255, (rgb >> 16) & 255, 0) end
    local pad = string.rep("\0", stride - CW // 2)
    gfx.target(c.img)
    for y = CH - 1, 0, -1 do
      local row = {}
      for x = 0, CW - 1, 2 do
        local c1, c2 = near(gfx.px(x, y)), near(gfx.px(x + 1, y))
        row[#row + 1] = string.char((c1 << 4) | c2)
      end
      if #pad > 0 then row[#row + 1] = pad end
      out[#out + 1] = table.concat(row)
    end
    gfx.target(0)
    local ok, err = fs.write(p, table.concat(out))
    if not ok then c.msg = tostring(err or "save failed") return false end
    c.path, c.mod, c.msg = p, false, "saved " .. p .. " (" .. sz .. "B)"
    stitle()
    return true
  end
  function P.set_wall()
    local p = c.path ~= "" and c.path or "/home/art1.bmp"
    if not P.save_bmp(p) then return end
    if not fs.exists("/etc") then fs.mkdir("/etc") end
    if not fs.exists("/etc/cravgwm") then fs.mkdir("/etc/cravgwm") end
    fs.write("/etc/cravgwm/cravgbg.xml", '<cravgbg mode="image" file="' .. p .. '" scale="stretch"/>\n')
    W.run("cravgbg " .. p)
    c.msg = "wallpaper: " .. p
  end
  return P
end
