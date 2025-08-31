#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Data/WorldGenTypes.h"
#include "ITerrainLODManager.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UTerrainLODManager : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for terrain Level-of-Detail management and performance optimization
 * Handles distance-based quality, culling, and adaptive performance
 */
class VIBEHEIM_API ITerrainLODManager
{
    GENERATED_BODY()

public:
    /**
     * Calculate appropriate LOD level for tile based on distance
     * @param TileCoord Coordinate of the tile
     * @param ViewerPosition Current camera/player position
     * @return LOD level (0 = highest quality, higher numbers = lower quality)
     */
    virtual int32 CalculateLODLevel(const FTileCoord& TileCoord, const FVector& ViewerPosition) = 0;
    
    /**
     * Update LOD levels for all visible tiles
     * @param ViewerPosition Current camera/player position
     */
    virtual void UpdateLODLevels(const FVector& ViewerPosition) = 0;
    
    /**
     * Check if tile should be visible based on distance and culling
     * @param TileCoord Coordinate of the tile
     * @param ViewerPosition Current camera/player position
     * @return True if tile should be rendered
     */
    virtual bool IsTileVisible(const FTileCoord& TileCoord, const FVector& ViewerPosition) = 0;
    
    /**
     * Optimize mesh detail based on performance requirements
     * @param TargetFrameTime Target frame time in milliseconds
     */
    virtual void OptimizeMeshDetail(float TargetFrameTime) = 0;

    /**
     * Get current LOD level for a specific tile
     * @param TileCoord Coordinate of the tile
     * @return Current LOD level or -1 if tile not managed
     */
    virtual int32 GetCurrentLODLevel(const FTileCoord& TileCoord) = 0;

    /**
     * Set maximum LOD levels supported by the system
     * @param MaxLODLevels Maximum number of LOD levels (typically 4-6)
     */
    virtual void SetMaxLODLevels(int32 MaxLODLevels) = 0;

    /**
     * Get performance statistics for LOD system
     * @return Number of LOD transitions this frame
     */
    virtual int32 GetLODTransitionsThisFrame() const = 0;

    /**
     * Reset frame statistics (called once per frame)
     */
    virtual void ResetFrameStats() = 0;
};