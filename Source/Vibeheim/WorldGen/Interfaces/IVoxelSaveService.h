#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "../Data/VoxelEditOp.h"
#include "IVoxelSaveService.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UVoxelSaveService : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for managing persistence of voxel modifications using per-chunk append-only JSONL format
 */
class VIBEHEIM_API IVoxelSaveService
{
    GENERATED_BODY()

public:
    /**
     * Record a voxel edit operation for persistence
     * @param EditOp The edit operation to record
     * @return True if the operation was successfully recorded
     */
    virtual bool RecordOp(const FVoxelEditOp& EditOp) = 0;

    /**
     * Flush all pending edit operations to disk with atomic writes
     * @return True if flush was successful
     */
    virtual bool FlushDirty() = 0;

    /**
     * Load and replay all edit operations for a specific chunk
     * @param ChunkCoordinate The chunk coordinate to restore
     * @return True if operations were successfully loaded and replayed
     */
    virtual bool LoadAndReplayForChunk(const FIntVector& ChunkCoordinate) = 0;

    /**
     * Optimize the chunk file by removing redundant operations
     * @param ChunkCoordinate The chunk coordinate to compact
     * @return True if compaction was successful
     */
    virtual bool CompactChunkFile(const FIntVector& ChunkCoordinate) = 0;

    /**
     * Check if there are unsaved edit operations
     * @return True if there are dirty operations pending flush
     */
    virtual bool HasDirtyOperations() const = 0;
};