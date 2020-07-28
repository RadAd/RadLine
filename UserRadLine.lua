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

command_fn["reg"] = FindPotentialReg
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

command_fn["alias"] = FindPotentialAlias
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

command_fn["where"] = FindPotentialWhere
command_fn["where.exe"] = FindPotentialWhere

git_cmds = {
    "clone", "init", "add", "mv", "reset", "rm", "bisect", "grep", "log", "show", "status",
    "branch", "checkout", "commit", "diff", "merge", "rebase", "tag", "fetch", "pull", "push", "help"
};

function FindPotentialGit(params, p)
    local s,i = GetParam(params, p)
    if p == 2 then -- TODO skip over options
        local f = {}
        concat_if(f, s:lower(), git_cmds)
        return f, i
    else
        return FindFiles(s.."*", FindFilesE.All), i
    end
end

command_fn["git"] = FindPotentialGit
command_fn["git.exe"] = FindPotentialGit
