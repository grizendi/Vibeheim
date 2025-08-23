# Struct Initialization Validation Scripts

This directory contains static analysis tools to validate UPROPERTY FGuid initialization patterns in Unreal Engine C++ code.

## Overview

The validation scripts ensure that all `UPROPERTY` members of type `FGuid` follow proper initialization patterns as required by UE5.6's reflection system. This prevents "StructProperty ... is not initialized properly" errors during engine startup.

## Files

- `validate_struct_initialization.py` - Main Python validation script
- `validate_struct_initialization.ps1` - PowerShell wrapper for Windows
- `validate_struct_initialization.bat` - Batch file wrapper for Windows
- `struct_initialization_suppressions.json` - Suppression file for exceptions
- `README.md` - This documentation file

## Quick Start

### Windows (PowerShell)
```powershell
# Scan Source directory
.\Scripts\validate_struct_initialization.ps1 -Directory "Source"

# Scan with suppressions
.\Scripts\validate_struct_initialization.ps1 -Directory "Source" -SuppressionFile "Scripts\struct_initialization_suppressions.json"
```

### Windows (Command Prompt)
```cmd
# Scan Source directory
Scripts\validate_struct_initialization.bat Source

# Scan with suppressions and save report
Scripts\validate_struct_initialization.bat Source --suppression-file Scripts\struct_initialization_suppressions.json --output validation_report.txt
```

### Cross-Platform (Python)
```bash
# Scan Source directory
python Scripts/validate_struct_initialization.py Source

# Scan with all options
python Scripts/validate_struct_initialization.py Source \
  --suppression-file Scripts/struct_initialization_suppressions.json \
  --output validation_report.txt \
  --fail-on-invalid
```

## Validation Rules

The validator checks for these patterns:

### ✅ Valid Patterns
```cpp
USTRUCT(BlueprintType)
struct FMyStruct
{
    GENERATED_BODY()

    // Valid: In-class initializer with FGuid()
    UPROPERTY()
    FGuid MyId = FGuid();

    // Valid: In-class initializer with FGuid::NewGuid()
    UPROPERTY()
    FGuid AnotherId = FGuid::NewGuid();
};
```

### ❌ Invalid Patterns
```cpp
USTRUCT(BlueprintType)
struct FMyStruct
{
    GENERATED_BODY()

    // Invalid: No initializer
    UPROPERTY()
    FGuid MyId;

    // Invalid: Invalid initializer
    UPROPERTY()
    FGuid AnotherId = SomeFunction();
};
```

## Suppression File

Use `struct_initialization_suppressions.json` to suppress validation for legitimate exceptions:

```json
{
  "suppressions": [
    "Source/MyModule/MyFile.h:FMyStruct::MyProperty",
    "FLegacyStruct::LegacyId",
    "*::DebugId"
  ]
}
```

Suppression patterns:
- `"file:struct::property"` - Suppress specific property in specific file
- `"struct::property"` - Suppress property in any file
- `"*::property"` - Suppress property name in any struct
- `"file"` - Suppress entire file

## CI Integration

The validation is integrated into GitHub Actions via `.github/workflows/struct-initialization-validation.yml`. It runs automatically on:
- Push to main/develop branches
- Pull requests to main/develop branches
- Changes to header files or validation scripts

## Command Line Options

### Python Script Options
```
python validate_struct_initialization.py <directory> [options]

Arguments:
  directory                 Directory to scan for header files

Options:
  --recursive, -r          Scan directory recursively (default: True)
  --suppression-file, -s   Path to suppression file (JSON format)
  --create-suppression-file Create a default suppression file
  --output, -o             Output file for the report (default: stdout)
  --fail-on-invalid        Exit with non-zero code if invalid properties found
```

### PowerShell Script Options
```
.\validate_struct_initialization.ps1 -Directory <path> [options]

Parameters:
  -Directory <path>           Directory to scan (required)
  -Recursive                  Scan recursively (default: true)
  -SuppressionFile <path>     Path to suppression file
  -CreateSuppressionFile <path> Create default suppression file
  -Output <path>              Output file for report
  -FailOnInvalid              Exit with error if invalid properties found
  -Help                       Show help message
```

## Examples

### Basic Validation
```bash
# Scan all header files in Source directory
python Scripts/validate_struct_initialization.py Source
```

### CI/CD Integration
```bash
# Fail build if invalid properties found
python Scripts/validate_struct_initialization.py Source \
  --suppression-file Scripts/struct_initialization_suppressions.json \
  --fail-on-invalid
```

### Generate Report
```bash
# Save detailed report to file
python Scripts/validate_struct_initialization.py Source \
  --output struct_validation_report.txt
```

### Create Suppression File
```bash
# Create default suppression file
python Scripts/validate_struct_initialization.py . \
  --create-suppression-file my_suppressions.json
```

## Troubleshooting

### Python Not Found
Ensure Python 3.6+ is installed and in your PATH:
```bash
python --version
```

### Permission Denied (Linux/Mac)
Make the script executable:
```bash
chmod +x Scripts/validate_struct_initialization.py
```

### False Positives
Add suppressions to `struct_initialization_suppressions.json` for legitimate exceptions.

### Integration with Build System

You can integrate this validation into your build process:

#### Unreal Build Tool Integration
Add to your module's Build.cs file:
```csharp
// In your module's Build.cs constructor
if (Target.Configuration == UnrealTargetConfiguration.Development)
{
    // Run validation as pre-build step
    System.Diagnostics.Process.Start("python", 
        "Scripts/validate_struct_initialization.py Source --fail-on-invalid");
}
```

#### Pre-commit Hook
Add to `.git/hooks/pre-commit`:
```bash
#!/bin/sh
python Scripts/validate_struct_initialization.py Source --fail-on-invalid
```

## Requirements

- Python 3.6 or higher
- Access to header files in the project
- (Optional) Git for pre-commit hooks
- (Optional) GitHub Actions for CI integration