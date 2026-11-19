--lua
-- cwm-deco - window frames: drawing and hit testing (module of cravgwm)
return function(W)
  local U, D = W.u, {}
  W.deco = D
  -- title buttons from the right: Close, Maximize, Iconify -> {name, x, y}
  local function buttons(w, fw)
    local th = W.cfg.theme
    local b, t = W.mets(w)
    local y, x = b + (t - th.bsize) // 2, fw - b - th.bgap - th.bsize
    return {{"Close", x, y}, {"Maximize", x - th.bsize - th.bgap, y}, {"Iconify", x - 2 * (th.bsize + th.bgap), y}}
  end
  function D.draw(w)
    local th = W.cfg.theme
    local b, t = W.mets(w)
    local fw, fh = W.fsize(w)
    local s = W.focus == w and th.a or th.i
    gfx.target(w.fs)
    gfx.clip()
    if b == 0 and t == 0 then gfx.blit(w.cs, 0, 0) gfx.target(0) return end
    gfx.fill(0, 0, fw, fh, s.bd)
    if t > 0 then
      if s.t1 ~= s.t2 then gfx.grad(b, b, fw - 2 * b, t, s.t1, s.t2, true) else gfx.fill(b, b, fw - 2 * b, t, s.t1) end
      local bs, sc, sz = buttons(w, fw), th.scale, th.bsize
      local room = bs[3][2] - b - 10
      local txt = U.fit(w.title, room, sc)
      local tx = th.align == "center" and b + 4 + (room - gfx.textw(txt, sc)) // 2 or b + 6
      gfx.text(tx, b + (t - 16 * sc) // 2, txt, s.tx, sc)
      for _, bt in ipairs(bs) do
        local x, y = bt[2], bt[3]
        gfx.fill(x, y, sz, sz, bt[1] == "Close" and th.close or bt[1] == "Maximize" and th.max or th.min)
        if bt[1] == "Close" then
          for i = 0, sz - 6 do gfx.fill(x + 2 + i, y + 2 + i, 2, 2, th.btx) gfx.fill(x + sz - 4 - i, y + 2 + i, 2, 2, th.btx) end
        elseif bt[1] == "Maximize" then
          gfx.fill(x + 3, y + 3, sz - 6, 2, th.btx) gfx.fill(x + 3, y + sz - 5, sz - 6, 2, th.btx)
          gfx.fill(x + 3, y + 3, 2, sz - 6, th.btx) gfx.fill(x + sz - 5, y + 3, 2, sz - 6, th.btx)
        else gfx.fill(x + 3, y + sz - 6, sz - 6, 2, th.btx) end
      end
    end
    if not w.shade then gfx.blit(w.cs, b, b + t) end
    gfx.target(0)
  end
  -- context under the point (x, y are relative to the frame)
  function D.hit(w, x, y)
    local th = W.cfg.theme
    local b, t = W.mets(w)
    local fw, fh = W.fsize(w)
    if b == 0 and t == 0 then return "Client" end
    local g, c = math.max(b, 4), 14
    local top, bot, lef, rig = y < g, y >= fh - g, x < g, x >= fw - g
    if (top and x < c) or (lef and y < c) then return "TLCorner" end
    if (top and x >= fw - c) or (rig and y < c) then return "TRCorner" end
    if (bot and x < c) or (lef and y >= fh - c) then return "BLCorner" end
    if (bot and x >= fw - c) or (rig and y >= fh - c) then return "BRCorner" end
    if top then return "Top" elseif bot then return "Bottom" elseif lef then return "Left" elseif rig then return "Right" end
    if y < b + t then
      for _, bt in ipairs(buttons(w, fw)) do if U.inside(x, y, bt[2], bt[3], th.bsize, th.bsize) then return bt[1] end end
      return "Titlebar"
    end
    return "Client"
  end
end
