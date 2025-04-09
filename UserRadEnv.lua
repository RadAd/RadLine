local win32 = require "lrwin32"

count = 0

function GetEnvironmentVariable(name)
	local nameu = name:upper()
	if nameu == "foo" then
		return "bar"
	elseif nameu == "count" then
		count = count + 1
		return count
	elseif nameu == "__PID__" then
		return win32.GetProcessId(win32.GetCurrentProcess())
	elseif nameu == "__TICK__" then
		return win32.GetTickCount()
	else
		return OrigGetEnvironmentVariable(name)
	end
end
