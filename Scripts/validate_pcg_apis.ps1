#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Validates that no legacy PCG APIs are used in the WorldGen module.

.DESCRIPTION
    This script checks for legacy pre-UE 5.6 PCG API usage in the WorldGen module.
    Legacy APIs include: RunGraph, GetGraphOutput, FPCGDataCollection, FPCGMetadata
    
    The only exception is PCGSchedulerExecutor.cpp which may use these internally
    as a wrapper for the UE 5.6 scheduler.

.PARAMETER FailOnLegacyAPI
    If set, the script will exit with code 1 if legacy APIs are detected.

.PARAMETER OutputFile
    Optional path to write the validation report.

.EXAMPLE
    .\validate_pcg_apis.ps1
    
.EXAMPLE
    .\validate_pcg_apis.ps1 -FailOnLegacyAPI -OutputFile pcg_validation.txt
#>

param(
    [switch]$FailOnLegacyAPI,
    [string]$OutputFile = ""
)

$ErrorActionPreference = "Stop"

# Get script directory and project root
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir
$WorldGenPath = Join-Path $ProjectRoot "Source\Vibeheim\WorldGen"

Write-Host "=== PCG Legacy API Detection ===" -ForegroundColor Cyan
Write-Host "Scanning: $WorldGenPath" -ForegroundColor Gray
Write-Host ""

# Define legacy API patterns
$LegacyPatterns = @(
    "RunGraph",
    "GetGraphOutput", 
    "FPCGDataCollection",
    "FPCGMetadata"
)

# Files to exclude from checks
$ExcludedFiles = @(
    "PCGSchedulerExecutor.cpp"
)

$Results = @()
$HasLegacyAPIs = $false

# Search for legacy APIs
foreach ($Pattern in $LegacyPatterns) {
    Write-Host "Checking for: $Pattern" -ForegroundColor Gray
    
    $Files = Get-ChildItem -Path $WorldGenPath -Recurse -Include "*.cpp","*.h" | 
        Where-Object { 
            $_.FullName -notmatch "\\Intermediate\\" -and 
            $_.FullName -notmatch "\\Binaries\\" -and
            $ExcludedFiles -notcontains $_.Name
        }
    
    foreach ($File in $Files) {
        $LineNumber = 0
        $Content = Get-Content $File.FullName
        
        foreach ($Line in $Content) {
            $LineNumber++
            if ($Line -match $Pattern) {
                $HasLegacyAPIs = $true
                $Result = [PSCustomObject]@{
                    File = $File.FullName.Replace($ProjectRoot, "").TrimStart("\")
                    Line = $LineNumber
                    Pattern = $Pattern
                    Content = $Line.Trim()
                }
                $Results += $Result
            }
        }
    }
}

# Generate report
$Report = @()
$Report += "=== PCG Legacy API Detection Report ==="
$Report += "Scan Date: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
$Report += "Scan Path: $WorldGenPath"
$Report += ""

if ($HasLegacyAPIs) {
    $Report += "❌ FAILED: Legacy PCG APIs detected!"
    $Report += ""
    $Report += "The following files contain legacy PCG API usage:"
    $Report += ""
    
    foreach ($Result in $Results) {
        $Report += "  File: $($Result.File)"
        $Report += "  Line: $($Result.Line)"
        $Report += "  API:  $($Result.Pattern)"
        $Report += "  Code: $($Result.Content)"
        $Report += ""
    }
    
    $Report += "Legacy APIs (pre-UE 5.6) are not allowed outside PCGSchedulerExecutor.cpp"
    $Report += "Please migrate to UE 5.6 scheduler APIs."
    $Report += "See Docs/PCG-5.6.md for migration guidance."
    
    Write-Host ($Report -join "`n") -ForegroundColor Red
} else {
    $Report += "✅ PASSED: No legacy PCG APIs detected"
    $Report += "All PCG code uses UE 5.6 scheduler APIs"
    
    Write-Host ($Report -join "`n") -ForegroundColor Green
}

# Check for PCGVersionGuard.h
$Report += ""
$Report += "=== PCGVersionGuard.h Verification ==="

$GuardPath = Join-Path $ProjectRoot "Source\Vibeheim\WorldGen\Public\PCGVersionGuard.h"
if (Test-Path $GuardPath) {
    $Report += "✅ PCGVersionGuard.h exists"
    Write-Host "✅ PCGVersionGuard.h exists" -ForegroundColor Green
} else {
    $Report += "❌ PCGVersionGuard.h not found!"
    $Report += "This header is required for engine version enforcement"
    Write-Host "❌ PCGVersionGuard.h not found!" -ForegroundColor Red
    $HasLegacyAPIs = $true
}

# Check for PCGVersionGuard.h includes in PCG files
$Report += ""
$Report += "=== PCGVersionGuard.h Include Check ==="

$PCGFiles = Get-ChildItem -Path $WorldGenPath -Recurse -Include "*.cpp" |
    Where-Object { 
        $_.FullName -notmatch "\\Intermediate\\" -and 
        $_.FullName -notmatch "\\Binaries\\"
    } |
    Where-Object {
        $Content = Get-Content $_.FullName -Raw
        $Content -match "UPCGSubsystem|UPCGComponent|UPCGMetadata|UPCGData"
    }

$MissingGuard = @()
foreach ($File in $PCGFiles) {
    $Content = Get-Content $File.FullName -Raw
    if ($Content -notmatch "PCGVersionGuard\.h") {
        $MissingGuard += $File.FullName.Replace($ProjectRoot, "").TrimStart("\")
    }
}

if ($MissingGuard.Count -gt 0) {
    $Report += "⚠️  WARNING: The following files use PCG APIs but don't include PCGVersionGuard.h:"
    foreach ($File in $MissingGuard) {
        $Report += "  - $File"
    }
    Write-Host "⚠️  WARNING: Some PCG files missing PCGVersionGuard.h include" -ForegroundColor Yellow
} else {
    $Report += "✅ All PCG files include PCGVersionGuard.h"
    Write-Host "✅ All PCG files include PCGVersionGuard.h" -ForegroundColor Green
}

# Write output file if specified
if ($OutputFile) {
    $OutputPath = Join-Path $ProjectRoot $OutputFile
    $Report | Out-File -FilePath $OutputPath -Encoding UTF8
    Write-Host ""
    Write-Host "Report written to: $OutputPath" -ForegroundColor Cyan
}

# Exit with error if legacy APIs found and FailOnLegacyAPI is set
if ($HasLegacyAPIs -and $FailOnLegacyAPI) {
    exit 1
}

exit 0
