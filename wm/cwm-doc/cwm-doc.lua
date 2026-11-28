--lua
--needs cravgwm
-- cwm-doc - reference for cravgwm config, apps, templates and clipboard (run in text console: cwm-doc)
local P = {[[
CRAVGWM - configuration reference (page 1/3)
Built-in defaults are used if /etc/cravgwm/rc.xml is missing. Templates live in /etc/cravgwm/tpl/.
Files: rc.xml  menu.xml  cravgcomp.xml  cravgbar.xml  cravgbg.xml  cravgrun.xml  cravgterm.xml  cravgfm.xml  exec-allow
Any file may contain <include href="/etc/cravgwm/other.xml"/>. Files are limited to 8192 bytes.
Start: boot with wmboot (pixel mode), then type: wm      Emergency exit: Ctrl+Alt+BackSpace
rc.xml:  <cravgwm>
 <desktops number="4"><name>web</name><name>code</name></desktops>
 <focus model="click|follow|none" raise="yes"/>   <placement policy="center|cascade|mouse"/>
 <autostart><run>cravgbg</run><run>cravgbar</run></autostart>   <menu file="/etc/cravgwm/menu.xml"/>
 <theme><title height="22" scale="1" align="left|center"/><border width="2"/><root color="#101820"/>
   <active title="#2a5caa" title2="#1b3f7a" text="#fff" border="#2a5caa"/><inactive .../>
   <buttons size="14" gap="4" close="#c33" max="#3a3" min="#ca3" text="#fff"/><menu bg= fg= sel= selfg= border= scale=/></theme>
Hotkeys: Super+Return/T term, Super+E fm, Super+G editor, Super+P paint, Super+D/R run, Alt+Tab, Alt+F4
]], [[
CRAVGWM - rc.xml bindings & templates (page 2/3)
 <keyboard><keybind key="A-Tab"><action name="NextWindow"/></keybind>
   <keybind key="W-Return"><action name="Execute"><command>cravgterm</command></action></keybind></keyboard>
 keys: modifiers S- C- A- W-(super) then a letter, digit, Tab Return Escape space BackSpace Delete Home End Prior Next Left Right Up Down F1..F10
 <mouse wheel="normal|inverted"><context name="Titlebar"><mousebind button="Left" action="Drag"><action name="Move"/></mousebind>
   <mousebind button="Left" action="DoubleClick"><action name="ToggleMaximize"/></mousebind></context> ...</mouse>
 contexts: Root Frame Client Titlebar Close Maximize Iconify Top Bottom Left Right TLCorner TRCorner BLCorner BRCorner
 actions: Execute(command) Exit Reconfigure ApplyTemplate(template,file) Close Raise Lower Focus Iconify Maximize Unmaximize
   ToggleMaximize Shade Unshade ToggleShade ToggleFullscreen ToggleDecorations ToggleAlwaysOnTop ToggleOmnipresent
   SendToDesktop(to) GoToDesktop(to=N|next|prev) NextWindow PreviousWindow MoveResizeTo(x,y,width,height) ShowMenu(menu) Move Resize
 menu.xml: built-in submenus: client-list-menu, client-menu, tpl-menu (lists /etc/cravgwm/tpl/*.xml and presets)
 Clipboard: Ctrl+A select all, Ctrl+C copy, Ctrl+X cut, Ctrl+V paste (shared with VirtualBox Host if enabled)
]], [[
CRAVGWM - programs (page 3/3)    install: stas install cravgwm
cravgedit      GUI text viewer/editor (.txt,.lua,.xml,.cfg,.log). Ctrl+S/O/N/A/C/X/V; saving XML configs reloads live.
cravgpaint     16-color 128x80 BMP Paint (Pen, Ers, Line, Rect, Fill, 1x/2x/3x, Open, Save, Wall to set wallpaper).
cravgterm      Terminal: GUI apps (fm edit paint run bg termconf rctpl), os.exec CLI, and os.run_tty (web luaide tetris ...).
               Copy/Paste: Ctrl+Shift+C copy buffer, Ctrl+V paste, Ctrl+A select input, or [Copy]/[Paste]/[Conf] buttons.
cravgbg        Live wallpaper: cravgbg FILE.bmp | next | gradient C1 C2 | solid C | pattern grid C1 C2 | reload.
cravgbar       Panel: launchers (Term, Files, Edit, Paint), desktops, taskbar, exec, clock.
cravgrun       Launcher (drun, window, run modes). Tab switches mode, Enter runs, Esc closes.
cravgfm        File manager: Enter/double-click opens via assoc or cravgedit; E edit, N new, W wallpaper, F7 mkdir, Del rm.
               Writable paths in Lua apps: /home, /docs, /lua, /etc/cravgwm.
]]}
local n = 1
while n >= 1 and n <= #P do
  term.color(7, 0) term.clear()
  term.write(P[n])
  term.color(8, 0) term.write("\n[Enter] next page   [b] back   [q] quit")
  local k = term.getkey()
  if k == 113 or k == 27 then break elseif k == 98 then n = n - 1 else n = n + 1 end
end
term.color(7, 0) term.clear()
