# Implementation Plan

## Phase 0: Foundations and Feature Toggles — COMPLETE

- [x] 0. Setup feature flags and console command infrastructure
  - Feature flags in `FWorldGenConfig`: `bEnableWater`, `bEnableRivers`, `bEnableRings`, `bEnablePCGGraphs`
  - Console commands: `wg.settings.show`, `wg.perf.export`, `wg.flags.show/set`
  - Performance instrumentation in TileStreamingService with CSV export

## Phase 1: Data Asset Configuration System — COMPLETE

- [x] 1. Create Data Asset classes for configuration management
  - Created `UWorldGenSettingsAsset` and `UBiomeDefinitionsAsset` (Primary Data Assets)
  - Implemented `FMacroWorldConfig`, `FBiomeRingDefinition`, `FWaterSystemConfig`, `FRiverSystemConfig`, `FStreamingBudgetsConfig`
  - Added asset resolution + validation to WorldGenManager with default paths

- [x] 2. Integrate Data Assets and create defaults
  - Default assets at `/Game/Data/WorldGen/DA_WorldGenSettings_Default` and `DA_BiomeDefinitions_Default`
  - ApplyFromAssets: WorldGenSettings copies MacroWorld/Water/Rivers/StreamingBudgets; applies key fields
  - BiomeService consumes `UBiomeDefinitionsAsset` via `SetBiomeDefinitions` and ring bands
  - `wg.settings.select/reload` implemented

## Phase 2: Streaming Budgets and Macro Topology — COMPLETE

- [x] 3. Implement macro topology in HeightfieldService
  - Continental ridged noise, radial island falloff, sea‑level clamping, underwater topology
  - Macro blend preserves cross‑tile continuity

- [x] 4. Add streaming budgets and async queue foundation
  - `FStreamingBudgetsConfig` integration, per‑stage budgets, requested + prefetch queues
  - Priority (near‑first) and VHM activation budgeting; metrics CSV export

## Phase 3: Biome Ring System — COMPLETE

- [x] 5. Implement biome ring progression in BiomeService
  - Distance‑based ring weights and allowed neighbors
  - Smooth blending with climate bias fallback

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

## Phase 5: Water System Integration — PENDING

- [ ] 7. Create WaterSystemService for water body management
  - Tile activation: spawn bodies; shoreline detection; material water masks

- [ ] 8. Extend TerrainMaterialSystem for water integration
  - Water mask + distance to water for shoreline/wetness blending; clean fallback when disabled

## Phase 6: Rivers and Lakes System — PENDING

- [ ] 9. Implement RiverFlowService for flow computation
  - Flow map per tile neighborhood; gradient/accumulation

- [ ] 10. Add river carving and lake placement to HeightfieldService
  - Pre‑normals carving; local minima for lakes; shoreline stamping

## Phase 7: Enhanced POI System — PENDING

- [ ] 11. Extend POIService for global uniqueness and terrain stamping
  - World‑level blue‑noise spacing, reservations, stamping ops, persistence reconciliation

## Phase 8: Complete Async Generation Pipeline — PARTIAL

- [~] 12. Complete AsyncGenerationPipeline in TileStreamingService
  - Full budgets for height + biome + PCG + VHM mesh pipeline; prefetch rings; spike detection (<= +8 ms over 3 s)
  - Note: runtime budget/prefetch console hooks remain backlog

## Phase 9: Enhanced Configuration and Runtime Control — PARTIAL

- [~] 13. Validation & control commands
  - Implemented: `wg.settings.show`, `wg.flags.show`, `wg.flags.set`, `wg.settings.select`, `wg.biomes.select`, `wg.settings.reload`, `wg.perf.export`, `wg.perf.summary`, `wg.test.determinism`, `wg.pcg.validate`, `wg.pcg.showdeps`
  - Backlog: `wg.map.export`, `wg.rings.validate`, `wg.rivers.export`, `wg.poi.validate`, `wg.streaming.budget`, `wg.prefetch`

## Phase 10: Persistence and Determinism — PENDING

- [ ] 14. Macro world changes & determinism
  - Replay terrain edits over macro changes; journals compatibility; version/migration hooks; determinism diagnostics

## Phase 11: Integration and Performance Validation — PENDING

- [ ] 15. Full service integration in WorldGenManager
  - Validate p50/p95/spikes, texture memory limits, cross‑tile continuity (rivers/shorelines/rings)

## Verification Gates Summary

- Gate E: Macro topology — coastline ratio, histogram shape, seam check
- Gate F: Water/rivers — river continuity = 0; coastal coverage >= 80%
- Gate G: PCG/POI — `wg.pcg.validate`/`wg.poi.validate` zero violations; density within +/- 20%
- Gate H: Pipeline/perf — p50/p95/spikes within targets; memory <= limits

