@echo off
REM Wrapper script to run PCG API validation on Windows
REM This script calls the PowerShell validation script

setlocal

set SCRIPT_DIR=%~dp0
set PS_SCRIPT=%SCRIPT_DIR%validate_pcg_apis.ps1

echo Running PCG API validation...
echo.

powershell -ExecutionPolicy Bypass -File "%PS_SCRIPT%" %*

exit /b %ERRORLEVEL%
