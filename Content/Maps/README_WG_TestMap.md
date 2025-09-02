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

- `wg.launch` - Initialize world generation systems
- `wg.seed <value>` - Set the world generation seed
- `wg.radii <gen> <load> <active>` - Set streaming radii
- `wg.reset` - Reset world generation and clear persistence

## Notes

- Commands only work when `/Game/Maps/WG_TestMap` is the active map
- The UWorldGenTestSubsystem will automatically register console commands
- The UWorldGenSeedSubsystem provides authoritative seed management