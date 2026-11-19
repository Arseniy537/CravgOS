--lua
-- cwm-xml - XML parser (module of cravgwm)
return function(W)
  local X, U = {}, W.u
  W.xml = X
  local Node = {}
  Node.__index = Node
  function Node.child(n, tag) for _, c in ipairs(n.k) do if c.tag == tag then return c end end end
  function Node.list(n, tag)
    local r = {}
    for _, c in ipairs(n.k) do if not tag or c.tag == tag then r[#r + 1] = c end end
    return r
  end
  function Node.get(n, name, def) local v = n.a[name] if v == nil then return def end return v end
  local function unesc(s)
    if not s:find("&", 1, true) then return s end
    local out, i = {}, 1
    while i <= #s do
      local c, e = s:sub(i, i), nil
      if c == "&" then e = s:find(";", i, true) end
      local ent = e and s:sub(i, e)
      local m = ent == "&lt;" and "<" or ent == "&gt;" and ">" or ent == "&amp;" and "&" or ent == "&quot;" and '"' or ent == "&apos;" and "'"
      if m then out[#out + 1] = m i = e + 1 else out[#out + 1] = c i = i + 1 end
    end
    return table.concat(out)
  end
  local function lineno(src, p)
    local n, i = 1, 1
    while true do
      local j = src:find("\n", i, true)
      if not j or j >= p then return n end
      n, i = n + 1, j + 1
    end
  end
  local function sp(src, i) while i <= #src and src:byte(i) <= 32 do i = i + 1 end return i end
  function X.parse(src)
    local root = setmetatable({tag = "#root", a = {}, k = {}, t = ""}, Node)
    local stack, cur, p, n = {root}, root, 1, #src
    local function fail(msg) return nil, "line " .. lineno(src, p) .. ": " .. msg end
    while p <= n do
      local lt = src:find("<", p, true)
      local txt = U.trim(src:sub(p, (lt or n + 1) - 1))
      if #txt > 0 then
        if cur == root then return fail("text outside of elements") end
        cur.t = cur.t .. unesc(txt)
      end
      if not lt then break end
      p = lt
      local skip = src:sub(p, p + 3) == "<!--" and "-->" or src:sub(p, p + 1) == "<?" and "?>"
      if skip then
        local e = src:find(skip, p + 2, true)
        if not e then return fail("unclosed comment") end
        p = e + #skip
      elseif src:sub(p, p + 1) == "</" then
        local e = src:find(">", p, true)
        if not e then return fail("'>' expected") end
        local tag = U.trim(src:sub(p + 2, e - 1))
        if #stack < 2 or cur.tag ~= tag then return fail("unexpected </" .. tag .. ">") end
        stack[#stack] = nil
        cur = stack[#stack]
        p = e + 1
      else
        local i, s = p + 1, p + 1
        while i <= n do local c = src:byte(i) if c <= 32 or c == 47 or c == 62 then break end i = i + 1 end
        local tag = src:sub(s, i - 1)
        if #tag == 0 then return fail("tag name expected") end
        local node = setmetatable({tag = tag, a = {}, k = {}, t = ""}, Node)
        while true do
          i = sp(src, i)
          local c = src:sub(i, i)
          if c == "" then return fail("tag <" .. tag .. "> is not closed") end
          if c == ">" or c == "/" then break end
          local s2 = i
          while i <= n do local b = src:byte(i) if b <= 32 or b == 61 or b == 62 or b == 47 then break end i = i + 1 end
          local name = src:sub(s2, i - 1)
          i = sp(src, i)
          if src:sub(i, i) ~= "=" then return fail("'=' expected after " .. name) end
          i = sp(src, i + 1)
          local q = src:sub(i, i)
          if q ~= '"' and q ~= "'" then return fail("quoted value expected") end
          local e = src:find(q, i + 1, true)
          if not e then return fail("unclosed attribute value") end
          node.a[name] = unesc(src:sub(i + 1, e - 1))
          i = e + 1
        end
        cur.k[#cur.k + 1] = node
        if src:sub(i, i) == "/" then
          if src:sub(i + 1, i + 1) ~= ">" then return fail("'>' expected") end
          p = i + 2
        else
          stack[#stack + 1] = node
          cur = node
          p = i + 1
        end
      end
    end
    if #stack > 1 then return fail("<" .. cur.tag .. "> is not closed") end
    if #root.k ~= 1 then return fail("one root element expected") end
    return root.k[1]
  end
end
