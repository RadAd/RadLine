-- TODO
-- Should FindFiles expand environment variables are should I do that explicitly before
-- Need a find reg values also

-- function DebugOut(s)
-- function GetEnv(s) -- because os.getenv doesn't reflect updates values
FindFilesE = { All = 0, DirOnly = 1, FileOnly = 2 }
-- function FindFiles(s, FindFilesE)
-- function FindEnv(s)
-- function FindRegKey(s)

function DebugOutLn(s)
    DebugOut(s .. "\n")
end

internal = {
    "assoc", "call", "cd", "chdir", "cls", "color", "copy", "date", "del", "dir", "echo", "endlocal", "erase", "exit", "for",
    "ftype", "goto", "if", "md", "mkdir", "mklink", "move", "path", "popd", "prompt", "pushd", "rem", "ren", "rd", "rmdir",
    "set", "setlocal", "shift", "start", "time", "title", "type", "ver", "verify", "vol"
}

function split(s, sep)
    local fields = {}

    local sep = sep or " "
    local pattern = string.format("([^%s]+)", sep)
    s:gsub(pattern, function(c) fields[#fields + 1] = c end)

    return fields
end

function concat(t, ...)
    for i,v in ipairs({...}) do
        for ii,vv in ipairs(v) do
            t[#t+1] = vv
        end
    end
    return t
end

-- TODO can I make beginswith a parameter?
function concat_if(t, s, ...)
    for i,v in ipairs({...}) do
        for ii,vv in ipairs(v) do
            if beginswith(vv, s) then
                t[#t+1] = vv
            end
        end
    end
    return t
end

local escape
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

  escape = function(s)
    return s:gsub(".", matches)
  end
end

function beginswith(s, t)
    return s:find("^"..escape(t)) ~= nil
end

DebugOut("RadLine ".._VERSION.."\n")

function FindExeFiles(s)
    local pathext = split(GetEnv("PATHEXT"), ";")
    local dot = s:match'^.*()%.' -- Find last of '.'
    local slash = s:match'^.*()\\' or 0 -- Find last of '\'
    local ext = (dot and dot > slash) and s:sub(dot):upper() or nil
    
    local f = {}
    for _,i in ipairs(pathext) do
        local dot = i:match'^.*()%.' -- Find last of '.'
        local iext = dot and i:sub(dot) or i
        if ext and beginswith(iext:upper(), ext) then
            concat(f, FindFiles(s.."*", FindFilesE.FileOnly))
        else
            concat(f, FindFiles(s.."*"..i, FindFilesE.FileOnly))
        end
    end
    return f
end

function FindPathExeFiles(s)
    local path = split(GetEnv("PATH"), ";")
    local f = {}
    for _,i in ipairs(path) do
        concat(f, FindExeFiles(i.."\\"..s))
    end
    return f
end

function FindEnvBegin(s)
    local count = 0
    for c in s:gmatch("%%") do
        count = count + 1
    end
    if count%2 == 1 then
        return s:match'^.*()%%' -- Find last of '%'
    else
        return nil
    end
end

command_sep = {
    ["&"] = true;
    ["|"] = true;
    ["&&"] = true;
    ["||"] = true;
}

redirect_sep = {
    [">"] = true;
    [">>"] = true;
    ["<"] = true;
    ["2>"] = true;
}

function GetParam(params, p)
    local s = params[p] or ""
    local i = s:match'^.*()\\' -- Find last of '\'
    i = i and i + 1 or 1
    if #s > 0 and s:sub(1, 1) == '"' then
        s = s:sub(2)
        if i == 1 then
            i = i + 1
        end
    end
    if #s > 0 and s:sub(#s, #s) == '"' then
        s = s:sub(1, -2)
    end
    return s, i
end

function FindPotentialDefault(params, p)
    local s,i = GetParam(params, p)
    return FindFiles(s.."*", FindFilesE.All), i
end

function FindPotentialDirs(params, p)
    local s,i = GetParam(params, p)
    return FindFiles(s.."*", FindFilesE.DirOnly), i
end

command_fn = {
    _default = FindPotentialDefault,
    cd = FindPotentialDirs,
    chdir = FindPotentialDirs,
    md = FindPotentialDirs,
    mkdir = FindPotentialDirs,
    pushd = FindPotentialDirs,
}

setmetatable(command_fn, {
    __index = function (t)
        return t._default
    end
})

function FindPotential(params, p)
    local s,i = GetParam(params, p)
    local e = FindEnvBegin(s)

    if e then
        return FindEnv(s:sub(e + 1)), e
    elseif p == 1 or command_sep[params[p - 1]] then
        local f ={}
        if i == 1 then
            concat_if(f, s:lower(), internal)
            concat(f, FindFiles(s.."*", FindFilesE.DirOnly))
            concat(f, FindExeFiles(s))
            concat(f, FindPathExeFiles(s))
            concat(f, FindAlias(s))
        else
            concat(f, FindFiles(s.."*", FindFilesE.DirOnly))
            concat(f, FindExeFiles(s))
        end
        return f, i
    elseif p > 1 and redirect_sep[params[p - 1]] then
        return FindFiles(s.."*", FindFilesE.All), i
    else
        local command = params[1]
        -- TODO Unquote and remove path ???
        local fn = command_fn[command]
        return fn(params, p), i
    end
end
