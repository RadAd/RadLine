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
call :add Bin\%PLATFORM%Release\RadLine.dll
call :add Bin\%PLATFORM%Release\RLoadDll.exe
call :add Lua\Bin%PLATFORM%\Lua.dll
call :add RadLine.lua
call :add UserRadLine.lua
goto :eof

:add
echo *** %~dp1 %~nx1
setlocal
cd /d %~dp1
7z a %ZIPFILE% %~nx1
goto :eof
