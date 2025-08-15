# World Generation System Setup and Testing Guide

## Overview

This guide provides step-by-step instructions for setting up and manually testing the voxel world generation system built on the LegacyVoxelPlugin. The system includes procedural terrain, multiple biomes, points of interest (POIs), dungeon portals, and chunk streaming.

## Prerequisites

- Unreal Engine 5 project with Vibeheim loaded
- VoxelPluginLegacy properly installed and configured
- Basic understanding of Unreal Engine Blueprint and Level editing

## Implementation Status Note

This guide covers testing for the world generation system. Some advanced features like detailed biome visualization, POI placement, and dungeon portals may not be fully implemented yet. The guide provides frameworks for testing these features when they become available, while focusing on the core functionality that is currently working:

- ✅ **Core terrain generation** - Fully implemented
- ✅ **Chunk streaming** - Fully implemented  
- ✅ **Console commands** - Basic commands implemented
- ⚠️ **Biome system** - Framework exists, visualization may be limited
- ⚠️ **POI system** - Framework exists, may not be fully functional
- ⚠️ **Portal system** - Framework exists, may not be fully functional

## Step 1: Setting Up Your Test Level

### Create a New Level for Testing

1. **Create New Level:**
   - In Content Browser, right-click in your desired folder
   - Select **Miscellaneous > Level**
   - Name it `WorldGenTestLevel`

2. **Set Up Basic Level Components:**
   - Delete the default floor and lighting if present
   - Add a **Directional Light** for basic lighting
   - Add a **Sky Atmosphere** for realistic sky
   - Add an **Exponential Height Fog** for atmosphere (optional)

### Place the WorldGenManager

1. **Add WorldGenManager to Level:**
   - In the **Place Actors** panel, search for "WorldGenManager"
   - Drag **WorldGenManager** into your level
   - Position it at the origin (0, 0, 0)

2. **Configure WorldGenManager:**
   - Select the WorldGenManager in the level
   - In the **Details** panel, configure these settings:
     - **Auto Track Player**: Enable this to automatically follow the player
     - **Player Tracking Interval**: Set to 1.0 seconds
     - **Enable Debug Logging**: Enable for detailed logs

### Set Up Player Start and Camera

1. **Add Player Start:**
   - Search for "Player Start" in Place Actors
   - Place it at a reasonable height above the terrain (e.g., 0, 0, 500)

2. **Configure Game Mode (Optional):**
   - Create a custom Game Mode Blueprint if you want specific player behavior
   - Set the Default Pawn Class to your preferred character

## Step 2: Basic System Testing

### Test World Generation Manager Setup
1. **Open your test level**
2. **Click Play in Editor**
3. **Open Console** (press ` key)
4. **Run:** `wg.ShowStreamingStats`

**Expected Results:**
- ✅ Seed value displayed (default: 1337)
- ✅ WorldGenManager shows "Ready: Yes"
- ✅ Player anchor location displayed
- ✅ Loaded chunks count > 0

### Test Available Console Commands
1. **In Console, type:** `wg.` and press Tab
2. **Verify available commands:**
   - `wg.Seed` - Change world generation seed
   - `wg.StreamRadius` - Adjust streaming radius
   - `wg.DebugMasks` - Toggle biome visualization
   - `wg.DebugChunks` - Show chunk boundaries
   - `wg.ShowStreamingStats` - Display streaming statistics
   - `wg.TestErrorLogging` - Test error logging system
   - `wg.TestFallbackGeneration` - Test fallback terrain generation

## Step 3: Terrain Generation Testing

### Test Basic Terrain Generation
1. **Set deterministic seed:** `wg.Seed 42`
2. **Stop and restart Play in Editor**
3. **Switch to free camera mode:** Press F8 (Eject from Pawn)
4. **Fly around the world** using WASD + mouse
5. **Look for:**
   - ✅ Continuous voxel terrain without gaps
   - ✅ Smooth chunk boundaries (no visible seams)
   - ✅ Varied terrain height and features

### Test Chunk Streaming Visualization
1. **Enable chunk debug visualization:** `wg.DebugChunks true`
2. **Fly around in different directions**
3. **Observe:**
   - ✅ Chunk boundaries visualized as wireframes
   - ✅ New chunks loading ahead of your camera
   - ✅ Distant chunks unloading behind you
   - ✅ Smooth movement without stuttering

### Test Different Seeds
1. **Try different seeds:**
   - `wg.Seed 12345`
   - `wg.Seed 999`
   - `wg.Seed 0`
2. **Restart the level each time** (Stop and Play)
3. **Verify each seed produces different terrain layouts**

### Test Fallback Generation (Optional)
1. **Test the fallback system:** `wg.TestFallbackGeneration 5 5 0`
2. **Check Output Log for:**
   - ✅ "Fallback generation test result: Success"
   - ✅ Structured error logging entries

## Step 4: Biome System Testing

### Enable Biome Visualization
1. **Enable biome debug overlays:** `wg.DebugMasks true`
2. **Fly around the world in free camera mode**
3. **Look for:**
   - ✅ Different colored overlays representing biomes (if implemented)
   - ✅ Smooth transitions between biome colors
   - ✅ Distinct terrain characteristics in different areas

**Note:** The biome visualization may not be fully implemented yet. If you don't see colored overlays, focus on observing terrain variations that suggest different biome types.

### Manual Biome Inspection
1. **Fly to different terrain areas systematically**
2. **Look for terrain variations that suggest different biomes:**
   - **Flat areas:** Could represent meadows or plains
   - **Elevated areas:** Could represent hills or forest regions
   - **Low-lying areas:** Could represent swamps or valleys
3. **Check terrain transitions:**
   - ✅ Smooth elevation changes between different terrain types
   - ✅ Natural-looking terrain features
   - ✅ No abrupt or unnatural terrain boundaries

### Create Biome Test Locations Blueprint

1. **Create a new Blueprint:**
   - Right-click in Content Browser
   - Select **Blueprint Class > Actor**
   - Name it `BP_BiomeTestMarkers`

2. **Add Components:**
   - Add multiple **Static Mesh Components**
   - Use simple shapes (cubes or spheres) as markers
   - Position them at different terrain locations
   - Set different colors for each terrain type you observe

3. **Place in Level:**
   - Drag the Blueprint into your test level
   - Position markers at terrain boundaries for easy reference

## Step 5: Points of Interest (POI) Setup and Testing

**Note:** The POI system may not be fully implemented yet. This section provides a framework for testing POI functionality when it becomes available.

### Create POI Visualization Blueprint

1. **Create POI Marker Blueprint:**
   - Create new **Blueprint Class > Actor**
   - Name it `BP_POIMarker`
   - Add a **Static Mesh Component** (use a distinctive shape like a crystal or tower)
   - Add a **Text Render Component** to display POI information
   - Set a bright, visible material

2. **Configure POI Marker:**
   - In the Blueprint, add variables for POI Type and Description
   - Set the Text Render to display the POI information
   - Make it visible from a distance (large scale, bright colors)

### Manual POI Discovery and Testing

1. **Explore the world systematically:**
   - Fly in a grid pattern across different terrain areas
   - Look for any generated structures or distinctive features
   - Note their locations and characteristics

2. **If POIs are implemented, verify placement quality:**
   - ✅ POIs are properly embedded in terrain (not floating)
   - ✅ POIs are not buried underground
   - ✅ POIs maintain reasonable spacing
   - ✅ POIs are placed on appropriate terrain slopes
   - ✅ POIs fit naturally within their terrain context

3. **Document Interesting Locations:**
   - Create a simple text file or spreadsheet
   - Record coordinates and terrain characteristics
   - Note any unusual terrain features for future POI placement

### Create Exploration Level Sequence

1. **Create a Level Sequence:**
   - In Content Browser, create **Cinematics > Level Sequence**
   - Name it `Exploration_TestSequence`

2. **Set up Camera Path:**
   - Add a **Cine Camera Actor** to your level
   - In the Level Sequence, add the camera
   - Create a path that visits interesting terrain locations
   - Set appropriate timing for inspection

3. **Use for Consistent Testing:**
   - Play the sequence to systematically explore the world
   - Pause at interesting locations for detailed inspection

## Step 6: Dungeon Portal System Setup and Testing

**Note:** The dungeon portal system may not be fully implemented yet. This section provides a framework for testing portal functionality when it becomes available.

### Create Portal Visualization Blueprint

1. **Create Portal Marker Blueprint:**
   - Create new **Blueprint Class > Actor**
   - Name it `BP_PortalMarker`
   - Add a **Static Mesh Component** (use a portal-like shape or archway)
   - Add particle effects or glowing materials to make it distinctive
   - Add a **Box Collision Component** for interaction detection

2. **Add Portal Interaction:**
   - In the Blueprint Event Graph, add **OnComponentBeginOverlap** event
   - Connect it to a **Print String** node to log portal interactions
   - Add sound effects or visual feedback when approached

### Manual Portal Discovery and Testing

1. **Search for distinctive terrain features:**
   - Look for unique terrain formations that could house portals
   - Check both surface and underground areas (if accessible)
   - Note locations that would be suitable for portal placement

2. **If portals are implemented, test accessibility:**
   - ✅ Portals are accessible to players
   - ✅ Portal entrances are not blocked by terrain
   - ✅ Portals have clear visual indicators
   - ✅ Portals are placed in appropriate locations (not on steep slopes)

3. **Document Potential Portal Locations:**
   - Record coordinates of interesting terrain features
   - Note the terrain context for each location
   - Identify locations that would be suitable for rare portal placement

### Create Location Testing Widget

1. **Create UI Widget Blueprint:**
   - Create **Blueprint Class > Widget Blueprint**
   - Name it `WBP_LocationTester`

2. **Add UI Elements:**
   - Add **Text** blocks to display location information
   - Add **Button** to teleport to marked locations
   - Add **Scroll Box** to list all discovered interesting locations

3. **Implement Location Teleportation:**
   - In the widget Blueprint, add functions to teleport the player
   - Use **Set Actor Location** to move to marked coordinates
   - Add this widget to your test level for easy location navigation

## Step 7: Performance and Streaming Testing

### Monitor Streaming Performance

1. **Set up Performance Monitoring:**
   - Open console and run: `wg.ShowStreamingStats`
   - Enable additional stats: `stat fps` and `stat unit`
   - Keep these stats visible during testing

2. **Test Streaming Under Load:**
   - Fly rapidly in different directions
   - Make sudden direction changes
   - Move between different biomes quickly

3. **Check Performance Metrics:**
   - ✅ Frame rate remains stable (>30 FPS minimum)
   - ✅ Loaded chunks count stays reasonable (under 100)
   - ✅ No excessive generating chunks count (under 10)
   - ✅ No visible stuttering during movement

### Create Performance Testing Blueprint

1. **Create Performance Monitor Blueprint:**
   - Create **Blueprint Class > Actor**
   - Name it `BP_PerformanceMonitor`

2. **Add Performance Tracking:**
   - Add **Text Render Components** to display real-time stats
   - Use **Event Tick** to update performance metrics
   - Display FPS, memory usage, and chunk counts

3. **Add to Test Level:**
   - Place the performance monitor in your test level
   - Position it where it's easily visible during testing

### Memory Usage Testing

1. **Monitor Memory Usage:**
   - In console, type: `stat memory`
   - Watch memory usage during extended play sessions
   - Note any significant increases over time

2. **Test Extended Play Sessions:**
   - Play for 15-30 minutes continuously
   - Fly around extensively to load many chunks
   - Monitor for memory leaks or excessive usage

3. **Verify Chunk Unloading:**
   - Enable chunk debug: `wg.DebugChunks true`
   - Fly away from an area and return
   - Verify chunks unload and reload properly

## Step 8: Visual Quality Testing and Setup

### Create Visual Quality Checklist Blueprint

1. **Create Quality Checklist Widget:**
   - Create **Blueprint Class > Widget Blueprint**
   - Name it `WBP_QualityChecklist`

2. **Add Checklist Items:**
   - Add **Check Box** widgets for each quality criterion
   - Add **Text** labels describing what to check
   - Include items like "No terrain gaps", "Smooth biome transitions", etc.

3. **Add Screenshot Functionality:**
   - Add **Button** to take screenshots at current location
   - Use **Execute Console Command** node with "shot" command
   - Automatically name screenshots with coordinates

### Manual Visual Inspection Process

1. **Systematic Visual Inspection:**
   - Fly around at various altitudes (ground level, 500m, 1000m)
   - Check different times of day (if day/night cycle exists)
   - Inspect from multiple angles

2. **Look for Visual Issues:**
   - ❌ Terrain gaps or holes in the voxel mesh
   - ❌ Floating geometry or disconnected terrain pieces
   - ❌ Abrupt biome transitions (hard color/texture changes)
   - ❌ Texture streaming issues or blurry textures
   - ❌ LOD popping (sudden geometry changes)
   - ❌ Z-fighting or flickering surfaces

3. **Document Issues:**
   - Take screenshots of any problems found
   - Note the coordinates where issues occur
   - Record the seed value and settings used

### Create Visual Reference Points

1. **Set up Reference Locations:**
   - Place **Target Point** actors at key locations
   - Name them descriptively (e.g., "BiomeBoundary_Meadow_Forest")
   - Use these for consistent testing across different seeds

2. **Create Reference Screenshot Blueprint:**
   - Create a Blueprint that teleports to each reference point
   - Automatically takes screenshots from multiple angles
   - Saves screenshots with consistent naming convention

### Lighting and Atmosphere Setup

1. **Configure Proper Lighting:**
   - Ensure **Directional Light** is properly configured
   - Set appropriate sun angle and intensity
   - Enable **Atmosphere Sun Light** if using Sky Atmosphere

2. **Test Different Lighting Conditions:**
   - Test during different times of day
   - Check how terrain looks in shadows vs. direct light
   - Verify biome colors are visible under various lighting

## Step 9: Comprehensive Manual Testing

### Create Master Test Level

1. **Set up Comprehensive Test Level:**
   - Create a new level named `WorldGen_MasterTest`
   - Include all the Blueprints created in previous steps:
     - BP_BiomeTestMarkers
     - BP_POIMarker
     - BP_PortalMarker
     - BP_PerformanceMonitor
     - WBP_QualityChecklist

2. **Add Test Control Panel:**
   - Create a **Widget Blueprint** named `WBP_TestControlPanel`
   - Add buttons for common test functions:
     - Change seed values
     - Toggle debug visualizations
     - Teleport to test locations
     - Reset performance counters

### Test Different Seeds Systematically

1. **Create Seed Test Protocol:**
   - Test with seeds: 42, 12345, 999, 0, 1337 (default)
   - For each seed, restart the level completely
   - Document the unique characteristics of each seed

2. **Verify Determinism:**
   - Set seed to 999: `wg.Seed 999`
   - Note terrain features at coordinates (0,0), (1000,1000), (-500,500)
   - Restart level with same seed
   - Verify identical terrain at same coordinates

3. **Document Seed Variations:**
   - Create a spreadsheet or document
   - Record notable features for each seed
   - Include screenshots of key areas

### Create Final Validation Checklist

1. **System Initialization Checklist:**
   - [ ] WorldGenManager loads without errors
   - [ ] Console commands respond correctly
   - [ ] Player anchor tracking works
   - [ ] Streaming stats display properly

2. **Terrain Generation Checklist:**
   - [ ] Terrain generates continuously without gaps
   - [ ] Chunk boundaries are seamless
   - [ ] Different seeds produce different terrain
   - [ ] Same seed produces identical terrain

3. **Biome System Checklist:**
   - [ ] Terrain shows variation suggesting different biome types
   - [ ] Terrain transitions are smooth and natural
   - [ ] Debug visualization works (if implemented)
   - [ ] Terrain characteristics match expected patterns

4. **POI System Checklist (if implemented):**
   - [ ] POIs appear in appropriate locations
   - [ ] POIs are properly embedded in terrain
   - [ ] POI spacing is reasonable
   - [ ] POIs fit their terrain context

5. **Portal System Checklist (if implemented):**
   - [ ] Portals are distinct from regular POIs
   - [ ] Portals are accessible
   - [ ] Portal placement makes sense
   - [ ] Portal spacing is appropriate

6. **Performance Checklist:**
   - [ ] Frame rate remains stable during movement
   - [ ] Memory usage stays reasonable
   - [ ] Chunks load and unload properly
   - [ ] No significant stuttering or hitches

## Step 10: Advanced Testing Scenarios

### Stress Testing Setup

1. **Create Stress Test Blueprint:**
   - Create **Blueprint Class > Pawn**
   - Name it `BP_StressTester`
   - Add automatic movement patterns (circles, figure-8s, random)
   - Include speed controls and path recording

2. **High Load Testing:**
   - Set high streaming radius: `wg.StreamRadius 10`
   - Use the stress test pawn to move rapidly
   - Monitor performance during extended high-load periods

3. **Edge Case Testing:**
   - Test at extreme coordinates (±50000, ±50000)
   - Test rapid seed changes (change seed every 30 seconds)
   - Test with multiple WorldGenManagers (if applicable)

### Multiplayer Testing Setup (if applicable)

1. **Set up Multiplayer Test:**
   - Create a multiplayer session with 2-4 players
   - Have players spread out across different areas
   - Test synchronized world generation

2. **Verify Multiplayer Consistency:**
   - Ensure all players see identical terrain
   - Check that POIs and portals appear in same locations
   - Test chunk streaming with multiple viewpoints

## Troubleshooting Common Issues

### "WorldGenManager not found"
- Ensure WorldGenManager is placed in the level
- Check that the manager is properly initialized in BeginPlay
- Verify the manager appears in the World Outliner
- Check the Output Log for initialization errors

### Terrain Generation Issues
- Verify VoxelPluginLegacy is properly installed and enabled
- Check Plugin settings in Project Settings
- Look for error messages in Output Log during terrain generation
- Ensure the level has proper lighting setup

### Performance Issues
- Reduce streaming radius: `wg.StreamRadius 4`
- Check memory usage with `stat memory`
- Monitor frame timing with `stat unit`
- Test on different hardware configurations
- Verify LOD settings are appropriate for your hardware

### Visual Quality Issues
- Enable debug visualization: `wg.DebugMasks true`
- Check lighting setup (Directional Light, Sky Atmosphere)
- Verify material assignments on voxel meshes
- Test at different times of day
- Check for proper texture streaming settings

### Blueprint Issues
- Ensure all custom Blueprints compile without errors
- Check that Blueprint references are properly set
- Verify component hierarchies in Blueprint editor
- Test Blueprint functionality in isolation

## Expected Results Summary

### ✅ Successful Test Indicators
- WorldGenManager initializes without errors
- Console commands execute and provide expected output
- Terrain generates continuously without visible gaps
- Terrain shows natural variation and smooth transitions
- Frame rate remains stable during movement
- Memory usage stays within reasonable bounds
- All custom Blueprints function as expected
- Streaming stats show reasonable performance metrics

### ❌ Issues to Investigate
- Console commands return "not found" errors
- Terrain has visible gaps, holes, or seams
- Terrain transitions are abrupt or unnatural
- Significant frame rate drops during movement
- Memory usage increases continuously over time
- Blueprint compilation errors or runtime failures
- WorldGenManager fails to initialize properly
- Streaming performance is poor or unstable

## Performance Guidelines

### Realistic Performance Targets
- **Frame rate:** Maintain >30 FPS during normal movement
- **Memory usage:** Monitor for gradual increases over time
- **Chunk loading:** Should be smooth without noticeable hitches
- **Visual quality:** No obvious artifacts or rendering issues

### Monitoring Tools
- `wg.ShowStreamingStats` - View world generation metrics
- `stat memory` - Monitor memory usage
- `stat fps` - Display frame rate
- `stat unit` - Show detailed frame timing
- Blueprint performance monitors you created

This manual testing guide provides a comprehensive approach to validating your world generation system through hands-on testing and Blueprint-based tools. The focus is on practical, observable results rather than automated testing systems.