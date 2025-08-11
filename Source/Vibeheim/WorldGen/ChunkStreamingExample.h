#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/WorldGenSettings.h"
#include "ChunkStreamingManager.h"
#include "ChunkStreamingExample.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogChunkStreamingExample, Log, All);

class UVoxelPluginAdapter;

/**
 * Example actor demonstrating chunk streaming and LOD management
 * Shows how to integrate the streaming system with gameplay
 */
UCLASS(BlueprintType, Blueprintable)
class VIBEHEIM_API AChunkStreamingExample : public AActor
{
    GENERATED_BODY()

public:
    AChunkStreamingExample();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    /** Log streaming statistics */
    void LogStreamingStats();

public:
    /**
     * Enable or disable collision up to LOD1 (performance testing)
     * @param bEnabled Whether to enable collision up to LOD1
     */
    UFUNCTION(BlueprintCallable, Category = "Chunk Streaming")
    void SetCollisionUpToLOD1(bool bEnabled);

    /**
     * Force load a chunk at a specific world location
     * @param WorldLocation The world location to load
     * @param TargetLOD The LOD level to load at
     */
    UFUNCTION(BlueprintCallable, Category = "Chunk Streaming")
    void ForceLoadChunkAtLocation(const FVector& WorldLocation, EChunkLOD TargetLOD);

    /**
     * Get all currently loaded chunks
     * @return Array of loaded chunk data
     */
    UFUNCTION(BlueprintCallable, Category = "Chunk Streaming")
    TArray<FStreamingChunk> GetCurrentlyLoadedChunks() const;

    /**
     * Get the voxel adapter instance
     * @return Pointer to the voxel adapter
     */
    UFUNCTION(BlueprintCallable, Category = "Chunk Streaming")
    UVoxelPluginAdapter* GetVoxelAdapter() const { return VoxelAdapter; }

protected:
    /** World generation settings */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming Settings")
    FWorldGenSettings StreamingSettings;

    /** Interval for logging stats in seconds */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (ClampMin = "1.0", ClampMax = "60.0"))
    float StatsLogInterval = 5.0f;

private:
    /** The voxel plugin adapter managing the world */
    UPROPERTY()
    TObjectPtr<UVoxelPluginAdapter> VoxelAdapter;

    /** Timer for stats logging */
    float StatsLogTimer = 0.0f;
};