@echo off
rem wasdmod for Windows, without the app: starts wasdmod.ps1, the script beside this
rem file, with the PowerShell every Windows has. Both are plain text: open them in
rem Notepad to read what they do. Nothing is installed outside the game folder.
rem   wasdmod-windows.cmd            the key layout editor, with the mod's buttons
rem   wasdmod-windows.cmd install    (or status, uninstall, off, on, find; a game folder may follow)
setlocal
title wasdmod
if not exist "%~dp0wasdmod.ps1" (
    echo wasdmod.ps1 isn't beside this file. Unzip the whole wasdmod folder first, then start wasdmod-windows.cmd from it.
    pause
    exit /b 1
)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0wasdmod.ps1" %*
set code=%errorlevel%
rem Started by a double click, something went wrong: the window stays, to read why.
if "%~1"=="" if not "%code%"=="0" pause
exit /b %code%
