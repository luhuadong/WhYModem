@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%deploy-windows.ps1" %*

if errorlevel 1 (
    echo.
    echo Deployment failed.
    exit /b 1
)

endlocal
