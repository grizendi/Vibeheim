# World Generation Integration Testing Guide

## Overview

This document describes how to run integration tests for the World Generation system to verify all components work together correctly.

## Automated Integration Test

### Running the Test

1. Open the Unreal Editor with the Vibeheim project
2. Open the Output Log window (Window → Developer Tools → Output Log)
3. In the console, run: `wg.IntegrationTest`

### What the Test Covers

The automated integration test verifies:

1. **System Initialization** - All services (Noise, Climate, Heightfield, Biome, PCG, POI) initialize correctly
2. **Terrain Generation Consistency** - Same seed produces identical terrain
3. **Terrain Editing and Persistence** - Modifications are saved and restored correctly
4. **Biome System Integration** - Biomes are determined correctly based on climate data
5. **PCG Content Generation** - Vegetation and content spawn properly
6. **POI Generation and Placement** - Points of interest are placed in valid locations
7. **Performance Validation** - Generation times are within acceptable limits

### Expected Output

A successful test run will show:
```
=== WORLD GENERATION INTEGRATION TEST ===
--- Test 1: System Initialization ---
✓ All services initialized successfully
--- Test 2: Terrain Generation Consistency ---
✓ Terrain generation is deterministic
...
=== INTEGRATION TEST RESULTS ===
Tests Passed: 7/7
✓ ALL INTEGRATION TESTS PASSED
```

## Manual Testing Checklist

After the automated tests pass, perform these manual tests:

### 1. Performance and Visual Quality Test (60s fly-through)
- Enable streaming radius visualization: `wg.ShowStreaming 1`
- Enable performance HUD: `wg.ShowPerformance 1`
- Fly through the world for 60 seconds at various speeds
- **Verify**: Stable frame rate, no visual seams between tiles, smooth streaming

### 2. Vegetation Persistence Test
- Find an area with trees/vegetation
- Use terrain editing tools to "chop" trees (remove vegetation instances)
- Move away from the area (beyond streaming radius)
- Return to the same location
- **Verify**: Removed vegetation instances remain removed

### 3. Terrain Editing Persistence Test
- Use all 4 terrain brushes in different areas:
  - `wg.TerrainRaise 100 100 15 10` (raise terrain)
  - `wg.TerrainLower 200 200 15 8` (lower terrain)  
  - `wg.TerrainFlatten 300 300 20 0.5` (flatten terrain)
  - `wg.TerrainSmooth 400 400 12 0.3` (smooth terrain)
- Move away from edited areas
- Return to each location
- **Verify**: All terrain edits persist, vegetation cleared from edited areas

### 4. POI Placement and Stamping Test
- Generate POIs in different biomes: `wg.TestPOI 0 0 1` (Meadows), `wg.TestPOI 1 1 2` (Forest)
- Check POI locations: `wg.ListPOIs 0 0 1000`
- **Verify**: POIs appear in sensible locations (flat ground, appropriate biomes)
- **Verify**: Terrain stamping applied correctly around POIs

## Troubleshooting

### Common Issues

**Test Failure: "Failed to get WorldGen settings"**
- Ensure WorldGenSettings.json exists in Config/ directory
- Run `wg.ResetSettings` to create default settings

**Test Failure: Terrain persistence**
- Check that the project has write permissions to save terrain modifications
- Verify SaveGame directory exists and is writable

**Performance Issues**
- Adjust streaming radii: `wg.StreamRadius 6 4 2` (smaller values)
- Reduce vegetation density: `wg.VegDensity 0.5`
- Check PCG performance: `wg.ShowPCGDebug 1`

### Debug Commands

Useful commands for debugging integration issues:

- `wg.Help` - List all available commands
- `wg.ShowSettings` - Display current world generation settings
- `wg.BasicTest` - Test basic system functionality
- `wg.ValidateSettings` - Check for configuration issues
- `wg.ClearCache` - Clear all caches and force regeneration

## Requirements Verification

This integration test verifies the following requirements:

- **Requirement 5.1**: Dynamic streaming maintains target frame rates
- **Requirement 5.2**: Player modifications to heightfield are preserved  
- **Requirement 5.4**: Content loading operations maintain smooth gameplay

The test ensures all underlying systems (heightfield generation, biome determination, PCG content spawning, POI placement, and terrain editing) work together seamlessly to provide a stable, persistent world generation experience.