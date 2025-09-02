#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Engine/Texture2D.h"
#include "Data/WorldGenTypes.h"
#include "IHeightfieldTextureManager.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UHeightfieldTextureManager : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for managing heightfield texture conversion and streaming
 * Converts heightfield arrays to GPU textures for VHM rendering
 */
class VIBEHEIM_API IHeightfieldTextureManager
{
    GENERATED_BODY()

public:
    /**
     * Create height texture from heightfield data
     * @param TileCoord Coordinate of the tile
     * @param HeightData Array of height values (64x64 for standard tiles)
     * @return Created texture or nullptr on failure
     */
    virtual UTexture2D* CreateHeightTexture(const FTileCoord& TileCoord, const TArray<float>& HeightData) = 0;
    
    /**
     * Update existing texture with modified height data
     * @param TileCoord Coordinate of the tile
     * @param Modifications Array of heightfield modifications to apply
     * @return True if texture update succeeded
     */
    virtual bool UpdateHeightTexture(const FTileCoord& TileCoord, const TArray<FHeightfieldModification>& Modifications) = 0;
    
    /**
     * Create normal map texture from height data
     * @param TileCoord Coordinate of the tile
     * @param HeightData Array of height values
     * @return Created normal texture or nullptr on failure
     */
    virtual UTexture2D* CreateNormalTexture(const FTileCoord& TileCoord, const TArray<float>& HeightData) = 0;
    
    /**
     * Get existing height texture for a tile
     * @param TileCoord Coordinate of the tile
     * @return Existing texture or nullptr if not found
     */
    virtual UTexture2D* GetHeightTexture(const FTileCoord& TileCoord) = 0;

    /**
     * Remove texture for a tile (cleanup when unloading)
     * @param TileCoord Coordinate of the tile
     */
    virtual void RemoveTexture(const FTileCoord& TileCoord) = 0;
    
    /**
     * Manage texture memory and streaming
     * Performs garbage collection and memory optimization
     */
    virtual void OptimizeTextureMemory() = 0;

    /**
     * Get current texture memory usage in MB
     * @return Memory usage in megabytes
     */
    virtual float GetTextureMemoryUsageMB() const = 0;
};
