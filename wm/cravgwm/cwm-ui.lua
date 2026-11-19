--lua
-- cwm-ui - helpers for programs: line editor, config loading (module of cravgwm)
return function(W)
  local UI = {}
  W.ui = UI
  function UI.input(t) t = t or "" return {t = t, p = #t} end
  -- editing keys; returns true if the text or the cursor changed
  function UI.edit(e, k, m)
    local t, p = e.t, e.p
    if k == 8 then if p > 0 then e.t, e.p = t:sub(1, p - 1) .. t:sub(p + 1), p - 1 return true end
    elseif k == term.DEL then if p < #t then e.t = t:sub(1, p) .. t:sub(p + 2) return true end
    elseif k == term.LEFT then e.p = math.max(0, p - 1) return true
    elseif k == term.RIGHT then e.p = math.min(#t, p + 1) return true
    elseif k == term.HOME then e.p = 0 return true
    elseif k == term.END then e.p = #t return true
    elseif k >= 32 and k < 256 and (m or 0) & 6 == 0 then e.t, e.p = t:sub(1, p) .. string.char(k) .. t:sub(p + 1), p + 1 return true end
    return false
  end
  -- text of the editor with a cursor at (x, y); width w in pixels
  function UI.draw_input(e, x, y, w, fg, scale)
    scale = scale or 1
    local cols = w // (8 * scale)
    local from = math.max(0, e.p - cols + 1)
    gfx.text(x, y, e.t:sub(from + 1, from + cols), fg, scale)
    if (os.ms() // 500) % 2 == 0 then gfx.fill(x + (e.p - from) * 8 * scale, y + 14 * scale, 8 * scale, 2 * scale, fg) end
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
