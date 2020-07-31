@echo off
setlocal
call :package x64
call :package Win32
pause
goto :eof

:package
set PLATFORM=%1
set ZIPFILE=%CD%\RadLine%PLATFORM%.zip
del %ZIPFILE%
7z a %ZIPFILE% Bin\%PLATFORM%Release\RadLine.dll Bin\%PLATFORM%Release\RLoadDll.exe Lua\Bin%PLATFORM%\Lua.dll RadLine.lua UserRadLine.lua
7z rn %ZIPFILE% Bin\%PLATFORM%Release\RadLine.dll RadLine.dll Bin\%PLATFORM%Release\RLoadDll.exe RLoadDll.exe Lua\Bin%PLATFORM%\Lua.dll Lua.dll
goto :eof
