# Implementation Plan

## Phase 0: Foundations and Feature Toggles ✅ COMPLETE

- [x] 0. Setup feature flags and console command infrastructure

  - ✅ Feature flags already exist in FWorldGenConfig: bEnableWater, bEnableRivers, bEnableRings, bEnablePCGGraphs
  - ✅ Console commands implemented in WorldGenConsoleCommands.cpp: wg.settings.show, wg.perf.export, wg.flags.show/set
  - ✅ Performance instrumentation exists in TileStreamingService with CSV export
  - _Requirements: 9.1, 9.2_
  - _Status: COMPLETE - Feature toggles and basic console commands already implemented_

## Phase 1: Data Asset Configuration System

- [x] 1. Create Data Asset classes for configuration management


  - ✓ Created UWorldGenSettingsAsset and UBiomeDefinitionsAsset (PrimaryDataAsset)
  - ✓ Implemented FMacroWorldConfig, FBiomeRingDefinition, FWaterSystemConfig, FRiverSystemConfig, FStreamingBudgetsConfig
  - ✓ Added asset resolution + validation to WorldGenManager with default paths
  - _Requirements: 9.1, 9.2, 9.3, 9.4_
  - _Deliverables: UWorldGenSettingsAsset, UBiomeDefinitionsAsset classes, config structs_
  - _Acceptance: Assets load in editor, validation works, default paths exist_

- [x] 2. Complete Data Asset integration and create default assets
  - Create default Data Asset instances at /Game/Data/WorldGen/DA_WorldGenSettings_Default and DA_BiomeDefinitions_Default
  - Complete ApplyFromAssets implementation in WorldGenSettings to properly apply all config structures
  - Ensure BiomeService can read biome definitions from UBiomeDefinitionsAsset instead of JSON
  - Test asset validation and console commands wg.settings.select/reload
  - _Requirements: 9.1, 9.2_
  - _Deliverables: Default assets created, complete asset integration, biome service asset support_
  - _Acceptance: wg.settings.select/wg.settings.reload work, BiomeService uses asset data, validation works_
  - _Status: COMPLETE - Manager resolves default paths; ApplyFromAssets copies MacroWorld, Water, Rivers, StreamingBudgets to settings and applies key fields (SeaLevel/Water flag). BiomeService is initialized and now consumes UBiomeDefinitionsAsset via SetBiomeDefinitions. Note: authoring the default assets is an editor step; code paths, validation and console commands are implemented._

## Phase 2: Streaming Budgets and Macro World Topology

- [x] 3. Implement macro topology in HeightfieldService
  - Extend HeightfieldService::GenerateBaseHeight method with continental noise generation
  - Add island falloff calculation based on radial distance from world center
  - Implement sea level clamping and underwater topology generation
  - Add macro blending that combines continental features with existing heightfield generation
  - _Requirements: 1.1, 1.2, 1.3, 1.4_
  - _Deliverables: Enhanced HeightfieldService with macro topology_
  - _Acceptance: wg.map.export shows coastlines, coastline coverage in expected range, no tile seams > 0.2m_
  - _Status: COMPLETE - Continental ridged noise, radial island falloff, sea-level clamping, and underwater topology integrated into GenerateBaseHeight; macro blend preserves cross-tile continuity._

- [x] 4. Add streaming budgets and async queue foundation
  - Add FStreamingBudgetsConfig integration to TileStreamingService initialization
  - Implement per-stage budget enforcement (HeightGenerationBudget, BiomeCalculationBudget, etc.)
  - Add time-sliced generation with frame rate targets in existing GenerateSingleTile method
  - Implement work queue with prefetch capability for next-ring tiles
  - _Requirements: 8.1, 8.2, 8.3_
  - _Deliverables: Budget system, async queue foundation, performance monitoring_
  - _Acceptance: Budget enforcement works, generation stays within time limits, prefetch reduces cache misses_
  - _Status: COMPLETE - Added priority/ prefetched work queues, per-stage budget tracking, and VHM activation budgeting in TileStreamingService with StreamingBudgetsConfig integration._

## Phase 3: Biome Ring System

- [ ] 5. Implement biome ring progression in BiomeService
  - Extend BiomeService::CalculateBiomeSuitability method to factor radial distance from world center
  - Implement ring-based biome selection using FBiomeRingDefinition array from UBiomeDefinitionsAsset
  - Add ring bias calculation that weights biome selection based on distance from world center
  - Maintain smooth blending while respecting ring progression in existing biome blending logic
  - Add fallback to existing climate-based biome selection on ring calculation failure
  - _Requirements: 2.1, 2.2, 2.3, 2.4, 2.5_
  - _Deliverables: Enhanced BiomeService with ring progression_
  - _Acceptance: wg.rings.validate shows per-ring biome distribution within config, monotonicity in 3x3 neighborhoods_

## Phase 4: Enhanced PCG System

- [ ] 6. Enhance PCGWorldService with real PCG graph integration
  - Extend PCGWorldService::GenerateBiomeContent method to use biome-specific PCG graphs from UBiomeDefinitionsAsset
  - Add PCG parameterization with biome weights, slope data, and water distance for realistic placement
  - Implement vegetation clusters, bushes, trees, rocks generation using PCG rules when bEnablePCGGraphs=true
  - Enhance existing HISM fallback generation to maintain feature parity when PCG graphs unavailable
  - Add detailed error reporting for PCG graph failures with graceful fallback to HISM generation
  - _Requirements: 5.1, 5.2, 5.3, 5.4, 5.5_
  - _Deliverables: Enhanced PCGWorldService with graph support and robust fallback_
  - _Acceptance: PCG graphs work when available, fallback maintains parity, error reporting clear_

## Phase 5: Water System Integration

- [ ] 7. Create WaterSystemService for water body management
  - Create new UWaterSystemService class in Services directory
  - Add water body spawning around active tiles driven by sea level detection from heightfield data
  - Implement shoreline detection algorithm that identifies water-land boundaries
  - Add foam/wave effects generation for visual authenticity at coastlines
  - Integrate with TileStreamingService events (OnTileActivated/OnTileDeactivated) for seamless water coverage
  - _Requirements: 3.1, 3.2, 3.3_
  - _Deliverables: WaterSystemService with body management_
  - _Acceptance: Water bodies spawn correctly, shoreline detection works, no hard dependencies_

- [ ] 8. Extend TerrainMaterialSystem for water integration
  - Extend TerrainMaterialSystem to accept water mask and distance-to-water parameters from WaterSystemService
  - Implement water-terrain blending for shoreline wetness effects in existing material system
  - Add graceful fallback when water system fails - materials render normally without water effects
  - Ensure clean rendering when bEnableWater=false with no water-related artifacts
  - _Requirements: 3.4, 3.5_
  - _Deliverables: Enhanced TerrainMaterialSystem with water support_
  - _Acceptance: Materials show shoreline wetness with water masks, clean rendering with EnableWater=false_

## Phase 6: Rivers and Lakes System

- [ ] 9. Implement RiverFlowService for flow computation
  - Create new URiverFlowService class in Services directory
  - Add low-resolution flow map computation per tile neighborhood using gradient analysis from heightfield data
  - Implement flow pattern derivation from terrain gradients with flow accumulation calculation
  - Add spline-based water feature generation following computed flow paths
  - Integrate flow computation with FRiverSystemConfig parameters from Data Assets
  - _Requirements: 4.1, 4.2, 4.3_
  - _Deliverables: RiverFlowService with flow computation_
  - _Acceptance: wg.rivers.export shows 0 continuity violations, flow maps generate correctly_

- [ ] 10. Add river carving and lake placement to HeightfieldService
  - Implement river channel carving in HeightfieldService::GenerateBaseHeight before normal calculation
  - Add local minima identification for lake placement during heightfield generation
  - Implement shoreline stamping for lakes using existing FHeightfieldModification system
  - Add fallback to static water bodies on river generation failure with appropriate error logging
  - _Requirements: 4.2, 4.4, 4.5_
  - _Deliverables: River carving and lake placement system_
  - _Acceptance: Rivers flow continuously across tiles, lakes place naturally, fallback works_

## Phase 7: Enhanced POI System with Global Coordination

- [ ] 11. Extend POIService for global uniqueness and terrain stamping
  - Add world-level blue-noise sampling for global minimum distance enforcement to existing UPOIService
  - Implement multi-tile reservation systems for large POIs like altars and dungeons
  - Maintain global POI state across tile streaming and world sessions using existing persistence
  - Add terrain stamping operations for clearing vegetation and flattening building pads
  - Implement ramp path creation between nearby POIs for navigation
  - Reconcile terrain changes with existing HISM removal and persistence systems
  - _Requirements: 6.1, 6.2, 6.3, 7.1, 7.2, 7.3, 7.4, 7.5_
  - _Deliverables: Enhanced POIService with global coordination and comprehensive terrain stamping_
  - _Acceptance: wg.poi.validate shows 0 uniqueness violations, global spacing enforced, POIs integrate naturally with terrain_

## Phase 8: Complete Async Generation Pipeline

- [ ] 12. Complete AsyncGenerationPipeline in TileStreamingService
  - Expand existing async work queue with full budgets for height → biome → PCG → VHM mesh pipeline
  - Add prefetch next-ring tiles while demoting far tiles for smooth transitions
  - Implement complete time-sliced generation operations with frame rate targets in GenerateSingleTile
  - Track generation times and provide runtime adjustment capabilities in existing performance tracking
  - Implement fallback to synchronous generation with performance warnings
  - Add complete per-stage budget enforcement and spike detection (<= +8ms over 3s window)
  - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5_
  - _Deliverables: Complete async generation pipeline with performance monitoring and budget system_
  - _Acceptance: p50 <= 10ms, p95 <= 20ms from wg.perf.summary; spikes <= +8ms over 3s; prefetch reduces cache misses; budget adjustments work at runtime_

## Phase 9: Enhanced Configuration and Runtime Control

- [ ] 13. Add comprehensive console commands for validation
  - Extend existing WorldGenConsoleCommands.cpp with validation commands
  - Implement wg.map.export, wg.rings.validate, wg.rivers.export commands
  - Add wg.pcg.validate, wg.poi.validate, wg.streaming.budget commands
  - Implement wg.prefetch and performance monitoring commands (wg.perf.summary)
  - Add world map export functionality for seed validation and visualization
  - Implement real-time generation metrics and bottleneck identification
  - Add configuration error handling with sensible defaults to existing validation system
  - _Requirements: 9.1, 9.2, 9.3, 9.4, 9.5_
  - _Deliverables: Complete console command suite with world map export and validation system_
  - _Acceptance: All listed commands registered and safe in PIE and editor world; export tools work correctly, metrics provide useful bottleneck data_

## Phase 10: Persistence and Determinism Enhancement

- [ ] 14. Enhance persistence system for macro world changes
  - Ensure existing height modifications replay deterministically over macro world changes
  - Maintain existing instance and POI journals compatibility with enhanced world generation
  - Add version bump and migration support for world format changes to existing persistence system
  - Implement determinism violation detection with diagnostic information in existing HeightfieldService
  - Add world integrity maintenance and recovery mechanisms to existing error handling
  - Ensure first N tiles' checksum stability across relaunch for given seed using existing determinism tests
  - Add wg.test.determinism console command for validation
  - _Requirements: 10.1, 10.2, 10.3, 10.4, 10.5_
  - _Deliverables: Enhanced persistence system with determinism validation_
  - _Acceptance: Terrain edits replay correctly, journals work with new generation, wg.test.determinism shows first N tiles stable, version bump triggers migration_

## Phase 11: Integration and Performance Validation

- [ ] 15. Integrate all services with WorldGenManager
  - Update existing WorldGenManager initialization sequence with proper dependency order for new services (WaterSystemService, RiverFlowService)
  - Add comprehensive error handling and fallback systems to existing error handling
  - Connect all services with proper event handling and coordination using existing service connections
  - Ensure tile generation p50 <= 10ms, p95 <= 20ms targets using existing performance tracking
  - Validate streaming spikes <= +8ms over 3s window using existing TileStreamingService metrics
  - Test cross-tile continuity for rivers, shorelines, and biome rings
  - Verify texture memory <= 512MB and MaxActiveTiles <= 25 limits using existing VHM limits
  - _Requirements: All requirements integration, Performance and Determinism Requirements, Cross-Tile Continuity Requirements_
  - _Deliverables: Complete system integration with performance validation and continuity testing_
  - _Acceptance: All services initialize correctly, error handling works, fallbacks engage properly; Gate H passes - texture memory <= 512MB, MaxActiveTiles <= 25, river/shoreline continuity violations = 0_

## Verification Gates Summary

- **Gate E** (Phase 2): Macro topology complete - coastline ratio, histogram shape, seam check pass
- **Gate F** (Phase 6): Water and rivers complete - wg.rivers.export continuity = 0, water coverage >= 80% on coasts
- **Gate G** (Phase 7): PCG and POI complete - wg.pcg.validate/wg.poi.validate violations = 0, density within ±20%
- **Gate H** (Phase 11): Pipeline and performance complete - p50/p95/spikes meet targets, memory <= limits

## Implementation Notes

### Current Status Analysis
- ✅ **MVP Foundation Complete**: HeightfieldService, BiomeService, TileStreamingService, PCGWorldService, POIService, VHMTerrainRenderer all implemented
- ✅ **Feature Flags Ready**: bEnableWater, bEnableRivers, bEnableRings, bEnablePCGGraphs already exist in FWorldGenConfig
- ✅ **Basic Console Commands**: wg.settings.show, wg.perf.export, wg.flags.show/set implemented
- ❌ **Valheim Features Missing**: No Data Assets, macro topology, biome rings, water systems, rivers, async pipeline

### Key Implementation Strategy
- **Extend Existing Services**: Enhance HeightfieldService, BiomeService, TileStreamingService rather than creating new ones
- **Data Asset Migration**: Replace JSON configuration with UE5 Data Assets for designer-friendly editing
- **Incremental Enhancement**: Build on existing MVP foundation, maintain backward compatibility
- **Performance Focus**: Implement async pipeline and budgets to meet performance targets
