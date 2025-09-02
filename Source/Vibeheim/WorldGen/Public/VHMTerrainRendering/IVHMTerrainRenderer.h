#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Data/WorldGenTypes.h"
#include "VHMTerrainRendering/VHMTypes.h"
#include "IVHMTerrainRenderer.generated.h"

// Forward declarations
class UWorldGenSettings;
class UHeightfieldService;
class UTileStreamingService;
class UVirtualHeightfieldMeshComponent;

UINTERFACE(MinimalAPI, Blueprintable)
class UVHMTerrainRendererInterface : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for VHM terrain rendering coordination
 * Manages VirtualHeightfieldMeshComponent lifecycle and integration with world generation services
 */
class VIBEHEIM_API IVHMTerrainRendererInterface
{
    GENERATED_BODY()

public:
    /**
     * Initialize VHM system with world generation services
     * @param Settings World generation configuration
     * @param HeightfieldService Service for heightfield data generation
     * @param TileStreamingService Service for tile streaming coordination
     * @return True if initialization succeeded
     */
    virtual bool Initialize(UWorldGenSettings* Settings, 
                          UHeightfieldService* HeightfieldService,
                          UTileStreamingService* TileStreamingService) = 0;
    
    /**
     * Create terrain mesh for a specific tile
     * @param TileCoord Coordinate of the tile to create mesh for
     * @return True if mesh creation succeeded
     */
    virtual bool CreateTerrainMeshForTile(const FTileCoord& TileCoord) = 0;
    
    /**
     * Update terrain mesh when heightfield data changes
     * @param TileCoord Coordinate of the tile to update
     * @param Modifications Array of heightfield modifications to apply
     * @return True if mesh update succeeded
     */
    virtual bool UpdateTerrainMesh(const FTileCoord& TileCoord, const TArray<FHeightfieldModification>& Modifications) = 0;
    
    /**
     * Remove terrain mesh when tile is unloaded
     * @param TileCoord Coordinate of the tile to remove mesh for
     */
    virtual void RemoveTerrainMesh(const FTileCoord& TileCoord) = 0;
    
    /**
     * Get VHM component for a specific tile
     * @param TileCoord Coordinate of the tile
     * @return VHM component instance or nullptr if not found
     */
    virtual UVirtualHeightfieldMeshComponent* GetVHMComponent(const FTileCoord& TileCoord) = 0;

    /**
     * Update LOD levels for all visible tiles based on viewer position
     * @param ViewerPosition Current camera/player position
     */
    virtual void UpdateLODLevels(const FVector& ViewerPosition) = 0;

    /**
     * Get current VHM performance statistics
     * @return Performance statistics structure
     */
    virtual FVHMPerformanceStats GetPerformanceStats() const = 0;

    /**
     * Handle tile streaming events (tile loaded/unloaded)
     * @param TileCoord Coordinate of the affected tile
     * @param bTileLoaded True if tile was loaded, false if unloaded
     */
    virtual void OnTileStreamingEvent(const FTileCoord& TileCoord, bool bTileLoaded) = 0;

    /**
     * Cleanup VHM system resources
     */
    virtual void Cleanup() = 0;
};
