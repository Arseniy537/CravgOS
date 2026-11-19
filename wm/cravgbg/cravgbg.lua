--lua
--needs cravgwm
-- cravgbg - wallpaper in the style of feh: solid, gradient, pattern, BMP image. Config: /etc/cravgwm/cravgbg.xml
local M = {single = true}
function M.new(W, args)
  local U = W.u
  local root = W.ui.config("cravgbg")
  local a = (root and root.a) or {mode = "gradient", color = "0x0B1D3A", to = "0x3A6EA5", direction = "vertical"}
  local c = {title = "cravgbg", kind = "desktop", class = "cravgbg", w = W.sw, h = W.sh, mode = a.mode or "solid"}
  local img
  if c.mode == "image" or c.mode == "tile" then
    local s, e = gfx.bmp(a.file or "")
    if not s then W.log("cravgbg: " .. tostring(e)) c.mode = "solid" else img = s end
  end
  function c:draw(s, w, h)
    local c1, c2 = U.color(a.color or a.from, 0x203040), U.color(a.to, 0x000000)
    if self.mode == "gradient" then gfx.grad(0, 0, w, h, c1, c2, a.direction ~= "horizontal")
    elseif self.mode == "pattern" then
      gfx.fill(0, 0, w, h, c1)
      local st = U.num(a.size, 32)
      for y = 0, h // st do for x = 0, w // st do
        if a.pattern == "dots" then gfx.fill(x * st + st // 2 - 1, y * st + st // 2 - 1, 3, 3, c2)
        elseif a.pattern == "grid" then gfx.fill(x * st, y * st, st, 1, c2) gfx.fill(x * st, y * st, 1, st, c2)
        elseif (x + y) % 2 == 0 then gfx.fill(x * st, y * st, st, st, c2) end
      end end
    elseif img then
      gfx.fill(0, 0, w, h, c1)
      local iw, ih = gfx.sizeof(img)
      local how = a.scale or "fill"
      if self.mode == "tile" then for y = 0, h // ih do for x = 0, w // iw do gfx.blit(img, x * iw, y * ih) end end
      elseif how == "stretch" then gfx.scale(img, 0, 0, w, h)
      elseif how == "center" then gfx.blit(img, (w - iw) // 2, (h - ih) // 2)
      else
        local k = how == "fit" and math.min(w * 100 // iw, h * 100 // ih) or math.max(w * 100 // iw, h * 100 // ih)
        local dw, dh = iw * k // 100, ih * k // 100
        gfx.scale(img, (w - dw) // 2, (h - dh) // 2, dw, dh)
      end
    else gfx.fill(0, 0, w, h, c1) end
  end
  return c
end
return M
