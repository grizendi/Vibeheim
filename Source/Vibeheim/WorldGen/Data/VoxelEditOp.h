#pragma once

#include "CoreMinimal.h"
#include "VoxelEditOp.generated.h"

UENUM(BlueprintType)
enum class EVoxelCSG : uint8
{
    Add     UMETA(DisplayName = "Add"),
    Subtract UMETA(DisplayName = "Subtract")
};

/**
 * Represents a single voxel modification operation for persistence
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FVoxelEditOp
{
    GENERATED_BODY()

    /** World position center of the edit operation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel Edit")
    FVector Center = FVector::ZeroVector;

    /** Radius of the spherical edit operation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel Edit", meta = (ClampMin = "1.0"))
    float Radius = 100.0f;

    /** Type of CSG operation (Add/Subtract) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel Edit")
    EVoxelCSG Operation = EVoxelCSG::Add;

    /** The chunk coordinate affected by this operation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel Edit")
    FIntVector AffectedChunk = FIntVector::ZeroValue;

    /** Timestamp when the operation was performed (Unix timestamp in seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel Edit")
    double Timestamp = 0.0;

    FVoxelEditOp()
    {
        Timestamp = FDateTime::Now().ToUnixTimestamp();
    }

    FVoxelEditOp(const FVector& InCenter, float InRadius, EVoxelCSG InOperation, const FIntVector& InAffectedChunk)
        : Center(InCenter)
        , Radius(InRadius)
        , Operation(InOperation)
        , AffectedChunk(InAffectedChunk)
        , Timestamp(FDateTime::Now().ToUnixTimestamp())
    {
    }
};