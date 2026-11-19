--lua
--needs cravgwm
-- cwm-doc - reference for the cravgwm configuration files (run in the text console: cwm-doc)
local P = {[[
CRAVGWM - configuration reference (page 1/3)
Built-in defaults are used if /etc/cravgwm/rc.xml is missing. To customize:
  mkdir /etc ; mkdir /etc/cravgwm ; edit /etc/cravgwm/rc.xml
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
]], [[
CRAVGWM - rc.xml bindings (page 2/3)
 <keyboard><keybind key="A-Tab"><action name="NextWindow"/></keybind>
   <keybind key="W-Return"><action name="Execute"><command>cravgterm</command></action></keybind></keyboard>
 keys: modifiers S- C- A- W-(super) then a letter, digit, Tab Return Escape space BackSpace Delete Home End Prior Next Left Right Up Down F1..F10
 <mouse wheel="normal|inverted"><context name="Titlebar"><mousebind button="Left" action="Drag"><action name="Move"/></mousebind>
   <mousebind button="Left" action="DoubleClick"><action name="ToggleMaximize"/></mousebind></context> ...</mouse>
 contexts: Root Frame Client Titlebar Close Maximize Iconify Top Bottom Left Right TLCorner TRCorner BLCorner BRCorner
 buttons: Left Right Middle Up Down (wheel), with modifiers (A-Left)     actions: Press Click DoubleClick Drag Release
 actions: Execute(command) Exit Reconfigure Close Raise Lower Focus Iconify Maximize Unmaximize ToggleMaximize Shade Unshade ToggleShade
   ToggleFullscreen ToggleDecorations ToggleAlwaysOnTop ToggleOmnipresent SendToDesktop(to) GoToDesktop(to=N|next|prev) NextWindow PreviousWindow
   MoveResizeTo(x,y,width,height; 50% allowed) ShowMenu(menu) Move Resize
 <applications><application class="cravgterm"><desktop>2</desktop><decor>no</decor><maximized>yes</maximized><position x= y=/><size w= h=/>
   <layer>below|normal|above</layer><focus>no</focus><alpha>90</alpha><omnipresent>yes</omnipresent></application></applications>
 menu.xml: <menus><menu id="root-menu"><item label="Terminal"><action name="Execute"><command>cravgterm</command></action></item>
   <separator/><menu id="sub" label="More"><item .../></menu></menu></menus>   built-in ids: client-list-menu, client-menu
]], [[
CRAVGWM - programs (page 3/3)    install: stas install NAME     start: Execute action, autostart, cravgrun or the terminal
cravgcomp.xml  <cravgcomp><shadow enabled="yes" radius="14" opacity="110" x="0" y="5" color="#000"/><fade enabled="yes" in="150" out="110"/>
   <opacity active="100" inactive="85"/><corners radius="8"/><rule class="cravgbar" shadow="no" opacity="95" corners="0"/></cravgcomp>
cravgbar.xml   <cravgbar position="top|bottom" height="28" bg= fg= highlight= scale="1" taskwidth="170"><items><launcher label="T" command="cravgterm"/>
   <desktops/><taskbar/><text value="..."/><exec command="free" interval="10000"/><clock format="%H:%M:%S %d.%m.%Y"/></items></cravgbar>
cravgbg.xml    <cravgbg mode="solid|gradient|pattern|image|tile" color= from= to= direction="vertical|horizontal" pattern="checker|dots|grid" size="32"
   file="/etc/cravgwm/wall.bmp" scale="fill|fit|stretch|center"/>      (BMP up to 8192 bytes: 24/32/8/4/1 bit, no compression)
cravgrun.xml   <cravgrun width="520" lines="8" modes="drun,window,run" prompt= bg= fg= selbg= selfg= border=><entry name="Files" command="cravgfm"/>
   <scan dir="/apps" prefix="cravg"/></cravgrun>        Tab changes mode, Enter runs, Esc closes
cravgterm.xml  <cravgterm width="640" height="400" fg= bg= scale="1" prompt="$ " scrollback="200"><alias name="ll" value="ls"/></cravgterm>
   shell commands work only if the path /apps/cravgterm is a line of /etc/cravgwm/exec-allow (os.exec is denied otherwise)
cravgfm.xml    <cravgfm width= height= bg= fg= selbg= dircolor= start="/" hidden="no" preview="200"><assoc ext="bmp" command="cravgbg %f"/><bookmark path="/lua"/></cravgfm>
   files can be changed only under /lua. Keys: Enter, BackSpace, F5, Del, F7, Alt+1..9 bookmarks
Log: errors appear in red at the top left of the screen for 15 seconds.
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
