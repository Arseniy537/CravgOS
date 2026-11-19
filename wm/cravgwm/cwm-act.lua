--lua
-- cwm-act - actions for key and mouse bindings (module of cravgwm)
return function(W)
  local U = W.u
  local A = {}
  W.act = A
  local function pct(v, base, def)               -- число или "50%"
    if not v then return def end
    if v:sub(-1) == "%" then return (tonumber(v:sub(1, -2)) or 0) * base // 100 end
    return tonumber(v) or def
  end
  -- запуск программы: "имя аргументы"
  function W.run(cmd)
    cmd = U.trim(cmd or "")
    if cmd == "" then return end
    local sp = cmd:find(" ", 1, true)
    local name, args = cmd:sub(1, (sp or #cmd + 1) - 1), sp and U.trim(cmd:sub(sp + 1)) or ""
    return W.spawn(name, args)
  end
  function A.Execute(c, a) W.run(a.command or a.execute) end
  function A.Exit() W.quit = true end
  function A.Reconfigure() W.reconf = true end
  function A.Close(c) if c.w then W.close(c.w) end end
  function A.Raise(c) if c.w then W.raise(c.w) end end
  function A.Lower(c) if c.w then W.lower(c.w) end end
  function A.Focus(c) if c.w and not c.w.kind then W.focus_win(c.w) end end
  function A.Iconify(c) if c.w then W.iconify(c.w) end end
  function A.Maximize(c) if c.w then W.maximize(c.w, true) end end
  function A.Unmaximize(c) if c.w then W.maximize(c.w, false) end end
  function A.ToggleMaximize(c) if c.w then W.maximize(c.w, not c.w.max) end end
  function A.Shade(c) if c.w then W.shade(c.w, true) end end
  function A.Unshade(c) if c.w then W.shade(c.w, false) end end
  function A.ToggleShade(c) if c.w then W.shade(c.w, not c.w.shade) end end
  function A.ToggleFullscreen(c) if c.w then W.fullscreen(c.w, not c.w.full) end end
  function A.ToggleDecorations(c) if c.w then W.nodecor(c.w, not c.w.nodecor) end end
  function A.ToggleAlwaysOnTop(c) local w = c.w if w and not w.kind then w.layer = w.layer == 3 and 2 or 3 W.raise(w) end end
  function A.ToggleOmnipresent(c) local w = c.w if w and not w.kind then w.desk = w.desk == 0 and W.desk or 0 W.damage(0, 0, W.sw, W.sh) end end
  function A.SendToDesktop(c, a) if c.w then W.send(c.w, tonumber(a.to) or W.desk) end end
  function A.GoToDesktop(c, a)
    local to = a.to or "1"
    if to == "next" then W.goto(W.desk % W.cfg.desktops + 1) elseif to == "previous" or to == "prev" then W.goto((W.desk - 2) % W.cfg.desktops + 1)
    else W.goto(tonumber(to) or W.desk) end
  end
  function A.NextWindow() W.cycle(1) end
  function A.PreviousWindow() W.cycle(-1) end
  function A.MoveResizeTo(c, a)
    local w = c.w
    if not w or w.kind then return end
    local wx, wy, ww, wh = W.work()
    if w.max then W.maximize(w, false) end
    local b, t = W.mets(w)
    local nw, nh = pct(a.width, ww, w.cw + 2 * b), pct(a.height, wh, w.ch + t + 2 * b)
    W.move(w, wx + pct(a.x, ww, w.x - wx), wy + pct(a.y, wh, w.y - wy))
    W.resize(w, nw - 2 * b, nh - t - 2 * b)
  end
  function A.ShowMenu(c, a)
    if not W.menu then dofile("/apps/cwm-menu")(W) end
    W.menu.show(a.menu or "root-menu", c)
  end
  function A.Move(c) if c.w and not c.w.max then W.input.start("move", c.w, c.ev) end end
  function A.Resize(c) if c.w and not c.w.max then W.input.start("resize", c.w, c.ev, c.ctx) end end
  function W.do_acts(list, ctx)
    for _, a in ipairs(list) do
      local f = A[a.name]
      if f then f(ctx, a.a) else W.log("unknown action " .. a.name) end
    end
  end
end
