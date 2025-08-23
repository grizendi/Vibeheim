@echo off
REM Batch wrapper for struct initialization validation
REM This provides a simple Windows interface to the Python validation script

setlocal enabledelayedexpansion

REM Default values
set "DIRECTORY="
set "RECURSIVE=--recursive"
set "SUPPRESSION_FILE="
set "OUTPUT="
set "FAIL_ON_INVALID="
set "SHOW_HELP="

REM Parse command line arguments
:parse_args
if "%~1"=="" goto :run_validation
if /i "%~1"=="--help" set "SHOW_HELP=1" & goto :show_help
if /i "%~1"=="-h" set "SHOW_HELP=1" & goto :show_help
if /i "%~1"=="--directory" set "DIRECTORY=%~2" & shift & shift & goto :parse_args
if /i "%~1"=="-d" set "DIRECTORY=%~2" & shift & shift & goto :parse_args
if /i "%~1"=="--suppression-file" set "SUPPRESSION_FILE=--suppression-file %~2" & shift & shift & goto :parse_args
if /i "%~1"=="-s" set "SUPPRESSION_FILE=--suppression-file %~2" & shift & shift & goto :parse_args
if /i "%~1"=="--output" set "OUTPUT=--output %~2" & shift & shift & goto :parse_args
if /i "%~1"=="-o" set "OUTPUT=--output %~2" & shift & shift & goto :parse_args
if /i "%~1"=="--fail-on-invalid" set "FAIL_ON_INVALID=--fail-on-invalid" & shift & goto :parse_args
if /i "%~1"=="--no-recursive" set "RECURSIVE=" & shift & goto :parse_args

REM If first argument doesn't start with -, treat it as directory
if not "%~1"=="" if not "%~1:~0,1%"=="-" set "DIRECTORY=%~1" & shift & goto :parse_args

shift
goto :parse_args

:show_help
echo UPROPERTY FGuid Initialization Validator
echo.
echo Usage: validate_struct_initialization.bat [directory] [options]
echo.
echo Arguments:
echo   directory                   Directory to scan for header files
echo.
echo Options:
echo   --help, -h                  Show this help message
echo   --directory, -d ^<path^>      Directory to scan (alternative to positional)
echo   --suppression-file, -s ^<path^> Path to suppression file (JSON format)
echo   --output, -o ^<path^>         Output file for the report (default: console)
echo   --fail-on-invalid           Exit with error code if invalid properties found
echo   --no-recursive              Don't scan subdirectories
echo.
echo Examples:
echo   validate_struct_initialization.bat Source
echo   validate_struct_initialization.bat --directory Source --output report.txt
echo   validate_struct_initialization.bat Source --suppression-file suppressions.json --fail-on-invalid
echo.
goto :eof

:run_validation
REM Check if directory is provided
if "%DIRECTORY%"=="" (
    echo Error: Directory argument is required
    echo Use --help for usage information
    exit /b 1
)

REM Get script directory
set "SCRIPT_DIR=%~dp0"
set "PYTHON_SCRIPT=%SCRIPT_DIR%validate_struct_initialization.py"

REM Check if Python script exists
if not exist "%PYTHON_SCRIPT%" (
    echo Error: Python validation script not found: %PYTHON_SCRIPT%
    exit /b 1
)

REM Check if Python is available
python --version >nul 2>&1
if errorlevel 1 (
    echo Error: Python not found. Please install Python 3.6+ and ensure it's in your PATH.
    exit /b 1
)

REM Build command
set "PYTHON_CMD=python "%PYTHON_SCRIPT%" "%DIRECTORY%" %RECURSIVE% %SUPPRESSION_FILE% %OUTPUT% %FAIL_ON_INVALID%"

echo Running UPROPERTY FGuid validation...
echo Command: %PYTHON_CMD%
echo.

REM Execute Python script
%PYTHON_CMD%
set "EXIT_CODE=%ERRORLEVEL%"

if %EXIT_CODE% equ 0 (
    echo.
    echo ✅ Validation completed successfully
) else (
    echo.
    echo ❌ Validation failed with exit code: %EXIT_CODE%
)

exit /b %EXIT_CODE%