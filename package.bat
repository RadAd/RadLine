@echo off
setlocal
if "%~1" == "" echo Specify type>&2 && exit /b 1
call :package %1
rem call :package x64
rem call :package Win32
rem pause
goto :eof

:package
set PLATFORM=%1
set ZIPFILE=%CD%\RadLine%PLATFORM%.zip
if exist %ZIPFILE% del %ZIPFILE%
call 7z a %ZIPFILE% Bin\%PLATFORM%Release\RadLine.dll Bin\%PLATFORM%Release\RLoadDll.exe Lua\Bin%PLATFORM%\Lua54.dll Lua\Bin%PLATFORM%\lrwin32.dll RadLine.lua UserRadLine.lua
call 7z rn %ZIPFILE% Bin\%PLATFORM%Release\RadLine.dll RadLine.dll Bin\%PLATFORM%Release\RLoadDll.exe RLoadDll.exe Lua\Bin%PLATFORM%\Lua54.dll Lua54.dll Lua\Bin%PLATFORM%\lrwin32.dll lrwin32.dll
goto :eof
