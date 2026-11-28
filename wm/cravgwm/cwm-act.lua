--lua
-- cwm-act - actions for key and mouse bindings and rc.xml templates (module of cravgwm)
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
    if args ~= "" then
      for _, w in ipairs(W.wins) do
        if w.c.app == name and w.c.apply then w.c:apply(args) return w end
      end
    end
    return W.spawn(name, args)
  end
  local TPLS = {
    default = '<cravgwm><desktops number="4"><name>web</name><name>code</name><name>files</name><name>misc</name></desktops>'
      .. '<focus model="click" raise="yes"/><placement policy="center"/><autostart><run>cravgbg</run><run>cravgbar</run></autostart>'
      .. '<menu file="/etc/cravgwm/menu.xml"/><theme><title height="22" align="left"/><border width="2"/><root color="#101820"/>'
      .. '<active title="#2a5caa" title2="#1b3f7a" text="#ffffff" border="#2a5caa"/><inactive title="#555555" text="#cccccc" border="#555555"/></theme></cravgwm>\n',
    dark = '<cravgwm><desktops number="4"><name>web</name><name>code</name><name>files</name><name>misc</name></desktops>'
      .. '<focus model="follow" raise="yes"/><placement policy="cascade"/><autostart><run>cravgbg</run><run>cravgbar</run></autostart>'
      .. '<menu file="/etc/cravgwm/menu.xml"/><theme><title height="22" align="left"/><border width="2"/><root color="#0d1117"/>'
      .. '<active title="#3b4261" title2="#1f2335" text="#c0caf5" border="#7aa2f7"/><inactive title="#1e2030" text="#565f89" border="#292e42"/>'
      .. '<menu bg="#1a1b26" fg="#c0caf5" sel="#3b4261" selfg="#ffffff" border="#7aa2f7"/></theme></cravgwm>\n',
    compact = '<cravgwm><desktops number="2"><name>main</name><name>work</name></desktops>'
      .. '<focus model="click" raise="yes"/><placement policy="cascade"/><autostart><run>cravgbg</run><run>cravgbar</run></autostart>'
      .. '<menu file="/etc/cravgwm/menu.xml"/><theme><title height="18" align="left"/><border width="1"/>'
      .. '<buttons size="12" gap="3"/><active title="#1e6655" text="#ffffff" border="#1e6655"/><inactive title="#383838" text="#aaaaaa" border="#383838"/></theme></cravgwm>\n',
    classic = '<cravgwm><desktops number="4"><name>1</name><name>2</name><name>3</name><name>4</name></desktops>'
      .. '<focus model="click" raise="yes"/><placement policy="center"/><autostart><run>cravgbg</run><run>cravgbar</run></autostart>'
      .. '<menu file="/etc/cravgwm/menu.xml"/><theme><title height="24" align="center"/><border width="3"/>'
      .. '<active title="#1b4f72" title2="#2874a6" text="#ffffff" border="#2874a6"/><inactive title="#4a4a4a" text="#cccccc" border="#4a4a4a"/></theme></cravgwm>\n'
  }
  W.tpls = {"default", "dark", "compact", "classic"}
  function A.ApplyTemplate(c, a)
    local nm = U.trim((a and (a.template or a.name)) or "default")
    local fp = (a and a.file) or ("/etc/cravgwm/tpl/" .. nm .. (nm:sub(-4) == ".xml" and "" or ".xml"))
    local data = fs.exists(fp) and fs.read(fp) or TPLS[nm]
    if not data then W.log("unknown template: " .. nm) return end
    if not fs.exists("/etc") then fs.mkdir("/etc") end
    if not fs.exists("/etc/cravgwm") then fs.mkdir("/etc/cravgwm") end
    local ok, err = fs.write("/etc/cravgwm/rc.xml", data)
    if not ok then W.log("template write: " .. tostring(err)) return end
    if W.menu and W.menu.reload then W.menu.reload() end
    W.reconf = true
  end
  function A.Execute(c, a) W.run(a.command or a.execute) end
  function A.Exit() W.quit = true end
  function A.Reconfigure() if W.menu and W.menu.reload then W.menu.reload() end W.reconf = true end
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
