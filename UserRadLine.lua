require "RadLine"

cmds = {}
setmetatable(cmds, {
    __index = LookUpExt
})

cmd_output = {}
setmetatable(cmd_output, {
    __index = LookUpExt
})

function FindPotentialAlias(params, p)
    local s,i = GetParam(params, p)
    if p == 2 then
        return FindAlias(s), i
    else
        -- TODO This should really start the command again and recall FindPotentital
        return FindFiles(s.."*", FindFilesE.All), i
    end
end

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

function FindPotentialEnv(params, p)
    local s,i = GetParam(params, p)
    if p == 2 then
        return FindEnv(s, false), i
    else
        return FindPotentialDefault(params, p)
    end
end

command_fn["set"] = FindPotentialEnv

function FindPotentialExe(params, p)
    local s,i = GetParam(params, p)
    if p == 2 then
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

command_fn["start"] = FindPotentialExe

function FindPotentialWhere(params, p)
    local s,i = GetParam(params, p)
    if p > 1 and params[p - 1] == "/R" then
        return FindFiles(s.."*", FindFilesE.DirOnly), i
    else
        return FindPathExeFiles(s), i
    end
end

command_fn["where.exe"] = FindPotentialWhere
command_options["where.exe"] = { "/r", "/q", "/f", "/t" }

command_fn["regsvr32.exe"] = FindPotentialDlls
command_options["regsvr32.exe"] = { "/u", "/s", "/i", "/n" }

command_command["ping.exe"] = FindPotentialDefault
command_command["ping.exe"] = { "www.google.com" }

function FindPotentialReg(params, p)
    local s,i = GetParam(params, p)
    if p == 3 then
        return FindRegKey(s), i
    else
        return FindPotentialDefault(params, p)
    end
end

command_fn["reg.exe"] = FindPotentialReg
command_command["reg.exe"] = {
    "query", "add", "delete", "copy", "save", "load",
    "unload", "restore", "compare", "export", "import", "flags"
}

command_fn["winget.exe"] = FindPotentialDefault
command_command["winget.exe"] = {
    "install", "show", "source", "search", "list",
    "upgrade", "uninstall", "hash", "validate", "settings",
    "features", "export", "import"
}
command_options["winget.exe"] = {
    "-e", "--exact",
    "--verbose", "--verbose-logs"
}

command_fn["git.exe"] = FindPotentialDefault
command_command["git.exe"] = {
    "clone", "init", "add", "mv", "reset", "rm", "bisect", "grep", "log", "show", "status",
    "branch", "checkout", "commit", "diff", "merge", "rebase", "tag", "fetch", "pull", "push", "help"
};

command_fn["scoop.cmd"] = FindPotentialDefault
command_command["scoop.cmd"] = {
    "alias", "bucket", "cache", "checkup", "cleanup", "config", "create", "depends", "export", "help",
    "hold", "home", "info", "install", "list", "prefix", "reset", "search", "status", "unhold",
    "uninstall", "update", "virustotal", "which"
}

want "UserRadLine2"
