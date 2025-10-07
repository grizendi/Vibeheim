# PCG 5.6 Designer Migration Guide

## Overview

This guide summarizes the authoring changes required for Vibeheim's move to the Unreal Engine 5.6
Procedural Content Generation (PCG) scheduler. It is targeted at level and biome designers and
assumes familiarity with Vibeheim's tile-based streaming workflow. Use it as the primary reference
when updating existing graphs or building new content on the 5.6 runtime path.

## Wiring Execution Dependency Pins

UE 5.6 executes graphs through a multithreaded scheduler. Any node that advertises an **Execution
Dependency** pin must be explicitly wired to the upstream node that provides ordering or context.
Examples include *Get Landscape Data*, *Get Actor Data*, and custom blueprint nodes that read from
world state. Without wiring the dependency pin these nodes can run on arbitrary threads, resulting
in stale data, race conditions, or tile-to-tile nondeterminism.

- **Always wire dependency pins** on getters that have no data inputs.
- **Chain side-effect nodes** (e.g., blueprint utilities that spawn or mutate actors) through the
dependency pin so their order matches your intent.
- **Documented deterministic pattern:** Root → Terrain Sampler → (Dependency Pin) → Getter →
   Modifier → Output. The left-hand branch feeds data, the right-hand dependency branch locks
   execution order.

When migrating a legacy graph follow this checklist:

1. Locate every node with a dependency pin (`HasExecutionDependencyPin()` in details panel).
2. If the pin is not wired, connect it to the node that establishes the correct scope (typically the
tile input or a spatial filter).
3. For hierarchical graphs, wire the dependency pin from the partitioning node so child tiles sample
world data at the correct LOD.
4. Use the `wg.pcg.showdeps <GraphPath>` console command to list unwired dependency pins and fix them
before saving the graph.

## Canonical Attribute Reference

The runtime scheduler expects a specific attribute contract on the input and output data sets. The
table below lists the canonical attribute names, their types, and the scope where they are consumed.
Use the exact casing shown.

| Attribute Name | Type | Scope | Purpose |
| -------------- | ---- | ----- | ------- |
| `AverageHeight` | `float` | Parameter | Average tile height for heuristic spawning |
| `MinHeight` | `float` | Parameter | Minimum sampled height |
| `MaxHeight` | `float` | Parameter | Maximum sampled height |
| `AverageSlope` | `float` | Parameter & Point | Tile-wide slope average (param) and per-point slope (point) |
| `MaxSlope` | `float` | Parameter | Maximum slope for the tile |
| `WaterCoverage` | `float` | Parameter | Fraction of the tile covered by water |
| `AverageAboveWater` | `float` | Parameter | Average offset above water level |
| `AverageBelowWater` | `float` | Parameter | Average offset below water level |
| `MinWaterDistance` | `float` | Parameter | Closest distance to water within the tile |
| `SeaLevel` | `float` | Parameter | Sea level reference for biome heuristics |
| `BiomeId` | `int32` | Parameter | Numeric biome identifier |
| `TileSeed` | `int32` | Parameter | Deterministic tile seed (`Hash(TileX,TileY,BiomeId,GlobalSeed)`) |
| `TileX` / `TileY` | `int32` | Parameter | Tile coordinate indices |
| `TileSize` | `float` | Parameter | Tile world size in meters |
| `BiomeWeight` | `float` | Parameter or Point | Blend factor for biome mixing |
| `StaticMesh` / `Mesh` | `FSoftObjectPath` or `UStaticMesh*` | Point | Target mesh for instancing |
| `InstanceScale` | `FVector` | Point | Final instance scale |
| `InstanceRotation` | `FRotator` | Point | Final instance rotation |
| `IsActive` | `bool` | Point | Whether the instance should spawn |
| `InstanceId` | `FGuid` or `FString` | Point | Stable instance identifier for persistence |

> **Tip:** The extraction path accepts either `StaticMesh` or `Mesh`. Prefer soft object paths for
> runtime packaging safety. Every point should populate `InstanceScale`, `InstanceRotation`,
> `InstanceId`, and `IsActive` to keep persistence and deduplication working.

## Runtime Generation Expectations

- **Partitioning:** Author graphs to operate on a single 64×64 meter tile. Use the tile parameter
  data (`TileParameters` input) for tile metadata instead of custom constants.
- **Frustum Culling:** Runtime generation honors the project setting `bEnableFrustumCulling` and the
  CVars `vhm.pcg.frustum.enable` / `vhm.pcg.frustum.margin`. Keep expensive subgraphs behind spatial
  filters so frustum skipping removes their workload when out of view.
- **Concurrency:** The scheduler enforces the `MaxConcurrentPCGTasks` setting (CVar
  `vhm.pcg.max_concurrent`). Design graphs to complete comfortably within the budget (target <1ms per
  tile) so we can keep concurrency high without starving streaming.
- **Fallback Path:** If the scheduler fails or times out the service falls back to the legacy HISM
  generator. Graphs should gracefully tolerate being skipped for a tile. Do not depend on side effects
  that must run every frame.
- **Determinism:** Set both the PCG component seed and the metadata `TileSeed`. Use the provided
  hash helper when authoring blueprint utilities to avoid divergent seeds between runs.

## Rollout Controls

- Runtime execution is guarded by **Enable PCG Graphs** in the Vibeheim project
  settings (`bEnablePCGGraphs`). Leave this enabled for dev/editor builds and
  stage production rollouts biome-by-biome.
- Use `wg.settings set pcggraphs 0|1` to toggle during playtests. The change is
  immediate and persists until settings are reloaded.
- When the flag is off, graphs continue to cook and validate but execution
  falls back to the HISM generator—plan designer reviews accordingly.

## Example Authoring Patterns

1. **Biome Vegetation Pass**
   - Inputs: `TileParameters`, `Tile`
   - Flow: Sample Tile → Density Filter → Scatter → Mesh Instancer
   - Dependency: Wire instancer dependency pin to scatter output.
   - Attributes: Fill `InstanceScale`, `InstanceRotation`, `StaticMesh`, `InstanceId` via custom nodes
     if the default scatter does not provide them.

2. **Get Actor Data Cleanup**
   - Inputs: `TileParameters`
   - Flow: `Get Actor Data` (Dependency wired to Tile Parameters) → Bounds Filter → Difference → Output
   - Use execution dependency to ensure actor sampling runs after tile bounds are computed.

3. **Timed Sequencer or VFX**
   - Run sequencer nodes from an explicit dependency chain. The scheduler may run sequences in
     parallel otherwise, resulting in random ordering or missing triggers.

## Common Pitfalls

- **Missing dependency wiring** causes non-deterministic sampling of landscape and actors.
- **Incorrect attribute types** (e.g., storing `InstanceScale` as `float`) will log warnings and drop
  instances. Match the canonical types exactly.
- **Forgetting TileSeed** makes point generation differ between sessions, breaking persistence.
- **Relying on side effects during fallback**—the HISM path only spawns logical instance data. Always
  guard side-effect nodes behind runtime checks if they must not run during fallback.
- **Over-scheduling** heavy graphs can starve the streaming budget. Profile with telemetry (see
  `vhm.pcg.telemetry.csv`) and rebalance rule density if a biome exceeds ~1ms/tile.

## Console Variables and Live Tuning

Designers can adjust runtime behaviour without recompiling:

| CVar | Description | Default |
| ---- | ----------- | ------- |
| `vhm.pcg.max_concurrent` | Overrides `MaxConcurrentPCGTasks` at runtime | `4` |
| `vhm.pcg.frustum.enable` | Enables/disables scheduler frustum culling | `1` |
| `vhm.pcg.frustum.margin` | Global margin (cm) added to the frustum bounds | `500` |
| `vhm.pcg.poll_ms` | Polling cadence for synchronous waits (editor/tests) | `12` |
| `vhm.pcg.timeout_ms` | Timeout for synchronous waits (editor/tests) | `5000` |
| `vhm.pcg.telemetry.csv` | When non-zero, writes telemetry rows to `Saved/PCG/pcg_tasks.csv` | `0` |

Use the project settings panel to author default values; the CVars provide temporary overrides for
playtest sessions.

### Mirroring Project Settings

- Defaults live in `UVibeheimSettings::FWorldGenConfig`. Whenever the editor
  starts or settings are hot-reloaded, the subsystem mirrors the struct values
  into the corresponding CVars so command-line overrides remain transient.
- Changing CVars at runtime triggers the `OnChanged` handlers and updates
  `WorldGenSettings` in memory. Designers can therefore prototype tweaks via
  console commands and bake the final numbers into project settings once
  satisfied.
- Remember to check in updated settings assets after adjusting defaults so the
  build farm and packaged builds inherit the tuned values.

## Validation Workflow

1. Wire all execution dependency pins and run `wg.pcg.showdeps` to confirm none are unwired.
2. Run `wg.pcg.validate <Biome>` to ensure the graph exports the canonical attributes.
3. Generate a test tile via the automation tests or PIE and confirm telemetry logs show successful
   scheduler completions (no fallback unless expected).
4. Compare generation hashes between runs to confirm determinism when seeds are unchanged.

Following these guidelines keeps authored graphs compatible with the new scheduler and ensures the
runtime service can enforce determinism, concurrency, and graceful fallback across all biomes.
