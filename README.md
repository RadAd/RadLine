RadLine
=======

Improved command line completion for cmd.exe

![Windows](https://img.shields.io/badge/platform-Windows-blue.svg)
[![Releases](https://img.shields.io/github/release/RadAd/RadLine.svg)](https://github.com/RadAd/RadLine/releases/latest)
[![Build](https://img.shields.io/appveyor/ci/RadAd/RadLine.svg)](https://ci.appveyor.com/project/RadAd/RadLine)

Details
=======

`RLoadDll.exe` injects the `RadLine.dll` into `cmd.exe` process.

On startup the `RadLine.dll` replaces the `ReadConsole` function with its own. This allows it to intercept the command line completion behaviour.

Install
=======

To install, download the zip and decompress to `%APPDATA%\Radsoft\RadLine`

Add the following lines to your cmd startup script:
```
set RADLINEDIR=%APPDATA%\Radsoft\RadLine
"%RADLINEDIR%\RLoadDll.exe" /d - "%RADLINEDIR%\RadLine.dll"
```
