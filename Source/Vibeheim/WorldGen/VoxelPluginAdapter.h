#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Engine/World.h"
#include "Interfaces/IVoxelWorldService.h"
#include "Interfaces/IVoxelEditService.h"
#include "Interfaces/IVoxelSaveService.h"
#include "Data/WorldGenSettings.h"
#include "Data/VoxelEditOp.h"

// Voxel Plugin includes
#include "VoxelWorld.h"
#include "VoxelWorldCreateInfo.h"
#include "VoxelGenerators/VoxelGeneratorPicker.h"
#include "VoxelIntBox.h"

#include "ChunkStreamingManager.h"
#include "FallbackTerrainGenerator.h"
#include "VoxelPluginAdapter.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogWorldGen, Log, All);

/**
 * Adapter class that implements voxel services using VoxelPluginLegacy
 * Provides integration between WorldGen system and VoxelPluginLegacy
 */
UCLASS(BlueprintType, Blueprintable)
class VIBEHEIM_API UVoxelPluginAdapter : public UObject, public IVoxelWorldService, public IVoxelEditService, public IVoxelSaveService
{
    GENERATED_BODY()

public:
    UVoxelPluginAdapter();

    // IVoxelWorldService interface
    virtual bool Initialize(const FWorldGenSettings& Settings) override;
    virtual void SetSeed(int64 Seed, int32 Version) override;
    virtual bool BuildWorldAsync(AActor* PlayerAnchor) override;
    virtual void TickStreaming(float DeltaTime) override;
    virtual bool RebuildChunkAsync(const FIntVector& ChunkCoordinate) override;
    virtual bool IsInitialized() const override;

    // IVoxelEditService interface
    virtual bool ApplySphere(const FVector& Center, float Radius, EVoxelCSG Operation) override;
    virtual bool QueueAsyncRemesh(const FIntVector& ChunkCoordinate) override;
    virtual bool HasPendingEdits() const override;

    // IVoxelSaveService interface
    virtual bool RecordOp(const FVoxelEditOp& EditOp) override;
    virtual bool FlushDirty() override;
    virtual bool LoadAndReplayForChunk(const FIntVector& ChunkCoordinate) override;
    virtual bool CompactChunkFile(const FIntVector& ChunkCoordinate) override;
    virtual bool HasDirtyOperations() const override;

    /**
     * Check if VoxelPluginLegacy is available and properly loaded
     * @return True if the plugin is available
     */
    UFUNCTION(BlueprintCallable, Category = "Voxel Plugin Adapter")
    static bool IsVoxelPluginAvailable();

    /**
     * Get the current voxel world instance
     * @return Pointer to the voxel world, nullptr if not created
     */
    UFUNCTION(BlueprintCallable, Category = "Voxel Plugin Adapter")
    AVoxelWorld* GetVoxelWorld() const { return VoxelWorld; }

    /**
     * Initialize the adapter with settings (Blueprint callable)
     * @param Settings Configuration parameters for world generation
     * @return True if initialization was successful
     */
    UFUNCTION(BlueprintCallable, Category = "Voxel Plugin Adapter")
    bool InitializeAdapter(const FWorldGenSettings& Settings) { return Initialize(Settings); }

    /**
     * Get the chunk streaming manager
     * @return Pointer to the streaming manager, nullptr if not initialized
     */
    UFUNCTION(BlueprintCallable, Category = "Voxel Plugin Adapter")
    UChunkStreamingManager* GetStreamingManager() const { return StreamingManager; }

    /**
     * Get current streaming statistics
     * @param OutLoadedChunks Number of currently loaded chunks
     * @param OutGeneratingChunks Number of chunks currently being generated
     * @param OutAverageGenerationTime Average generation time in milliseconds
     * @param OutP95GenerationTime P95 generation time in milliseconds
     */
    UFUNCTION(BlueprintCallable, Category = "Voxel Plugin Adapter")
    void GetStreamingStats(int32& OutLoadedChunks, int32& OutGeneratingChunks, 
                          float& OutAverageGenerationTime, float& OutP95GenerationTime) const
    {
        if (StreamingManager)
        {
            StreamingManager->GetStreamingStats(OutLoadedChunks, OutGeneratingChunks, OutAverageGenerationTime, OutP95GenerationTime);
        }
        else
        {
            OutLoadedChunks = OutGeneratingChunks = 0;
            OutAverageGenerationTime = OutP95GenerationTime = 0.0f;
        }
    }

    /**
     * Test fallback generation for console commands (public wrapper)
     * @param ChunkCoordinate The chunk that failed to generate
     * @param ErrorMessage The error message describing the failure
     * @param bAttemptFallback Whether to attempt fallback generation
     * @return True if fallback generation was successful or not needed
     */
    UFUNCTION(BlueprintCallable, Category = "Voxel Plugin Adapter")
    bool TestFallbackGeneration(const FIntVector& ChunkCoordinate, const FString& ErrorMessage, bool bAttemptFallback = true)
    {
        return HandleChunkGenerationFailure(ChunkCoordinate, ErrorMessage, bAttemptFallback);
    }

    /**
     * Test structured error logging for console commands (public wrapper)
     * @param ErrorMessage The error message
     * @param ChunkCoordinate The chunk coordinate for context
     * @param AdditionalContext Optional additional context information
     */
    UFUNCTION(BlueprintCallable, Category = "Voxel Plugin Adapter")
    void TestStructuredErrorLogging(const FString& ErrorMessage, const FIntVector& ChunkCoordinate, const FString& AdditionalContext = TEXT(""))
    {
        LogStructuredError(ErrorMessage, ChunkCoordinate, AdditionalContext);
    }

protected:
    /**
     * Create and configure the voxel world actor
     * @return True if world creation was successful
     */
    bool CreateVoxelWorld();

    /**
     * Configure voxel world settings from WorldGen settings
     * @param Settings The WorldGen settings to apply
     */
    void ConfigureVoxelWorldSettings(const FWorldGenSettings& Settings);

    /**
     * Validate plugin availability and log errors if not available
     * @return True if validation passed
     */
    bool ValidatePluginAvailability();

    /**
     * Convert world coordinates to chunk coordinates
     * @param WorldPosition Position in world space
     * @return Chunk coordinate containing the world position
     */
    FIntVector WorldToChunkCoordinate(const FVector& WorldPosition) const;

    /**
     * Get the save file path for a specific chunk
     * @param ChunkCoordinate The chunk coordinate
     * @return Full file path for the chunk's save file
     */
    FString GetChunkSaveFilePath(const FIntVector& ChunkCoordinate) const;

    /**
     * Handle chunk generation failure with structured error reporting
     * @param ChunkCoordinate The chunk that failed to generate
     * @param ErrorMessage The error message describing the failure
     * @param bAttemptFallback Whether to attempt fallback generation
     * @return True if fallback generation was successful or not needed
     */
    bool HandleChunkGenerationFailure(const FIntVector& ChunkCoordinate, const FString& ErrorMessage, bool bAttemptFallback = true);

    /**
     * Log structured error with seed and chunk coordinates
     * @param ErrorMessage The error message
     * @param ChunkCoordinate The chunk coordinate for context
     * @param AdditionalContext Optional additional context information
     */
    void LogStructuredError(const FString& ErrorMessage, const FIntVector& ChunkCoordinate, const FString& AdditionalContext = TEXT("")) const;

private:
    /** The voxel world instance managed by this adapter */
    UPROPERTY()
    TObjectPtr<AVoxelWorld> VoxelWorld;

    /** Current world generation settings */
    UPROPERTY()
    FWorldGenSettings CurrentSettings;

    /** Current player anchor for streaming */
    UPROPERTY()
    TObjectPtr<AActor> PlayerAnchor;

    /** Whether the adapter has been properly initialized */
    bool bIsInitialized;

    /** Current seed for world generation */
    int64 CurrentSeed;

    /** Current world generation version */
    int32 CurrentVersion;

    /** Pending edit operations waiting to be flushed */
    TArray<FVoxelEditOp> PendingEditOps;

    /** Timer for auto-flush of edit operations */
    float FlushTimer;

    /** Whether there are unsaved edit operations */
    bool bHasDirtyOperations;

    /** Chunk streaming manager for LOD and streaming control */
    UPROPERTY()
    TObjectPtr<UChunkStreamingManager> StreamingManager;

    /** Fallback terrain generator for failed chunk generation */
    UPROPERTY()
    TObjectPtr<UFallbackTerrainGenerator> FallbackGenerator;

    /** Map of chunks that have failed generation (for retry tracking) */
    TMap<FIntVector, int32> FailedChunks;

    /** Maximum retry attempts before using fallback generation */
    static constexpr int32 MaxRetryAttempts = 1;
};