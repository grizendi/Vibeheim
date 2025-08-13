# World Generation Networking Scaffold

This document describes the minimal networking scaffold added to support future multiplayer functionality in the world generation system.

## Overview

The networking scaffold provides the foundation for multiplayer world generation without blocking single-player development. It includes:

- **WorldGenGameState**: Manages replicated world generation properties
- **Networking RPCs**: Server/client communication for voxel edits and chunk synchronization
- **Authority Validation**: Basic security for world generation operations
- **Late-Join Support**: Chunk synchronization for players joining mid-game

## Components

### AWorldGenGameState

The main networking component that handles:

- **Replicated Properties**:
  - `Seed`: World generation seed for consistency across clients
  - `WorldGenVersion`: Version tracking for compatibility

- **Server RPCs**:
  - `Server_ApplyEdit`: Apply voxel edit operations with validation
  - `Server_RequestChunkSync`: Handle chunk synchronization requests

- **Client RPCs**:
  - `Client_ApplyChunkSync`: Send chunk data to requesting clients

- **Multicast RPCs**:
  - `Multicast_ApplyEdit`: Broadcast voxel edits to all clients

### Key Features

#### Deterministic World Generation
- Replicated seed ensures all clients generate identical worlds
- Version tracking prevents compatibility issues
- Chunk-based synchronization for efficient networking

#### Voxel Edit Networking
- Server authority for all world modifications
- Validation to prevent cheating and abuse
- Efficient broadcasting to all connected clients

#### Late-Join Support
- Chunk synchronization for new players
- Edit operation replay for modified chunks
- Distance-based validation to prevent abuse

#### Security & Validation
- Server-side validation of all edit operations
- Distance checks to prevent remote editing
- Rate limiting through operation count limits

## Usage Examples

### Basic Setup

```cpp
// In your GameMode or similar initialization code
AWorldGenGameState* GameState = GetWorld()->GetGameState<AWorldGenGameState>();
if (GameState && GameState->HasWorldGenAuthority())
{
    // Initialize world generation on server
    GameState->InitializeWorldGeneration(1337, 1);
}
```

### Applying Voxel Edits

```cpp
// Server-side edit application
FVoxelEditOp EditOp(Location, Radius, EVoxelCSG::Subtract, ChunkCoordinate);
GameState->ApplyVoxelEdit(EditOp, true); // Broadcast to clients

// Client-side edit request (would typically be in PlayerController)
GameState->Server_ApplyEdit(EditOp);
```

### Late-Join Synchronization

```cpp
// Client requests chunk sync after joining
GameState->RequestChunkSync(ChunkCoordinate);

// Server automatically sends chunk data with all edit operations
```

## Integration with WorldGenManager

The networking scaffold is designed to work alongside the existing `AWorldGenManager`:

1. **Initialization**: GameState manages replicated properties, WorldGenManager handles local generation
2. **Edit Operations**: GameState handles networking, WorldGenManager applies changes locally
3. **Consistency**: Both systems use the same seed and version for deterministic results

## Future Expansion

This scaffold provides the foundation for:

- **Player-specific world modifications**: Per-player edit permissions
- **Chunk ownership**: Assign chunks to specific players for editing rights
- **Conflict resolution**: Handle simultaneous edits from multiple players
- **Performance optimization**: Batch operations and delta compression
- **Persistence integration**: Network-aware save/load systems

## Configuration

Key configuration options in `AWorldGenGameState`:

- `MaxEditOperationsPerChunk`: Limit memory usage (default: 1000)
- `MaxChunkSyncDistance`: Prevent abuse of sync requests (default: 10000cm)
- `bEnableNetworkingDebugLog`: Enable detailed logging for debugging

## Testing

The `AWorldGenNetworkingExample` class demonstrates:

- Initialization of networked world generation
- Application of networked voxel edits
- Late-join player handling
- World consistency validation

## Notes

- This is a **minimal scaffold** - full multiplayer requires additional implementation
- Single-player functionality is **completely unaffected**
- All networking is **optional** - the system works without network components
- **Server authority** is enforced for all world modifications
- **Deterministic generation** ensures consistency across all clients

## Dependencies

The networking scaffold requires:
- `NetCore` module (added to Vibeheim.Build.cs)
- Unreal Engine's built-in networking system
- Existing world generation components (WorldGenManager, VoxelPluginAdapter, etc.)