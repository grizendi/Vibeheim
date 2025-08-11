#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "../Data/WorldGenSettings.h"
#include "IVoxelWorldService.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UVoxelWorldService : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for managing voxel world creation, streaming, and chunk management
 */
class VIBEHEIM_API IVoxelWorldService
{
    GENERATED_BODY()

public:
    /**
     * Initialize the voxel world with the given settings
     * @param Settings Configuration parameters for world generation
     * @return True if initialization was successful
     */
    virtual bool Initialize(const FWorldGenSettings& Settings) = 0;

    /**
     * Set the deterministic seed and version for world generation
     * @param Seed The world generation seed
     * @param Version The world generation version for compatibility
     */
    virtual void SetSeed(int64 Seed, int32 Version) = 0;

    /**
     * Begin streaming world chunks around the specified player anchor
     * @param PlayerAnchor The actor to stream world around
     * @return True if streaming was successfully initiated
     */
    virtual bool BuildWorldAsync(AActor* PlayerAnchor) = 0;

    /**
     * Update chunk loading and unloading based on streaming settings
     * @param DeltaTime Time since last tick
     */
    virtual void TickStreaming(float DeltaTime) = 0;

    /**
     * Force regeneration of a specific chunk
     * @param ChunkCoordinate The chunk coordinate to rebuild
     * @return True if rebuild was queued successfully
     */
    virtual bool RebuildChunkAsync(const FIntVector& ChunkCoordinate) = 0;

    /**
     * Check if the voxel world service is properly initialized
     * @return True if the service is ready for use
     */
    virtual bool IsInitialized() const = 0;
};