# World Generation Coordinate System and Tiling Contract

## Core Constants (Locked Values)

These values are locked and must not be changed without updating the entire world generation system:

- **Tile Size**: 64 meters
- **Sample Spacing**: 1 meter  
- **Max Terrain Height**: 120 meters
- **Samples Per Tile**: 64x64 (derived from tile size / sample spacing)

## World Origin Semantics

The world generation system uses **world-space sampling** with no tile-local bias:

- All coordinates are absolute world positions
- No coordinate transformation or offset is applied at tile boundaries
- Height sampling uses world coordinates directly: `Height = SampleNoise(WorldX, WorldY)`
- This ensures seamless borders between tiles without coordinate discontinuities

## Tile Coordinate System

### FTileCoord Structure
```cpp
struct FTileCoord {
    int32 X, Y;
    
    // Convert world position to tile coordinate
    static FTileCoord FromWorldPosition(FVector WorldPos, float TileSize = 64.0f);
    
    // Convert tile coordinate to world center position
    FVector ToWorldPosition(float TileSize = 64.0f) const;
};
```

### Coordinate Conversion
- **World to Tile**: `TileX = floor(WorldX / 64.0f)`, `TileY = floor(WorldY / 64.0f)`
- **Tile to World**: `WorldX = (TileX + 0.5f) * 64.0f`, `WorldY = (TileY + 0.5f) * 64.0f`
- Tile coordinates represent the tile containing a world position
- Tile-to-world conversion returns the center of the tile

### Hashing
FTileCoord uses combined hashing for efficient TMap usage:
```cpp
friend uint32 GetTypeHash(const FTileCoord& Coord) {
    return HashCombine(GetTypeHash(Coord.X), GetTypeHash(Coord.Y));
}
```

## Streaming Radii Semantics

The system uses three concentric radii around the player:

### Generate Radius (9 tiles)
- **Purpose**: Tiles within this radius have their terrain and content generated
- **Behavior**: Procedural heightfield and PCG content is created
- **Performance**: Must complete within performance targets (≤2ms per tile)

### Load Radius (5 tiles)  
- **Purpose**: Tiles within this radius are kept in memory
- **Behavior**: Generated data is loaded and ready for activation
- **Memory**: Includes heightfield data, PCG instances, and modification deltas

### Active Radius (3 tiles)
- **Purpose**: Tiles within this radius have full simulation active
- **Behavior**: Physics, AI, gameplay systems are fully active
- **Performance**: Most expensive operations are limited to this radius

### Radius Relationships
- Active ⊆ Load ⊆ Generate (Active radius ≤ Load radius ≤ Generate radius)
- Player movement triggers updates to maintain these radii
- Tiles outside Generate radius are unloaded completely
- Tiles between Load and Active radius are dormant but ready

## Deterministic Seeding

All random number generation uses deterministic seeding based on:
```
FinalSeed = Hash(GlobalSeed, TileCoord.X, TileCoord.Y, PrototypeId, InstanceIndex)
```

### Seed Components
- **GlobalSeed**: 64-bit world seed from settings
- **TileCoord**: Tile coordinates (X, Y)
- **PrototypeId**: Unique ID for content type (tree species, rock type, etc.)
- **InstanceIndex**: Index within the tile for multiple instances of same type

### Benefits
- Identical results across multiple runs with same seed
- Network synchronization through seed validation
- Reproducible content for debugging and testing
- Efficient storage (only seed needed, not full state)

## Performance Guardrails

### Timing Targets
- **Tile Generation**: ≤2ms per tile (heightfield + biome determination)
- **PCG Generation**: ≤1ms per tile (vegetation and POI placement)
- **Total Budget**: ≤3ms per tile for complete generation

### Memory Limits
- **HISM Instances**: ≤10,000 per tile
- **Heightfield Data**: 64x64 floats = 16KB per tile
- **PCG Instance Data**: Variable based on content density

### Quality Assurance
- Automated tests verify determinism across platforms
- Performance profiling ensures targets are met
- Visual regression tests validate content placement
- Border seam tests ensure seamless tile transitions

## Integration with UE5 Systems

### World Partition
- Uses 128m cells (2x2 tiles per cell)
- Authored content (towns, dungeons) uses standard World Partition streaming
- Procedural content uses custom tile-based streaming
- Both systems coexist without interference

### PCG Framework
- Partitioned grids align with 64m tile boundaries
- Deterministic seeding adapter provides consistent PCG seeds
- Runtime PCG execution for dynamic content modification
- HISM optimization for performance at scale

### Virtual Heightfield Mesh (VHM)
- Height textures generated from tile data
- Seamless LOD transitions across tile boundaries
- Runtime editing support for terrain modification
- Integration with RVT for material blending