local win32 = require "lrwin32"
require "Utils"

function poutput(cmd)
    local openPop = io.popen(cmd, 'r')
    if openPop == nil then
        print("Error popen: "..cmd)
        return nil
    end
    local output = openPop:read('*all')
    openPop:close()
    return output
end

env = {}

function env.__PID__()
    return win32.GetProcessId(win32.GetCurrentProcess())
end

env.__TICK__ = win32.GetTickCount

function env.__USERCD__()
    return win32.GetCurrentDirectory():gsub("^"..OrigGetEnvironmentVariable("USERPROFILE"):escape(), "~")
end

function GetEnvironmentVariable(name)
    if name:beginswith('[') and name:endswith(']') then
        local o = poutput(name:sub(2, -2))
        o = o:gsub('[\r\n]$', '')
        o = o:gsub('[\r\n]', ';')
        print(o)
        return o
    else
        local nameu = name:upper()
        local e = env[nameu]
        if e == nil then
            return OrigGetEnvironmentVariable(name)
        elseif type(e) == "function" then
            return e()
        else
            return e
        end
    end
end
