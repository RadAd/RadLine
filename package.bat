@echo off
setlocal
call :package %1
rem call :package x64
rem call :package Win32
rem pause
goto :eof

:package
set PLATFORM=%1
set ZIPFILE=%CD%\RadLine%PLATFORM%.zip
if exist %ZIPFILE% del %ZIPFILE%
7z a %ZIPFILE% Bin\%PLATFORM%Release\RadLine.dll Bin\%PLATFORM%Release\RLoadDll.exe Lua\Bin%PLATFORM%\Lua.dll Lua\Bin%PLATFORM%\lrwin32.dll RadLine.lua UserRadLine.lua
7z rn %ZIPFILE% Bin\%PLATFORM%Release\RadLine.dll RadLine.dll Bin\%PLATFORM%Release\RLoadDll.exe RLoadDll.exe Lua\Bin%PLATFORM%\Lua.dll Lua.dll Lua\Bin%PLATFORM%\lrwin32.dll lrwin32.dll
goto :eof
