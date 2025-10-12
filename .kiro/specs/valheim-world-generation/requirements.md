# Requirements Document

## Introduction

The Valheim-Style World Generation extends the MVP worldgen with continental topology, biome rings, water/river systems, and rich PCG content. It builds on WorldGenManager, HeightfieldService, BiomeService, TileStreamingService, and VHMTerrainRenderer, while adopting Unreal Engine 5.6 best practices and the new PCG runtime scheduler.

## Requirements

### 1. Macro World Topology and Continental Shaping

- Create low‑frequency continental shaping blended into the base heightfield.
- Apply island/radial falloff for coastlines; clamp sea level with stable underwater topology.
- Expose parameters: `WorldRadiusMeters`, `ContinentScale`, `IslandFalloff`, `OceanDepth`, `CoastSharpness`.
- Provide graceful fallback to base heightfield path with error logging.

### 2. Biome Ring System with Distance‑Based Progression

- Support ring‑based biome progression (e.g., Meadows → Forest → Mountains → …) via radial distance from world center.
- Maintain smooth blending while respecting ring ordering and allowed neighbors.
- Configure per‑biome ring bands/weights via Data Assets (no JSON at runtime).
- Fallback to climate‑only selection if ring data is missing.

### 3. Water System Integration

- Spawn water bodies around active tiles based on sea level and heightfield.
- Detect shorelines and expose water distance/masks to materials.
- Integrate with streaming events for seamless coverage and clean fallback when disabled.

### 4. Rivers and Lakes with Flow Networks

- Compute low‑resolution flow maps (per tile neighborhood) from height gradients.
- Carve river channels into the heightfield pre‑normals and spawn spline water.
- Identify local minima for lakes and apply shoreline stamping.
- Continue with static water if flow computation fails.

### 5. Enhanced PCG with Real Graphs (UE 5.6)

- Use biome‑specific PCG graphs when PCG is available; otherwise, fallback to HISM generation.
- Parameterize PCG with tile metrics (slope/height/water distance/biome weight) via `UPCGParamData`/`UPCGPointData` and `UPCGMetadata`.
- Enforce deterministic ordering via Execution Dependency pins and scheduler.
- Add frustum culling, telemetry, and console validation.
- Provide robust fallback for headless/server builds via `VHM_PCG_ENABLED`.

### 6. Global POI and Stamping

- Support world‑scale blue‑noise distribution, global reservation, and terrain stamping for POIs.

### 7. Async Generation & Streaming

- Budgeted, time‑sliced pipeline for height → biome → PCG → VHM mesh.
- Prefetch outer rings; prioritize near‑player tiles; enforce cache size and hysteresis.

### 8. Performance and Determinism

- Tile generation p50 <= 10 ms, p95 <= 20 ms (height+biome; excluding mesh upload) within target radii.
- Activation spikes <= +8 ms over ~3 s window.
- Texture memory for height + normals <= 512 MB (ActiveRadius=3, LoadRadius=5).
- Determinism: first N tiles’ checksum stable per seed.

### 9. Configuration (Data Assets)

- Replace JSON with Data Assets for designer‑authored worldgen settings and biome rings/defs.
- Keep JSON loaders gated behind `WITH_EDITORONLY_DATA` for dev use only, with deprecation logging.

## Configuration Schema (Data Assets)

- `UWorldGenSettingsAsset` (Primary Data Asset)
  - Macro Topology: `FMacroWorldConfig`
  - Water: `FWaterSystemConfig`
  - Rivers: `FRiverSystemConfig`
  - Streaming: `FStreamingBudgetsConfig`
  - Defaults: seed/radii/perf budgets and toggles (water/rings/rivers/pcggraphs)
- `UBiomeDefinitionsAsset` (Primary Data Asset)
  - Per‑biome definitions and `TArray<FBiomeRingDefinition>`

Runtime‑mutable: seed, streaming radii, and budgets (via console); persisted to SaveGame, not Data Assets.

## Validation and Tooling

Implemented console commands:
- Settings/runtime: `wg.settings.show`, `wg.flags.show`, `wg.flags.set`, `wg.settings.select`, `wg.biomes.select`, `wg.settings.reload`
- Performance: `wg.perf.export`, `wg.perf.summary`, `wg.test.determinism`
- PCG validation: `wg.pcg.validate`, `wg.pcg.showdeps`

Optional (backlog): `wg.map.export`, `wg.rings.validate`, `wg.rivers.export`, `wg.poi.validate`, `wg.streaming.budget`, `wg.prefetch`

Verification gates:
- Gate E (World shape): coastline ratio within expected range, border seam <= 0.2 m, histogram matches continental profile
- Gate F (Water & rivers): coastal coverage >= 80%, river continuity violations = 0, spline counts reasonable vs. flow
- Gate G (PCG & POI): instance density within +/- 20% of targets, uniqueness = 0 violations
- Gate H (Pipeline & perf): p50/p95/spikes within targets, memory <= limits

## UE 5.6 PCG Guidelines Adopted

- Compile‑time guard: `PCGVersionGuard.h` enforces UE 5.6.x and validates PCG API presence.
- Conditional builds: `VHM_PCG_ENABLED` allows server/headless HISM fallback.
- Scheduler path: `UPCGSubsystem::ScheduleGraphAsync` with task tracking and release.
- Attribute access: `UPCGMetadata` + typed attributes; parameter vs. point metadata separation.
- Dependency wiring: Execution Dependency pins for deterministic ordering.
- Frustum culling: `vhm.pcg.frustum.enable` and `vhm.pcg.frustum.margin` (global/biome/component overrides).
- Telemetry: counters + optional CSV (`vhm.pcg.telemetry.csv` → `Saved/PCG/pcg_tasks.csv`).

## Implementation Traceability

- Macro topology: `Source/Vibeheim/WorldGen/Private/Services/HeightfieldService.cpp`
- Biome rings: `Source/Vibeheim/WorldGen/Private/Services/BiomeService.cpp`
- Streaming budgets/prefetch: `Source/Vibeheim/WorldGen/Private/Services/TileStreamingService.cpp`
- PCG runtime: `Source/Vibeheim/WorldGen/Private/Services/PCGWorldService.cpp` (scheduler, metadata, frustum, telemetry, fallback)
- Console commands: `Source/Vibeheim/WorldGen/Private/WorldGenConsoleCommands.cpp`, `Source/Vibeheim/WorldGen/Private/WorldGenTestSubsystem.cpp`
- Engine guards: `Source/Vibeheim/WorldGen/Public/PCGVersionGuard.h`

