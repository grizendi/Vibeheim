# World Generation Integration Testing

This document describes the comprehensive integration testing system for the world generation pipeline.

## Overview

The integration testing system validates the complete world generation workflow, including:
- WorldGenManager initialization and coordination
- Biome transition smoothness and visual quality
- POI and dungeon portal functionality
- End-to-end player interaction scenarios
- Complete system workflow validation

## Test Components

### 1. Integration Test Level

**Location**: `Content/_Assets/Levels/WorldGenIntegrationTest.umap`

A dedicated test level containing:
- Pre-configured WorldGenManager with deterministic test settings (Seed: 42)
- WorldGenIntegrationTestController for automated testing
- PlayerStart positioned at origin for consistent player spawn testing
- Visual markers at key test locations for manual validation
- Proper lighting setup for visual quality assessment
- Test chunks pre-defined for consistent POI/portal testing

**Level Configuration:**
- WorldGenManager configured with test-specific settings
- Integration test controller with automatic testing enabled
- Multiple test locations marked for biome transition testing
- Chunk boundaries visualized for streaming validation
- Performance monitoring enabled for timing validation

### 2. Automated Test Suite

**Location**: `Source/Vibeheim/WorldGen/Tests/WorldGenTests.cpp`

Comprehensive automation tests including:

#### FWorldGenIntegrationTest
- Tests complete system initialization with all subsystems
- Validates all subsystem availability and functionality
- Tests biome evaluation at multiple locations
- Validates POI and portal generation across test chunks
- Checks streaming statistics and placement stats
- Verifies system component integration

#### FBiomeTransitionSmoothnesTest
- Samples biome transitions along multiple transects in different directions
- Validates smooth blend weight changes between adjacent samples
- Tests transition quality across all biome types
- Configurable tolerance for blend weight changes (default: 0.1)
- Comprehensive coverage of different world regions

#### FPOIPortalEndToEndTest
- Tests complete POI and portal generation workflow
- Validates placement results and accessibility
- Checks biome compatibility for all placements
- Tests interaction between POIs and portals
- Validates retrieval of active instances
- Verifies placement statistics accuracy

#### FCompleteWorkflowTest
- Tests full initialization → player setup → generation → streaming → cleanup workflow
- Validates runtime settings updates and persistence
- Tests player anchor functionality (manual and automatic)
- Verifies chunk rebuild and regeneration capabilities
- Tests graceful cleanup and error handling

#### FWorldGenPipelineIntegrationTest
- Comprehensive end-to-end pipeline validation
- Tests all major systems working together
- Validates biome evaluation across 10+ diverse locations
- Tests POI and portal generation across 11 test chunks
- Verifies streaming statistics and performance metrics
- Validates placement statistics for both POIs and portals
- Tests active instance retrieval and management

### 3. Integration Test Controller

**Location**: `Source/Vibeheim/WorldGen/Tests/WorldGenIntegrationTestController.h/.cpp`

Blueprint-accessible test controller that can be placed in levels:

#### Key Features
- Automatic test execution on BeginPlay (configurable)
- Blueprint-callable test functions
- Detailed logging and result tracking
- Configurable test parameters
- Visual quality validation through automated sampling

#### Available Tests
- `RunAllTests()` - Execute complete test suite
- `TestBiomeTransitionSmoothness()` - Validate biome transitions
- `TestPOIPortalFunctionality()` - Test POI/portal systems
- `ValidateVisualQuality()` - Automated visual quality checks
- `TestCompleteWorkflow()` - End-to-end workflow validation

### 4. Console Commands

**Location**: `Source/Vibeheim/WorldGen/WorldGenConsoleCommands.h/.cpp`

Interactive console commands for manual testing:

#### Available Commands
- `wg.RunIntegrationTests` - Run all integration tests using WorldGenIntegrationTestController
- `wg.TestBiomeTransitions [X Y] [radius] [samples]` - Test biome transitions at specified location
- `wg.TestPOIPortalSystem [ChunkX ChunkY ChunkZ]` - Test POI/portal system for specific chunk
- `wg.ValidateVisualQuality [X Y Z]` - Validate visual quality through automated sampling

**Command Features:**
- Automatic WorldGenManager detection in current level
- Comprehensive parameter validation and error reporting
- Detailed logging with success/failure indicators
- Configurable test parameters for different scenarios
- Integration with WorldGenIntegrationTestController when available

## Running Tests

### Automated Testing

#### Using Unreal Engine Automation Framework
```bash
# Run all integration tests
UnrealEditor.exe YourProject.uproject -ExecCmds="Automation RunTests WorldGen.Integration; quit" -unattended -nullrhi

# Run specific test category
UnrealEditor.exe YourProject.uproject -ExecCmds="Automation RunTests WorldGen.Integration.CompleteSystem; quit" -unattended -nullrhi
```

#### Using Test Scripts
```bash
# Windows
Scripts\RunIntegrationTests.bat

# Linux/Mac
Scripts/RunIntegrationTests.sh
```

**Test Scripts Features:**
- Automatic project path detection
- Comprehensive error checking and reporting
- Headless execution with `-nullrhi` for CI/CD compatibility
- Detailed success/failure reporting
- Instructions for manual console command testing

### Manual Testing

#### In-Editor Testing
1. Open `WorldGenIntegrationTest.umap`
2. Place `WorldGenIntegrationTestController` in the level
3. Set `bAutoTestOnBeginPlay = true` in the controller
4. Play the level to run tests automatically

#### Console Command Testing
1. Open any level with WorldGenManager
2. Open console (` key)
3. Run commands like `wg.RunIntegrationTests`

## Test Configuration

### Test Parameters

#### Biome Transition Testing
- **BiomeTransitionTolerance**: Maximum allowed blend weight change (default: 0.1)
- **SampleRadius**: Radius for transition sampling (default: 500.0)
- **SampleCount**: Number of samples per test location (default: 20)

#### Visual Quality Testing
- **VisualQualityThreshold**: Minimum quality score (default: 0.8)
- **DefaultSampleCount**: Default number of samples (default: 20)

#### POI/Portal Testing
- **MinSeparation**: Minimum distance between POIs and portals (default: 100.0)
- **MaxDistance**: Maximum distance from origin for placement (default: 10000.0)

### Test Settings

Tests use deterministic settings for reproducibility:
```cpp
FWorldGenSettings TestSettings;
TestSettings.Seed = 42; // Fixed seed for determinism
TestSettings.WorldGenVersion = 1;
TestSettings.PluginSHA = TEXT("integration_test");
TestSettings.VoxelSizeCm = 50.0f;
TestSettings.ChunkSize = 32;
```

## Validation Criteria

### Biome Transition Smoothness
- ✅ Blend weight changes ≤ 0.1 between adjacent samples
- ✅ No abrupt biome boundary artifacts
- ✅ Smooth transitions across all biome types

### POI and Portal Functionality
- ✅ Valid world locations (non-zero)
- ✅ Proper biome compatibility
- ✅ No overlapping placements
- ✅ Correct activation states
- ✅ Retrievable through manager APIs

### Visual Quality
- ✅ Primary biome is never None
- ✅ Blend weights are non-negative
- ✅ Blend weights sum to approximately 1.0
- ✅ Quality score ≥ threshold

### Complete Workflow
- ✅ Successful system initialization
- ✅ All subsystems available and ready
- ✅ Player anchor functionality
- ✅ Runtime settings updates
- ✅ Graceful cleanup

## Troubleshooting

### Common Issues

#### "WorldGenManager not found"
- Ensure WorldGenManager is placed in the test level
- Check that the manager is properly initialized
- Verify the manager's `IsWorldGenReady()` returns true

#### "Test failures in biome transitions"
- Check BiomeTransitionTolerance setting
- Verify biome system initialization
- Review noise generation parameters

#### "POI/Portal placement failures"
- Check spawn rule configurations
- Verify chunk coordinates are valid
- Review placement attempt statistics

### Debug Logging

Enable detailed logging for troubleshooting:
```cpp
// In WorldGenIntegrationTestController
bEnableDetailedLogging = true;

// In WorldGenManager
bEnableDebugLogging = true;
DebugLoggingInterval = 5.0f;
```

### Performance Considerations

- Integration tests may take 30-60 seconds to complete
- Use `-nullrhi` flag for headless testing to improve performance
- Consider running tests on dedicated test machines for CI/CD

## Continuous Integration

### Automated Test Execution

The integration tests are designed to run in CI/CD pipelines:

```yaml
# Example GitHub Actions workflow
- name: Run Integration Tests
  run: |
    UnrealEditor.exe ${{ env.PROJECT_PATH }} \
      -ExecCmds="Automation RunTests WorldGen.Integration; quit" \
      -unattended -nullrhi -nosplash
```

### Test Result Parsing

Test results are logged in structured format for easy parsing:
```
✓ WorldGenIntegrationTest: PASSED - All systems initialized
✗ BiomeTransitionSmoothness: FAILED - Rough transition at location 2
```

## Extending Tests

### Adding New Test Cases

1. Create new test class inheriting from automation test base
2. Implement test logic following existing patterns
3. Add appropriate logging and validation
4. Register test in the automation framework

### Custom Validation

Add custom validation functions to `WorldGenIntegrationTestController`:
```cpp
UFUNCTION(BlueprintCallable, Category = "Integration Test")
bool ValidateCustomFeature(const FCustomTestParams& Params);
```

## Test Coverage Summary

### Completed Integration Tests

✅ **Complete Integration Test Level** (`WorldGenIntegrationTest.umap`)
- WorldGenManager properly configured and placed
- WorldGenIntegrationTestController with comprehensive test suite
- Visual markers and test locations for manual validation
- Performance monitoring and logging enabled

✅ **Automated Test Suite** (5 comprehensive tests)
- `FWorldGenIntegrationTest` - Complete system integration
- `FBiomeTransitionSmoothnesTest` - Biome transition validation
- `FPOIPortalEndToEndTest` - POI/portal functionality testing
- `FCompleteWorkflowTest` - End-to-end workflow validation
- `FWorldGenPipelineIntegrationTest` - Comprehensive pipeline testing

✅ **Console Command Integration**
- `wg.RunIntegrationTests` - Execute all tests via console
- `wg.TestBiomeTransitions` - Interactive biome testing
- `wg.TestPOIPortalSystem` - Interactive POI/portal testing
- `wg.ValidateVisualQuality` - Interactive visual validation

✅ **Automated Test Scripts**
- `RunIntegrationTests.bat` - Windows automation script
- `RunIntegrationTests.sh` - Linux/Mac automation script
- CI/CD compatible with headless execution
- Comprehensive error reporting and validation

### Requirements Coverage

This integration testing system addresses the following requirements:

- **Requirement 2.3**: ✅ Biome transition smoothness validation through automated sampling
- **Requirement 3.2**: ✅ POI placement and functionality testing with end-to-end validation
- **Requirement 3.4**: ✅ Dungeon portal system validation with player interaction testing
- **Requirement 5.1**: ✅ Complete workflow and end-to-end testing with comprehensive pipeline validation

### Test Statistics

- **Total Integration Tests**: 5 automated tests
- **Test Locations**: 10+ biome evaluation points
- **Test Chunks**: 11+ chunk coordinates for POI/portal testing
- **Console Commands**: 4 interactive testing commands
- **Test Scripts**: 2 platform-specific automation scripts
- **Coverage**: All major world generation systems and workflows

The comprehensive test suite ensures that all world generation systems work together correctly, meet the specified quality standards, and provide reliable end-to-end functionality for players.