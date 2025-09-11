# Vibeheim WorldGen – MVP (updated)

Goal: Big outdoor world with ring‑biased biomes, fast tile streaming, PCG trees/rocks, a few POIs, and terrain brushes that persist. Single‑player only.

Out of Scope (Phase 2): Networking/replication, World Partition tuning, RVT/decals, undo/redo, fancy POI retries, compression/CRC, dynamic navmesh.
Perf targets: TileGen ~2 ms (height+biome), PCG ~1 ms/tile typical.

## Current Implementation Status

Completed systems:
- Core world generation (deterministic heightfield)
- Climate and ring bias (temperature, moisture, ring)
- Biome system (data‑driven) integrated with climate
- PCG‑based vegetation/content (HISM) with deterministic seeding
- Tile streaming (LRU, radii = Generate/Load/Active)
- VHM terrain rendering integration (procedural fallback)
- Persistence (terrain deltas + instance journals)
- Terrain editing with 4 brushes (raise/lower/smooth/noise), real‑time VHM updates, PCG clearing, persistence
- Determinism testing command: `wg.test.determinism <seed> [tiles] [-writebaseline]`
- Logging with generation/streaming timers

Pending:
- Performance CSV export (`wg.perf.export`) with headers: TileX,TileY,GenMs,PCGMs,StreamInMs,GTOverheadMs,ThreadSpikesMs
- Final polish (help strings, concise error codes, optional `wg.status`)

## Hard Gates
- Gate A: First ring renders in PIE, no visible cracks when orbiting LOD boundaries
- Gate B: 3 edits (raise/lower/smooth) apply with ~100ms visible update; nearby PCG cleared; restart and edits persist
- Gate C: Same seed, two launches → identical height/biome checksums for first N tiles
- Gate D: CSV shows worker p50 ~10ms, p95 ~20ms; max spike ~+8ms over baseline during activation burst

## Implementation Plan (condensed)

- [x] 1. Subsystem + Core Commands
  - `UWorldGenTestSubsystem` registers runtime commands (wg.launch, wg.seed, wg.radii, wg.reset, wg.validate, wg.gateA)
  - Single authoritative seed source via seed subsystem
  - Map guard: commands act only on `/Game/Maps/WG_TestMap`

- [x] 2. Test Map + VHM Integration
  - Fixed streaming ring (Generate=9, Load=5, Active=3)
  - VHM components created for tiles within Active radius; fallback procedural mesh path
  - Basic seam prevention and normal/tangent handling

- [x] 3. Terrain Editing System
  - Brushes: raise/lower/smooth/noise with radius & falloff
  - Console commands:
    - `wg.edit.raise <x> <y> <radius> <strength>`
    - `wg.edit.lower <x> <y> <radius> <strength>`
    - `wg.edit.smooth <x> <y> <radius> <strength>`
    - `wg.edit.noise <x> <y> <radius> <strength>`
  - VHM visible update within target on edited area; PCG cleared within ~1.25× brush radius
  - Persistence: journaled deltas under `Saved/WorldGen/TerrainDeltas/`

- [x] 4. Determinism Testing System
  - `wg.test.determinism <seed> [tiles] [-writebaseline]`
  - Checksums cover heightfield + biome ID + climate; spiral order; baseline stored at `Saved/Vibeheim/WorldGen/Determinism/`
  - Prints only first 3 diffs on mismatch

- [ ] 5. Performance Monitoring + CSV Export
  - Collect GenMs, PCGMs, StreamInMs, GTOverheadMs, ThreadSpikesMs; `wg.perf.export` writes to `Saved/Vibeheim/WorldGen/Perf/`

- [ ] 6. Final Polish & Validation
  - Help per command, `wg.status`, error code consistency; 5‑minute traversal check; verify Gates A–D

## Command Reference (delta)
- Terrain editing: `wg.edit.raise|lower|smooth|noise <x> <y> <radius> <strength>`
- Determinism: `wg.test.determinism <seed> [tiles] [-writebaseline]`
- Seed/radii/core: `wg.launch`, `wg.seed <n>`, `wg.radii <gen> <load> <active>`, `wg.reset`, `wg.validate`, `wg.gateA`

## Paths
- Terrain deltas: `Saved/WorldGen/TerrainDeltas/`
- Determinism baselines: `Saved/Vibeheim/WorldGen/Determinism/`
- (Planned) Perf CSV: `Saved/Vibeheim/WorldGen/Perf/`

