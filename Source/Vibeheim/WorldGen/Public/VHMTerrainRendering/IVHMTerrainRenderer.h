#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Data/WorldGenTypes.h"
#include "VHMTerrainRendering/VHMTypes.h"
#include "IVHMTerrainRenderer.generated.h"

// Forward declarations
class UWorldGenSettings;
class UHeightfieldService;
class UVirtualHeightfieldMeshComponent;

UINTERFACE(MinimalAPI, Blueprintable)
class UVHMTerrainRenderer : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for VHM terrain rendering coordination
 * Manages the lifecycle of VirtualHeightfieldMeshComponent instances
 */
class VIBEHEIM_API IVHMTerrainRenderer
{
    GENERATED_BODY()

public:
    /**
     * Initialize VHM system with world generation services
     * @param Settings World generation configuration
     * @param HeightfieldSvc Service providing heightfield data
     * @return True if initialization succeeded
     */
    virtual bool Initialize(UWorldGenSettings* Settings, UHeightfieldService* HeightfieldSvc) = 0;
    
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
     * Get current performance statistics
     * @return Performance stats structure
     */
    virtual FVHMPerformanceStats GetPerformanceStats() const = 0;

    /**
     * Update LOD levels for all visible tiles based on viewer position
     * @param ViewerPosition Current camera/player position
     */
    virtual void UpdateLODLevels(const FVector& ViewerPosition) = 0;

    /**
     * Check if VHM system is properly initialized
     * @return True if system is ready for use
     */
    virtual bool IsInitialized() const = 0;
};