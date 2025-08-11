#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "../Data/VoxelEditOp.h"
#include "IVoxelEditService.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UVoxelEditService : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for handling runtime voxel modifications with CSG operations
 */
class VIBEHEIM_API IVoxelEditService
{
    GENERATED_BODY()

public:
    /**
     * Apply a spherical CSG operation to the voxel world
     * @param Center World position center of the sphere
     * @param Radius Radius of the spherical operation
     * @param Operation Type of CSG operation (Add/Subtract)
     * @return True if the operation was successfully applied
     */
    virtual bool ApplySphere(const FVector& Center, float Radius, EVoxelCSG Operation) = 0;

    /**
     * Queue a chunk for asynchronous remeshing after edits
     * @param ChunkCoordinate The chunk coordinate to remesh
     * @return True if remesh was queued successfully
     */
    virtual bool QueueAsyncRemesh(const FIntVector& ChunkCoordinate) = 0;

    /**
     * Check if there are pending edit operations being processed
     * @return True if edits are still being processed
     */
    virtual bool HasPendingEdits() const = 0;
};