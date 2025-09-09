# WG_TestMap Creation Instructions

This file documents the required setup for the WG_TestMap that needs to be created manually in the Unreal Editor.

## Map Location
- Path: `/Game/Maps/WG_TestMap`
- File: `Content/Maps/WG_TestMap.umap`

## Required Setup

1. **Create New Level**
   - File → New Level → Empty Level
   - Save as `WG_TestMap` in `Content/Maps/`

2. **Add WorldGenManager Actor**
   - Place one `AWorldGenManager` actor in the scene
   - This actor will be automatically found by the test subsystem

3. **Basic Scene Setup**
   - Add a PlayerStart for testing
   - Optional: Add basic lighting (Directional Light, Sky Light)
   - Optional: Add a simple sky sphere for visual reference

## Console Commands Available

Once the map is created and loaded, these commands will be available:

- `wg.test` - Simple test to verify subsystem is working
- `wg.debug` - Debug world generation system initialization
- `wg.launch` - Initialize world generation systems
- `wg.testtile [x] [y]` - Test generation of a single tile (default: origin)
- `wg.cleanup` - Cleanup all VHM terrain actors (use if crashes occur)
- `wg.seed <value>` - Set the world generation seed
- `wg.radii <gen> <load> <active>` - Set streaming radii
- `wg.reset` - Reset world generation and clear persistence
- `wg.validate` - Validate VHM integration and test world setup
- `wg.gateA` - Execute Gate A test (orbit seam validation)

## Troubleshooting

If `wg.launch` is not working:

1. **Check if commands are available**: Run `wg.test` first
2. **Verify you're in the correct map**: Must be in `/Game/Maps/WG_TestMap`
3. **Debug system initialization**: Run `wg.debug` to see what services are missing
4. **Test single tile generation**: Run `wg.testtile` to test basic functionality
5. **Check the console output**: Look for specific initialization errors
6. **Try in PIE (Play in Editor)**: Commands work in both editor and game modes

## Debug Workflow

When you see errors after `wg.launch`:

1. Run `wg.debug` - Shows which services initialized successfully
2. Run `wg.testtile` - Tests basic tile generation
3. If you get crashes about "Cannot generate unique name", run `wg.cleanup` first
4. Check console for specific error messages about missing services
5. Look for "CreateHeightTexture failed" or "Rendering system not ready" errors

## Crash Recovery

If you get the "Cannot generate unique name for VHMTerrain" crash:

1. **Stop PIE** and return to editor
2. **Run `wg.cleanup`** to remove all VHM actors
3. **Restart PIE** and try `wg.launch` again

## Fixed Streaming Configuration

The test world uses fixed streaming radii for consistent testing:
- **Generate Radius**: 9 tiles
- **Load Radius**: 5 tiles  
- **Active Radius**: 3 tiles
- **Hysteresis**: Tiles kept alive 1 ring beyond Active (4 tiles) to prevent ping-pong

## VHM Integration Features

- **Automatic WorldGenManager Spawning**: If no WorldGenManager exists, one will be spawned automatically
- **Boundary Stitching**: Enabled by default to prevent seams between tiles
- **Flat Meadow Fallback**: If VHM component creation fails, falls back to flat meadow terrain
- **Tile Streaming Integration**: VHM components created/destroyed based on Active radius
- **Performance Monitoring**: Tracks VHM mesh generation times and performance metrics

## Error Codes

- **VHM001**: VHM component creation failed, using flat meadow fallback

## Gate A Testing

**Gate A** validates seamless terrain rendering across tile boundaries:

1. **Run Gate A Test**: Execute `wg.gateA` in console
2. **Expected Results**:
   - ✓ Boundary stitching enabled
   - ✓ Streaming radii configured (9/5/3)
   - ✓ 3x3 tile grid generated around origin
   - ✓ VHM components created or fallback applied
   - ✓ Performance within acceptable limits

3. **Manual Validation**:
   - Orbit camera around tile boundaries at different distances
   - Verify no visible gaps between tiles
   - Check for smooth lighting transitions
   - Ensure no foliage popping at LOD blend bands

## Notes

- Commands only work when `/Game/Maps/WG_TestMap` is the active map
- The UWorldGenTestSubsystem will automatically register console commands
- The UWorldGenSeedSubsystem provides authoritative seed management
- VHM components are only created for tiles within the Active radius (3 tiles)
- Boundary stitching prevents visual seams at tile borders
- Gate A test generates tiles around origin for seam validation