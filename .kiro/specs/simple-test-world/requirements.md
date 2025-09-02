# Requirements Document

## Introduction

The Simple Test World feature creates a demonstration world that showcases the integrated VHM terrain rendering and world generation systems. This world serves as a validation environment for testing all implemented features including procedural terrain generation, biome transitions, VHM rendering, PCG vegetation, POI placement, and terrain editing capabilities. The test world loads via /Game/Maps/WG_TestMap and uses UWorldSubsystem orchestration for deterministic, testable validation.

## Requirements

### Requirement 1

**User Story:** As a developer, I want to quickly launch a test world that demonstrates all implemented features, so that I can validate the integration between VHM rendering and world generation systems.

#### Acceptance Criteria

1. WHEN the test world is launched THEN the system SHALL load /Game/Maps/WG_TestMap with UWorldSubsystem orchestration that boots the pipeline in World::BeginPlay
2. WHEN the world loads THEN the system SHALL display four biomes (Meadows, Forest, Mountains, Ocean) with clear color/foliage/material cues and seamless transition bands of 1-2 tiles width
3. WHEN the world renders THEN the system SHALL use VHM for terrain display with no cracks across tile borders at LOD changes and continuous normals/tangents across borders
4. WHEN the world initializes THEN the system SHALL follow init order: Terrain → Biome solve → PCG → POI, with PCG starting only after height+biome for a tile is Ready
5. WHEN two runs use the same seed THEN the system SHALL produce identical tile checksums for height and biome ID for the first N tiles
6. IF any system fails to initialize THEN the system SHALL fall back to flat height + meadow material and log a single LogWorldGen Error with error code

### Requirement 2

**User Story:** As a developer, I want to test terrain editing functionality in the test world, so that I can verify that VHM rendering updates correctly when terrain is modified.

#### Acceptance Criteria

1. WHEN terrain editing commands (raise, lower, smooth, noise) are used with brush radius/falloff params THEN the system SHALL update VHM heightfield and collision within ≤100ms from input to visible result
2. WHEN terrain is modified THEN the system SHALL clear or re-sample PCG within radius = brushRadius × 1.25 of the edit with no floating trees/rocks
3. WHEN terrain edits are made THEN the system SHALL persist modifications to Saved/Vibeheim/WorldGen/Mods/ using InstancePersistence/TileInstanceJournal with version number
4. WHEN the world is reloaded THEN the system SHALL restore all terrain modifications from saved data, with version mismatch logging warning and safely ignoring unsupported entries
5. WHEN LOD transitions occur near edited areas THEN the system SHALL maintain visual consistency with no cracks and no pre-edit geometry revealed during camera orbit tests

### Requirement 3

**User Story:** As a developer, I want visual debugging tools in the test world, so that I can inspect system behavior and troubleshoot issues effectively.

#### Acceptance Criteria

1. WHEN console toggles are used THEN the system SHALL support: wg.debug.tiles 1 (tile bounds, FTileCoord, LOD index), wg.debug.biomes 1 (overlay biome id + blend factor), wg.debug.pcg 1 (spawn counts per archetype per tile), wg.perf 1
2. WHEN PNG exports are requested via wg.export height|biome|climate [tileX tileY] THEN the system SHALL write 1024×1024 files to Saved/Vibeheim/WorldGen/Exports/ with timestamped names reflecting current data
3. WHEN UE stat groups are accessed THEN the system SHALL provide Stat WorldGen, Stat VHM, Stat PCGWorld showing tile gen time, PCG pass time, and streaming queues
4. WHEN debug visualization is active THEN the system SHALL display clear visual indicators without performance impact on release builds
5. WHEN debugging is disabled THEN the system SHALL return to normal rendering performance with no debug overhead

### Requirement 4

**User Story:** As a developer, I want the test world to demonstrate streaming performance, so that I can validate that content loads smoothly as the player moves through the world.

#### Acceptance Criteria

1. WHEN the player moves through the world THEN the system SHALL stream tiles according to configured radii (Generate=9, Load=5, Active=3 in tile units) with moving 1 tile triggering ring events once with no thrash
2. WHEN tiles are generated THEN the system SHALL meet performance targets: Game thread overhead ≤0.5ms per newly activated tile, Worker thread tile generation p50 ≤10ms/p95 ≤20ms, PCG pass p50 ≤8ms/p95 ≤15ms
3. WHEN the LRU cache operates THEN the system SHALL cap resident tiles by VRAM/CPU memory budget (configurable target 1.5GB VRAM) and evict outwards with 1-tile hysteresis to prevent ping-pong
4. WHEN streaming occurs THEN the system SHALL maintain stable frame rates with no spikes >8ms above baseline during tile activation on mid-tier GPU
5. WHEN session ends THEN the system SHALL write CSV to Saved/Vibeheim/WorldGen/Perf/ with per-tile lifecycle timestamps (Queued, Generated, Baked, PCG, StreamedIn, Evicted) and frame-time overlays

### Requirement 5

**User Story:** As a developer, I want the test world to be easily configurable, so that I can test different scenarios and parameter combinations.

#### Acceptance Criteria

1. WHEN configuration commands are used THEN the system SHALL support: wg.seed <int>, wg.tileSize <m>, wg.radii <gen load active>, wg.biomes.file "<path>" (default Content/Vibeheim/Config/BiomeDefinitions.json)
2. WHEN runtime hot-reload is attempted THEN the system SHALL support safe changes (PCG densities, foliage archetypes, POI enable/disable, debug colors) and print "requires restart" for unsafe changes (tile size, biome schema, climate solver constants)
3. WHEN wg.reset is executed THEN the system SHALL delete Saved/Vibeheim/WorldGen/Mods/* and flush streaming with confirmation prompt in editor, no prompt in -game
4. WHEN parameters are changed THEN the system SHALL trigger incremental rebuild only where safe, otherwise require restart
5. WHEN the test world launches THEN the system SHALL use one authoritative seed from UWorldGenSettings feeding terrain, biome, PCG, and POI systems

## Console Commands

- wg.launch — loads /Game/Maps/WG_TestMap and starts pipeline (for PIE convenience)
- wg.seed <n> — sets seed and restarts world gen (with confirmation)
- wg.radii <gen load active> — applies streaming radii
- wg.debug.tiles 0|1, wg.debug.biomes 0|1, wg.debug.pcg 0|1, wg.perf 0|1
- wg.export height|biome|climate [x y] — exports PNG to Saved/Vibeheim/WorldGen/Exports/
- wg.reset — clears persistence & regenerates

## Definition of Done

- No errors or warnings from WorldGen/VHM log categories during 5-minute traversal across ≥30 tiles
- Deterministic replay: same seed twice yields identical height/biome checksums for first 10 tiles
- Edit round-trip: perform 3 edits, quit, relaunch, edits persist and pass LOD seam check
- Performance SLOs (Requirement 4) met with CSV exported
- No frame-time spikes >8ms above baseline during tile activation on mid-tier GPU