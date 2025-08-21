# Vibeheim WorldGen — MVP (Valheim feel, minimal)

Goal: Big outdoor world with ring-biased biomes, fast tile streaming, PCG trees/rocks, a few POIs, and three terrain brushes that persist. Single-player only.

Out of Scope (Phase 2): Networking/replication, World Partition tuning, RVT/decals, undo/redo, fancy POI retries, compression/CRC, dynamic navmesh.
Perf targets: TileGen ≤ ~2 ms (height+biome), PCG ≤ ~1 ms/tile typical.




# Implementation Plan

- [x] 1. Set up project structure and core interfaces

  - Create WorldGen module directory structure under Source/Vibeheim/WorldGen/
  - Update Vibeheim.Build.cs to include PCG Framework, World Partition, and RVT dependencies
  - Define IHeightfieldService, IPCGWorldService, and IWorldPartitionService interfaces
  - Create base data structures: FWorldGenSettings, FBiomeDefinition, FHeightfieldModification
  - _Requirements: 4.1, 4.2_

- [x] 1.5. Define core coordinate and tiling contract
  - Lock units (meters), tile size (64m), sample spacing (1m), max height (120m)
  - Define world origin semantics with world-space sampling (no tile-local bias)
  - Document Generate/Load/Active radii semantics and FTileCoord hashing
  - Create guardrails in WorldGenSettings.json with performance targets
  - _Requirements: 1.1_

- [x] 2. Implement configuration and settings system
  - Create WorldGenSettings.h/.cpp with FWorldGenSettings structure
  - Implement JSON configuration loader for /Config/WorldGenSettings.json
  - Add validation for heightfield resolution, streaming radius, and PCG parameters
  - Create unit tests for settings validation and JSON parsing
  - _Requirements: 1.1, 4.2_

- [x] 3. Create custom runtime heightfield system
  - Implement CPU height generation v1 (deterministic, easy to debug)
  - Set up Virtual Heightfield Mesh (VHM) rendering for runtime-editable terrain
  - Generate height, normal, slope data with optional thermal smoothing
  - Add deterministic noise suite with domain warp capabilities
  - Implement height texture upload path for GPU rendering
  - _Requirements: 1.1, 1.2, 4.1_

- [x] 3.5. Implement climate and ring systems
  - Create Temperature(x,y) system (latitudinal + altitude lapse + noise)
  - Implement Moisture(x,y) system (coast distance + noise)
  - Add ring bias by distance from world center with exposed tunables
  - Create debug PNG exports: height/biome/temp/moisture/slope/rings
  - _Requirements: 2.1, 2.2_

- [x] 4. Implement deterministic world generation (COMPLETED)
  - ✅ Create seeded PRNG system with xxHash64 hashing helpers
  - ✅ Implement chunked height generation with seamless borders (world-space sampling)
  - ✅ Add tile checksum API for integrity and replication validation
  - ✅ Create tests: multi-run determinism, border seam equality, checksum stability
  - ✅ Add world versioning for future save compatibility
  - _Requirements: 1.1, 1.2, 1.3_
  - _Status: Full deterministic system implemented with comprehensive validation and testing_

- [x] 5. Build biome system with PCG integration (COMPLETED)
  - ✅ Create data-driven biome definitions (implemented in BiomeService.cpp)
  - ✅ Implement biome suitability calculation based on climate data
  - ✅ Add smooth biome transitions with configurable blend widths
  - ✅ Initial set: Meadows, Forest, Mountains, Ocean implemented
  - ✅ Integrate climate data (temperature/moisture) for biome determination
  - ✅ **FIXED**: Complete DetermineTileBiome() method with proper sampling logic
  - ✅ Add biome data-driven JSON configuration loading with full save/load support
  - ✅ Create comprehensive BiomeDefinitions.json with vegetation and POI rules
  - ❌ Implement runtime PCG integration for biome-specific content generation (separate task)
  - _Requirements: 2.1, 2.2, 2.3, 2.4_
  - _Status: Full biome system implemented with JSON configuration, climate integration, and debug tools_

- [x] 6. Create PCG-based vegetation and content system (COMPLETED)
  - ✅ Set up PCG graphs per biome spawning trees/rocks/plants via HISM for performance
  - ✅ Create deterministic seeding adapter: (Seed, TileCoord, PrototypeId, Index) → PCG seed
  - ✅ Implement runtime PCG with partitioned grids aligned to tile size
  - ✅ Add add/remove operations for gameplay (fell trees, mine rocks)
  - ✅ Create multi-scale PCG partitioning for different content types
  - ✅ Full integration with WorldGenManager and streaming systems
  - ✅ Comprehensive debug commands and performance monitoring
  - ✅ Fallback generation system when PCG Framework is unavailable
  - _Requirements: 3.1, 3.2, 3.5_
  - _Status: Full PCG system implemented with HISM optimization, deterministic seeding, runtime operations, and complete debug tooling_

- [x] 6.5. Implement instance and POI persistence
  - ✅ Create per-tile .inst file: instance GUID add/remove journal with compression
  - ✅ Persist spawned POIs, destroyed foliage, placed structures/loot
  - ✅ Implement replay journal after terrain/delta load with PCG spawn reconciliation
  - ✅ Add efficient instance tracking and modification systems
  - _Requirements: 5.3_
  - _Status: Complete instance persistence system implemented with journal-based tracking, POI persistence, file I/O operations, and comprehensive testing_

- [x] 7) Procedural streaming + tiny LRU (COMPLETED)
  - ✅ Procedural tile streamer (separate from WP). Respect radii: Generate=9, Load=5, Active=3.
  - ✅ Simple LRU cache (≈9×9 cap). Evict tiles that drift outside Load radius.
  - ✅ Perf counters: active tiles, avg tile-gen ms, spikes.
  - _Status: Full TileStreamingService implemented with comprehensive LRU cache, radius-based streaming, performance metrics, and efficient tile eviction_

- [x] 9) Runtime terrain edits (3 brushes only) (COMPLETED)
  - ✅ Raise / Flatten / Smooth (radius + strength), batched per frame, clipped per tile.
  - ✅ When ground changes, clear nearby veg instances (HISM) deterministically.
  - ✅ Console commands for testing: wg.TerrainRaise/Lower/Flatten/Smooth
  - ✅ Comprehensive terrain modification system with falloff curves
  - (No undo/redo; no navmesh rebuilds in MVP.)
  - _Status: Full terrain editing system implemented with 4 operations (Add/Subtract/Flatten/Smooth), vegetation clearing integration, console command testing, and proper tile-based modification system_

- [ ] 10) Persistence (simple)
  - `.terra` height-delta grid per tile (raw or RLE); apply on load before queries.
  - `.inst` journal: add/remove instance GUIDs (trees/rocks/POIs).
  - Save on checkpoints and on quit. (No CRC/migrations/background IO.)

- [ ] 11) POIs (single pass)
  - Blue-noise (or stratified) placement with spacing + slope/altitude gates.
  - Optional small flatten/clear stamp under POI.
  - (No retries/backoff/collision gymnastics in MVP.)

- [ ] 14) Logging (minimal)
  - One `LogWorldGen` category; include seed + tile coords.
  - Basic timers around: height build, biome classify, PCG spawn, streaming tick.

- [ ] 15) Sanity tests (3 only)
  - Determinism: same seed/coords → same tile checksum.
  - Seams: 2×2 tiles share identical border heights/biomes.
  - PCG determinism: fixed (Seed, Tile, PrototypeId) → same instances.

- [ ] 16) Integration pass
  - 60s fly-through with radii on; verify stable perf + no visual seams.
  - Chop trees, leave area, return → instances persist removed.
  - Edit ground with 3 brushes, leave/return → edits persist and veg cleared.
  - POIs appear in sensible spots; stamp applied.

## Performance Guardrails

These values should be locked in WorldGenSettings.json:

```json
{
  "TileSizeMeters": 64,
  "SampleSpacingMeters": 1,
  "MaxTerrainHeight": 120,
  "SeaLevel": 0,
  "GenerateRadius": 9,
  "LoadRadius": 5,
  "ActiveRadius": 3,
  "PerfTargets": {
    "TileGenMs": 2,
    "PCGMsPerTile": 1
  },
  "Seed": "global 64-bit; all RNG derived from (Seed, Tile, PrototypeId, Index)"
}
```

## Phase 2 Backlog (post-MVP)
- Networking & replication (server-auth tiles, edit delta stream, TileStateHash)
- World Partition integration & priority tuning for authored sublevels
- RVT (paths/wear, foliage RVT sampling), material polish
- Undo/redo for terrain edits; dynamic navmesh rebuild windows
- POI retries/backoff/exclusion radii refinements
- Persistence compression + CRC/migrations + background IO
- Stress/perf benches, fuzzers, visual golden-image tests
