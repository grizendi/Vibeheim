#!/bin/bash
# World Generation Integration Test Runner for Linux/Mac
# This script runs comprehensive integration tests for the world generation system

echo "========================================"
echo "World Generation Integration Test Suite"
echo "========================================"

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_PATH="$SCRIPT_DIR/../Vibeheim.uproject"

# Check if project file exists
if [ ! -f "$PROJECT_PATH" ]; then
    echo "ERROR: Project file not found at $PROJECT_PATH"
    echo "Please run this script from the Scripts directory"
    exit 1
fi

echo "Project: $PROJECT_PATH"
echo

# Run integration tests using Unreal Engine automation framework
echo "Running integration tests..."
echo

# Run all WorldGen integration tests
UnrealEditor "$PROJECT_PATH" -ExecCmds="Automation RunTests WorldGen.Integration; quit" -unattended -nullrhi -nosplash -log

# Check if the command succeeded
if [ $? -ne 0 ]; then
    echo
    echo "ERROR: Integration tests failed"
    echo "Check the log files for detailed error information"
    exit 1
fi

echo
echo "========================================"
echo "Integration tests completed successfully"
echo "========================================"
echo
echo "Check the automation test results in the Unreal Editor logs"
echo "or run individual tests using console commands:"
echo "  wg.RunIntegrationTests"
echo "  wg.TestBiomeTransitions"
echo "  wg.TestPOIPortalSystem"
echo "  wg.ValidateVisualQuality"
echo