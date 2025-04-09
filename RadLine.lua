-- TODO
-- Need a find reg values also

local win32 = require "lrwin32"

-- function FindAlias(s)
-- function FindRegKey(s)

function DebugOutLn(s)
    win32.OutputDebugString(s .. "\n")
end

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
            if beginswith(vv:lower(), s) then
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
    -- TODO better? s:sub(1, #t) == t
    return s:find("^"..escape(t)) ~= nil
end

function table.contains(table, element)
  for _, value in pairs(table) do
    if value == element then
      return true
    end
  end
  return false
end

function CaseInsensitiveLess(s1, s2)
    return s1:lower() < s2:lower()
end

DebugOutLn("RadLine ".._VERSION)

internal = {
    "assoc", "call", "cd", "chdir", "cls", "color", "copy", "date", "del", "dir", "echo", "endlocal", "erase", "exit", "for",
    "ftype", "goto", "if", "md", "mkdir", "mklink", "move", "path", "popd", "prompt", "pushd", "rem", "ren", "rd", "rmdir",
    "set", "setlocal", "shift", "start", "time", "title", "type", "ver", "verify", "vol"
}

function FileNameIsDir(s)
    return s:sub(-1) == "\\";
end

function FileNameLess(s1, s2)
    if FileNameIsDir(s1) and not FileNameIsDir(s2) then
        return true;
    elseif not FileNameIsDir(s1) and FileNameIsDir(s2) then
        return false;
    else
        return CaseInsensitiveLess(s1, s2)
    end
end

FindFilesE = { All = 0, DirOnly = 1, FileOnly = 2 }
function FindFiles(s, e)
    assert(table.contains(FindFilesE, e), "e=" .. tostring(e) .. " is not in FindFilesE")
    s = s:gsub('"', '')

    if beginswith(s, "~\\") and tonumber(win32.GetEnvironmentVariable("RADLINE_TILDE") or 0) ~= 0 then
        s = "%USERPROFILE%" .. s:sub(2, -1)
    end

    win32.SetEnvironmentVariable("CD", win32.GetCurrentDirectory())
    s = win32.ExpandEnvironmentStrings(s)
    win32.SetEnvironmentVariable("CD", nil)

    local files = {}
    local FindFileData = {}
    local hFind = win32.FindFirstFile(s, FindFileData)
    if hFind ~= win32.INVALID_HANDLE_VALUE then
        repeat
            if FindFileData.cFileName ~= "." and FindFileData.cFileName ~= ".." then
            --if FindFileData.cFileName ~= "." and FindFileData.cFileName ~= ".." and (FindFileData.dwFileAttributes & win32.FILE_ATTRIBUTE.HIDDEN) == 0 then
                local add = true;
                if     e == FindFilesE.FileOnly and (FindFileData.dwFileAttributes & win32.FILE_ATTRIBUTE.DIRECTORY) ~= 0 then add = false;
                elseif e == FindFilesE.DirOnly  and (FindFileData.dwFileAttributes & win32.FILE_ATTRIBUTE.DIRECTORY) == 0 then add = false;
                end
                if add then
                    if (FindFileData.dwFileAttributes & win32.FILE_ATTRIBUTE.DIRECTORY) ~= 0 then
                        FindFileData.cFileName = FindFileData.cFileName.."\\"
                    end
                    files[#files+1] = FindFileData.cFileName
                end
            end
        until not win32.FindNextFile(hFind, FindFileData)
        win32.FindClose(hFind);
    end

    table.sort(files, FileNameLess)

    return files;
end

function FindExeFiles(s)
    local pathext = split(win32.GetEnvironmentVariable("PATHEXT"), ";")
    local dot = s:match'^.*()%.' -- Find last of '.'
    local slash = s:match'^.*()\\' or 0 -- Find last of '\'
    local ext = (dot and dot > slash) and s:sub(dot):upper() or nil

    local f = {}
    if not ext then
        for _,i in ipairs(pathext) do
            concat(f, FindFiles(s.."*"..i, FindFilesE.FileOnly))
        end
    else
        for _,i in ipairs(pathext) do
            local dot = i:match'^.*()%.' -- Find last of '.'
            local iext = (dot and dot > 1) and i:sub(dot) or nil
            if beginswith(i:upper(), ext) then
                concat(f, FindFiles(s..i:sub(#ext + 1), FindFilesE.FileOnly))
            end
            if iext and beginswith(iext:upper(), ext) then
                concat(f, FindFiles(s..i:sub(dot + #ext), FindFilesE.FileOnly))
            end
            --concat(f, FindFiles(s.."*"..i, FindFilesE.FileOnly))
        end
    end
    return f
end

function FindPathExeFiles(s)
    local path = split(win32.GetEnvironmentVariable("PATH"), ";")
    local f = {}
    for _,i in ipairs(path) do
        concat(f, FindExeFiles(i.."\\"..s))
    end
    return f
end

function FindEnv(s, enclose)
    s = s:upper()
    local e = {}

    local env = win32.GetEnvironmentStrings();
    for k,v in pairs(env) do
        if beginswith(k:upper(), s) then
            if enclose then
                e[#e+1] = "%"..k.."%"
            else
                e[#e+1] = k
            end
        end
    end

    return e;
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
    local i = s:match'^.*()[\\/]' -- Find last of '\' or '/'
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

function LookUpExt(t, key)
    local pathext = split(win32.GetEnvironmentVariable("PATHEXT"), ";")
    for _,i in ipairs(pathext) do
        local f = rawget(t, (key..i):lower())
        if f then
            return f
        end
    end
    return rawget(t, "_default")
end

setmetatable(command_fn, {
    __index = LookUpExt
})

other_env = {
    "%CD%", "%CMDCMDLINE%", "%CMDEXTVERSION%", "%DATE%", "%ERRORLEVEL%", "%RANDOM%", "%TIME%",
    "%FIRMWARE_TYPE%", "%HighestNumaNodeNumber%",
    "%__APPDIR__%", "%__CD__%", "%__PID__%", "%__TICK__%", "%RAWPROMPT%", "%RADLINE_LOADED%", "%RADLINE_DIR%",
    "%=ExitCode%", "%=ExitCodeAscii%", "%=C:%", "%=D:%", "%=E:%", "%=F:%"
}

function GetCmdOutput(cmd)
    local d = {}
    local f1 = io.popen(cmd, 'r')
    for line in f1:lines() do
        d[#d+1] = line
    end
    f1:close()
    return d
end

function FindPotential(params, p)
    local s,i = GetParam(params, p)
    local e = FindEnvBegin(s)

    if e then
        local f = {}
        concat(f, FindEnv(s:sub(e + 1), true))
        concat_if(f, s:sub(e):lower(), other_env)
        return f, (e + i - 1)
    elseif p == 1 or command_sep[params[p - 1]] then
        local f = {}
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
