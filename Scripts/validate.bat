@echo off
REM Simple batch file to run struct initialization validation
REM This is the easiest way to run validation on Windows

echo Running UPROPERTY FGuid validation...
powershell -ExecutionPolicy Bypass -File "%~dp0run_struct_validation.ps1" %*

if %ERRORLEVEL% equ 0 (
    echo.
    echo Validation completed successfully!
) else (
    echo.
    echo Validation failed!
    exit /b %ERRORLEVEL%
)