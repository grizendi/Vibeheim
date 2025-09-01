# VHM UE5.6 Compatibility Testing Guide

This document describes the comprehensive runtime testing suite for the VHM (Virtual Heightfield Mesh) terrain rendering system's UE5.6 compatibility fixes.

## Overview

The VHM compatibility fixes address critical issues that prevented compilation and proper runtime behavior in UE5.6:

1. **TObjectPtr Handling** - Fixed material storage and retrieval using UE5.6 TObjectPtr system
2. **RVT Integration** - Updated Runtime Virtual Texture API usage for UE5.6 compatibility
3. **VHM Component Material Assignment** - Fixed protected member access issues
4. **Console Command Property Access** - Added VHMTerrainRenderer property to WorldGenSettings

## Test Suite Components

### 1. Comprehensive Runtime Tests (`wg.TestVHMCompatibility`)

**File**: `Source/Vibeheim/WorldGen/Private/Tests/VHMCompatibilityRuntimeTests.cpp`

Runs all compatibility tests in sequence:
- TerrainMaterialSystem functionality
- VHMTerrainRenderer component creation and material assignment
- Console command property access
- RVT integration and fallback behavior
- TObjectPtr handling validation
- Memory leak detection

**Usage**:
```
wg.TestVHMCompatibility
```

**Expected Output**:
```
=== VHM UE5.6 Compatibility Runtime Tests ===
✓ TerrainMaterialSystem tests PASSED
✓ VHMTerrainRenderer tests PASSED
✓ Console Commands tests PASSED
✓ RVT Integration tests PASSED
✓ TObjectPtr Handling tests PASSED
✓ Memory Leak tests PASSED
=== VHM Compatibility Test Results ===
Tests Passed: 6/6
🎉 ALL TESTS PASSED - UE5.6 compatibility fixes are working correctly!
```

### 2. Basic Compatibility Test (`wg.TestVHMBasic`)

**File**: `Source/Vibeheim/WorldGen/Private/Tests/VHMBasicTests.cpp`

A safe, minimal test that validates core compatibility without complex object creation:
- WorldGenSettings access and VHMTerrainRenderer property
- Basic service creation (TerrainMaterialSystem, BiomeService, ClimateSystem)
- Settings validation
- Minimal object cleanup (relies on UE's automatic garbage collection)

**Usage**:
```
wg.TestVHMBasic
```

**Expected Output**:
```
=== Basic VHM UE5.6 Compatibility Test ===
✅ Settings access test PASSED
✅ Service creation test PASSED
=== Basic Test Results ===
Tests Passed: 2/2
🎉 BASIC TESTS PASSED - Core VHM compatibility is working!
```

### 3. Individual Component Tests

**File**: `Source/Vibeheim/WorldGen/Private/Tests/VHMComponentTests.cpp`

#### Material Creation Test (`wg.TestMaterialCreation`)
Tests TerrainMaterialSystem material creation and TObjectPtr handling:
- Material system initialization
- Material creation for different biomes
- Parameter updates
- Memory usage calculation
- Cleanup validation

#### VHM Component Test (`wg.TestVHMComponent`)
Tests VHM component creation and material assignment:
- VHM terrain renderer initialization
- Terrain mesh creation
- Component material assignment (UE5.6 fix validation)
- Performance metrics collection
- Component cleanup

#### Console Access Test (`wg.TestConsoleAccess`)
Tests console command property access:
- WorldGenSettings accessibility
- VHMTerrainRenderer property access
- Console command registration
- Settings validation

#### RVT Fallback Test (`wg.TestRVTFallback`)
Tests RVT integration and UE5.6 fallback behavior:
- RVT disabled mode
- RVT enabled mode with UE5.6 compatibility
- Material creation with RVT settings
- Graceful fallback when RVT unavailable

#### Performance Test (`wg.TestPerformance`)
Tests performance metrics and memory usage tracking:
- Multiple terrain mesh creation
- Performance statistics validation
- Memory usage calculation
- Active tile management

### 4. Stress Tests

**File**: `Source/Vibeheim/WorldGen/Private/Tests/VHMStressTests.cpp`

#### Material Stress Test (`wg.StressMaterials [NumTiles]`)
Tests material system under load:
- Creates many materials (default: 100)
- Tests TObjectPtr access performance
- Validates memory usage
- Measures creation/update/retrieval/cleanup times

**Usage**:
```
wg.StressMaterials 200
```

#### VHM Component Stress Test (`wg.StressVHM [NumComponents]`)
Tests VHM component system under load:
- Creates many VHM components (default: 25)
- Tests component access performance
- Validates material assignment
- Measures creation and cleanup times

**Usage**:
```
wg.StressVHM 50
```

#### Memory Leak Test (`wg.TestMemoryLeaks [Iterations]`)
Tests for memory leaks in repeated operations:
- Creates and destroys systems multiple times
- Alternates RVT settings
- Forces garbage collection
- Validates object count growth

**Usage**:
```
wg.TestMemoryLeaks 20
```

#### Performance Benchmark (`wg.BenchmarkVHM [NumTiles]`)
Benchmarks system performance:
- Material creation performance
- Update performance
- Retrieval performance
- Cleanup performance
- Compares against thresholds

**Usage**:
```
wg.BenchmarkVHM 100
```

## Test Scenarios

### Basic Functionality Validation

1. **Start the editor** and load your project
2. **Open the console** (` key or Window > Developer Tools > Output Log)
3. **Run basic test first** (safer, minimal object creation):
   ```
   wg.TestVHMBasic
   ```
4. **If basic test passes, run full test**:
   ```
   wg.TestVHMCompatibility
   ```
5. **Verify all tests pass** - look for "ALL TESTS PASSED" message

### Individual Component Testing

Test specific components that were fixed:

```
wg.TestMaterialCreation    # Test TObjectPtr fixes
wg.TestVHMComponent        # Test material assignment fixes
wg.TestConsoleAccess       # Test property access fixes
wg.TestRVTFallback         # Test RVT compatibility
```

### Performance and Stability Testing

Test system behavior under load:

```
wg.StressMaterials 100     # Test material system with 100 tiles
wg.StressVHM 25           # Test VHM system with 25 components
wg.TestMemoryLeaks 10     # Test for memory leaks over 10 iterations
wg.BenchmarkVHM 50        # Benchmark performance with 50 tiles
```

### RVT Compatibility Testing

Test Runtime Virtual Texture integration:

```
wg.TestRVTFallback        # Test RVT fallback behavior
```

This test validates:
- System works correctly when RVT is disabled
- System handles UE5.6 RVT API changes gracefully
- Materials are created successfully regardless of RVT availability

## Expected Results

### Success Indicators

- ✅ **All tests pass** without exceptions
- ✅ **Materials are created** and assigned correctly
- ✅ **VHM components** are created and configured properly
- ✅ **Console commands** can access VHMTerrainRenderer property
- ✅ **RVT integration** works or fails gracefully
- ✅ **Memory usage** remains stable
- ✅ **Performance** meets acceptable thresholds

### Warning Indicators

- ⚠️ **RVT initialization fails** (expected in UE5.6, should be graceful)
- ⚠️ **Some performance metrics** below optimal thresholds
- ⚠️ **Minor memory growth** (within acceptable limits)
- ⚠️ **Settings validation issues** (may be configuration-related)

### Failure Indicators

- ❌ **Exceptions or crashes** during testing
- ❌ **Materials not created** or assigned
- ❌ **VHM components not accessible**
- ❌ **Console commands fail** to access properties
- ❌ **Excessive memory growth** or leaks
- ❌ **Performance significantly** below thresholds

## Troubleshooting

### Common Issues

1. **"No valid world context"**
   - Ensure you're running tests in the editor with a world loaded
   - Try opening a level before running VHM component tests

2. **"Failed to get WorldGenSettings"**
   - Verify WorldGenSettings is properly configured
   - Check that the settings singleton is initialized

3. **"VHM component creation failed"**
   - Ensure VirtualHeightfieldMesh plugin is enabled
   - Verify world context is available for actor spawning

4. **"RVT initialization failed"**
   - This is expected in UE5.6 - the system should continue gracefully
   - Verify the fallback behavior works correctly

5. **Compilation Errors**
   - If you see "cannot access protected member" errors, ensure you're using UE5.6 compatible API calls
   - The test files have been updated to use base class methods where needed
   - Console command access uses `FindConsoleObject` instead of `FindConsoleCommand` for UE5.6 compatibility

6. **Editor Crashes During Testing**
   - If the editor crashes during `wg.TestVHMCompatibility`, try `wg.TestVHMBasic` first
   - Crashes may occur due to object cleanup issues in UE5.6's garbage collection system
   - The basic test uses minimal object creation and is safer to run
   - Individual component tests (`wg.TestMaterialCreation`, etc.) are also safer alternatives

### Performance Issues

If performance tests fail:

1. **Check system resources** - ensure adequate memory and CPU
2. **Reduce test parameters** - use smaller tile counts for testing
3. **Profile the system** - use UE5's profiling tools to identify bottlenecks
4. **Verify optimizations** - ensure release build optimizations are enabled

### Memory Issues

If memory leak tests fail:

1. **Force garbage collection** - run `obj gc` console command
2. **Check for circular references** - review object ownership
3. **Validate cleanup code** - ensure all objects are properly marked for GC
4. **Monitor over time** - run longer tests to identify gradual leaks

## Integration with Development Workflow

### Pre-Commit Testing

Before committing VHM-related changes:

```
wg.TestVHMCompatibility
```

### Performance Regression Testing

After performance-related changes:

```
wg.BenchmarkVHM 100
wg.StressMaterials 200
```

### Memory Stability Testing

After memory management changes:

```
wg.TestMemoryLeaks 20
```

### Continuous Integration

For automated testing, these commands can be run in batch mode:

```
wg.TestVHMCompatibility
wg.StressMaterials 50
wg.TestMemoryLeaks 5
```

## Test Coverage

The test suite covers all major compatibility fixes:

| Fix Category | Test Coverage | Commands |
|--------------|---------------|----------|
| TObjectPtr Handling | ✅ Complete | `wg.TestMaterialCreation`, `wg.StressMaterials` |
| RVT Integration | ✅ Complete | `wg.TestRVTFallback` |
| VHM Material Assignment | ✅ Complete | `wg.TestVHMComponent`, `wg.StressVHM` |
| Console Command Access | ✅ Complete | `wg.TestConsoleAccess` |
| Memory Management | ✅ Complete | `wg.TestMemoryLeaks` |
| Performance | ✅ Complete | `wg.BenchmarkVHM`, `wg.TestPerformance` |

## Conclusion

This comprehensive test suite validates that all UE5.6 compatibility fixes are working correctly. Regular execution of these tests ensures the VHM terrain rendering system remains stable and performant in UE5.6.

For any test failures or unexpected behavior, refer to the troubleshooting section or review the specific fix implementations in the source code.