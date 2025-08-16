#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Engine/World.h"
#include "HAL/ThreadSafeBool.h"
#include "Async/AsyncWork.h"
#include "HAL/PlatformProcess.h"
#include "Data/WorldGenSettings.h"
#include "WorldGenTypes.h"
#include "PerformanceProfiler.h"
#include "ChunkStreamingManager.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogChunkStreaming, Log, All);

// Console variable for collision toggle
extern TAutoConsoleVariable<bool> CVarCollisionUpToLOD1;

/** Structure representing a chunk in the streaming system */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FStreamingChunk
{
    GENERATED_BODY()

    /** Chunk coordinate in chunk space */
    UPROPERTY(BlueprintReadOnly)
    FIntVector ChunkCoordinate;

    /** Current LOD level of this chunk */
    UPROPERTY(BlueprintReadOnly)
    EChunkLOD CurrentLOD;

    /** Target LOD level based on distance to player */
    UPROPERTY(BlueprintReadOnly)
    EChunkLOD TargetLOD;

    /** Distance to player anchor in chunks */
    UPROPERTY(BlueprintReadOnly)
    float DistanceToPlayer;

    /** Whether this chunk is currently being generated */
    UPROPERTY(BlueprintReadOnly)
    bool bIsGenerating;

    /** Whether this chunk has collision enabled */
    UPROPERTY(BlueprintReadOnly)
    bool bHasCollision;

    /** Generation start time for performance tracking */
    double GenerationStartTime;

    /** Priority for loading (lower = higher priority) */
    int32 LoadPriority;

    FStreamingChunk()
        : ChunkCoordinate(FIntVector::ZeroValue)
        , CurrentLOD(EChunkLOD::Unloaded)
        , TargetLOD(EChunkLOD::Unloaded)
        , DistanceToPlayer(0.0f)
        , bIsGenerating(false)
        , bHasCollision(false)
        , GenerationStartTime(0.0)
        , LoadPriority(INT32_MAX)
    {
    }

    FStreamingChunk(const FIntVector& InChunkCoordinate)
        : ChunkCoordinate(InChunkCoordinate)
        , CurrentLOD(EChunkLOD::Unloaded)
        , TargetLOD(EChunkLOD::Unloaded)
        , DistanceToPlayer(0.0f)
        , bIsGenerating(false)
        , bHasCollision(false)
        , GenerationStartTime(0.0)
        , LoadPriority(INT32_MAX)
    {
    }
};

/** Async task for chunk generation */
class FChunkGenerationTask : public FNonAbandonableTask
{
public:
    FChunkGenerationTask(const FIntVector& InChunkCoordinate, EChunkLOD InTargetLOD, class UChunkStreamingManager* InManager)
        : ChunkCoordinate(InChunkCoordinate)
        , TargetLOD(InTargetLOD)
        , Manager(InManager)
        , StartTime(FPlatformTime::Seconds())
    {
    }

    void DoWork();

    FORCEINLINE TStatId GetStatId() const
    {
        RETURN_QUICK_DECLARE_CYCLE_STAT(FChunkGenerationTask, STATGROUP_ThreadPoolAsyncTasks);
    }

private:
    FIntVector ChunkCoordinate;
    EChunkLOD TargetLOD;
    class UChunkStreamingManager* Manager;
    double StartTime;
};

typedef FAsyncTask<FChunkGenerationTask> FAsyncChunkGenerationTask;

/**
 * Manages chunk streaming and LOD for the world generation system
 * Handles prioritized loading, LOD management, and performance tracking
 */
UCLASS(BlueprintType, Blueprintable)
class VIBEHEIM_API UChunkStreamingManager : public UObject
{
    GENERATED_BODY()

public:
    UChunkStreamingManager();
    
    /** Destructor - ensures proper cleanup of async tasks */
    virtual ~UChunkStreamingManager();

    /**
     * Initialize the streaming manager with settings
     * @param Settings World generation settings
     * @param InVoxelWorldService The voxel world service to use for chunk operations
     * @return True if initialization was successful
     */
    bool Initialize(const FWorldGenSettings& Settings, class IVoxelWorldService* InVoxelWorldService);

    /**
     * Set the player anchor for streaming calculations
     * @param InPlayerAnchor The actor to stream around
     */
    UFUNCTION(BlueprintCallable, Category = "Chunk Streaming")
    void SetPlayerAnchor(AActor* InPlayerAnchor);

    /**
     * Update streaming system - should be called every tick
     * @param DeltaTime Time since last update
     */
    UFUNCTION(BlueprintCallable, Category = "Chunk Streaming")
    void UpdateStreaming(float DeltaTime);

    /**
     * Force load a specific chunk at target LOD
     * @param ChunkCoordinate The chunk to load
     * @param TargetLOD The LOD level to load at
     * @return True if load was queued successfully
     */
    UFUNCTION(BlueprintCallable, Category = "Chunk Streaming")
    bool ForceLoadChunk(const FIntVector& ChunkCoordinate, EChunkLOD TargetLOD);

    /**
     * Force unload a specific chunk
     * @param ChunkCoordinate The chunk to unload
     * @return True if unload was successful
     */
    UFUNCTION(BlueprintCallable, Category = "Chunk Streaming")
    bool ForceUnloadChunk(const FIntVector& ChunkCoordinate);

    /**
     * Get current streaming statistics
     * @param OutLoadedChunks Number of currently loaded chunks
     * @param OutGeneratingChunks Number of chunks currently being generated
     * @param OutAverageGenerationTime Average generation time in milliseconds
     * @param OutP95GenerationTime P95 generation time in milliseconds
     */
    UFUNCTION(BlueprintCallable, Category = "Chunk Streaming")
    void GetStreamingStats(int32& OutLoadedChunks, int32& OutGeneratingChunks, 
                          float& OutAverageGenerationTime, float& OutP95GenerationTime);

    /**
     * Get all currently loaded chunks
     * @return Array of streaming chunk data
     */
    UFUNCTION(BlueprintCallable, Category = "Chunk Streaming")
    TArray<FStreamingChunk> GetLoadedChunks() const;

    /**
     * Check if a chunk is currently loaded
     * @param ChunkCoordinate The chunk coordinate to check
     * @return True if the chunk is loaded
     */
    UFUNCTION(BlueprintCallable, Category = "Chunk Streaming")
    bool IsChunkLoaded(const FIntVector& ChunkCoordinate);

    /**
     * Get the current LOD of a chunk
     * @param ChunkCoordinate The chunk coordinate to check
     * @return The current LOD level, or Unloaded if not loaded
     */
    UFUNCTION(BlueprintCallable, Category = "Chunk Streaming")
    EChunkLOD GetChunkLOD(const FIntVector& ChunkCoordinate);

    /**
     * Get performance profiler for advanced metrics
     * @return Pointer to performance profiler, or nullptr if not available
     */
    FWorldGenPerformanceProfiler* GetPerformanceProfiler() const { return PerformanceProfiler.Get(); }

    /**
     * Run performance regression tests
     * @param NumTestChunks Number of recent chunks to analyze
     * @return Performance regression test results
     */
    UFUNCTION(BlueprintCallable, Category = "Performance")
    FPerformanceRegressionResults RunPerformanceRegressionTests(int32 NumTestChunks = 50);

    /**
     * Validate LOD0 memory usage against targets
     * @return True if memory usage is within acceptable limits
     */
    UFUNCTION(BlueprintCallable, Category = "Performance")
    bool ValidateLOD0MemoryUsage() const;

    /** Called when a chunk generation task completes */
    void OnChunkGenerationComplete(const FIntVector& ChunkCoordinate, EChunkLOD GeneratedLOD, double GenerationTime);
    
    /**
     * Shutdown the streaming manager and wait for all async tasks to complete
     * This should be called before the object is destroyed
     */
    UFUNCTION(BlueprintCallable, Category = "Chunk Streaming")
    void Shutdown();

protected:
    /**
     * Update chunk priorities and target LODs based on player position
     */
    void UpdateChunkPriorities();

    /**
     * Process chunk loading queue
     */
    void ProcessLoadingQueue();

    /**
     * Process chunk unloading for chunks beyond streaming radius
     */
    void ProcessUnloading();

    /**
     * Calculate target LOD for a chunk based on distance to player
     * @param ChunkCoordinate The chunk coordinate
     * @return The target LOD level
     */
    EChunkLOD CalculateTargetLOD(const FIntVector& ChunkCoordinate) const;

    /**
     * Calculate distance from chunk to player in chunk units
     * @param ChunkCoordinate The chunk coordinate
     * @return Distance in chunk units
     */
    float CalculateChunkDistanceToPlayer(const FIntVector& ChunkCoordinate) const;

    /**
     * Convert world position to chunk coordinate
     * @param WorldPosition Position in world space
     * @return Chunk coordinate
     */
    FIntVector WorldToChunkCoordinate(const FVector& WorldPosition) const;

    /**
     * Start async generation for a chunk
     * @param ChunkCoordinate The chunk to generate
     * @param TargetLOD The target LOD level
     */
    void StartChunkGeneration(const FIntVector& ChunkCoordinate, EChunkLOD TargetLOD);

    /**
     * Update performance statistics
     * @param GenerationTime Time taken for last generation in seconds
     */
    void UpdatePerformanceStats(double GenerationTime);

private:
    /** Current world generation settings */
    UPROPERTY()
    FWorldGenSettings CurrentSettings;

    /** Player anchor for streaming calculations */
    UPROPERTY()
    TObjectPtr<AActor> PlayerAnchor;

    /** Voxel world service for chunk operations */
    class IVoxelWorldService* VoxelWorldService;

    /** Map of all tracked chunks */
    TMap<FIntVector, FStreamingChunk> StreamingChunks;

    /** Priority queue for chunk loading (sorted by priority) */
    TArray<FIntVector> LoadingQueue;

    /** Set of chunks currently being generated */
    TSet<FIntVector> GeneratingChunks;

    /** Active async generation tasks */
    TMap<FIntVector, TSharedPtr<FAsyncChunkGenerationTask>> ActiveTasks;

    /** Whether the manager is initialized */
    bool bIsInitialized;

    /** Performance tracking */
    TArray<double> RecentGenerationTimes;
    double TotalGenerationTime;
    int32 TotalGeneratedChunks;
    float LastStatsLogTime;
    
    /** Rolling performance statistics logging */
    float LastRollingStatsLogTime;
    static constexpr float RollingStatsLogInterval = 2.0f; // Log every 2 seconds
    
    /** Performance profiler for detailed metrics tracking */
    TUniquePtr<FWorldGenPerformanceProfiler> PerformanceProfiler;
    
    /** Structured error logging with seed and chunk coordinates */
    void LogStructuredError(const FString& ErrorMessage, const FIntVector& ChunkCoordinate, const FString& AdditionalContext = TEXT("")) const;

    /** Threading */
    mutable FCriticalSection ChunkMapCriticalSection;
    mutable FCriticalSection StatsCriticalSection;
};