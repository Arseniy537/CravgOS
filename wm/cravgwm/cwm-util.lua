--lua
-- cwm-util - helpers: colors, strings, geometry, clock (module of cravgwm)
return function(W)
  local U = {}
  W.u = U
  local N = {black = 0, white = 0xFFFFFF, red = 0xFF0000, green = 0x008000, lime = 0x00FF00, blue = 0x0000FF, yellow = 0xFFFF00,
    cyan = 0x00FFFF, magenta = 0xFF00FF, gray = 0x808080, grey = 0x808080, silver = 0xC0C0C0, orange = 0xFFA500,
    navy = 0x000080, teal = 0x008080, purple = 0x800080, maroon = 0x800000, olive = 0x808000}
  local function hex(s)
    local v = 0
    for i = 1, #s do
      local c = s:byte(i)
      c = c >= 97 and c - 87 or c >= 65 and c - 55 or c - 48
      if c < 0 or c > 15 then return nil end
      v = v * 16 + c
    end
    return v
  end
  -- "#rrggbb", "#rgb", "0xRRGGBB", color names or a number
  function U.color(s, def)
    if type(s) == "number" then return s end
    if not s then return def end
    s = s:lower()
    if N[s] then return N[s] end
    if s:sub(1, 1) == "#" then s = s:sub(2) elseif s:sub(1, 2) == "0x" then s = s:sub(3) end
    local v = hex(s)
    if not v then return def end
    if #s == 3 then return (v // 256 * 17) * 65536 + (v // 16 % 16 * 17) * 256 + v % 16 * 17 end
    if #s == 6 then return v end
    return def
  end
  function U.num(s, def) return tonumber(s) or def end
  function U.bool(s, def)
    if s == nil then return def end
    s = tostring(s):lower()
    if s == "yes" or s == "true" or s == "1" or s == "on" then return true end
    if s == "no" or s == "false" or s == "0" or s == "off" then return false end
    return def
  end
  function U.trim(s)
    local a, b = 1, #s
    while a <= b and s:byte(a) <= 32 do a = a + 1 end
    while b >= a and s:byte(b) <= 32 do b = b - 1 end
    return s:sub(a, b)
  end
  function U.split(s, sep)
    local r, i = {}, 1
    while true do
      local j = s:find(sep, i, true)
      if not j then r[#r + 1] = s:sub(i) return r end
      r[#r + 1] = s:sub(i, j - 1)
      i = j + #sep
    end
  end
  function U.has(t, v) for _, x in ipairs(t) do if x == v then return true end end return false end
  function U.clamp(v, lo, hi) if v < lo then return lo end if v > hi then return hi end return v end
  function U.inside(px, py, x, y, w, h) return px >= x and py >= y and px < x + w and py < y + h end
  -- cut a string to a pixel width (8 px per character, scale 1..)
  function U.fit(s, w, scale)
    local n = w // (8 * (scale or 1))
    if #s <= n then return s end
    if n < 2 then return "" end
    return s:sub(1, n - 1) .. "~"
  end
  local DN = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"}
  local MN = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"}
  local function p2(v) return string.format("%02d", v) end
  -- %H %M %S %d %m %Y %y %a %b %%
  function U.clock(fmt)
    local d, out, i = os.date(), {}, 1
    while i <= #fmt do
      local c = fmt:sub(i, i)
      if c == "%" and i < #fmt then
        i = i + 1
        local k = fmt:sub(i, i)
        local v = k == "H" and p2(d.hour) or k == "M" and p2(d.min) or k == "S" and p2(d.sec) or k == "d" and p2(d.day)
          or k == "m" and p2(d.month) or k == "Y" and d.year or k == "y" and p2(d.year % 100) or k == "a" and DN[d.wday]
          or k == "b" and MN[d.month] or k == "%" and "%" or "%" .. k
        out[#out + 1] = v
      else out[#out + 1] = c end
      i = i + 1
    end
    return table.concat(out)
  end
end
