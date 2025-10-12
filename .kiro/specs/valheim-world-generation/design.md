# Valheim-Style World Generation Design

## Overview

This design layers Valheim‑style macro topology, biome ring progression, water/rivers, and richer content on top of the existing MVP worldgen. It aligns with Unreal Engine 5.6 and the new PCG runtime scheduler, while preserving robust fallbacks for headless/server builds.

## Architecture

Layers
- Macro Generation: continental shaping and biome ring progression
- Enhanced Base: heightfield generation, climate system, biome classification
- Water Systems: sea/shore, rivers/lakes
- Content: PCG graph runtime + fallback HISM placement; POIs and terrain stamping
- Performance: budgeted, time‑sliced async pipeline with prefetch

Services (core and new capabilities)
- WorldGenManager: orchestration, data asset resolution, service wiring
- HeightfieldService: base + macro blend + pre‑normals river carving
- BiomeService: climate + ring bias selection/blending
- TileStreamingService: LRU cache, radius management, budgeted/prefetch work queues
- PCGWorldService: scheduler‑based PCG runtime with metadata attributes; HISM fallback
- Water/River Services (future): sea/shore, splines, flow networks
- GlobalPOI/TerrainStamping (future): blue‑noise spacing, reservations, stamping ops

## Components and Interfaces

Key interfaces (selected)
- IMacroTopology (in HeightfieldService)
  - Initialize(FMacroWorldConfig)
  - BlendMacroTopology(BaseHeight, WorldPos)
  - CalculateIslandFalloff(WorldPos, WorldRadius)

- IBiomeRingService (in BiomeService)
  - SetBiomeRingDefinitions(TArray<FBiomeRingDefinition>)
  - CalculateRingBias(WorldPos, DistanceFromCenter)
  - GetBiomeForRing(RadialDistance, Climate)

- IEnhancedPCGService (UPCGWorldService)
  - GenerateBiomeContent(Tile, Biome, HeightData)
  - UpdateHISMInstances(Tile)
  - ValidatePCGGraph(GraphPath)
  - ClearPCGCache(), RemoveContentInArea(Box)

- IAsyncGenerationPipeline (TileStreamingService)
  - UpdateStreaming(PlayerTile)
  - GenerateTile(Tile)
  - ExportPerformanceCSV([Name])

## Data Models

Macro topology
```
struct FMacroWorldConfig {
  float WorldRadiusMeters;
  float ContinentScale;
  float IslandFalloff;
  float OceanDepth;
  float CoastSharpness;
  FNoiseSettings ContinentalNoise;
  FNoiseSettings CoastalNoise;
  UCurveFloat* IslandFalloffCurve;
};
```

Biome rings
```
struct FBiomeRingDefinition {
  EBiomeType BiomeType;
  float InnerRadius;
  float OuterRadius;
  float BlendWidth;
  float Weight;
  float HeightInfluence;
  float ClimateInfluence;
  TArray<EBiomeType> AllowedNeighbors;
};
```

Streaming budgets
```
struct FStreamingBudgetsConfig {
  float StreamingBudgetMsPerTick;
  int32 PrefetchRings;
  int32 WorkQueueThreads;
  int32 MaxActiveTiles;
  float HeightGenerationBudget;
  float BiomeCalculationBudget;
  float PCGGenerationBudget;
  float VHMMeshBudget;
};
```

## UE 5.6 PCG Integration

Adopted best practices
- Engine/API guard: include `PCGVersionGuard.h` in any TU touching PCG; enforces UE 5.6.x and presence of scheduler API.
- Conditional build: `VHM_PCG_ENABLED` (server targets may set to 0) with symmetric HISM fallback.
- Runtime scheduling: `UPCGSubsystem::ScheduleGraphAsync` via executor; track/release tasks; per‑biome anchor `UPCGComponent` under a transient anchor actor.
- Attribute handling: `UPCGParamData`/`UPCGPointData` + `UPCGMetadata` typed attributes. Parameter vs. point scopes validated.
- Execution ordering: prefer Execution Dependency pins for deterministic graph ordering.
- Frustum culling: toggle via `vhm.pcg.frustum.enable` and margin via `vhm.pcg.frustum.margin` (supports global/biome/component overrides).
- Telemetry: in‑engine counters + optional CSV to `Saved/PCG/pcg_tasks.csv` with `vhm.pcg.telemetry.csv`.

Fallback path
- When `VHM_PCG_ENABLED=0` or scheduling fails, use deterministic HISM generation with density heuristics and per‑biome meshes.

Validation and tooling
- `wg.pcg.validate <Biome|/Path/Graph>`: validates attributes, pin wiring, missing nodes; logs suggestions.
- `wg.pcg.showdeps </Path/Graph>`: enumerates Execution Dependency pins and warns about unwired nodes.

## Async Pipeline & Streaming

Pipeline (time‑sliced with budgets)
- Tile request → schedule(height + rivers + biome + PCG + VHM)
- Worker: macro blend + flow compute + carving
- Worker: base heightfield + normals/slopes + climate/biome
- Worker: PCG generation + stamping
- Game: water spawn + VHM mesh upload/activation

Budget application
- HeightGenerationBudget: macro + base + carving
- BiomeCalculationBudget: climate + rings + selection
- PCGGenerationBudget: graph execution + stamping
- VHMMeshBudget: mesh/material updates

Threading
- Worker threads (TaskGraph/ThreadPool): height/biome/PCG
- Game thread: streaming decisions, actor/components, VHM upload, water spawn

Streaming implementation
- LRU cache with hysteresis; near‑first prioritization; prefetch rings beyond generate radius.
- Perf export: `wg.perf.export` CSV with GenMs/PCGMs/StreamInMs/GTOverheadMs/ThreadSpikesMs.
- Perf summary: `wg.perf.summary` for p50/p95 from CSV.

## Integration with Existing Systems

- WorldGenManager
  - Resolve `UWorldGenSettingsAsset` and `UBiomeDefinitionsAsset` via soft refs/AssetManager.
  - Initialize services in dependency order; pass settings/ring defs to BiomeService and budgets to streaming.
- TileStreamingService
  - Work queues (requested + prefetch), per‑stage budgets, LRU/hysteresis, VHM notifications, spike computation.
- VHMTerrainRenderer
  - Accept activation/deactivation per‑tile; track mesh generation time to feed budgets/metrics.

## Configuration & Validation

Data Assets
- `UWorldGenSettingsAsset`: MacroWorld, Water, Rivers, StreamingBudgets, toggles; default assets under `/Game/Data/WorldGen/*`.
- `UBiomeDefinitionsAsset`: biome defs + ring bands; consumed by BiomeService.

Console commands (implemented)
- Settings/runtime: `wg.settings.show`, `wg.flags.show`, `wg.flags.set`, `wg.settings.select`, `wg.biomes.select`, `wg.settings.reload`
- Perf/determinism: `wg.perf.export`, `wg.perf.summary`, `wg.test.determinism`
- PCG validation: `wg.pcg.validate`, `wg.pcg.showdeps`

Optional (backlog)
- `wg.map.export`, `wg.rings.validate`, `wg.rivers.export`, `wg.poi.validate`, `wg.streaming.budget`, `wg.prefetch`

Verification gates
- World shape/coasts: coastline ratio within range, seam <= 0.2 m, histogram match
- Water/rivers: coastal coverage >= 80%, river continuity violations = 0
- PCG/POI: instance density within +/- 20% of targets, uniqueness violations = 0
- Pipeline/perf: p50 <= 10 ms, p95 <= 20 ms, spikes <= +8 ms; memory <= limits

## Implementation Alignment

- Prefer extending existing services over introducing new ones unless complexity demands it.
- Use Data Assets for designer workflows; gate JSON loaders under editor‑only dev flags.
- Keep server/headless parity with HISM fallback; centralize gates in `PCGVersionGuard.h`.
- Validate graphs early; ship helper console tools for designers and QA.

