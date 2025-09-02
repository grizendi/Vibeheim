#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Data/WorldGenTypes.h"
#include "ITerrainMaterialSystem.generated.h"

// Forward declarations
struct FBiomeDefinition;

UINTERFACE(MinimalAPI, Blueprintable)
class UTerrainMaterialSystem : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for terrain material management and biome-based rendering
 * Handles material blending, RVT integration, and biome-specific materials
 */
class VIBEHEIM_API ITerrainMaterialSystem
{
    GENERATED_BODY()

public:
    /**
     * Create material instance for a tile based on biome data
     * @param TileCoord Coordinate of the tile
     * @param BiomeData Biome information for material selection
     * @return Created material instance or nullptr on failure
     */
    virtual UMaterialInstanceDynamic* CreateTileMaterial(const FTileCoord& TileCoord, const FBiomeDefinition& BiomeData) = 0;
    
    /**
     * Update material parameters when biome data changes
     * @param TileCoord Coordinate of the tile
     * @param BiomeData Updated biome information
     * @return True if material update succeeded
     */
    virtual bool UpdateMaterialParameters(const FTileCoord& TileCoord, const FBiomeDefinition& BiomeData) = 0;
    
    /**
     * Handle material blending across tile boundaries
     * @param AdjacentTiles Array of adjacent tile coordinates for blending
     */
    virtual void BlendMaterialsAcrossTiles(const TArray<FTileCoord>& AdjacentTiles) = 0;
    
    /**
     * Initialize Runtime Virtual Texturing system for terrain texturing
     * @return True if RVT initialization succeeded
     */
    virtual bool InitializeRuntimeVirtualTexturing() = 0;

    /**
     * Get material instance for a specific tile
     * @param TileCoord Coordinate of the tile
     * @return Material instance or nullptr if not found
     */
    virtual UMaterialInstanceDynamic* GetTileMaterial(const FTileCoord& TileCoord) = 0;

    /**
     * Remove material for a tile (cleanup when unloading)
     * @param TileCoord Coordinate of the tile
     */
    virtual void RemoveTileMaterial(const FTileCoord& TileCoord) = 0;

    /**
     * Check if RVT system is available and initialized
     * @return True if RVT is ready for use
     */
    virtual bool IsRVTInitialized() const = 0;
};
