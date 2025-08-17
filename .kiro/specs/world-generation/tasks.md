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

- [ ] 5. Build biome system with PCG integration
  - Create data-driven Biomes.json (ring targets, temp/moisture gates, slope limits, weights)
  - Implement biome = argmax over weighted scores with configurable blend widths
  - Add smooth biome transitions with configurable falloffs and RVT mask authoring
  - Start with initial set: Meadows, Forest, Mountains (expandable later)
  - Integrate climate data (temperature/moisture) for biome determination
  - _Requirements: 2.1, 2.2, 2.3, 2.4_

- [ ] 6. Create PCG-based vegetation and content system
  - Set up PCG graphs per biome spawning trees/rocks/plants via HISM for performance
  - Create deterministic seeding adapter: (Seed, TileCoord, PrototypeId, Index) → PCG seed
  - Implement runtime PCG with partitioned grids aligned to tile size
  - Add add/remove operations for gameplay (fell trees, mine rocks)
  - Create multi-scale PCG partitioning for different content types
  - _Requirements: 3.1, 3.2, 3.5_

- [ ] 6.5. Implement instance and POI persistence
  - Create per-tile .inst file: instance GUID add/remove journal with compression
  - Persist spawned POIs, destroyed foliage, placed structures/loot
  - Implement replay journal after terrain/delta load with PCG spawn reconciliation
  - Add efficient instance tracking and modification systems
  - _Requirements: 5.3_

- [ ] 7. Implement hybrid streaming system
  - Set up World Partition for authored content only (towns, dungeons, vistas)
  - Build separate procedural tile streamer with LRU cache
  - Expose Generate/Load/Active radii (e.g., 9/5/3 tiles) in settings
  - Implement player-anchored streaming with prioritization and eviction rules
  - Add streaming metrics and performance monitoring
  - _Requirements: 5.1, 5.2, 3.3, 3.4_

- [ ] 7.5. Add networking and replication support
  - Implement server authoritative system: server generates tiles & edits
  - Create TileStateHash handshake with resync on mismatch
  - Add edit stream: brush ops, instance add/remove, POI spawns (compact messages)
  - Create network performance tests (rapid travel, mass edits)
  - _Requirements: 5.5_

- [x] 8. Create WorldGenManager coordination actor (IMPLEMENTED)
  - ✅ Implement AWorldGenManager as the main world generation coordinator
  - ✅ Add BeginPlay initialization with settings loading and system setup
  - ✅ Implement Tick method for streaming updates and performance monitoring
  - ✅ Create player anchor tracking for dynamic world streaming
  - ✅ Add error handling and fallback generation systems
  - _Requirements: 1.4, 5.1, 5.5_
  - _Status: Full implementation complete with all coordination functionality_

- [ ] 9. Implement runtime heightfield editing system
  - Create brush ops: add/subtract/flatten/smooth with radius & strength
  - Implement efficient batched height texture updates clipped per tile
  - Add trigger for partial navmesh rebuild windows on commit (throttled)
  - Create undo/redo stack per tile for terrain modifications
  - Store height deltas per tile for persistence and server authority
  - _Requirements: 5.3_

- [ ] 10. Build persistence system for world modifications
  - Create chunk-based save format: .terra (height deltas, RLE/compressed), .inst (journal)
  - Add version headers, CRC, migration hooks with autosave on checkpoints/shutdown
  - Implement modification replay on tile load with background IO queues
  - Store HISM instance removals (IDs) so chopped trees stay gone
  - Create efficient delta compression for modified tiles only
  - _Requirements: 5.3_

- [ ] 11. Create PCG-based POI placement system
  - Implement blue-noise/stratified placement with rarity, slope/altitude/water gates
  - Add POI prefab integration (scripts) with terrain stamping hooks (clear/flatten)
  - Create retry & backoff for failed placements with exclusion radii
  - Implement collision detection and terrain integration for POIs
  - Add POI persistence and replication for multiplayer consistency
  - _Requirements: 3.1, 3.2, 3.6_

- [ ] 12. Implement Runtime Virtual Texturing integration
  - Create RVT layers plan (albedo/normal/height/masks) with budget targets
  - Implement foliage sampling from RVT with path/wear decals authored into RVT
  - Add streaming/memory optimizations with layer audit tooling
  - Set up ground texture blending with foliage footprints
  - Create dynamic road/path rendering and POI clearing visualization
  - _Requirements: 2.3, 4.3_

- [ ] 13. Add debug tools and console commands
  - Implement console commands: wg.Seed, wg.StreamRadius, wg.ShowBiomes, wg.ShowPCGDebug
  - Create dump per-tile PNGs (height/biome/temp/moisture/slope) with tile hashes
  - Add World Partition cell overlays with wireframe & navmesh toggles
  - Implement performance HUD: tile gen time, PCG time, RVT pages, memory, HISM counts
  - Create visual debug overlays for biome boundaries and climate data
  - _Requirements: 4.4_

- [ ] 14. Implement comprehensive logging and error handling
  - Create LogWorldGen category with detailed error reporting
  - Add seed and chunk coordinates to all generation logs
  - Implement performance monitoring with timing metrics
  - Create fallback systems for failed heightfield generation
  - Add memory pressure detection and automatic optimization
  - _Requirements: 1.4, 4.4, 5.5_

- [ ] 15. Create automated tests for world generation (BASIC TESTS ONLY)
  - ❌ Create determinism tests (fixed seeds), border seam tests, checksum stability
  - ❌ Add PCG determinism tests (seed adapter), LRU eviction correctness
  - ❌ Implement streaming performance tests (rapid travel), save/load fuzzer
  - ❌ Create visual golden-image tests for biome transitions/content layout
  - ❌ Add memory usage profiling and cross-platform validation tests
  - _Requirements: 1.2, 1.3, 5.4_
  - _Status: BasicSystemTest.cpp exists with minimal functionality tests_

- [ ] 16. Integration testing and performance optimization
  - Run full pipeline soak: terrain → biomes → PCG → streaming → edits → persistence
  - Tune tile gen budgets (≤2ms height+biome) and PCG (≤1ms/tile typical)
  - Validate World Partition authored content + procedural tiles coexist cleanly
  - Verify edits, persistence, navmesh, and replication under stress
  - Test complete integration with performance targets and error handling
  - _Requirements: 2.3, 3.4, 5.1, 5.4_
## Perfor
mance Guardrails

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
