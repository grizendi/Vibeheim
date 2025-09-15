# Implementation Plan

## Phase 0: Foundations and Feature Toggles

- [ ] 0. Setup feature flags and console command infrastructure
  - Add feature flags to FWorldGenConfig: bEnableWater, bEnableRivers, bEnableRings, bEnablePCGGraphs
  - Re-enable console commands in WorldGenConsoleCommands.cpp with proper safety checks
  - Add performance instrumentation with CSV export functionality to TileStreamingService
  - _Requirements: 9.1, 9.2_
  - _Deliverables: Feature toggle system, console command registration, perf CSV structure_
  - _Acceptance: Feature flags toggle systems cleanly, console commands work in PIE and editor_

## Phase 1: Data Asset Configuration System

- [ ] 1. Create Data Asset classes for configuration management
  - Create UWorldGenSettingsAsset and UBiomeDefinitionsAsset primary data assets
  - Implement FMacroWorldConfig, FBiomeRingDefinition, FWaterSystemConfig, FRiverSystemConfig, FStreamingBudgetsConfig structs
  - Add asset resolution and lifecycle management to WorldGenManager
  - _Requirements: 9.1, 9.2, 9.3, 9.4_
  - _Deliverables: UWorldGenSettingsAsset, UBiomeDefinitionsAsset classes, config structs_
  - _Acceptance: Assets load in editor, validation works, default paths exist_

- [ ] 2. Migrate JSON configuration to Data Assets
  - Update WorldGenSettings to use TSoftObjectPtr for asset references
  - Implement ApplyFromAsset methods replacing LoadFromJSON
  - Add editor validation and default asset paths at /Game/Data/WorldGen/DA_*_Default
  - Maintain JSON fallback for development with deprecation warnings
  - _Requirements: 9.1, 9.2_
  - _Deliverables: Asset migration system, default assets, console commands wg.settings.select/reload_
  - _Acceptance: wg.settings.select/wg.settings.reload work, JSON loader logs deprecation when assets present_

## Phase 2: Streaming Budgets and Macro World Topology

- [ ] 3. Add streaming budgets and async queue foundation
  - Extend FWorldGenConfig with FStreamingBudgetsConfig fields
  - Add per-stage budget enforcement to TileStreamingService (HeightGenerationBudget, BiomeCalculationBudget, etc.)
  - Implement time-sliced generation with frame rate targets in existing GenerateSingleTile method
  - _Requirements: 8.1, 8.2, 8.3_
  - _Deliverables: Budget system, async queue foundation, performance monitoring_
  - _Acceptance: Budget enforcement works, generation stays within time limits_

- [ ] 4. Implement macro topology in HeightfieldService
  - Add FMacroWorldConfig struct to WorldGenTypes.h
  - Extend GenerateBaseHeight method with continental noise generation and island falloff
  - Add sea level clamping and underwater topology to existing heightfield generation
  - Integrate macro blending into existing HeightfieldService (no separate service)
  - _Requirements: 1.1, 1.2, 1.3, 1.4_
  - _Deliverables: Enhanced HeightfieldService with macro topology_
  - _Acceptance: wg.map.export shows coastlines, coastline coverage in expected range, no tile seams > 0.2m_

- [ ] 5. Add macro world configuration parameters
  - Extend FWorldGenConfig with macro world parameters (WorldRadiusMeters, ContinentScale, IslandFalloff, OceanDepth, CoastSharpness)
  - Update JSON configuration loading/saving to include macro world settings
  - Implement fallback to existing heightfield generation on failure
  - _Requirements: 1.4, 1.5_
  - _Deliverables: Macro world configuration system_
  - _Acceptance: Gate E passes - coastline ratio, histogram shape, seam check_

## Phase 3: Biome Ring System

- [ ] 6. Implement biome ring progression in BiomeService
  - Add FBiomeRingDefinition struct to WorldGenTypes.h
  - Extend existing CalculateBiomeSuitability method to factor radial distance from world center
  - Implement ring-based biome selection with configurable weights in DetermineBiome method
  - Maintain smooth blending while respecting ring progression in existing biome blending logic
  - _Requirements: 2.1, 2.2, 2.3, 2.4_
  - _Deliverables: Enhanced BiomeService with ring progression_
  - _Acceptance: wg.rings.validate shows per-ring biome distribution within config, monotonicity in 3x3 neighborhoods_

- [ ] 7. Add biome ring configuration system
  - Add ring configuration to existing BiomeDefinitions.json structure
  - Update LoadBiomesFromJSON method to parse ring definitions
  - Add fallback to existing climate-based biome selection on ring calculation failure
  - _Requirements: 2.4, 2.5_
  - _Deliverables: Biome ring configuration system_
  - _Acceptance: Ring progression works correctly, blend zones within tolerance_

## Phase 4: Enhanced PCG System (moved up for faster wins)

- [ ] 8. Enhance PCGWorldService with real PCG graph integration
  - Add bEnablePCGGraphs feature flag to FWorldGenConfig
  - Extend existing GenerateBiomeContent method to use biome-specific PCG graphs when available
  - Parameterize PCG with biome weights, slope data, and water distance in existing PCG generation
  - Enhance existing HISM fallback generation with vegetation clusters, bushes, trees, rocks
  - _Requirements: 5.1, 5.2, 5.3, 5.4_
  - _Deliverables: Enhanced PCGWorldService with graph support_
  - _Acceptance: PCG graphs work when available, fallback maintains parity_

- [ ] 9. Add PCG fallback system maintenance
  - Ensure parity between PCG graphs and existing HISM fallback generation
  - Add detailed error reporting for PCG graph failures to existing error handling
  - Maintain robust fallback path with feature parity in existing PCGWorldService
  - _Requirements: 5.5_
  - _Deliverables: Robust PCG fallback system_
  - _Acceptance: System works with and without PCG Framework, error reporting clear_

## Phase 5: Water System Integration

- [ ] 10. Extend TerrainMaterialSystem for water integration
  - Add bEnableWater feature flag to FWorldGenConfig
  - Extend existing TerrainMaterialSystem with water mask and distance-to-water parameters
  - Implement water-terrain blending for shoreline effects in existing material system
  - Add graceful fallback when water system fails to existing error handling
  - _Requirements: 3.4, 3.5_
  - _Deliverables: Enhanced TerrainMaterialSystem with water support_
  - _Acceptance: Materials show shoreline wetness with water masks, clean rendering with EnableWater=false_

- [ ] 11. Create WaterSystemService for water body management
  - Create new UWaterSystemService class in Services directory
  - Add water body spawning around active tiles driven by sea level detection
  - Implement shoreline detection and foam/wave effects generation
  - Integrate with existing TileStreamingService events for seamless water coverage
  - _Requirements: 3.1, 3.2, 3.3_
  - _Deliverables: WaterSystemService with body management_
  - _Acceptance: Water bodies spawn correctly, shoreline detection works, no hard dependencies_

## Phase 6: Rivers and Lakes System

- [ ] 12. Implement RiverFlowService for flow computation
  - Create IRiverFlowService interface and URiverFlowService class
  - Add low-resolution flow map computation per tile neighborhood using gradient analysis
  - Implement flow pattern derivation from terrain gradients
  - Add spline-based water feature generation following computed flow paths
  - _Requirements: 4.1, 4.2, 4.3_
  - _Deliverables: RiverFlowService with flow computation_
  - _Acceptance: wg.rivers.export shows 0 continuity violations, flow maps generate correctly_

- [ ] 13. Add river carving and lake placement
  - Implement river channel carving into heightfield before normal calculation
  - Add local minima identification for lake placement
  - Implement appropriate shoreline stamping for lakes
  - Add fallback to static water bodies on river generation failure
  - _Requirements: 4.2, 4.4, 4.5_
  - _Deliverables: River carving and lake placement system_
  - _Acceptance: Rivers flow continuously across tiles, lakes place naturally, fallback works_

## Phase 7: Enhanced POI System with Global Coordination

- [ ] 14. Extend POIService for global uniqueness and stamping
  - Add world-level blue-noise sampling for global minimum distance enforcement to existing UPOIService
  - Implement multi-tile reservation systems for large POIs like altars and dungeons
  - Maintain global POI state across tile streaming and world sessions
  - Add terrain stamping helpers for clearing trees and flattening pads
  - _Requirements: 6.1, 6.2, 6.3, 7.1, 7.2_
  - _Deliverables: Enhanced POIService with global coordination and stamping_
  - _Acceptance: wg.poi.validate shows 0 uniqueness violations, global spacing enforced_

- [ ] 15. Add comprehensive terrain stamping integration
  - Implement terrain stamping operations for clearing vegetation in defined areas
  - Add terrain flattening for building pads and structure foundations
  - Implement ramp path creation between nearby POIs for navigation
  - Reconcile terrain changes with HISM removal and persistence systems
  - _Requirements: 7.3, 7.4, 7.5_
  - _Deliverables: Comprehensive terrain stamping system_
  - _Acceptance: POIs integrate naturally with terrain, stamping operations work reliably_

## Phase 8: Complete Async Generation Pipeline

- [ ] 16. Complete AsyncGenerationPipeline in TileStreamingService
  - Expand async work queue with full budgets for height → biome → PCG → VHM mesh pipeline
  - Add prefetch next-ring tiles while demoting far tiles for smooth transitions
  - Implement complete time-sliced generation operations with frame rate targets
  - _Requirements: 8.1, 8.2, 8.3_
  - _Deliverables: Complete async generation pipeline_
  - _Acceptance: p50/p95 budgets from wg.perf.summary within targets, prefetch reduces cache misses_

- [ ] 17. Add comprehensive performance monitoring and budget management
  - Track generation times and provide runtime adjustment capabilities
  - Implement fallback to synchronous generation with performance warnings
  - Add complete per-stage budget enforcement and spike detection (≤ +8ms over 3s window)
  - _Requirements: 8.4, 8.5_
  - _Deliverables: Performance monitoring and budget system_
  - _Acceptance: Spikes ≤ +8ms over 3s window, budget adjustments work at runtime_

## Phase 9: Enhanced Configuration and Runtime Control

- [ ] 18. Add comprehensive console commands for validation
  - Implement wg.map.export, wg.rings.validate, wg.rivers.export commands
  - Add wg.pcg.validate, wg.poi.validate, wg.streaming.budget commands
  - Implement wg.prefetch and performance monitoring commands
  - Add Data Asset management commands (wg.settings.select, wg.biomes.select, etc.)
  - _Requirements: 9.1, 9.2, 9.3, 9.4_
  - _Deliverables: Complete console command suite_
  - _Acceptance: All listed commands registered and safe in PIE and editor world_

- [ ] 19. Implement world map export and validation tools
  - Add world map export functionality for seed validation and visualization
  - Implement real-time generation metrics and bottleneck identification
  - Add configuration error handling with sensible defaults
  - _Requirements: 9.3, 9.4, 9.5_
  - _Deliverables: World map export and validation system_
  - _Acceptance: Export tools work correctly, metrics provide useful bottleneck data_

## Phase 10: Persistence and Determinism Enhancement

- [ ] 20. Enhance persistence system for macro world changes
  - Ensure height modifications replay deterministically over macro world changes
  - Maintain instance and POI journals compatibility with enhanced world generation
  - Add version bump and migration support for world format changes
  - _Requirements: 10.1, 10.2, 10.3_
  - _Deliverables: Enhanced persistence system_
  - _Acceptance: Terrain edits replay correctly, journals work with new generation_

- [ ] 21. Add determinism validation and recovery
  - Implement determinism violation detection with diagnostic information
  - Add world integrity maintenance and recovery mechanisms
  - Ensure first N tiles' checksum stability across relaunch for given seed
  - _Requirements: 10.4, 10.5_
  - _Deliverables: Determinism validation system_
  - _Acceptance: wg.test.determinism shows first N tiles stable, version bump triggers migration_

## Phase 11: Integration and Performance Validation

- [ ] 22. Integrate all services with WorldGenManager
  - Update WorldGenManager initialization sequence with proper dependency order
  - Implement service integration layers (Macro, Enhanced, Water, Content, Performance)
  - Add comprehensive error handling and fallback systems
  - Connect all services with proper event handling and coordination
  - _Requirements: All requirements integration_
  - _Deliverables: Complete system integration_
  - _Acceptance: All services initialize correctly, error handling works, fallbacks engage properly_

- [ ] 23. Validate performance targets and cross-tile continuity
  - Ensure tile generation p50 ≤ 10ms, p95 ≤ 20ms targets
  - Validate streaming spikes ≤ +8ms over 3s window
  - Test cross-tile continuity for rivers, shorelines, and biome rings
  - Verify texture memory ≤ 512MB and MaxActiveTiles ≤ 25 limits
  - _Requirements: Performance and Determinism Requirements, Cross-Tile Continuity Requirements_
  - _Deliverables: Performance validation and continuity testing_
  - _Acceptance: Gate H passes - texture memory ≤ 512MB, MaxActiveTiles ≤ 25, river/shoreline continuity violations = 0_

## Verification Gates Summary

- **Gate E** (Phase 2): Macro topology complete - coastline ratio, histogram shape, seam check pass
- **Gate F** (Phase 6): Water and rivers complete - wg.rivers.export continuity = 0, water coverage ≥ 80% on coasts
- **Gate G** (Phase 7): PCG and POI complete - wg.pcg.validate/wg.poi.validate violations = 0, density within ±20%
- **Gate H** (Phase 11): Pipeline and performance complete - p50/p95/spikes meet targets, memory ≤ limits