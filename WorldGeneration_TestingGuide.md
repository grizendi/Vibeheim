# World Generation System Testing Guide

## Overview

This guide provides comprehensive step-by-step instructions for testing all features of the voxel world generation system built on the LegacyVoxelPlugin. The system includes procedural terrain, multiple biomes, points of interest (POIs), dungeon portals, and chunk streaming.

## Prerequisites

- Unreal Engine 5 project with Vibeheim loaded
- VoxelPluginLegacy properly installed and configured
- WorldGenManager placed in your test level
- Integration test level available at `Content/_Assets/Levels/WorldGenIntegrationTest.umap`

## Quick Start - Automated Testing

### Option 1: Run All Tests via Scripts

**Windows:**
```batch
# Navigate to project root and run
Scripts\RunIntegrationTests.bat
```

**Linux/Mac:**
```bash
# Navigate to project root and run
Scripts/RunIntegrationTests.sh
```

### Option 2: Use Integration Test Level

1. Open `Content/_Assets/Levels/WorldGenIntegrationTest.umap`
2. Click Play in Editor
3. Tests will run automatically if `bAutoTestOnBeginPlay = true` on the WorldGenIntegrationTestController
4. Check Output Log for results

## Manual Testing - Step by Step

### 1. Basic System Initialization

#### Test World Generation Manager Setup
1. **Open any level** with WorldGenManager placed
2. **Play in Editor**
3. **Open Console** (` key)
4. **Run:** `wg.ShowStreamingStats`

**Expected Results:**
- ✅ Seed value displayed (default: 1337)
- ✅ WorldGenManager shows "Ready: Yes"
- ✅ Player anchor location displayed
- ✅ Loaded chunks count > 0

#### Test Console Commands Registration
1. **In Console, type:** `wg.` and press Tab
2. **Verify available commands:**
   - `wg.Seed`
   - `wg.StreamRadius`
   - `wg.DebugMasks`
   - `wg.DebugChunks`
   - `wg.RunIntegrationTests`
   - `wg.TestBiomeTransitions`
   - `wg.TestPOIPortalSystem`
   - `wg.ValidateVisualQuality`

### 2. Terrain Generation Testing

#### Test Basic Terrain Generation
1. **Set deterministic seed:** `wg.Seed 42`
2. **Restart level** (Stop and Play again)
3. **Fly around the world** using F8 (Eject from Pawn)
4. **Look for:**
   - ✅ Continuous voxel terrain without gaps
   - ✅ Smooth chunk boundaries (no visible seams)
   - ✅ Varied terrain height and features

#### Test Chunk Streaming
1. **Enable chunk debug:** `wg.DebugChunks true`
2. **Fly rapidly in different directions**
3. **Observe:**
   - ✅ Chunk boundaries visualized as wireframes
   - ✅ New chunks loading ahead of player
   - ✅ Distant chunks unloading
   - ✅ No stuttering during rapid movement

#### Test Fallback Generation
1. **Test fallback system:** `wg.TestFallbackGeneration 5 5 0`
2. **Check Output Log for:**
   - ✅ "Fallback generation test result: Success"
   - ✅ Structured error logging entries

### 3. Biome System Testing

#### Test Biome Visualization
1. **Enable biome debug:** `wg.DebugMasks true`
2. **Fly around the world**
3. **Look for:**
   - ✅ Different colored overlays representing biomes
   - ✅ Smooth transitions between biome colors
   - ✅ At least 2-3 distinct biome types visible

#### Test Biome Transitions
1. **Test at origin:** `wg.TestBiomeTransitions 0 0 500 20`
2. **Test at different locations:** `wg.TestBiomeTransitions 2000 1500 300 15`
3. **Check Output Log for:**
   - ✅ "All biome transitions are smooth" message
   - ✅ No "Rough transition detected" warnings

#### Manual Biome Inspection
1. **Fly to different terrain areas**
2. **Look for distinct biome characteristics:**
   - **Meadows:** Rolling hills, gentle terrain
   - **BlackForest:** Elevated, dense terrain features
   - **Swamp:** Low-lying, flat wetland areas
3. **Verify smooth blending** at biome boundaries

### 4. Points of Interest (POI) Testing

#### Test POI Generation
1. **Generate POIs for specific chunk:** `wg.TestPOIPortalSystem 0 0 0`
2. **Try different chunks:** `wg.TestPOIPortalSystem 3 2 0`
3. **Check Output Log for:**
   - ✅ "Generated X POI placement results"
   - ✅ "POI and portal system test PASSED"
   - ✅ Valid placement statistics

#### Manual POI Discovery
1. **Fly around the world systematically**
2. **Look for pre-built structures** integrated into terrain
3. **Verify POI characteristics:**
   - ✅ Structures properly embedded in terrain
   - ✅ No floating or underground POIs
   - ✅ Minimum 150m spacing between POIs
   - ✅ POIs placed on appropriate slopes (<20 degrees)

#### Test POI Logic Systems
1. **Approach discovered POIs**
2. **Look for interactive elements** (if implemented)
3. **Verify POI integration** with surrounding terrain

### 5. Dungeon Portal System Testing

#### Test Portal Generation
1. **Run portal test:** `wg.TestPOIPortalSystem 1 1 0`
2. **Check for portal-specific results** in Output Log
3. **Verify:**
   - ✅ Portal placement results generated
   - ✅ Portal instances found in chunks
   - ✅ Valid portal statistics

#### Manual Portal Discovery
1. **Search for portal structures** while exploring
2. **Identify portal entry points** (distinct from regular POIs)
3. **Test portal interaction:**
   - ✅ Portals are accessible
   - ✅ Portal activation (if implemented)
   - ✅ Transportation to dungeon areas (if implemented)

### 6. Performance and Streaming Testing

#### Test Streaming Performance
1. **Monitor streaming stats:** `wg.ShowStreamingStats`
2. **Fly rapidly in different directions**
3. **Check for:**
   - ✅ Average generation time < 5ms
   - ✅ P95 generation time reasonable
   - ✅ Loaded chunks count stays reasonable
   - ✅ No excessive generating chunks count

#### Test Memory Usage
1. **Fly extensively around large world areas**
2. **Monitor Unreal's Stat Memory** (type `stat memory` in console)
3. **Verify:**
   - ✅ Memory usage stays within reasonable bounds
   - ✅ No memory leaks during extended play
   - ✅ Chunks unload properly when distant

#### Test Error Handling
1. **Test error logging:** `wg.TestErrorLogging`
2. **Check Output Log for:**
   - ✅ Structured error logging test completion
   - ✅ [STRUCTURED_ERROR] entries with proper formatting

### 7. Visual Quality Testing

#### Test Visual Quality at Various Locations
1. **Test at origin:** `wg.ValidateVisualQuality 0 0 100`
2. **Test at biome boundaries:** `wg.ValidateVisualQuality 1000 500 100`
3. **Test in different biomes:** `wg.ValidateVisualQuality -2000 1000 100`
4. **Check for:**
   - ✅ "Visual quality validation PASSED"
   - ✅ Valid primary biome (not "None")
   - ✅ Proper blend weight totals

#### Manual Visual Inspection
1. **Fly around at various altitudes**
2. **Look for visual issues:**
   - ❌ Terrain gaps or holes
   - ❌ Floating geometry
   - ❌ Abrupt biome transitions
   - ❌ Texture streaming issues
   - ❌ LOD popping

### 8. Comprehensive Integration Testing

#### Run Full Integration Test Suite
1. **Use console command:** `wg.RunIntegrationTests`
2. **Wait for completion** (30-60 seconds)
3. **Check Output Log for:**
   - ✅ "All integration tests PASSED"
   - ✅ Test statistics (passed/failed/total)
   - ✅ Individual test results

#### Test Different Seeds
1. **Change seed:** `wg.Seed 12345`
2. **Restart level**
3. **Verify:**
   - ✅ Different terrain layout
   - ✅ Different POI/portal placements
   - ✅ Consistent generation with same seed

#### Test Determinism
1. **Set seed:** `wg.Seed 999`
2. **Note terrain features** at specific coordinates
3. **Restart level** with same seed
4. **Verify:**
   - ✅ Identical terrain at same coordinates
   - ✅ Same POI placements
   - ✅ Consistent biome distribution

## Advanced Testing Scenarios

### Network/Multiplayer Testing (if applicable)
1. **Test with multiple players** (if networking implemented)
2. **Verify synchronized world generation**
3. **Test chunk streaming** with multiple viewpoints

### Performance Stress Testing
1. **Set high streaming radius:** `wg.StreamRadius 10`
2. **Fly rapidly in complex terrain**
3. **Monitor frame rate** and memory usage
4. **Test with multiple players** if applicable

### Edge Case Testing
1. **Test at world boundaries** (very high X/Y coordinates)
2. **Test rapid seed changes**
3. **Test with extreme settings** (very small/large chunk sizes)

## Troubleshooting Common Issues

### "WorldGenManager not found"
- Ensure WorldGenManager is placed in the level
- Check that the manager is properly initialized
- Verify `IsWorldGenReady()` returns true

### Terrain Generation Failures
- Check VoxelPluginLegacy installation
- Verify plugin compatibility
- Check for error messages in Output Log
- Test fallback generation system

### Performance Issues
- Reduce streaming radius: `wg.StreamRadius 4`
- Check memory usage with `stat memory`
- Verify LOD settings are appropriate
- Test on different hardware configurations

### Visual Quality Issues
- Enable debug visualization: `wg.DebugMasks true`
- Check biome transition smoothness
- Verify blend weight calculations
- Test at different world locations

## Expected Test Results Summary

### ✅ Successful Test Indicators
- All console commands execute without errors
- Streaming statistics show reasonable performance
- Biome transitions are smooth (no rough transition warnings)
- POI and portal systems generate valid placements
- Visual quality validation passes at multiple locations
- Integration tests report "PASSED" status
- Terrain appears continuous without gaps or seams
- Memory usage remains stable during extended play

### ❌ Failure Indicators to Investigate
- Console commands return "not found" errors
- Rough biome transitions detected
- POI/portal placement failures
- Visual quality validation failures
- Integration tests report "FAILED" status
- Terrain gaps, holes, or floating geometry
- Memory leaks or excessive memory usage
- Frame rate drops during chunk streaming

## Performance Benchmarks

### Target Performance Metrics
- **Average chunk generation time:** < 5ms
- **P95 chunk generation time:** < 20ms
- **Memory usage:** < 64MB within streaming radius
- **Triangle count:** < 8000 per LOD0 chunk
- **Frame rate:** Stable during rapid movement

### Monitoring Commands
- `wg.ShowStreamingStats` - View performance metrics
- `stat memory` - Monitor memory usage
- `stat fps` - Monitor frame rate
- `stat unit` - Monitor frame timing

This comprehensive testing guide covers all major features of your world generation system. Follow these steps systematically to ensure all components are working correctly and meeting performance requirements.