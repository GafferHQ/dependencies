@echo off
setlocal enabledelayedexpansion
if "%IBJOM_NUMBEROFJOBS%" == "" set IBJOM_NUMBEROFJOBS=20
BuildConsole /command="jom -j%IBJOM_NUMBEROFJOBS% %*" || exit /b !ERRORLEVEL!
endlocal
