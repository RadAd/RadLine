RadLine
=======

Improved command line completion for cmd.exe

![Windows](https://img.shields.io/badge/platform-Windows-blue.svg)
[![Releases](https://img.shields.io/github/release/RadAd/RadLine.svg)](https://github.com/RadAd/RadLine/releases/latest)
[![commits-since](https://img.shields.io/github/commits-since/RadAd/RadLine/latest.svg)](https://github.com/RadAd/RadLine/commits/master)
[![Build](https://img.shields.io/appveyor/ci/RadAd/RadLine.svg)](https://ci.appveyor.com/project/RadAd/RadLine)

Dependencies
-------

[![Minhook](https://img.shields.io/github/commits-since/m417z/minhook/fdcbfb2?label=Minhook%20commits%20since%20fdcbfb2)](https://github.com/m417z/minhook/commits/multihook/)


Details
-------

`RLoadDll.exe` injects the `RadLine.dll` into `cmd.exe` process.

On startup the `RadLine.dll` replaces the `ReadConsole` function with its own. This allows it to intercept the command line completion behaviour.

Command line completion
-----------------------

This was the main reason for creating this utility. Completion is looked up using the lua script [RadLine.lua](RadLine.lua).
The function `FindPotential` is called with the current command line and returns an array of potential matches.
The matches are then listed under the current line.

Auto terminate batch file
-------------------------

To enable:

```
set RADLINE_AUTO_TERMINATE_BATCH=1
```

This setting will auto-respond `y` to the `Terminate batch job (Y/N)?` prompt.

Expand Tilde
------------

The `~` at the start of an argument is expanded to `%USERPROFILE%`.

Pre command execution
----------------------

The contents of the environment variable `RADLINE_PRE` are prepended to the current command.

For example:
```
set RADLINE_PRE=echo Start
```

Post command execution
----------------------

The contents of the environment variable `RADLINE_POST` are appended to the current command.

For example:
```
set RADLINE_POST=echo Error: ^%ERRORLEVEL^%
```

Dynamic environment variables
-----------------------------

`__PID__` will show the process id of the cmd process.

`PROMPT` environment variables in the prompt variable will also be expanded.

`set foo=%(cmd /c echo bar)%` the command in the brackets will expand to the output of the command.

Install
-------

To install, download the [zip](https://github.com/RadAd/RadLine/releases/latest) and decompress to `%APPDATA%\Radsoft\RadLine`

Add the following lines to your cmd startup script:
```
set RADLINEDIR=%APPDATA%\Radsoft\RadLine
"%RADLINEDIR%\RLoadDll.exe" /d - "%RADLINEDIR%\RadLine.dll"
```
