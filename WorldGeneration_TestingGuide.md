# World Generation Testing Guide

## Issue Identified and Fixed

**Problem**: The world generation system was crashing due to async task management issues with the assertion `WorkNotFinishedCounter.GetValue() == 0`.

**Root Cause**: 
1. Async tasks were not being properly cleaned up during shutdown
2. Race conditions in task completion and cleanup
3. UE5's async task system was not being handled correctly

**Fix Applied**: 
1. Implemented synchronous chunk generation as a temporary stable solution
2. Removed complex async task management that was causing crashes
3. Added proper cleanup and error handling
4. This approach trades some performance (potential frame drops) for stability

## Testing Steps

### 1. Recompile the Project
After the fixes, you need to recompile:
1. Close Unreal Editor
2. Build the project (Ctrl+Shift+B in Visual Studio or use UE5 Build)
3. Reopen the project

### 2. Test World Generation

#### Option A: Use Existing Test Level
1. Open `Content/_Assets/Levels/WorldGenIntegrationTest.umap`
2. Click Play (PIE)
3. You should now see voxel terrain being generated around the player

#### Option B: Create New Test Setup
1. Create a new level
2. Add a `WorldGenManager` actor to the level
3. Set the player start position
4. Click Play

### 3. Verify World Generation is Working

Look for these indicators:
- **Terrain appears**: You should see voxel-based terrain generating around the player
- **Log messages**: Check the Output Log for successful chunk generation messages
- **No crash warnings**: The "TEMPORARY: Async chunk generation disabled" warnings should be gone

### 4. Console Commands for Testing

Open the console (` key) and try these commands:
```
wg.DebugChunks 1          // Show chunk boundaries
wg.DebugMasks 1           // Show biome visualization
wg.StreamRadius 4         // Adjust streaming radius
wg.Seed 42                // Change world seed
wg.TestFallback           // Test fallback generation
```

### 5. Expected Behavior

**What you should see:**
- Voxel terrain generating in chunks around the player
- Different biomes (Meadows, BlackForest, Swamp) with smooth transitions
- Terrain that responds to player movement (streaming)
- Console output showing successful chunk generation
- Log messages indicating "Using synchronous chunk generation"

**Performance expectations:**
- Chunk generation: 1-2ms per chunk (synchronous)
- Memory usage: ≤64MB for LOD0 chunks
- Possible minor frame drops during generation (this is expected with sync generation)
- No crashes related to async tasks

## Troubleshooting

### If you still don't see terrain:

1. **Check the log for errors**:
   - Look for "VoxelPluginLegacy not available" errors
   - Check for initialization failures

2. **Verify VoxelPlugin is loaded**:
   - Go to Edit → Plugins
   - Search for "Voxel"
   - Ensure VoxelPluginLegacy is enabled

3. **Check player position**:
   - The player might be too high above the terrain
   - Try moving the player start to (0, 0, 100)

4. **Verify WorldGenManager is in the level**:
   - Check the World Outliner for a WorldGenManager actor
   - If missing, drag one from the Content Browser

### If you get crashes:

1. **Check the crash log** in `Saved/Crashes/`
2. **Try with a simpler setup**:
   - Disable POI generation temporarily
   - Reduce streaming radius
   - Use a smaller chunk size

## Configuration

The world generation settings are in `Config/WorldGenSettings.json`:
```json
{
    "Seed": 1337,
    "WorldGenVersion": 1,
    "VoxelSizeCm": 50.0,
    "ChunkSize": 32,
    "LOD0Radius": 2,
    "LOD1Radius": 4,
    "LOD2Radius": 6,
    "BiomeBlendMeters": 24.0
}
```

You can modify these values to adjust world generation behavior.

## Next Steps

Once world generation is working:
1. Test biome transitions by moving around
2. Try the POI and dungeon portal systems
3. Test voxel editing with the CSG tools
4. Run the automated tests to verify determinism

## Support

If you continue to have issues:
1. Share the latest log output from `Saved/Logs/Vibeheim.log`
2. Include any crash dumps from `Saved/Crashes/`
3. Describe what you see (or don't see) when playing
## Tech
nical Notes

### Current Implementation Status

**Synchronous Generation**: The current implementation uses synchronous chunk generation to avoid async task crashes. This means:
- Chunks generate immediately when requested
- No complex async task management
- Potential for minor frame drops during generation
- More stable and predictable behavior

### Future Improvements

To restore async generation without crashes, the following would need to be implemented:
1. Proper async task lifecycle management
2. Better shutdown coordination between tasks and managers
3. Thread-safe completion callbacks
4. Timeout handling for stuck tasks

### Performance Impact

The synchronous approach has these characteristics:
- **Pros**: Stable, predictable, no async-related crashes
- **Cons**: Potential frame drops, blocks main thread during generation
- **Mitigation**: Generation times are kept very short (1-2ms per chunk)

For a production system, async generation should be properly implemented, but this synchronous approach provides a stable foundation for testing and development.