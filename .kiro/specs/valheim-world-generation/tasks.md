# Implementation Plan

## Phase 0: Foundations and Feature Toggles — COMPLETE

- [x] 0. Setup feature flags and console command infrastructure
  - Feature flags in `FWorldGenConfig`: `bEnableWater`, `bEnableRivers`, `bEnableRings`, `bEnablePCGGraphs`
  - Console commands: `wg.settings.show`, `wg.perf.export`, `wg.test.determinism`, `wg.perf.summary`
  - Performance instrumentation in TileStreamingService with CSV export
  - _Requirements: 9_

## Phase 1: Data Asset Configuration System — COMPLETE

- [x] 1. Create Data Asset classes for configuration management
  - Created `UWorldGenSettingsAsset` and `UBiomeDefinitionsAsset` (Primary Data Assets)
  - Implemented `FMacroWorldConfig`, `FBiomeRingDefinition`, `FWaterSystemConfig`, `FRiverSystemConfig`, `FStreamingBudgetsConfig`
  - Added asset resolution + validation to WorldGenManager with default paths
  - _Requirements: 9_

- [x] 2. Integrate Data Assets and create defaults
  - Default assets at `/Game/Data/WorldGen/DA_WorldGenSettings_Default` and `DA_BiomeDefinitions_Default`
  - ApplyFromAssets: WorldGenSettings copies MacroWorld/Water/Rivers/StreamingBudgets; applies key fields
  - BiomeService consumes `UBiomeDefinitionsAsset` via `SetBiomeDefinitions` and ring bands
  - `wg.settings.select/reload` implemented
  - _Requirements: 9_

## Phase 2: Streaming Budgets and Macro Topology — COMPLETE

- [x] 3. Implement macro topology in HeightfieldService
  - Continental ridged noise, radial island falloff, sea‑level clamping, underwater topology
  - Macro blend preserves cross‑tile continuity
  - _Requirements: 1_

- [x] 4. Add streaming budgets and async queue foundation
  - `FStreamingBudgetsConfig` integration, per‑stage budgets, requested + prefetch queues
  - Priority (near‑first) and VHM activation budgeting; metrics CSV export
  - _Requirements: 7_

## Phase 3: Biome Ring System — COMPLETE

- [x] 5. Implement biome ring progression in BiomeService
  - Distance‑based ring weights and allowed neighbors
  - Smooth blending with climate bias fallback
  - _Requirements: 2_

## Phase 4: Enhanced PCG System (UE 5.6) — COMPLETE

- [x] 6. PCGWorldService with real PCG graphs
  - Runtime scheduler: `UPCGSubsystem::ScheduleGraphAsync`
  - `UPCGParamData`/`UPCGPointData` with `UPCGMetadata` (parameter vs. point scopes validated)
  - Execution Dependency pin wiring for deterministic ordering
  - Frustum culling cvars: `vhm.pcg.frustum.enable`, `vhm.pcg.frustum.margin`
  - Telemetry counters + optional CSV (`vhm.pcg.telemetry.csv`) to `Saved/PCG/pcg_tasks.csv`
  - Per‑biome anchor `UPCGComponent` lifecycle and cleanup
  - HISM fallback when `VHM_PCG_ENABLED=0` or scheduling fails
  - All PCG TUs include `PCGVersionGuard.h`
  - _Requirements: 5_

## Phase 5: Water System Integration

- [ ] 7. Create WaterSystemService for water body management
- [ ] 7.1 Implement UWaterSystemService class with initialization
  - Create service class inheriting from UObject
  - Add Initialize method accepting FWaterSystemConfig
  - Implement tile-based water body spawning interface
  - _Requirements: 3_

- [ ] 7.2 Implement water body spawning for active tiles
  - Detect water areas based on heightfield and sea level
  - Spawn AWaterBody actors or components for water regions
  - Handle tile activation/deactivation events
  - _Requirements: 3_

- [ ] 7.3 Add shoreline detection system
  - Implement edge detection between land and water
  - Calculate distance-to-water field for each tile
  - Store shoreline data in tile streaming data
  - _Requirements: 3_

- [ ] 8. Extend TerrainMaterialSystem for water integration
- [ ] 8.1 Add water mask generation
  - Generate per-tile water mask texture based on heightfield
  - Integrate mask into material parameter collection
  - _Requirements: 3_

- [ ] 8.2 Implement distance-to-water blending
  - Calculate and store distance field for wetness effects
  - Add material parameters for shoreline blending
  - Implement fallback when water system is disabled
  - _Requirements: 3_

## Phase 6: Rivers and Lakes System

- [ ] 9. Implement RiverFlowService for flow computation
- [ ] 9.1 Create URiverFlowService class
  - Implement service initialization with FRiverSystemConfig
  - Add flow map data structures per tile
  - _Requirements: 4_

- [ ] 9.2 Implement flow accumulation algorithm
  - Calculate gradient-based flow direction per tile
  - Compute flow accumulation from neighborhood tiles
  - Generate flow map data for river placement
  - _Requirements: 4_

- [ ] 10. Add river carving and lake placement to HeightfieldService
- [ ] 10.1 Implement pre-normals river carving
  - Carve river channels based on flow map data
  - Apply carving before normal/slope calculation
  - Ensure cross-tile continuity for rivers
  - _Requirements: 4_

- [ ] 10.2 Add lake placement system
  - Identify local minima in heightfield for lake placement
  - Apply shoreline stamping around lakes
  - Integrate with water body spawning
  - _Requirements: 4_

## Phase 7: Enhanced POI System

- [ ] 11. Extend POIService for global uniqueness and terrain stamping
- [ ] 11.1 Implement world-level POI distribution
  - Add blue-noise spacing algorithm for global POI placement
  - Create POI reservation system to prevent duplicates
  - _Requirements: 6_

- [ ] 11.2 Add terrain stamping operations
  - Implement heightfield modification for POI placement
  - Add stamping operation types (flatten, raise, smooth)
  - Integrate with persistence system
  - _Requirements: 6_

- [ ] 11.3 Implement persistence reconciliation
  - Handle POI data across save/load cycles
  - Reconcile POI placements with terrain modifications
  - _Requirements: 6_

## Phase 8: Complete Async Generation Pipeline

- [ ] 12. Complete AsyncGenerationPipeline in TileStreamingService
- [ ] 12.1 Implement activation spike detection
  - Track frame time samples over rolling window (~3s)
  - Detect spikes exceeding +8ms threshold
  - Log spike events with tile coordinates and timing
  - _Requirements: 7, 8_

- [ ] 12.2 Add VHM mesh budget tracking
  - Integrate VHMMeshBudget into budget system
  - Track mesh upload/activation time per tile
  - Apply budget limits to prevent frame spikes
  - _Requirements: 7, 8_

- [ ] 12.3 Implement runtime budget adjustment commands
  - Add `wg.streaming.budget` console command
  - Allow runtime modification of per-stage budgets
  - Add `wg.prefetch` command to control prefetch rings
  - _Requirements: 7_

## Phase 9: Enhanced Configuration and Runtime Control

- [ ] 13. Implement remaining validation commands
- [ ] 13.1 Add map export command
  - Implement `wg.map.export` to export heightfield/biome data
  - Support various export formats (PNG, CSV)
  - _Requirements: 9_

- [ ] 13.2 Add ring validation command
  - Implement `wg.rings.validate` to check biome ring consistency
  - Validate ring boundaries and neighbor constraints
  - _Requirements: 2, 9_

- [ ] 13.3 Add river export command
  - Implement `wg.rivers.export` to export flow map data
  - Visualize river networks and flow accumulation
  - _Requirements: 4, 9_

- [ ] 13.4 Add POI validation command
  - Implement `wg.poi.validate` to check POI placement rules
  - Validate uniqueness and spacing constraints
  - _Requirements: 6, 9_

## Phase 10: Persistence and Determinism

- [ ] 14. Macro world changes & determinism
- [ ] 14.1 Implement terrain edit replay system
  - Add system to replay terrain modifications over macro changes
  - Handle version migration for heightfield modifications
  - _Requirements: 8_

- [ ] 14.2 Add journal compatibility system
  - Ensure modification journals work across world generation versions
  - Implement compatibility checks and migration paths
  - _Requirements: 8_

- [ ] 14.3 Implement determinism diagnostics
  - Add detailed logging for non-deterministic behavior
  - Create diagnostic tools to identify determinism issues
  - Extend `wg.test.determinism` with detailed reporting
  - _Requirements: 8_

## Phase 11: Integration and Performance Validation

- [ ] 15. Full service integration validation
- [ ] 15.1 Implement cross-tile continuity validation
  - Add validation for river continuity across tile boundaries
  - Check shoreline consistency at tile edges
  - Validate biome ring transitions
  - _Requirements: 1, 2, 3, 4_

- [ ] 15.2 Add texture memory validation
  - Implement memory tracking for heightfield textures
  - Validate memory usage stays within 512MB limit
  - Add `wg.memory.report` command
  - _Requirements: 8_

- [ ] 15.3 Implement performance target validation
  - Validate p50 <= 10ms, p95 <= 20ms for tile generation
  - Check activation spikes <= +8ms over 3s window
  - Create automated performance regression tests
  - _Requirements: 8_

## Verification Gates Summary

- Gate E: Macro topology — coastline ratio, histogram shape, seam check (COMPLETE)
- Gate F: Water/rivers — river continuity = 0; coastal coverage >= 80% (PENDING - Phase 5-6)
- Gate G: PCG/POI — validation commands; density within +/- 20% (PARTIAL - PCG complete, POI pending)
- Gate H: Pipeline/perf — p50/p95/spikes within targets; memory <= limits (PARTIAL - basic metrics complete, validation pending)

