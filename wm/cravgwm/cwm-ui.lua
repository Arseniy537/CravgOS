--lua
-- cwm-ui - helpers for programs: line editor with clipboard, config loading (module of cravgwm)
return function(W)
  local UI = {}
  W.ui = UI
  function UI.input(t) t = t or "" return {t = t, p = #t, from = 0} end
  local function srange(e)
    if not e.s or e.s == e.p then return nil end
    if e.s < e.p then return e.s, e.p end
    return e.p, e.s
  end
  local function delsel(e)
    local a, b = srange(e)
    if not a then return false end
    e.t = e.t:sub(1, a) .. e.t:sub(b + 1)
    e.p, e.s = a, nil
    return true
  end
  local function getsel(e)
    local a, b = srange(e)
    return a and e.t:sub(a + 1, b) or ""
  end
  local function basekey(k, ev)
    local b = ev and ev.base or k
    if b >= 65 and b <= 90 then b = b + 32 end
    return b
  end
  -- editing keys; returns true if the text, selection or cursor changed
  function UI.edit(e, k, m, ev)
    m = m or 0
    local t, p = e.t, e.p
    local ctrl, shift = (m & 2) ~= 0, (m & 1) ~= 0
    local b = basekey(k, ev)
    if ctrl and (m & 12) == 0 then
      if b == 97 or k == 1 then e.s, e.p = 0, #t return true end
      if b == 99 or k == 3 then
        local s = getsel(e)
        if s ~= "" and os.setclip then os.setclip(s) end
        return s ~= ""
      end
      if b == 120 or k == 24 then
        local s = getsel(e)
        if s ~= "" then if os.setclip then os.setclip(s) end delsel(e) return true end
        return false
      end
      if b == 118 or k == 22 then
        local s = os.getclip and os.getclip() or ""
        if s == "" then return false end
        local nl = s:find("\n", 1, true)
        if nl then s = s:sub(1, nl - 1) end
        if s:sub(-1) == "\r" then s = s:sub(1, -2) end
        if s == "" then return false end
        delsel(e)
        e.t = e.t:sub(1, e.p) .. s .. e.t:sub(e.p + 1)
        e.p = e.p + #s
        return true
      end
      return false
    end
    if k == 8 then
      if delsel(e) then return true end
      if p > 0 then e.t, e.p = t:sub(1, p - 1) .. t:sub(p + 1), p - 1 return true end
    elseif k == term.DEL then
      if delsel(e) then return true end
      if p < #t then e.t = t:sub(1, p) .. t:sub(p + 2) return true end
    elseif k == term.LEFT or k == term.RIGHT or k == term.HOME or k == term.END then
      local a, b = srange(e)
      if shift then if not e.s then e.s = p end else e.s = nil end
      if k == term.LEFT then e.p = (not shift and a) and a or math.max(0, p - 1)
      elseif k == term.RIGHT then e.p = (not shift and b) and b or math.min(#t, p + 1)
      elseif k == term.HOME then e.p = 0
      else e.p = #t end
      if e.s == e.p then e.s = nil end
      return true
    elseif k >= 32 and k < 256 and (m & 14) == 0 then
      delsel(e)
      e.t, e.p = e.t:sub(1, e.p) .. string.char(k) .. e.t:sub(e.p + 1), e.p + 1
      return true
    end
    return false
  end
  -- text of the editor with selection highlight and cursor at (x, y); width w in pixels
  function UI.draw_input(e, x, y, w, fg, scale, selbg)
    scale = scale or 1
    local cw = 8 * scale
    local cols = math.max(1, w // cw)
    local from = e.from or 0
    if e.p < from then from = e.p elseif e.p >= from + cols then from = e.p - cols + 1 end
    from = math.max(0, from)
    e.from = from
    local a, b = srange(e)
    if a then
      local va, vb = math.max(a, from), math.min(b, from + cols)
      if vb > va then gfx.fill(x + (va - from) * cw, y, (vb - va) * cw, 16 * scale, selbg or 0x2a5caa) end
    end
    gfx.text(x, y, e.t:sub(from + 1, from + cols), fg, scale)
    if (os.ms() // 500) % 2 == 0 then gfx.fill(x + (e.p - from) * cw, y + 14 * scale, cw, 2 * scale, fg) end
  end
  -- loads /etc/cravgwm/<name>.xml; on parse error writes to the WM log
  function UI.config(name)
    local root, err = W.xml.load("/etc/cravgwm/" .. name .. ".xml")
    if not root and err and not err:find("cannot read", 1, true) then W.log(name .. ": " .. err) end
    return root
  end
  function UI.color(s, d) return W.u.color(s, d) end
  function UI.num(s, d) return W.u.num(s, d) end
  function UI.box(x, y, w, h, bg, bd) gfx.fill(x, y, w, h, bd) gfx.fill(x + 1, y + 1, w - 2, h - 2, bg) end
end
