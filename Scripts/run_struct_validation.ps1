# Simple script to run struct initialization validation
# This is the main entry point for CI/CD and manual validation

param(
    [string]$Directory = "Source",
    [string]$SuppressionFile = "Scripts/struct_initialization_suppressions.json",
    [string]$OutputFile,
    [switch]$FailOnInvalid,
    [switch]$CreateSuppressionFile,
    [switch]$Help
)

if ($Help) {
    Write-Host @"
Struct Initialization Validation Runner

This script validates that all UPROPERTY FGuid members have proper initialization patterns.

Usage: .\run_struct_validation.ps1 [options]

Options:
  -Directory <path>           Directory to scan (default: Source)
  -SuppressionFile <path>     Suppression file path (default: Scripts/struct_initialization_suppressions.json)
  -OutputFile <path>          Save report to file (default: console output)
  -FailOnInvalid              Exit with error code if invalid properties found
  -CreateSuppressionFile      Create a default suppression file
  -Help                       Show this help

Examples:
  # Basic validation
  .\run_struct_validation.ps1

  # CI mode with failure on invalid properties
  .\run_struct_validation.ps1 -FailOnInvalid

  # Save report to file
  .\run_struct_validation.ps1 -OutputFile "validation_report.txt"

  # Create suppression file
  .\run_struct_validation.ps1 -CreateSuppressionFile
"@
    exit 0
}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

# Create suppression file if requested
if ($CreateSuppressionFile) {
    $suppressionContent = @{
        "suppressions" = @(
            "# Suppression patterns for UPROPERTY FGuid validation",
            "# Format: 'File:Struct::Property', 'Struct::Property', '*::Property', or 'File'",
            "# Examples:",
            "# `"Source/MyModule/MyFile.h:FMyStruct::MyProperty`"",
            "# `"FLegacyStruct::LegacyId`"",
            "# `"*::DebugId`""
        )
    }
    
    $suppressionJson = $suppressionContent | ConvertTo-Json -Depth 3
    $suppressionJson | Out-File -FilePath $SuppressionFile -Encoding UTF8
    
    Write-Host "Created suppression file: $SuppressionFile" -ForegroundColor Green
    exit 0
}

# Check if directory exists
if (-not (Test-Path $Directory)) {
    Write-Error "Directory '$Directory' does not exist"
    exit 1
}

Write-Host "Running UPROPERTY FGuid validation..." -ForegroundColor Cyan
Write-Host "Directory: $Directory"
Write-Host "Suppression file: $SuppressionFile"
Write-Host ""

# Try Python first, fall back to PowerShell implementation
$pythonScript = Join-Path $ScriptDir "validate_struct_initialization.py"
$usePython = $false

if (Test-Path $pythonScript) {
    try {
        $pythonVersion = python --version 2>&1
        if ($LASTEXITCODE -eq 0) {
            $usePython = $true
            Write-Host "Using Python validation: $pythonVersion" -ForegroundColor Green
        }
    } catch {
        # Python not available, use PowerShell fallback
    }
}

if ($usePython) {
    # Use Python implementation
    $pythonArgs = @($pythonScript, $Directory, "--recursive")
    
    if (Test-Path $SuppressionFile) {
        $pythonArgs += "--suppression-file", $SuppressionFile
    }
    
    if ($OutputFile) {
        $pythonArgs += "--output", $OutputFile
    }
    
    if ($FailOnInvalid) {
        $pythonArgs += "--fail-on-invalid"
    }
    
    try {
        & python @pythonArgs
        $exitCode = $LASTEXITCODE
    } catch {
        Write-Error "Failed to run Python validation: $_"
        exit 1
    }
} else {
    # Use PowerShell fallback implementation
    Write-Host "Python not available, using PowerShell implementation..." -ForegroundColor Yellow
    
    # Load suppressions
    $suppressions = @()
    if (Test-Path $SuppressionFile) {
        try {
            $suppressionData = Get-Content $SuppressionFile | ConvertFrom-Json
            $suppressions = $suppressionData.suppressions | Where-Object { -not $_.StartsWith("#") }
        } catch {
            Write-Warning "Could not load suppression file: $_"
        }
    }
    
    # Scan files
    $headerFiles = Get-ChildItem -Path $Directory -Filter "*.h" -Recurse
    $allProperties = @()
    $validProperties = @()
    $invalidProperties = @()
    $suppressedProperties = @()
    
    $uPropertyPattern = '^\s*UPROPERTY\s*\([^)]*\)\s*$'
    $fGuidPattern = '^\s*FGuid\s+(\w+)(?:\s*=\s*([^;]+))?\s*;\s*(?://.*)?$'
    $structPattern = '^\s*(?:USTRUCT\s*\([^)]*\)\s*)?struct\s+(?:VIBEHEIM_API\s+)?(\w+)'
    $validInitializers = @('FGuid()', 'FGuid::NewGuid()')
    
    foreach ($file in $headerFiles) {
        $content = Get-Content $file.FullName -Raw -ErrorAction SilentlyContinue
        if (-not $content) { continue }
        
        $lines = $content -split "`n"
        $lineNumber = 0
        
        foreach ($line in $lines) {
            $lineNumber++
            
            if ($line -match $uPropertyPattern) {
                for ($j = $lineNumber; $j -lt [Math]::Min($lineNumber + 5, $lines.Count); $j++) {
                    $nextLine = $lines[$j]
                    
                    if ($nextLine -match $fGuidPattern) {
                        $propertyName = $Matches[1]
                        $initializer = $Matches[2]
                        
                        # Find struct name
                        $structName = "UnknownStruct"
                        for ($k = $j - 1; $k -ge 0; $k--) {
                            if ($lines[$k] -match $structPattern) {
                                $structName = $Matches[1]
                                break
                            }
                        }
                        
                        $hasInitializer = $null -ne $initializer -and $initializer.Trim() -ne ""
                        $initializerValue = if ($hasInitializer) { $initializer.Trim() } else { $null }
                        
                        $property = [PSCustomObject]@{
                            File = $file.FullName
                            RelativeFile = $file.FullName.Replace((Get-Location).Path + "\", "")
                            LineNumber = $j + 1
                            StructName = $structName
                            PropertyName = $propertyName
                            HasInitializer = $hasInitializer
                            InitializerValue = $initializerValue
                            UPropertyLine = $line.Trim()
                            DeclarationLine = $nextLine.Trim()
                        }
                        
                        $allProperties += $property
                        
                        # Check if suppressed
                        $isSuppressed = $false
                        $patternsToCheck = @(
                            "$($property.RelativeFile):$($structName)::$($propertyName)",
                            "$($structName)::$($propertyName)",
                            "*::$($propertyName)",
                            $property.RelativeFile
                        )
                        
                        foreach ($pattern in $patternsToCheck) {
                            if ($pattern -in $suppressions) {
                                $isSuppressed = $true
                                break
                            }
                        }
                        
                        if ($isSuppressed) {
                            $suppressedProperties += $property
                        } elseif ($hasInitializer -and ($initializerValue -in $validInitializers)) {
                            $validProperties += $property
                        } else {
                            $invalidProperties += $property
                        }
                        
                        break
                    }
                }
            }
        }
    }
    
    # Generate report
    $report = @()
    $report += "=" * 80
    $report += "UPROPERTY FGuid Initialization Validation Report"
    $report += "=" * 80
    $report += ""
    $report += "Total files scanned: $($headerFiles.Count)"
    $report += "Total UPROPERTY FGuid properties found: $($allProperties.Count)"
    $report += "Valid properties: $($validProperties.Count)"
    $report += "Invalid properties: $($invalidProperties.Count)"
    $report += "Suppressed properties: $($suppressedProperties.Count)"
    $report += ""
    
    if ($invalidProperties.Count -gt 0) {
        $report += "VALIDATION FAILED"
        $report += ""
        $report += "Invalid Properties (require fixes):"
        $report += "-" * 40
        
        foreach ($prop in $invalidProperties) {
            $report += "File: $($prop.RelativeFile):$($prop.LineNumber)"
            $report += "Struct: $($prop.StructName)"
            $report += "Property: $($prop.PropertyName)"
            $report += "Declaration: $($prop.DeclarationLine)"
            if ($prop.HasInitializer) {
                $report += "Current initializer: $($prop.InitializerValue)"
                $report += "Issue: Invalid initializer pattern"
            } else {
                $report += "Issue: Missing in-class initializer"
            }
            $report += "Expected: FGuid() or FGuid::NewGuid()"
            $report += ""
        }
    } else {
        $report += "VALIDATION PASSED"
        $report += ""
    }
    
    if ($suppressedProperties.Count -gt 0) {
        $report += "Suppressed Properties:"
        $report += "-" * 20
        foreach ($prop in $suppressedProperties) {
            $report += "$($prop.RelativeFile):$($prop.LineNumber) - $($prop.StructName)::$($prop.PropertyName)"
        }
        $report += ""
    }
    
    if ($validProperties.Count -gt 0) {
        $report += "Valid Properties:"
        $report += "-" * 15
        foreach ($prop in $validProperties) {
            $report += "VALID: $($prop.RelativeFile):$($prop.LineNumber) - $($prop.StructName)::$($prop.PropertyName) = $($prop.InitializerValue)"
        }
        $report += ""
    }
    
    $reportText = $report -join "`n"
    
    # Output report
    if ($OutputFile) {
        $reportText | Out-File -FilePath $OutputFile -Encoding UTF8
        Write-Host "Report written to: $OutputFile" -ForegroundColor Green
    } else {
        Write-Host $reportText
    }
    
    # Set exit code
    $exitCode = if ($FailOnInvalid -and $invalidProperties.Count -gt 0) { 1 } else { 0 }
}

# Final status
if ($exitCode -eq 0) {
    Write-Host ""
    Write-Host "Validation completed successfully" -ForegroundColor Green
} else {
    Write-Host ""
    Write-Host "Validation failed" -ForegroundColor Red
}

exit $exitCode