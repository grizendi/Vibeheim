# Implementation Plan

## Phase 0: Foundation Types and Configuration

- [x] 1. Add EWorldGenBuildMode enum and extend FWorldGenConfig
  - [x] 1.1 Add EWorldGenBuildMode enum to WorldGenTypes.h
    - Define RuntimeStreaming, EditorBuildOnce, Hybrid values
    - _Requirements: 1.1_
  - [x] 1.2 Add BuildMode field to FWorldGenConfig
    - Default to RuntimeStreaming for backward compatibility
    - _Requirements: 1.1, 1.5_
  - [x] 1.3 Add optional bUseWorldPartitionStreaming flag to FWorldGenConfig
    - Used mainly for Hybrid and legacy maps
    - _Requirements: 1.4, 7.1_

- [x] 2. Create FWorldBuildState struct and UWorldGenBuildStateAsset
  - [x] 2.1 Create WorldGenBuildState.h with FWorldBuildState struct
    - Include BuiltSeed, WorldGenVersion, LastBuildTime, PCGBuildHash, bIsBaked
    - Implement IsValid() and IsCompatibleWith(const FWorldGenConfig&) helpers
    - _Requirements: 2.1, 2.2_
  - [x] 2.2 Write property test for FWorldBuildState round-trip serialization
    - **Property 2: Build State Persistence Round-Trip**
    - **Validates: Requirements 2.1, 2.2**
  - [x] 2.3 Create UWorldGenBuildStateAsset data asset class
    - Wraps FWorldBuildState with UpdateFromBuild() method
    - _Requirements: 2.1_

- [x] 3. Checkpoint - Foundation types
  - Manual Editor validation (no automation):
    - Open Vibeheim.uproject in UE 5.7 and ensure hot-reload completes without errors.
    - Create a `WorldGenBuildStateAsset` data asset in Content Browser; confirm BuiltSeed/WorldGenVersion/PCGBuildHash fields are editable and saved.
    - Switch `FWorldGenConfig.BuildMode` across RuntimeStreaming/EditorBuildOnce/Hybrid in project/world settings and verify values persist after re-open.
    - PIE smoke: start PIE on a test map and confirm no WorldGen compile/log errors related to build state or build mode.

---

## Phase 1: Terrain Resource and Prebaked Data

- [x] 4. Create UWorldGenTerrainResource data asset
  - [x] 4.1 Create WorldGenTerrainResource.h with coordinate metadata
    - WorldOrigin, TileSizeMeters, SampleSpacingMeters
    - MinHeight, MaxHeight, SeaLevel
    - _Requirements: 4.1, 4.4_
  - [x] 4.2 Implement tile-indexed height texture storage
    - `TMap<FIntPoint, TSoftObjectPtr<UTexture2D>> HeightTextures`
    - _Requirements: 4.1_
  - [x] 4.3 Implement tile-indexed biome cache
    - `TMap<FIntPoint, FBiomeResult> BiomeCache` (dominant per tile)
    - _Requirements: 9.2, 11.1_
  - [x] 4.4 Implement GetHeightAtWorldPosition() query method
    - Convert world pos -> tile -> UV, sample from texture
    - _Requirements: 4.4, 9.2_
  - [x] 4.5 Write property test for height query consistency
    - **Property 5: Prebaked Height Query Consistency**
    - **Validates: Requirements 4.1, 4.2, 4.4, 9.2**
  - [x] 4.6 Implement GetBiomeAtWorldPosition() and HasTileData()
    - Use BiomeCache plus WorldPosToTile()
    - _Requirements: 9.2, 11.2_
- [x] 4.7 Implement WorldPosToTile() helper
    - Converts world position to FTileCoord using WorldOrigin/TileSize
    - _Requirements: 4.4, 9.2_

- [x] 5. Extend FTileCoord with PCG grid conversion helpers
  - [x] 5.1 Add ToPCGGridCell() method to FTileCoord
    - Expects integer ratio between TileSize and PCGGridSize
    - _Requirements: 5.2_
  - [x] 5.2 Add FromPCGGridCell() static method
    - Construct FTileCoord from PCG grid cell
    - _Requirements: 5.2_
  - [x] 5.3 Add IsAlignedWithPCGGrid() static method with tolerance
    - Integer-ratio check with float tolerance
    - _Requirements: 5.1, 5.4_
  - [x] 5.4 Write property test for PCG grid round-trip conversion
    - **Property 6: PCG Grid Alignment Round-Trip**
    - **Validates: Requirements 5.1, 5.2**

- [x] 6. Checkpoint - Terrain resource and tile/grid helpers
  - Run tests and fix regressions.

---

## Phase 2: Mode-Aware WorldGenManager

- [x] 7. Modify AWorldGenManager for build mode awareness
  - [x] 7.1 Add build state loading in BeginPlay
    - Load UWorldGenBuildStateAsset companion asset
    - Validate against current FWorldGenConfig via IsCompatibleWith()
    - _Requirements: 2.2, 2.3, 2.4, 2.5_
  - [x] 7.2 Implement mode-based service initialization
    - RuntimeStreaming: existing tile streaming behavior
    - EditorBuildOnce with valid build state: skip TileStreamingService, rely on WP
    - Hybrid: initialize both WP and selected runtime systems
    - _Requirements: 1.2, 1.3, 1.4, 7.1_
  - [x] 7.3 Write property test for build mode streaming behavior
    - **Property 1: Build Mode Determines Streaming Behavior**
    - **Validates: Requirements 1.1, 1.2, 1.3, 1.4**
  - [x] 7.4 Implement seed mismatch detection and handling
    - On BuiltSeed != config seed:
      - Log error
      - Apply configurable stale policy (fallback to runtime or require rebuild)
    - _Requirements: 2.3, 2.4, 2.5_
  - [x] 7.5 Write property test for seed mismatch detection
    - **Property 3: Seed Mismatch Detection**
    - **Validates: Requirements 2.3, 2.4**
  - [x] 7.6 Add runtime generation warning for EditorBuildOnce mode
    - Log warning if TileStreamingService or heavy generation is triggered in baked mode (except explicit debug paths)
    - _Requirements: 9.5_
  - [x] 7.7 Implement runtime seed query helper
    - Ensure seed queried at runtime returns:
      - BuildState.BuiltSeed when baked
      - Config.Seed when non-baked/legacy
    - _Requirements: 9.4_

- [x] 8. Checkpoint - WorldGenManager
  - Run tests and fix regressions.

---

## Phase 3: VHM Terrain Renderer Prebaked Mode

- [x] 9. Extend UVHMTerrainRenderer for prebaked heightfield support
  - [x] 9.1 Ensure bUsePrebakedHeightfield setting exists in FVHMSettings and is exposed in config
    - _Requirements: 4.1_
  - [x] 9.2 Modify Initialize() to support prebaked mode
    - Bind to UWorldGenTerrainResource when prebaked mode is active and build state is valid
    - _Requirements: 4.1_
  - [x] 9.3 Bypass HeightfieldService::GenerateHeightfield in prebaked mode
    - Use UWorldGenTerrainResource for terrain height data instead of runtime generation
    - _Requirements: 4.2, 4.4, 9.2_
  - [x] 9.4 Implement fallback handling for missing prebaked data
    - Log error, optionally fall back to runtime generation based on config
    - _Requirements: 4.3_
  - [x] 9.5 Sanity tests for prebaked vs runtime rendering
    - A/B comparisons on a small test world
    - _Requirements: 4.1-4.5_

- [x] 10. Checkpoint - VHM integration
  - Run tests and fix regressions.

---

## Phase 4: PCG External Data Provider

- [x] 11. Create UWorldGenExternalDataProvider
  - [x] 11.1 Create WorldGenExternalDataProvider.h/.cpp
    - Store pointers to HeightfieldService, BiomeService, ClimateSystem
    - _Requirements: 11.1, 11.2_
  - [x] 11.2 Implement CreateParamDataForCell()
    - Build UPCGParamData with deterministic height/biome/climate aggregates for a cell
    - _Requirements: 11.1_
  - [x] 11.3 Implement query methods (GetTerrainHeight, GetBiomeData, GetClimateData)
    - Query worldgen services or UWorldGenTerrainResource under the hood
    - For use by custom PCG nodes and runtime PCG
    - _Requirements: 11.2, 11.4_
  - [x] 11.4 Implement external-data failure handling
    - If required terrain/biome/climate data is missing:
      - Fail the relevant PCG execution path gracefully
      - Log graph name, cell, and missing data
    - _Requirements: 11.5_
  - [x] 11.5 Write property test for PCG external data determinism
    - **Property 7: PCG External Data Determinism**
    - **Validates: Requirements 11.1, 11.3, 11.4**

- [x] 12. Checkpoint - External data provider
  - Run tests and fix regressions.

---

## Phase 5: Editor Build Utility

- [x] 13. Create UWorldGenBuildUtility (Editor-only)
  - [x] 13.1 Create WorldGenBuildUtility.h/.cpp in Editor folder
    - Wrap in `#if WITH_EDITOR`
    - _Requirements: 3.1_
  - [x] 13.2 Implement editor context validation
    - Check `GIsEditor && !IsRunningGame()`, fail otherwise
    - _Requirements: 10.5_
  - [x] 13.3 Write property test for build context validation
    - **Property 11: Build Context Validation**
    - **Validates: Requirements 10.5**
  - [x] 13.4 Implement InitializeServicesForBuild()
    - Initialize worldgen services in editor-only context
    - _Requirements: 3.1_
  - [x] 13.5 Implement BuildTerrainForTile()
    - Use HeightfieldService to generate tile, write to prebaked textures for UWorldGenTerrainResource
    - _Requirements: 3.1, 3.2_
  - [x] 13.6 Write property test for terrain build determinism
    - **Property 4: Terrain Build Determinism**
    - **Validates: Requirements 3.1, 3.2, 11.3**
  - [x] 13.7 Implement TriggerPCGOfflineBuild()
    - Invoke PCG World Partition Builder (commandlet or `pcg.BuildComponents`)
    - Wire in UWorldGenExternalDataProvider as needed
    - _Requirements: 6.1, 6.4_
  - [x] 13.8 Implement SaveBuildState()
    - Create/update UWorldGenBuildStateAsset with seed, version, PCGBuildHash, timestamp, bIsBaked
    - _Requirements: 3.3, 2.1_
  - [x] 13.9 Implement progress reporting
    - Fire OnBuildProgress delegate; show Slate notifications
    - _Requirements: 3.5_
  - [x] 13.10 Implement error handling and partial build recovery
    - Log per-tile/cell errors
    - Continue building remaining tiles where policy allows
    - Ensure offline PCG failure preserves existing content
    - _Requirements: 3.4, 6.5, Error Handling section_

- [x] 14. Checkpoint - Editor build utility
  - Run tests and fix regressions.

---

## Phase 6: PCG World Actor and Partition Grid

- [x] 15. Implement PCGWorldActor integration
  - [x] 15.1 Add PCGWorldActor creation/configuration in build utility
    - Create if missing
    - Configure grid size to match TileSizeMeters or integer multiple
    - _Requirements: 5.3, 5.5_
  - [x] 15.2 Validate PCG grid alignment during build
    - Use FTileCoord::IsAlignedWithPCGGrid()
    - Log warning if misaligned and suggest configuration
    - _Requirements: 5.1, 5.4_
  - [x] 15.3 Configure biome PCG graphs for partitioned/hierarchical generation
    - Set partitioned and hierarchical options on PCG components used for base world graphs
    - _Requirements: 6.1, 5.1_

- [x] 16. Checkpoint - PCG partition setup
  - Run tests and fix regressions.

---

## Phase 7: Delta System Integration

- [x] 17. Modify InstancePersistence for delta-only mode
  - [x] 17.1 Add base content detection
    - Distinguish base PCG instances (from WP baked content) from player modifications
    - _Requirements: 8.4, 6.3_
  - [x] 17.2 Filter base content from persistence
    - Only persist player-introduced or modified instances
    - _Requirements: 8.4, 6.3_
  - [x] 17.3 Write property test for delta system isolation
    - **Property 8: Delta System Isolation**
    - **Validates: Requirements 8.1, 8.2, 8.4**
  - [x] 17.4 Implement delta application on world load
    - Load WP base content first
    - Apply instance and terrain deltas deterministically
    - _Requirements: 8.3_
  - [x] 17.5 Write property test for delta application determinism
    - **Property 9: Delta Application Determinism**
    - **Validates: Requirements 8.3**
  - [x] 17.6 Implement delta application error handling
    - Log error, continue with base state
    - Mark save as degraded if delta set is unusable
    - _Requirements: 8.5_

- [ ] 18. Checkpoint - Delta system
  - Run tests and fix regressions.

---

## Phase 8: Console Commands and Tooling

- [ ] 19. Extend WorldGenConsoleCommands with build commands
  - [ ] 19.1 Implement `wg.build.world` command
    - Trigger full world build with optional seed parameter
    - _Requirements: 10.2, 10.1_
  - [ ] 19.2 Implement `wg.build.status` command
    - Display current build state (seed, version, timestamp, validity)
    - _Requirements: 10.3_
  - [ ] 19.3 Implement `wg.build.validate` command
    - Verify build state consistency with current config
    - _Requirements: 10.4_
  - [ ] 19.4 Implement `wg.build.terrain` command
    - Build terrain only (no PCG)
    - _Requirements: 10.1, 3.1, 3.2_
  - [ ] 19.5 Implement `wg.build.pcg` command
    - Build PCG only (requires prebaked terrain)
    - _Requirements: 6.1, 10.1_

- [ ] 20. Add editor menu integration
  - [ ] 20.1 Add "Rebuild Vibeheim World" menu option
    - Executes full build pipeline via UWorldGenBuildUtility
    - _Requirements: 10.1_
  - [ ] 20.2 Create an Editor Utility Widget for world builds
    - [ ] 20.2.1 Create WBP_WorldGenBuilder (Editor Utility Widget)
      - Buttons: “Build World”, “Build Terrain Only”, “Build PCG Only”
      - Fields: Seed override, map path, mode display (RuntimeStreaming / EditorBuildOnce / Hybrid)
      - Uses: UWorldGenBuildUtility, console commands from Phase 8
    - [ ] 20.2.2 Bind widget actions to UWorldGenBuildUtility
      - Call BuildWorldFromSeed with specified seed/map
      - Display FWorldBuildState (seed, version, timestamp, baked/stale)
    - [ ] 20.2.3 Show real-time progress
      - Subscribe to OnBuildProgress and OnBuildComplete
      - Display progress bar and status text
    - [ ] 20.2.4 Add access point in the editor
      - Window → Vibeheim → “World Build Pipeline” opens the widget

- [ ] 21. Checkpoint - Tools
  - Run tests and fix regressions.

---

## Phase 9: World Partition Streaming Integration

- [ ] 22. Implement World Partition streaming for baked worlds
  - [ ] 22.1 Configure Data Layers for PCG content
    - TerrainClutter, Trees, Rocks, POIs, Dynamic layers
    - _Requirements: 7.3_
  - [ ] 22.2 Ensure PCG graphs target correct Data Layers
    - Configure PCG Spawn Actor / Create Actor nodes
    - _Requirements: 6.2, 7.3_
  - [ ] 22.3 Disable TileStreamingService for WP-loaded cells in baked mode
    - Avoid duplicate generation when WP loads cells
    - _Requirements: 7.2, 9.1_
  - [ ] 22.4 Write property test for runtime generation bypass
    - **Property 10: Runtime Generation Bypass in Baked Mode**
    - Assert TileStreamingService::UpdateStreaming() is not called in EditorBuildOnce with valid build state
    - **Validates: Requirements 9.1, 7.2**
  - [ ] 22.5 Implement WP fallback for non-WP maps
    - Fall back to TileStreamingService streaming and log legacy mode
    - _Requirements: 7.5_
  - [ ] 22.6 Configure HLOD for PCG Data Layers
    - Ensure Trees/Rocks/POIs actors participate in HLOD generation where enabled
    - _Requirements: 7.4_
  - [ ] 22.7 Configure dynamic PCG runtime usage
    - Ensure dynamic content graphs:
      - Use PCG Runtime Generation mode
      - Target Dynamic data layer only
      - Do not interfere with offline-baked PCG layers
    - _Requirements: 9.3, 7.3_

- [ ] 23. Final Checkpoint - End-to-end validation
  - Run unit, property-based, and integration tests (full build -> load -> play).
