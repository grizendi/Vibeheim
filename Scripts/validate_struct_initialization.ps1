# PowerShell wrapper for struct initialization validation
# This script provides a Windows-friendly interface to the Python validation script

param(
    [Parameter(Mandatory=$true)]
    [string]$Directory,
    
    [switch]$Recursive = $true,
    
    [string]$SuppressionFile,
    
    [string]$CreateSuppressionFile,
    
    [string]$Output,
    
    [switch]$FailOnInvalid,
    
    [switch]$Help
)

if ($Help) {
    Write-Host @"
UPROPERTY FGuid Initialization Validator

Usage: .\validate_struct_initialization.ps1 -Directory <path> [options]

Parameters:
  -Directory <path>           Directory to scan for header files (required)
  -Recursive                  Scan directory recursively (default: true)
  -SuppressionFile <path>     Path to suppression file (JSON format)
  -CreateSuppressionFile <path> Create a default suppression file
  -Output <path>              Output file for the report (default: console)
  -FailOnInvalid              Exit with error code if invalid properties found
  -Help                       Show this help message

Examples:
  # Scan Source directory recursively
  .\validate_struct_initialization.ps1 -Directory "Source"
  
  # Create suppression file
  .\validate_struct_initialization.ps1 -CreateSuppressionFile "suppressions.json" -Directory "."
  
  # Scan with suppressions and save report
  .\validate_struct_initialization.ps1 -Directory "Source" -SuppressionFile "suppressions.json" -Output "validation_report.txt"
  
  # CI mode - fail on invalid properties
  .\validate_struct_initialization.ps1 -Directory "Source" -FailOnInvalid
"@
    exit 0
}

# Get the directory where this script is located
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PythonScript = Join-Path $ScriptDir "validate_struct_initialization.py"

# Check if Python script exists
if (-not (Test-Path $PythonScript)) {
    Write-Error "Python validation script not found: $PythonScript"
    exit 1
}

# Check if Python is available
try {
    $pythonVersion = python --version 2>&1
    Write-Host "Using Python: $pythonVersion"
} catch {
    Write-Error "Python not found. Please install Python 3.6+ and ensure it's in your PATH."
    exit 1
}

# Build arguments for Python script
$pythonArgs = @($PythonScript, $Directory)

if ($Recursive) {
    $pythonArgs += "--recursive"
}

if ($SuppressionFile) {
    $pythonArgs += "--suppression-file", $SuppressionFile
}

if ($CreateSuppressionFile) {
    $pythonArgs += "--create-suppression-file", $CreateSuppressionFile
}

if ($Output) {
    $pythonArgs += "--output", $Output
}

if ($FailOnInvalid) {
    $pythonArgs += "--fail-on-invalid"
}

# Execute Python script
Write-Host "Running UPROPERTY FGuid validation..."
Write-Host "Command: python $($pythonArgs -join ' ')"
Write-Host ""

try {
    $result = & python @pythonArgs
    $exitCode = $LASTEXITCODE
    
    if ($exitCode -eq 0) {
        Write-Host "✅ Validation completed successfully" -ForegroundColor Green
    } else {
        Write-Host "❌ Validation failed with exit code: $exitCode" -ForegroundColor Red
    }
    
    exit $exitCode
} catch {
    Write-Error "Failed to execute validation script: $_"
    exit 1
}