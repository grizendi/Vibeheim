@echo off
REM World Generation Integration Test Runner for Windows
REM This script runs comprehensive integration tests for the world generation system

echo ========================================
echo World Generation Integration Test Suite
echo ========================================

REM Set project path
set PROJECT_PATH=%~dp0..\Vibeheim.uproject

REM Check if project file exists
if not exist "%PROJECT_PATH%" (
    echo ERROR: Project file not found at %PROJECT_PATH%
    echo Please run this script from the Scripts directory
    pause
    exit /b 1
)

echo Project: %PROJECT_PATH%
echo.

REM Run integration tests using Unreal Engine automation framework
echo Running integration tests...
echo.

REM Run all WorldGen integration tests
UnrealEditor.exe "%PROJECT_PATH%" -ExecCmds="Automation RunTests WorldGen.Integration; quit" -unattended -nullrhi -nosplash -log

REM Check if the command succeeded
if %ERRORLEVEL% neq 0 (
    echo.
    echo ERROR: Integration tests failed with error code %ERRORLEVEL%
    echo Check the log files for detailed error information
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo ========================================
echo Integration tests completed successfully
echo ========================================
echo.
echo Check the automation test results in the Unreal Editor logs
echo or run individual tests using console commands:
echo   wg.RunIntegrationTests
echo   wg.TestBiomeTransitions
echo   wg.TestPOIPortalSystem
echo   wg.ValidateVisualQuality
echo.

pause