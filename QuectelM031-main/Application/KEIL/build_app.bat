@echo off
setlocal

set UV_PATH=C:\Keil_v5\UV4\UV4.exe
set PROJECT_PATH=%~dp0Application.uvproj
set LOG_PATH=%~dp0build.log

:: Run UV4.exe synchronously (no start)
"%UV_PATH%" -b "%PROJECT_PATH%" -o "%LOG_PATH%"

:: Wait for a second to ensure log is flushed
timeout /t 1 >nul

:: Print the build log to VS Code terminal
type "%LOG_PATH%"

endlocal
