function string.split(s, sep)
    local fields = {}

    local sep = sep or " "
    local pattern = string.format("([^%s]+)", sep)
    s:gsub(pattern, function(c) fields[#fields + 1] = c end)

    return fields
end

do
  local matches =
  {
    ["^"] = "%^";
    ["$"] = "%$";
    ["("] = "%(";
    [")"] = "%)";
    ["%"] = "%%";
    ["."] = "%.";
    ["["] = "%[";
    ["]"] = "%]";
    ["*"] = "%*";
    ["+"] = "%+";
    ["-"] = "%-";
    ["?"] = "%?";
    ["\0"] = "%z";
  }

  string.escape = function(s)
    return s:gsub(".", matches)
  end
end

function string.beginswith(s, t)
    -- TODO better? s:sub(1, #t) == t
    return s:find("^"..t:escape()) ~= nil
end

function string.endswith(s, t)
    -- TODO better? s:sub(-#t) == t
    return s:find(t:escape().."$") ~= nil
end

function string.replace(str, this, that)
    -- return str:gsub(regexEscape(this), that:gsub("%%", "%%%%")) -- For some unknown reason this isn't working
    local t = that:gsub("%%", "%%%%") -- only % needs to be escaped for 'that'
    return str:gsub(this:escape(), t)
end

function table.contains(table, element)
  for _, value in pairs(table) do
    if value == element then
      return true
    end
  end
  return false
end

function table.concat(t, ...)
    for i,v in ipairs({...}) do
        for ii,vv in ipairs(v) do
            t[#t+1] = vv
        end
    end
    return t
end

-- TODO can I make beginswith a parameter?
function table.concat_if(t, s, ...)
    for i,v in ipairs({...}) do
        for ii,vv in ipairs(v) do
            if vv:lower():beginswith(s) then
                t[#t+1] = vv
            end
        end
    end
    return t
end
