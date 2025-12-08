# Editor World Build Pipeline – Requirements

## Introduction

The Editor World Build Pipeline transforms Vibeheim's world generation from a runtime-centric streaming system into a Valheim-style "bake once, then play" model. This architectural shift moves heavy terrain, biome, and PCG generation into an editor-time build pipeline, leveraging Unreal Engine 5.7's PCG Generation Modes (Partitioned, Hierarchical, Runtime), World Partition integration, and PCG offline/World Partition builders. The runtime becomes lightweight, handling only player-driven deltas (buildings, terrain edits) while World Partition manages cell streaming of baked content.

## Glossary

- **World Partition (WP)**: UE's system for streaming large worlds by dividing them into cells loaded based on player proximity.

- **PCG Generation Modes**: PCG's modes for executing graphs:

  - **Partitioned Generation**: Splits a PCG Component's domain into a grid of subcomponents per cell.

  - **Hierarchical Generation**: Allows multiple grid resolutions within a single PCG graph (coarse/medium/fine).

  - **Runtime Generation**: Executes PCG graphs at runtime for dynamic or small-scope content.

- **PCGWorldActor**: Actor defining the PCG partition grid for a world.

- **PCG Offline/World Partition Builder**: PCG-specific builder invoked from console or World Partition builder commandlet to generate PCG components into WP cells and save results.

- **Build State**: Metadata tracking whether a world has been baked (seed, version, timestamp, hash).

- **Delta System**: Runtime persistence for player modifications only (not base world content).

- **FTileCoord**: Existing tile coordinate system used by HeightfieldService/BiomeService/ClimateSystem.

- **Prebaked Heightfield**: Height and related terrain data generated in the editor and stored as textures or landscape assets, consumed at runtime without regenerating.

## Requirements

### Requirement 1: Generation Mode Configuration

**User Story:** As a developer, I want explicit generation modes so that I can choose between editor-baked worlds and legacy runtime streaming.

#### Acceptance Criteria

1. WHEN `WorldGenManager` initializes THEN the system SHALL read `EWorldGenBuildMode` from `FWorldGenConfig` to determine generation behavior.

2. WHEN `EWorldGenBuildMode` is `EditorBuildOnce` and build state indicates world is baked THEN the system SHALL skip `TileStreamingService` initialization and runtime world generation.

3. WHEN `EWorldGenBuildMode` is `RuntimeStreaming` THEN the system SHALL use existing tile streaming behavior unchanged.

4. WHEN `EWorldGenBuildMode` is `Hybrid` THEN the system SHALL use World Partition for static baked content and runtime streaming only for explicitly configured dynamic systems (e.g. special runtime-only PCG or debug worlds).

5. IF build mode configuration is missing THEN the system SHALL default to `RuntimeStreaming` and emit a warning log with guidance to configure `EWorldGenBuildMode`.


---

### Requirement 2: World Build State Tracking

**User Story:** As a developer, I want persistent build state metadata so that the runtime knows whether to use baked content or regenerate.

#### Acceptance Criteria

1. WHEN an editor world build completes successfully THEN the system SHALL persist an `FWorldBuildState` containing at minimum: seed, worldgen version, timestamp, and a PCG build hash/checksum.

2. WHEN `WorldGenManager` initializes in `EditorBuildOnce` mode THEN the system SHALL load and validate `FWorldBuildState` from a companion asset associated with the map (e.g. `/Game/Maps/VibeheimMain_BuildState`).

3. WHEN the build state seed differs from the current configuration seed THEN the system SHALL log an error and either:

   - Fall back to runtime generation, OR

   - Explicitly mark the world as "stale" and require a rebuild,  

   according to a configurable policy.

4. WHEN the build state version is incompatible with the current worldgen version THEN the system SHALL prompt for rebuild (in editor) or fall back gracefully at runtime according to configuration.

5. IF build state asset is missing or corrupt THEN the system SHALL treat the world as unbaked, log a warning, and use runtime generation in `RuntimeStreaming` mode, or block play-in-editor world start in `EditorBuildOnce` mode until a build is performed.

---

### Requirement 3: Editor World Build Utility

**User Story:** As a level designer, I want an editor utility to bake the entire world from a seed so that I can generate deterministic content offline.

#### Acceptance Criteria

1. WHEN `UWorldGenBuildUtility::BuildWorldFromSeed(Seed)` is invoked in an editor context THEN the system SHALL:

   - Load the target map.

   - Initialize WorldGen services (HeightfieldService, ClimateSystem, BiomeService, etc.) in editor-only mode.

   - Iterate over all world extents using `FTileCoord` in a manner consistent with the World Partition / PCG partition grid.

   - Generate heightfield and biome/climate data for each relevant tile/cell.

2. WHEN building terrain THEN the system SHALL write height data to prebaked textures and/or landscape assets that are consumable by `VHMTerrainRenderer` (or equivalent terrain renderer) without requiring runtime heightfield generation.

3. WHEN world build completes THEN the system SHALL:

   - Save all modified World Partition cells.

   - Save or update the associated `FWorldBuildState` asset.

4. WHEN the build encounters errors on specific tiles or cells THEN the system SHALL:

   - Log detailed diagnostics including tile/cell coordinates and error category.

   - Continue processing remaining tiles/cells where feasible, unless a hard failure policy is configured.

5. WHILE build is in progress THEN the system SHALL report progress via editor notifications and/or a progress bar, including current tile/cell index and an estimated remaining count.

---

### Requirement 4: Prebaked Heightfield Support

**User Story:** As a developer, I want `VHMTerrainRenderer` to consume prebaked height textures so that runtime avoids CPU-heavy heightfield generation.

#### Acceptance Criteria

1. WHEN `FWorldGenConfig::VHMSettings::bUsePrebakedHeightfield` is true AND a valid `UWorldGenTerrainResource` is present THEN `VHMTerrainRenderer` SHALL bind directly to prebaked height data (textures or landscape) for terrain rendering.

2. WHEN prebaked mode is active THEN the system SHALL NOT call `HeightfieldService::GenerateHeightfield` at runtime for static world tiles/cells.

3. WHEN prebaked data is missing or incomplete for a tile/cell in prebaked mode THEN the system SHALL log an error and:

   - Optionally fall back to runtime generation for that area if a fallback flag is enabled, OR

   - Render a safe placeholder and mark the world as requiring rebuild.

4. WHEN querying terrain height at runtime for world geometry (e.g. pathfinding, gameplay queries) THEN the system SHALL read from `UWorldGenTerrainResource` rather than generating new height samples.

5. IF `bUsePrebakedHeightfield` is disabled THEN the system SHALL use existing runtime heightfield generation behavior unchanged.

---

### Requirement 5: PCG Partition Grid Alignment

**User Story:** As a developer, I want the tile grid aligned with PCG partitioning so that PCG uses engine-native cell management.

#### Acceptance Criteria

1. WHEN configuring the PCG partition grid for the world THEN the system SHALL set the PCG partition cell size (`PartitionGridSize` or equivalent) equal to, or an integer multiple of, `TileSizeMeters` defined in `FWorldGenConfig`.

2. WHEN converting `FTileCoord` to PCG partition coordinates THEN `FTileCoord::ToPCGGridCell()` SHALL return the corresponding PCG partition cell in a deterministic and reversible manner.

3. WHEN a `PCGWorldActor` exists in the map THEN the system SHALL use its grid definition as the authoritative partition grid for PCG-based world generation.

4. WHEN tile size and PCG grid size are misaligned (e.g. non-integer ratio) THEN the system SHALL log a warning with the computed ratio and a recommended configuration.

5. IF no `PCGWorldActor` exists during editor build THEN the system SHALL create or configure one with grid settings that respect the chosen `TileSizeMeters` / World Partition cell size and save it as part of the map.

---

### Requirement 6: PCG Offline / World Partition Builder Integration

**User Story:** As a developer, I want PCG content generated via offline PCG builders so that generated actors persist in World Partition cells.

#### Acceptance Criteria

1. WHEN the editor world build triggers PCG generation for base world content THEN the system SHALL invoke a PCG offline builder (for example, a PCG World Partition builder via editor command or World Partition Builder commandlet) rather than executing large-scale graphs purely at runtime.

2. WHEN PCG graphs execute as part of the offline build THEN generated actors SHALL be assigned to the correct World Partition Data Layers and HLOD Layers according to project configuration.

3. WHEN offline PCG build completes successfully THEN the system SHALL NOT use `InstancePersistence` or equivalent runtime persistence for base world PCG content that now exists in World Partition.

4. WHEN PCG graphs require biome/terrain/climate data THEN the system SHALL obtain it from the worldgen services via PCG external data or parameter data (see Requirement 11), not by duplicating generation logic inside the PCG graph.

5. IF the offline PCG builder fails for a given cell/graph THEN the system SHALL:

   - Log a detailed error including the graph name, cell, and failure reason.

   - Preserve existing baked content for unaffected cells and graphs.


---

### Requirement 7: World Partition Streaming Integration

**User Story:** As a developer, I want World Partition to handle cell streaming so that custom tile streaming is retired for baked worlds.

#### Acceptance Criteria

1. WHEN `EWorldGenBuildMode` is `EditorBuildOnce` and a valid build state exists THEN World Partition SHALL manage all world cell loading based on player position, and WP streaming settings SHALL be the primary control for spatial streaming.

2. WHEN World Partition loads a cell that contains baked terrain and PCG actors THEN the system SHALL NOT trigger `TileStreamingService` generation for that cell.

3. WHEN configuring Data Layers THEN the system SHALL define layers (at minimum) for terrain clutter, trees, rocks, and Points of Interest (POIs), and PCG graphs SHALL target these Data Layers consistently.

4. WHEN HLOD is enabled for the world THEN PCG-generated actors participating in large-scale environment dressing SHALL be eligible for HLOD generation via World Partition HLOD builder or equivalent.

5. IF World Partition is disabled or not available for a given map THEN the system SHALL fall back to `TileStreamingService`-based streaming and log that the map is operating in legacy streaming mode.

---

### Requirement 8: Runtime Delta System

**User Story:** As a developer, I want `InstancePersistence` to handle only player modifications so that base world content uses World Partition persistence.

#### Acceptance Criteria

1. WHEN a player modifies terrain (e.g. digs, raises ground) THEN the system SHALL persist the delta via the HeightfieldService modification journal or an equivalent delta representation, without changing prebaked base height data.

2. WHEN a player builds or destroys structures or decor THEN the system SHALL persist the resulting instances via `InstancePersistence` (or similar) as part of a delta layer separate from base PCG instances.

3. WHEN loading a baked world THEN the system SHALL:

   - Load World Partition base content.

   - Apply all relevant terrain and instance deltas deterministically on top of that content.

4. WHEN `InstancePersistence` saves its data THEN it SHALL NOT include base PCG instances that exist in World Partition; only player-introduced or modified instances SHALL be persisted.

5. IF delta application fails on load (e.g. corrupt delta data) THEN the system SHALL:

   - Log an error.

   - Continue with base world state without applying the faulty delta set, and clearly mark the affected save as degraded.

---

### Requirement 9: Lightweight Runtime Manager

**User Story:** As a developer, I want a minimal runtime worldgen path so that baked worlds have near-zero generation overhead.

#### Acceptance Criteria

1. WHEN `EWorldGenBuildMode` is `EditorBuildOnce` AND `FWorldBuildState` is valid THEN `WorldGenManager` SHALL NOT tick `TileStreamingService` or trigger any heavy runtime world generation for base terrain or PCG.

2. WHEN runtime systems query height/biome/climate for static world positions THEN the system SHALL read from prebaked `UWorldGenTerrainResource` (and any associated biome/climate caches) rather than regenerating those values procedurally.

3. WHEN dynamic content (e.g. temporary encounters, event-driven foliage) spawns via PCG at runtime THEN the system SHALL use PCG Runtime Generation mode for that content only and avoid interacting with offline-baked PCG layers.

4. WHEN seed is queried at runtime THEN the system SHALL return the seed recorded in `FWorldBuildState` for baked worlds and the current configuration seed for non-baked/legacy worlds.

5. IF heavy runtime generation (heightfield or large PCG graphs) is triggered in `EditorBuildOnce` mode outside of explicit debug tools THEN the system SHALL log a warning and identify the offending callsites for correction.

---

### Requirement 10: Build Pipeline Tooling

**User Story:** As a level designer, I want editor menu options and console commands to trigger world builds so that I can iterate on world generation.

#### Acceptance Criteria

1. WHEN a user selects a "Rebuild Vibeheim World" menu option in the editor THEN the system SHALL execute the full world build pipeline (terrain + PCG) using the currently configured seed and settings.

2. WHEN a user runs `wg.build.world <seed>` in an editor console THEN the system SHALL trigger an editor build with the specified seed and update `FWorldBuildState` on success.

3. WHEN a user runs `wg.build.status` THEN the system SHALL display current build state in the output log (seed, version, timestamp, validity, and whether the map is considered baked).

4. WHEN a user runs `wg.build.validate` THEN the system SHALL verify that `FWorldBuildState` is consistent with current configuration (seed, worldgen version, major terrain/PCG parameters) and report mismatches.

5. IF a full build is triggered while running PIE or in a non-editor context THEN the system SHALL reject the request with a clear error message explaining that world builds run only from editor tools or commandlets.

---

### Requirement 11: PCG External Data / Param Data Integration

**User Story:** As a developer, I want PCG graphs to consume Vibeheim's worldgen data (height, biome, climate, POIs) as external data so that PCG doesn't re-implement world logic.

#### Acceptance Criteria

1. WHEN executing PCG graphs as part of the editor world build THEN the system SHALL provide deterministic height/biome/climate/POI information via PCG external data or parameter data objects created from `HeightfieldService`, `BiomeService`, `ClimateSystem`, and related services.

2. WHEN a PCG node inside a Vibeheim world graph needs terrain height or biome classification THEN it SHALL obtain those values from:

   - Custom PCG nodes that query the worldgen services, OR

   - Supplied external/param data,  

   and SHALL NOT duplicate noise or biome logic directly inside the graph.

3. WHEN regenerating the same world with identical seed and configuration in editor THEN the external data provided to PCG graphs SHALL produce identical results, ensuring deterministic placement of PCG content.

4. WHEN PCG graphs are executed in runtime/hybrid scenarios for dynamic content THEN the same external data provider or custom nodes SHALL be used to keep behavior consistent with editor builds.

5. IF external data provisioning fails during PCG execution (e.g. missing terrain resource) THEN the system SHALL:

   - Fail the relevant PCG component gracefully.

   - Log an error including the graph, cell, and missing data source.

   - Leave previously baked content unchanged.

---

## Implementation Traceability

- Generation modes: `Source/Vibeheim/WorldGen/Public/WorldGenConfig.h` (new `EWorldGenBuildMode` enum and related config).

- Build state: `Source/Vibeheim/WorldGen/Public/WorldGenBuildState.h` (new `FWorldBuildState` struct and asset).

- Build utility: `Source/Vibeheim/WorldGen/Private/Editor/WorldGenBuildUtility.cpp` (new editor-only class implementing world build pipeline).

- Terrain resource: `Source/Vibeheim/WorldGen/Public/WorldGenTerrainResource.h` (new `UWorldGenTerrainResource` asset).

- PCG alignment: `Source/Vibeheim/WorldGen/Public/TileCoord.h` (extend `FTileCoord` with PCG grid conversion helpers).

- PCG external data provider: `Source/Vibeheim/WorldGen/Public/WorldGenExternalDataProvider.h` and `.../Private/WorldGenExternalDataProvider.cpp` (new integration layer for PCG external/param data).

- Console commands: `Source/Vibeheim/WorldGen/Private/WorldGenConsoleCommands.cpp` (extend with build-related commands).

- WorldGenManager: `Source/Vibeheim/WorldGen/Private/WorldGenManager.cpp` (mode-aware initialization and runtime behavior).

- PCG integration: `Source/Vibeheim/WorldGen/Private/PCGWorldService.cpp` (offline builder integration and runtime PCG usage).
