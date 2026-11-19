--lua
-- cwm-xmlf - load XML files with <include href="..."/> (module of cravgwm)
return function(W)
  local X = W.xml
  local function expand(n, depth)
    local i = 1
    while i <= #n.k do
      local c = n.k[i]
      if c.tag == "include" then
        if depth >= 4 then return nil, "includes are nested too deeply" end
        local inc, err = X.load(c.a.href or "", depth + 1)
        if not inc then return nil, err end
        table.remove(n.k, i)
        for j, kid in ipairs(inc.k) do table.insert(n.k, i + j - 1, kid) end
        i = i + #inc.k
      else
        local ok, err = expand(c, depth)
        if not ok then return nil, err end
        i = i + 1
      end
    end
    return n
  end
  -- file -> root node, or nil and an error message
  function X.load(path, depth)
    local src = fs.read(path)
    if not src then return nil, "cannot read " .. path end
    local root, err = X.parse(src)
    if not root then return nil, path .. ": " .. err end
    return expand(root, depth or 0)
  end
end
