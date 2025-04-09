require "RadLine"

cmds = {}
setmetatable(cmds, {
    __index = LookUpExt
})

cmd_output = {}
setmetatable(cmd_output, {
    __index = LookUpExt
})

options = {}
setmetatable(options, {
    __index = LookUpExt
})

function FindPotentialEnv(params, p)
    local s,i = GetParam(params, p)
    if p == 2 then
        return FindEnv(s, false), i
    else
        return FindPotentialDefault(params, p)
    end
end

function FindPotentialAlias(params, p)
    local s,i = GetParam(params, p)
    if p == 2 then
        return FindAlias(s), i
    else
        -- TODO This should really start the command again and recall FindPotentital
        return FindFiles(s.."*", FindFilesE.All), i
    end
end

function FindPotentialWhere(params, p)
    local s,i = GetParam(params, p)
    if s:sub(1, 1) == "/" then
        local command = params[1]
        local f = {}
        table.concat_if(f, s:lower(), options[command])
        return f
    elseif p > 1 and params[p - 1] == "/R" then
        return FindFiles(s.."*", FindFilesE.DirOnly), i
    else
        return FindPathExeFiles(s), i
    end
end

command_fn["where.exe"] = FindPotentialWhere
options["where.exe"] = { "/r", "/q", "/f", "/t" }

function FindPotentialExe(params, p)
    local s,i = GetParam(params, p)
    if s:sub(1, 1) == "/" then
        local command = params[1]
        local f = {}
        table.concat_if(f, s:lower(), options[command])
        return f
    elseif p == 2 then
        local f = {}
        table.concat_if(f, s:lower(), internal)
        table.concat(f, FindFiles(s.."*", FindFilesE.DirOnly))
        table.concat(f, FindExeFiles(s))
        table.concat(f, FindPathExeFiles(s))
        return f, i
    else
        return FindFiles(s.."*", FindFilesE.All), i
    end
end

function FindPotentialRegsvr32(params, p)
    local s,i = GetParam(params, p)
    if s:sub(1, 1) == "/" then
        local command = params[1]
        local f = {}
        table.concat_if(f, s:lower(), options[command])
        DebugOutLn("f "..#f)
        return f
    else
        return FindFiles(s.."*.dll", FindFilesE.FileOnly), i
    end
end

command_fn["regsvr32.exe"] = FindPotentialRegsvr32
options["regsvr32.exe"] = { "/u", "/s", "/i", "/n" }

function FindPotentialCmdOutput(params, p)
    local s,i = GetParam(params, p)
    if p == 2 then
        local command = params[1]
        local f = {}
        table.concat_if(f, s:lower(), GetCmdOutput(cmd_output[command]))
        return f
    else
        return FindFiles(s.."*", FindFilesE.All), i
    end
end

function FindPotentialCmds(params, p)
    local s,i = GetParam(params, p)
    if p == 2 then
        local command = params[1]
        local f = {}
        table.concat_if(f, s:lower(), cmds[command])
        return f
    else
        return {}
    end
end

function FindPotentialCmdsOrFile(params, p)
    local s,i = GetParam(params, p)
    if p == 2 then -- TODO skip over options
        local command = params[1]
        local f = {}
        table.concat_if(f, s:lower(), cmds[command])
        return f
    else
        return FindFiles(s.."*", FindFilesE.All), i
    end
end

function FindPotentialCmdsOrDir(params, p)
    local s,i = GetParam(params, p)
    if p == 2 then -- TODO skip over options
        local command = params[1]
        local f = {}
        table.concat_if(f, s:lower(), cmds[command])
        return f
    else
        return FindFiles(s.."*", FindFilesE.DirOnly), i
    end
end

command_fn["set"] = FindPotentialEnv

command_fn["start"] = FindPotentialExe

command_fn["ping.exe"] = FindPotentialCmds
cmds["ping.exe"] = { "www.google.com" }

command_fn["scoop.cmd"] = FindPotentialCmdsOrFile
cmds["scoop.cmd"] = {
    "alias", "bucket", "cache", "checkup", "cleanup", "config", "create", "depends", "export", "help",
    "hold", "home", "info", "install", "list", "prefix", "reset", "search", "status", "unhold",
    "uninstall", "update", "virustotal", "which"
}

command_fn["winget.exe"] = FindPotentialCmdsOrFile
cmds["winget.exe"] = {
    "install", "show", "source", "search", "list",
    "upgrade", "uninstall", "hash", "validate", "settings",
    "features", "export", "import"
}

command_fn["git.exe"] = FindPotentialCmdsOrFile
cmds["git.exe"] = {
    "clone", "init", "add", "mv", "reset", "rm", "bisect", "grep", "log", "show", "status",
    "branch", "checkout", "commit", "diff", "merge", "rebase", "tag", "fetch", "pull", "push", "help"
};

function FindPotentialReg(params, p)
    local s,i = GetParam(params, p)
    if p == 2 and i == 1 then
        local command = params[1]
        local f = {}
        table.concat_if(f, s:lower(), cmds[command])
        return f, i
    elseif p == 3 then
        return FindRegKey(s), i
    else
        return FindPotentialDefault(params, p)
    end
end

command_fn["reg.exe"] = FindPotentialReg
cmds["reg.exe"] = {
    "query", "add", "delete", "copy", "save", "load",
    "unload", "restore", "compare", "export", "import", "flags"
}

want "UserRadLine2"
