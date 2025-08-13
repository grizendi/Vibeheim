# Implementation Plan

- [x] 1. Set up project structure and core interfaces
  - Create WorldGen module directory structure under Source/Vibeheim/WorldGen/
  - Update Vibeheim.Build.cs to include VoxelPluginLegacy dependencies
  - Define IVoxelWorldService, IVoxelEditService, and IVoxelSaveService interfaces
  - Create base data structures: FWorldGenSettings, FVoxelEditOp, FBiomeData
  - _Requirements: 4.1, 4.2_

- [x] 2. Implement configuration and settings system
  - Create WorldGenSettings.h/.cpp with FWorldGenSettings structure
  - Implement JSON configuration loader for /Config/WorldGenSettings.json
  - Add validation for configuration parameters (chunk size, LOD radii, etc.)
  - Create unit tests for settings validation and JSON parsing
  - _Requirements: 1.1, 4.2_

- [x] 3. Create VoxelPluginAdapter foundation
  - Create VoxelPluginAdapter.h/.cpp class that implements IVoxelWorldService, IVoxelEditService, and IVoxelSaveService interfaces
  - Initialize VoxelPluginLegacy integration with basic world creation using Voxel plugin dependencies
  - Configure voxel world settings (50cm voxel size, 32³ chunk size) from FWorldGenSettings
  - Add plugin availability validation and error reporting with LogWorldGen category
  - Create unit tests for VoxelPluginAdapter initialization and basic functionality
  - _Requirements: 4.1, 4.3, 4.4_

- [x] 4. Implement deterministic noise and PRNG system
  - Create NoiseGenerator class with seed-mixed PRNG system (Seed ^ Hash(ChunkCoord) ^ FeatureTag)
  - Implement Perlin noise generation for terrain and biome masks
  - Add versioning support (WorldGenVersion, PluginSHA tracking)
  - Create unit tests for deterministic generation across multiple runs
  - _Requirements: 1.1, 1.2_

- [x] 4.1 Integrate NoiseGenerator with BiomeSystem
  - Replace BiomeSystem's simple noise implementation with NoiseGenerator
  - Update BiomeSystem to use deterministic PRNG for all noise generation
  - Ensure biome generation remains deterministic with new noise system
  - Update BiomeSystem tests to verify determinism with NoiseGenerator
  - _Requirements: 1.1, 1.2, 2.1_

- [x] 5. Build biome generation system
  - Implement BiomeSystem with three biome types (Meadows, BlackForest, Swamp)
  - Create biome noise masks with normalized weights and 24m blend zones
  - Implement max-weight wins logic for overlapping biomes
  - Apply height offsets after blend calculation
  - Add biome-specific terrain characteristics (data-only: debug colors, material params)
  - _Requirements: 2.1, 2.2, 2.3, 2.4_

- [x] 6. Implement chunk streaming and LOD management
  - Create chunk streaming system with prioritized loading based on player proximity
  - Implement LOD system (LOD0: 2 chunks, LOD1: 4 chunks, LOD2: 6 chunks)
  - Enforce collision only up to LOD1 rule (add cvar to toggle for perf testing)
  - Add async chunk generation with ≤5ms average build time target and P95 ≤9ms
  - Implement chunk unloading beyond streaming radius
  - _Requirements: 5.1, 5.2, 5.4_

- [x] 7. Create WorldGenManager actor
  - Implement AWorldGenManager as the main coordination actor
  - Add BeginPlay initialization with settings loading
  - Implement Tick method for streaming updates
  - Create player anchor tracking for world streaming
  - Add basic error handling and fallback generation (log seed + chunk coord on failures)
  - _Requirements: 1.4, 5.1, 5.5_

- [x] 8. Complete voxel edit system implementation
  - Implement actual CSG operations in VoxelPluginAdapter (ApplySphere method body)
  - Add async remeshing queue for edited chunks
  - Implement chunk modification tracking
  - Create unit tests for CSG operations and remeshing
  - _Requirements: 5.3_

- [x] 9. Complete persistence system implementation
  - Implement actual file I/O operations in VoxelPluginAdapter save service methods
  - Create per-chunk JSONL file format for edit operations
  - Implement write-behind timer (3 second flush) with atomic writes
  - Add LoadAndReplayForChunk functionality for chunk restoration
  - Implement file compaction on chunk unload
  - Create tests for save/load cycle integrity
  - _Requirements: 5.3_

- [x] 10. Create WorldGenManager actor
  - Implement AWorldGenManager as the main coordination actor
  - Add BeginPlay initialization with settings loading using WorldGenConfigManager
  - Implement Tick method for streaming updates via VoxelPluginAdapter
  - Create player anchor tracking for world streaming
  - Add basic error handling and fallback generation (log seed + chunk coord on failures)
  - Wire together all existing systems (VoxelPluginAdapter, BiomeSystem, ChunkStreamingManager)
  - _Requirements: 1.4, 5.1, 5.5_

- [x] 11. Implement POI placement system
  - Create POISystem class with placement rules engine (150m spacing, <20° slope, altitude bands)
  - Implement stamp order: terrain flatten → place prefab → resolve overlaps
  - Add 5 retry attempts with fallback to skipping
  - Wire all POI placement to deterministic PRNG (Seed ^ Hash(ChunkCoord) ^ 'POI')
  - Create basic POI prefab integration system
  - Add waterline clearance validation for applicable POIs
  - _Requirements: 3.1, 3.2, 3.6_

- [x] 12. Create dungeon portal system
  - Create DungeonPortalSystem.h/.cpp class as special POI types
  - Implement portal data structures: FDungeonPortal, FPortalSpawnRule
  - Add portal interaction detection using collision components
  - Create simple teleport to fixed sub-level functionality using UGameplayStatics::OpenLevel
  - Create basic portal prefab with visual indicators and interaction prompts
  - Integrate portal system with POISystem for placement rules
  - _Requirements: 3.3, 3.4_

- [x] 13. Add console commands and debug tools
  - Create WorldGenConsoleCommands.cpp with console command implementations
  - Implement console commands: wg.Seed, wg.StreamRadius, wg.DebugMasks, wg.DebugChunks
  - Add biome mask overlay rendering using debug draw functions
  - Create chunk boundary wireframe visualization in WorldGenManager
  - Implement performance metrics display with STAT_GROUP and Unreal Insights trace markers
  - Add POI placement validation indicators with debug spheres and text
  - Create debug UI widget for real-time world generation statistics
  - _Requirements: 4.4_

- [x] 14. Enhance logging and error handling
  - Enhance LogWorldGen category with structured error reporting in all systems
  - Add seed and chunk coordinates to all error logs across VoxelPluginAdapter and ChunkStreamingManager
  - Implement rolling mean/P95 build-time logging every 2 seconds in ChunkStreamingManager
  - Create fallback terrain generator: single-octave heightmap for failed generation
  - Add gray proxy grid mesh generation for completely failed chunks (one retry before proxy)
  - Implement graceful degradation system in WorldGenManager for system failures
  - _Requirements: 1.4, 4.4, 5.5_

- [x] 15. Complete automated tests for determinism and performance


  - Complete WorldGenTests.cpp with comprehensive determinism tests (currently only has stub)
  - Implement determinism tests with fixed seeds for terrain height validation (±1cm tolerance)
  - Add chunk seam tests: sample vertices along shared edges for bit-identical heights
  - Complete POI placement consistency verification tests across multiple runs
  - Add biome blending determinism tests to verify consistent results across runs
  - Implement cross-platform determinism validation tests for Windows/Linux compatibility
  - Add regression tests for noise generation consistency
  - _Requirements: 1.2, 1.3, 5.4_

- [x] 16. Complete integration test level and end-to-end validation





  - Complete integration test level with WorldGenManager placed and configured
  - Finish complete world generation pipeline tests with all systems integrated
  - Complete biome transition smoothness validation through automated sampling
  - Finish POI and dungeon portal functionality end-to-end tests with player interaction
  - Complete automated tests for complete world generation workflow
  - _Requirements: 2.3, 3.2, 3.4, 5.1_

- [x] 17. Complete performance optimization and validation



  - Complete performance profiling to ensure LOD0 radius ≤64MB mesh memory total
  - Finish LOD0 chunks ≤8k triangles verification through automated mesh analysis
  - Complete streaming performance tests during rapid player movement with telemetry
  - Finish performance regression tests for CI/CD pipeline
  - Complete chunk generation timing optimization to meet ≤5ms average build time target
  - _Requirements: 5.1, 5.4_

- [x] 18. Add minimal networking scaffold (optional)





  - Create WorldGenGameState.h/.cpp with replicated world generation properties
  - Replicate Seed and WorldGenVersion on GameState for multiplayer consistency
  - Create Server_ApplyEdit and Multicast_ApplyEdit RPCs for voxel modifications
  - Add late-join support: Server_RequestChunkSync → Client_ApplyChunkSync RPCs
  - Implement basic authority validation for world generation operations
  - Foundation for future multiplayer without blocking single-player development
  - _Requirements: 1.2_

