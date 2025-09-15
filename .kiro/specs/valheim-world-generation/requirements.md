# Requirements Document

## Introduction

The Valheim-Style World Generation feature transforms the existing MVP world generation system into a comprehensive Valheim-inspired world with macro topology, biome ring systems, water bodies, rivers/lakes, and advanced PCG content generation. This system builds upon the existing heightfield foundation to create authentic Valheim-style exploration with continental shaping, coastal distribution, ring-based biome progression, flowing water systems, and rich procedural content that matches the depth and feel of Valheim's world design.

## Requirements

### Requirement 1: Macro World Topology and Continental Shaping

**User Story:** As a player, I want to explore a world with realistic continental topology and coastal regions, so that the world feels like a believable landmass with proper geography.

#### Acceptance Criteria

1. WHEN the world generates THEN the system SHALL create continental noise with low-frequency ridged patterns for realistic landmass shapes
2. WHEN generating terrain THEN the system SHALL apply island falloff by radial distance to create natural coastlines
3. WHEN terrain reaches sea level THEN the system SHALL clamp ocean depths and create proper underwater topology
4. WHEN configuring world generation THEN the system SHALL expose WorldRadiusMeters, ContinentScale, IslandFalloff, OceanDepth, and CoastSharpness parameters
5. IF continental shaping fails THEN the system SHALL fallback to existing heightfield generation with error logging

### Requirement 2: Biome Ring System with Distance-Based Progression

**User Story:** As a player, I want to experience biome progression that follows Valheim's ring system, so that exploration has meaningful progression from safe starting areas to dangerous outer regions.

#### Acceptance Criteria

1. WHEN the world generates THEN the system SHALL implement biome rings including Meadows, Forest, Mountains, Swamp, Plains, Mistlands, DeepNorth, and Ashlands
2. WHEN determining biomes THEN the system SHALL use radial distance from world center to influence biome selection with configurable ring weights
3. WHEN biomes transition THEN the system SHALL maintain smooth blending while respecting ring-based progression rules
4. WHEN configuring biomes THEN the system SHALL support target radial bands and weights per biome type in JSON configuration
5. IF ring calculation fails THEN the system SHALL fallback to climate-based biome selection with appropriate logging

### Requirement 3: Water System Integration with Bodies and Shorelines

**User Story:** As a player, I want to see realistic water bodies, shorelines, and coastal features, so that the world feels alive with proper water systems.

#### Acceptance Criteria

1. WHEN water generates THEN the system SHALL spawn Water Bodies around active tiles driven by sea level detection
2. WHEN shorelines form THEN the system SHALL detect coastlines and generate foam/wave effects for visual authenticity
3. WHEN tiles stream THEN the system SHALL integrate water spawning with tile streaming events for seamless water coverage
4. WHEN materials blend THEN the system SHALL extend TerrainMaterialSystem to accept water mask or distance-to-water parameters
5. IF water system fails THEN the system SHALL continue terrain generation without water features and log errors

### Requirement 4: Rivers and Lakes with Flow Networks

**User Story:** As a player, I want to discover rivers and lakes that feel natural and connected, so that water features enhance exploration and provide navigation landmarks.

#### Acceptance Criteria

1. WHEN generating terrain THEN the system SHALL precompute low-resolution flow maps per tile neighborhood using gradient analysis
2. WHEN rivers form THEN the system SHALL derive flow patterns from terrain gradients and carve channels into heightfield before normal calculation
3. WHEN water flows THEN the system SHALL spawn spline-based water features following computed flow paths
4. WHEN lakes generate THEN the system SHALL identify local minima and create lake placement with appropriate shoreline stamping
5. IF river generation fails THEN the system SHALL continue with static water bodies and log flow computation errors

### Requirement 5: Enhanced PCG with Real Graph Integration

**User Story:** As a player, I want rich procedural content that uses proper PCG graphs, so that the world feels detailed and varied like Valheim's environments.

#### Acceptance Criteria

1. WHEN PCG Framework is available THEN the system SHALL replace fallback generation with biome-specific PCG graphs
2. WHEN content spawns THEN the system SHALL parameterize PCG with biome weights, slope data, and water distance for realistic placement
3. WHEN generating clusters THEN the system SHALL spawn vegetation clusters, bushes, trees, rocks, and roads using PCG rules
4. WHEN content places THEN the system SHALL respect terrain-aware stamping and collision rules for natural integration
5. IF PCG graphs fail THEN the system SHALL fallback to existing HISM generation with detailed error reporting

### Requirement 6: Global POI System with World-Scale Uniqueness

**User Story:** As a player, I want to discover unique locations and structures that feel special and properly spaced, so that exploration rewards are meaningful and well-distributed.

#### Acceptance Criteria

1. WHEN POIs generate THEN the system SHALL implement world-level blue-noise sampling for global minimum distance enforcement
2. WHEN placing unique structures THEN the system SHALL support multi-tile reservation systems for large POIs like altars and dungeons
3. WHEN POIs persist THEN the system SHALL maintain global POI state across tile streaming and world sessions
4. WHEN structures place THEN the system SHALL use terrain stamping library for clearing trees, flattening pads, and creating ramp paths
5. IF global POI placement fails THEN the system SHALL continue with per-tile POI generation and log coordination errors

### Requirement 7: Advanced Terrain Stamping and Integration

**User Story:** As a developer, I want comprehensive terrain stamping tools, so that POIs and structures integrate naturally with the procedural terrain.

#### Acceptance Criteria

1. WHEN structures place THEN the system SHALL provide terrain stamping operations for clearing vegetation in defined areas
2. WHEN POIs generate THEN the system SHALL support terrain flattening for building pads and structure foundations
3. WHEN paths create THEN the system SHALL implement ramp path generation between nearby POIs for navigation
4. WHEN stamping occurs THEN the system SHALL reconcile terrain changes with HISM removal and persistence systems
5. IF stamping operations fail THEN the system SHALL place POIs without terrain modification and log stamping errors

### Requirement 8: Streaming Performance and Async Generation Pipeline

**User Story:** As a player, I want smooth world streaming without performance hitches, so that exploration feels seamless and responsive.

#### Acceptance Criteria

1. WHEN tiles generate THEN the system SHALL implement async work queue with budgets for height → biome → PCG → VHM mesh pipeline
2. WHEN streaming occurs THEN the system SHALL prefetch next-ring tiles while demoting far tiles for smooth transitions
3. WHEN generation runs THEN the system SHALL maintain frame rate targets through time-sliced generation operations
4. WHEN performance monitoring THEN the system SHALL track generation times and provide runtime adjustment capabilities
5. IF async generation fails THEN the system SHALL fallback to synchronous generation with performance warnings

### Requirement 9: Enhanced Configuration and Runtime Control

**User Story:** As a developer, I want comprehensive configuration options and runtime controls, so that world generation can be tuned and debugged effectively.

#### Acceptance Criteria

1. WHEN configuring generation THEN the system SHALL move hardcoded radii and parameters to JSON configuration files
2. WHEN debugging THEN the system SHALL provide runtime console commands for tweaking generation parameters
3. WHEN validating worlds THEN the system SHALL support world map export functionality for seed validation and visualization
4. WHEN monitoring performance THEN the system SHALL expose real-time generation metrics and bottleneck identification
5. IF configuration loading fails THEN the system SHALL use sensible defaults and log configuration errors

### Requirement 10: Persistence and Determinism Enhancement

**User Story:** As a player, I want world modifications and generated content to persist reliably, so that my changes to the world are preserved across sessions.

#### Acceptance Criteria

1. WHEN terrain edits occur THEN the system SHALL ensure height modifications replay deterministically over macro world changes
2. WHEN instances modify THEN the system SHALL maintain instance and POI journals that work with enhanced world generation
3. WHEN versions change THEN the system SHALL provide version bump and migration support for world format changes
4. WHEN persistence fails THEN the system SHALL maintain world integrity and provide recovery mechanisms
5. IF determinism breaks THEN the system SHALL detect and report determinism violations with diagnostic information

## Performance and Determinism Requirements

### Performance Targets
- **Tile Generation**: p50 ≤ 10 ms, p95 ≤ 20 ms for height+biome per tile in 9×9 generate radius
- **Streaming Spikes**: activation spike ≤ +8 ms over 3s window (matching existing Gate D requirements)
- **VHM Mesh Creation**: average per-tile StreamInMs ≤ 6 ms with MaxActiveTiles ≤ 25
- **Texture Memory**: Height + normal textures ≤ 512 MB when ActiveRadius=3 and LoadRadius=5
- **Determinism**: First N tiles' checksum stable across relaunch for a given seed

### Cross-Tile Continuity Requirements
- **Rivers**: Flow lines SHALL remain continuous across tile edges with < 1 sample mismatch at boundaries
- **Water Bodies**: Shorelines align across adjacent tiles with no height seams > 0.2 m at coasts
- **Biome Rings**: Ring classification transitions are monotonic in radial distance within tolerance

## Configuration Schema Changes

### Macro World Parameters
```json
{
  "MacroWorld": {
    "WorldRadiusMeters": 10000.0,
    "ContinentScale": 0.001,
    "IslandFalloff": 2.0,
    "OceanDepth": -50.0,
    "CoastSharpness": 1.5
  }
}
```

### Biome Ring Configuration
```json
{
  "BiomeRings": [
    {
      "BiomeType": 0,
      "InnerRadius": 0.0,
      "OuterRadius": 1000.0,
      "BlendWidth": 200.0,
      "Weight": 1.0
    }
  ]
}
```

### Rivers and Lakes Parameters
```json
{
  "Rivers": {
    "MinWidth": 5.0,
    "MaxWidth": 20.0,
    "BedDepth": 2.0,
    "SplineSmooth": 0.5,
    "LakeMinRadius": 50.0
  }
}
```

### Streaming Budget Configuration
```json
{
  "StreamingBudgets": {
    "StreamingBudgetMsPerTick": 2.0,
    "PrefetchRings": 2,
    "WorkQueueThreads": 2,
    "MaxActiveTiles": 25
  }
}
```

### Water System Integration
```json
{
  "WaterSystem": {
    "EnableWaterSystem": true,
    "ShorelineFoam": {
      "Enable": true,
      "FoamWidth": 10.0
    }
  }
}
```

### Persistence and Versioning
```json
{
  "Persistence": {
    "WorldGenVersion": 2,
    "JournalMigrationPolicy": "replay"
  }
}
```

## Validation and Tooling Requirements

### Required Console Commands
- `wg.map.export [seed] [out.png]`: Export height/biome/water overview for seed validation
- `wg.rings.validate [samples=10000]`: Report biome distribution by ring band and blending stats
- `wg.rivers.export [tileX] [tileY]`: Dump flow vectors and spline segments for neighborhood
- `wg.pcg.validate [tileX] [tileY]`: Print instance counts per rule and collision/stamp conflicts
- `wg.poi.validate [radius]`: Enforce global minimum distances and uniqueness, print violations
- `wg.streaming.budget <ms>`: Adjust pipeline budgets
- `wg.prefetch <rings>`: Adjust prefetch ring count

### Verification Gates
- **Gate E - World Shape & Coasts**: Coastline ratio, height histogram, seam checks pass
- **Gate F - Water & Rivers**: Water coverage on coasts, river continuity, spline counts, lake minima count validated
- **Gate G - PCG & POI**: Instance density by biome, cluster stats, uniqueness violations = 0
- **Gate H - Pipeline & Perf**: Budgets met (p50/p95), spike caps, memory limits enforced

## Compatibility Requirements

### Plugin Dependencies
- **UE Water Plugin**: Optional, controlled by EnableWaterSystem flag with graceful fallback
- **PCG Framework**: Optional, maintains robust fallback HISM path with feature parity
- **VHM System**: Required, with texture memory budget enforcement and MaxActiveTiles limits

## Implementation Traceability

### File Locations for Key Features
- **Macro topology**: `Source/Vibeheim/WorldGen/Private/Services/HeightfieldService.cpp:239`
- **Biome rings**: `Source/Vibeheim/WorldGen/Private/Services/BiomeService.cpp:110, 203`
- **Water integration**: New `UWaterService` in `Source/Vibeheim/WorldGen/Private/WorldGenManager.cpp`
- **Rivers**: Pre-pass carving in `HeightfieldService.cpp:59` before normals/erosion
- **PCG graphs**: `Source/Vibeheim/WorldGen/Private/Services/PCGWorldService.cpp:119, 161`
- **Streaming budgets**: `Source/Vibeheim/WorldGen/Private/Services/TileStreamingService.cpp:97, 167, 302, 491`
- **Materials**: `Source/Vibeheim/WorldGen/Private/VHMTerrainRendering/TerrainMaterialSystem.cpp`