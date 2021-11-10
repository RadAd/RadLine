function FindPotentialEnv(params, p)
    local s,i = GetParam(params, p)
    if p == 2 then
        return FindEnv(s, false), i
    else
        return FindPotentialDefault(params, p)
    end
end

command_fn["set"] = FindPotentialEnv
command_fn["p"] = FindPotentialEnv

reg_cmds = {
    "query", "add", "delete", "copy", "save", "load",
    "unload", "restore", "compare", "export", "import", "flags"
}

function FindPotentialReg(params, p)
    local s,i = GetParam(params, p)
    if p == 2 and i == 1 then
        local f = {}
        concat_if(f, s:lower(), reg_cmds)
        return f, i
    elseif p == 3 then
        return FindRegKey(s), i
    else
        return FindPotentialDefault(params, p)
    end
end

command_fn["reg.exe"] = FindPotentialReg

function FindPotentialAlias(params, p)
    local s,i = GetParam(params, p)
    if p == 2 then
        return FindAlias(s), i
    else
        -- TODO This should really start the command again and recall FindPotentital
        return FindFiles(s.."*", FindFilesE.All), i
    end
end

command_fn["alias.bat"] = FindPotentialAlias

where_options = { "/R", "/Q", "/F", "/T" }

function FindPotentialWhere(params, p)
    local s,i = GetParam(params, p)
    if s:sub(1, 1) == "/" then
        local f = {}
        concat_if(f, s:upper(), where_options)
        return f, i
    elseif p > 1 and params[p - 1] == "/R" then
        return FindFiles(s.."*", FindFilesE.DirOnly), i
    else
        return FindPathExeFiles(s), i
    end
end

command_fn["where.exe"] = FindPotentialWhere

function FindPotentialExe(params, p)
    local s,i = GetParam(params, p)
    if s:sub(1, 1) == "/" then
        local f = {}
        -- concat_if(f, s:upper(), where_options)
        return f, i
    else
        local f = {}
        concat(f, FindFiles(s.."*", FindFilesE.DirOnly))
        concat(f, FindExeFiles(s))
        concat(f, FindPathExeFiles(s))
        return f, i
    end
end

command_fn["!"] = FindPotentialExe
command_fn["start"] = FindPotentialExe
command_fn["gsudo.exe"] = FindPotentialExe

regsvr32_options = { "/u", "/s", "/i", "/n" }

function FindPotentialRegsvr32(params, p)
    local s,i = GetParam(params, p)
    if s:sub(1, 1) == "/" then
        local f = {}
        concat_if(f, s:lower(), regsvr32_options)
        return f, i
    else
        return FindFiles(s.."*.dll", FindFilesE.FileOnly), i
    end
end

command_fn["regsvr32.exe"] = FindPotentialRegsvr32

cmd_output = {}

setmetatable(cmd_output, {
    __index = LookUpExt
})

function FindPotentialCmdOutput(params, p)
    local s,i = GetParam(params, p)
    if p == 2 then
        local command = params[1]
        local f = {}
        concat_if(f, s:lower(), GetCmdOutput(cmd_output[command]))
        return f
    else
        return FindFiles(s.."*", FindFilesE.All), i
    end
end

cmds = {}

setmetatable(cmds, {
    __index = LookUpExt
})

function FindPotentialCmds(params, p)
    local s,i = GetParam(params, p)
    if p == 2 then
        local command = params[1]
        local f = {}
        concat_if(f, s:lower(), cmds[command])
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
        concat_if(f, s:lower(), cmds[command])
        return f
    else
        return FindFiles(s.."*", FindFilesE.All), i
    end
end

command_fn["ping.exe"] = FindPotentialCmds
cmds["ping.exe"] = { "www.google.com", "radad.boxathome.net" }

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
