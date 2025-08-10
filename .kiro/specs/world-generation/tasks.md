# Implementation Plan

- [ ] 1. Set up project structure and core interfaces
  - Create WorldGen module directory structure under Source/Vibeheim/WorldGen/
  - Update Vibeheim.Build.cs to include VoxelPluginLegacy dependencies
  - Define IVoxelWorldService, IVoxelEditService, and IVoxelSaveService interfaces
  - Create base data structures: FWorldGenSettings, FVoxelEditOp, FBiomeData
  - _Requirements: 4.1, 4.2_

- [ ] 2. Implement configuration and settings system
  - Create WorldGenSettings.h/.cpp with FWorldGenSettings structure
  - Implement JSON configuration loader for /Config/WorldGenSettings.json
  - Add validation for configuration parameters (chunk size, LOD radii, etc.)
  - Create unit tests for settings validation and JSON parsing
  - _Requirements: 1.1, 4.2_

- [ ] 3. Create VoxelPluginAdapter foundation
  - Implement VoxelPluginAdapter class that implements all three service interfaces
  - Initialize VoxelPluginLegacy integration with basic world creation
  - Configure voxel world settings (50cm voxel size, 32³ chunk size)
  - Add plugin availability validation and error reporting
  - _Requirements: 4.1, 4.3, 4.4_

- [ ] 4. Implement deterministic noise and PRNG system
  - Create seed-mixed PRNG system (Seed ^ Hash(ChunkCoord) ^ FeatureTag)
  - Implement Perlin noise generation for terrain and biome masks
  - Add versioning support (WorldGenVersion, PluginSHA tracking)
  - Create unit tests for deterministic generation across multiple runs
  - _Requirements: 1.1, 1.2_

- [ ] 5. Build biome generation system
  - Implement BiomeSystem with three biome types (Meadows, BlackForest, Swamp)
  - Create biome noise masks with normalized weights and 24m blend zones
  - Implement max-weight wins logic for overlapping biomes
  - Apply height offsets after blend calculation
  - Add biome-specific terrain characteristics (data-only: debug colors, material params)
  - _Requirements: 2.1, 2.2, 2.3, 2.4_

- [ ] 6. Implement chunk streaming and LOD management
  - Create chunk streaming system with prioritized loading based on player proximity
  - Implement LOD system (LOD0: 2 chunks, LOD1: 4 chunks, LOD2: 6 chunks)
  - Enforce collision only up to LOD1 rule (add cvar to toggle for perf testing)
  - Add async chunk generation with ≤5ms average build time target and P95 ≤9ms
  - Implement chunk unloading beyond streaming radius
  - _Requirements: 5.1, 5.2, 5.4_

- [ ] 7. Create WorldGenManager actor
  - Implement AWorldGenManager as the main coordination actor
  - Add BeginPlay initialization with settings loading
  - Implement Tick method for streaming updates
  - Create player anchor tracking for world streaming
  - Add basic error handling and fallback generation (log seed + chunk coord on failures)
  - _Requirements: 1.4, 5.1, 5.5_

- [ ] 8. Implement voxel edit system with CSG operations
  - Create EVoxelCSG enum and ApplySphere implementation
  - Add async remeshing queue for edited chunks
  - Implement chunk modification tracking
  - Create unit tests for CSG operations and remeshing
  - _Requirements: 5.3_

- [ ] 9. Build persistence system for voxel edits
  - Implement per-chunk JSONL file format for edit operations
  - Create write-behind timer (3 second flush) with atomic writes
  - Add flush on shutdown and on chunk unload to prevent data loss
  - Add LoadAndReplayForChunk functionality for chunk restoration
  - Implement file compaction on chunk unload
  - Create tests for save/load cycle integrity
  - _Requirements: 5.3_

- [ ] 10. Implement POI placement system
  - Create POI placement rules engine (150m spacing, <20° slope, altitude bands)
  - Implement stamp order: terrain flatten → place prefab → resolve overlaps
  - Add 5 retry attempts with fallback to skipping
  - Wire all POI placement to deterministic PRNG (Seed ^ Hash(ChunkCoord) ^ 'POI')
  - Create basic POI prefab integration system
  - Add waterline clearance validation for applicable POIs
  - _Requirements: 3.1, 3.2, 3.6_

- [ ] 11. Create dungeon portal system
  - Implement dungeon portals as special POI types
  - Create simple teleport to fixed sub-level functionality
  - Add portal interaction detection and level transition
  - Create basic portal prefab with visual indicators
  - _Requirements: 3.3, 3.4_

- [ ] 12. Add console commands and debug tools
  - Implement console commands: wg.Seed, wg.StreamRadius, wg.DebugMasks, wg.DebugChunks
  - Create biome mask overlay rendering for debugging
  - Add chunk boundary wireframe visualization
  - Implement performance metrics display with StatGroup and Unreal Insights trace markers
  - Create POI placement validation indicators
  - _Requirements: 4.4_

- [ ] 13. Implement logging and error handling
  - Create LogWorldGen category with detailed error reporting
  - Add seed and chunk coordinates to all error logs
  - Implement rolling mean/P95 build-time logging every 2 seconds
  - Create concrete fallback terrain: single-octave heightmap for failed generation
  - Add gray proxy grid mesh for completely failed chunks (one retry before proxy)
  - _Requirements: 1.4, 4.4, 5.5_

- [ ] 14. Create automated tests for determinism and performance
  - Build automated tests with fixed seeds for terrain height validation (±1cm tolerance)
  - Add chunk seam tests: sample vertices along shared edges for bit-identical heights
  - Create POI placement consistency verification tests
  - Implement performance benchmarks for chunk generation timing
  - Add memory usage profiling tests during streaming
  - Create cross-platform determinism validation tests
  - _Requirements: 1.2, 1.3, 5.4_

- [ ] 15. Integration testing and optimization
  - Test complete world generation pipeline with all systems integrated
  - Validate biome transition smoothness and visual quality
  - Test POI and dungeon portal functionality end-to-end
  - Optimize performance: within LOD0 radius ≤64MB mesh memory total, LOD0 chunks ≤8k triangles
  - Verify streaming performance during rapid player movement
  - _Requirements: 2.3, 3.2, 3.4, 5.1, 5.4_

- [ ] 16. Add minimal networking scaffold (optional)
  - Replicate Seed and WorldGenVersion on GameState for multiplayer consistency
  - Create Server_ApplyEdit and Multicast_ApplyEdit RPCs for voxel modifications
  - Add late-join support: Server_RequestChunkSync → Client_ApplyChunkSync
  - Foundation for future multiplayer without blocking single-player development
  - _Requirements: 1.2_