@echo off
REM SAMPR desktop launcher — run with:  .\launch.bat   (note the .\ prefix in PowerShell)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0launch.ps1" %*
if errorlevel 1 (
    echo.
    echo Launch failed. Press any key to close.
    pause >nul
    exit /b 1
)