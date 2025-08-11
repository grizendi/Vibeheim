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
};