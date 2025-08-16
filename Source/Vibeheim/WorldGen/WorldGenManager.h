#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Data/WorldGenSettings.h"
#include "VoxelPluginAdapter.h"
#include "BiomeSystem.h"
#include "ChunkStreamingManager.h"
#include "WorldGenConfigManager.h"
#include "POISystem.h"
#include "DungeonPortalSystem.h"
#include "NoiseGenerator.h"
#include "WorldGenManager.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogWorldGenManager, Log, All);

/**
 * Main coordination actor for the world generation system
 * Manages initialization, streaming, and coordination between all world generation subsystems
 */
UCLASS(BlueprintType, Blueprintable)
class VIBEHEIM_API AWorldGenManager : public AActor
{
    GENERATED_BODY()

public:
    AWorldGenManager();
    
    /** Destructor - ensures cleanup even if EndPlay isn't called */
    virtual ~AWorldGenManager();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    /**
     * Initialize the world generation system with custom settings
     * @param CustomSettings Optional custom settings to use instead of loading from config
     * @return True if initialization was successful
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    bool InitializeWorldGeneration(const FWorldGenSettings& CustomSettings = FWorldGenSettings());

    /**
     * Set the player anchor for world streaming
     * @param NewPlayerAnchor The actor to stream world around (usually the player pawn)
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    void SetPlayerAnchor(AActor* NewPlayerAnchor);

    /**
     * Get the current player anchor
     * @return The current player anchor, nullptr if not set
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    AActor* GetPlayerAnchor() const { return PlayerAnchor; }

    /**
     * Force regeneration of a specific chunk
     * @param ChunkCoordinate The chunk coordinate to rebuild
     * @return True if rebuild was queued successfully
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    bool RebuildChunk(const FIntVector& ChunkCoordinate);

    /**
     * Get the current world generation settings
     * @return Reference to the current settings
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    const FWorldGenSettings& GetWorldGenSettings() const;

    /**
     * Update world generation settings at runtime
     * @param NewSettings The new settings to apply
     * @return True if settings were updated successfully
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    bool UpdateWorldGenSettings(const FWorldGenSettings& NewSettings);

    /**
     * Get the voxel plugin adapter
     * @return Pointer to the voxel plugin adapter, nullptr if not initialized
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    UVoxelPluginAdapter* GetVoxelPluginAdapter() const { return VoxelPluginAdapter; }

    /**
     * Get the biome system
     * @return Pointer to the biome system, nullptr if not initialized
     */
    const FBiomeSystem* GetBiomeSystem() const { return BiomeSystem.Get(); }

    /**
     * Get the chunk streaming manager
     * @return Pointer to the streaming manager, nullptr if not initialized
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    UChunkStreamingManager* GetChunkStreamingManager() const { return ChunkStreamingManager; }

    /**
     * Get current streaming statistics
     * @param OutLoadedChunks Number of currently loaded chunks
     * @param OutGeneratingChunks Number of chunks currently being generated
     * @param OutAverageGenerationTime Average generation time in milliseconds
     * @param OutP95GenerationTime P95 generation time in milliseconds
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    void GetStreamingStats(int32& OutLoadedChunks, int32& OutGeneratingChunks, 
                          float& OutAverageGenerationTime, float& OutP95GenerationTime) const;

    /**
     * Check if the world generation system is fully initialized and ready
     * @return True if all systems are initialized and ready
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    bool IsWorldGenReady() const { return bIsInitialized && bIsReady; }

    /**
     * Evaluate biome at a specific world location
     * @param WorldLocation Location in world space
     * @return Biome evaluation result
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    FBiomeEvaluation EvaluateBiomeAtLocation(const FVector& WorldLocation) const;

    /**
     * Get the POI system
     * @return Pointer to the POI system, nullptr if not initialized
     */
    const FPOISystem* GetPOISystem() const { return POISystem.Get(); }

    /**
     * Get the dungeon portal system
     * @return Pointer to the dungeon portal system, nullptr if not initialized
     */
    const FDungeonPortalSystem* GetDungeonPortalSystem() const { return DungeonPortalSystem.Get(); }

    /**
     * Generate POIs for a specific chunk
     * @param ChunkCoordinate The chunk coordinate to generate POIs for
     * @return Array of placement results for this chunk
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    TArray<FPOIPlacementResult> GeneratePOIsForChunk(const FIntVector& ChunkCoordinate);

    /**
     * Get all POI instances in a specific chunk
     * @param ChunkCoordinate The chunk coordinate to query
     * @return Array of POI instances in the chunk
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    TArray<FPOIInstance> GetPOIsInChunk(const FIntVector& ChunkCoordinate) const;

    /**
     * Get all currently active POI instances
     * @return Array of all active POI instances
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    TArray<FPOIInstance> GetAllActivePOIs() const;

    /**
     * Add a custom POI spawn rule
     * @param SpawnRule The spawn rule to add
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    void AddPOISpawnRule(const FPOISpawnRule& SpawnRule);

    /**
     * Remove a POI spawn rule by type name
     * @param POITypeName The type name of the rule to remove
     * @return True if the rule was found and removed
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    bool RemovePOISpawnRule(const FString& POITypeName);

    /**
     * Get POI placement statistics
     * @param OutTotalAttempts Total placement attempts made
     * @param OutSuccessfulPlacements Number of successful placements
     * @param OutFailedPlacements Number of failed placements
     * @param OutAverageAttemptsPerPOI Average attempts per successful POI
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    void GetPOIPlacementStats(int32& OutTotalAttempts, int32& OutSuccessfulPlacements, 
                             int32& OutFailedPlacements, float& OutAverageAttemptsPerPOI) const;

    /**
     * Generate dungeon portals for a specific chunk
     * @param ChunkCoordinate The chunk coordinate to generate portals for
     * @return Array of placement results for this chunk
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    TArray<FPortalPlacementResult> GeneratePortalsForChunk(const FIntVector& ChunkCoordinate);

    /**
     * Get all portal instances in a specific chunk
     * @param ChunkCoordinate The chunk coordinate to query
     * @return Array of portal instances in the chunk
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    TArray<FDungeonPortal> GetPortalsInChunk(const FIntVector& ChunkCoordinate) const;

    /**
     * Get all currently active portal instances
     * @return Array of all active portal instances
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    TArray<FDungeonPortal> GetAllActivePortals() const;

    /**
     * Add a custom portal spawn rule
     * @param SpawnRule The spawn rule to add
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    void AddPortalSpawnRule(const FPortalSpawnRule& SpawnRule);

    /**
     * Remove a portal spawn rule by type name
     * @param PortalTypeName The type name of the rule to remove
     * @return True if the rule was found and removed
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    bool RemovePortalSpawnRule(const FString& PortalTypeName);

    /**
     * Get portal placement statistics
     * @param OutTotalAttempts Total placement attempts made
     * @param OutSuccessfulPlacements Number of successful placements
     * @param OutFailedPlacements Number of failed placements
     * @param OutAverageAttemptsPerPortal Average attempts per successful portal
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    void GetPortalPlacementStats(int32& OutTotalAttempts, int32& OutSuccessfulPlacements, 
                                int32& OutFailedPlacements, float& OutAverageAttemptsPerPortal) const;

    /**
     * Automatically find and set the player pawn as the anchor
     * @return True if a player pawn was found and set as anchor
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    bool AutoSetPlayerAnchor();

protected:
    /**
     * Called when PIE ends to ensure proper cleanup
     */
    void OnPIEEnded(bool bIsSimulating);

protected:
    /**
     * Initialize all world generation subsystems
     * @return True if all subsystems were initialized successfully
     */
    bool InitializeSubsystems();

    /**
     * Load configuration using the config manager
     * @return True if configuration was loaded successfully
     */
    bool LoadConfiguration();

    /**
     * Handle initialization failure with appropriate error logging and fallback
     * @param FailedSystem Name of the system that failed to initialize
     * @param ErrorMessage Detailed error message
     */
    void HandleInitializationFailure(const FString& FailedSystem, const FString& ErrorMessage);

    /**
     * Implement graceful degradation for system failures
     * @param FailedSystem Name of the system that failed
     * @param ErrorMessage Error message describing the failure
     * @return True if graceful degradation was successful
     */
    bool AttemptGracefulDegradation(const FString& FailedSystem, const FString& ErrorMessage);

    /**
     * Log structured error with seed and context information
     * @param ErrorMessage The error message
     * @param AdditionalContext Optional additional context information
     */
    void LogStructuredError(const FString& ErrorMessage, const FString& AdditionalContext = TEXT("")) const;

    /**
     * Update player anchor tracking
     * @param DeltaTime Time since last update
     */
    void UpdatePlayerAnchorTracking(float DeltaTime);

    /**
     * Handle streaming updates
     * @param DeltaTime Time since last update
     */
    void UpdateStreaming(float DeltaTime);

    /**
     * Log current system status for debugging
     */
    void LogSystemStatus() const;

private:
    /** Configuration manager for loading and managing settings */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Generation", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UWorldGenConfigManager> ConfigManager;

    /** Voxel plugin adapter for voxel world management */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Generation", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UVoxelPluginAdapter> VoxelPluginAdapter;

    /** Chunk streaming manager for LOD and streaming control */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Generation", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UChunkStreamingManager> ChunkStreamingManager;

    /** Biome generation system */
    TUniquePtr<FBiomeSystem> BiomeSystem;

    /** POI placement and management system */
    TUniquePtr<FPOISystem> POISystem;

    /** Dungeon portal placement and management system */
    TUniquePtr<FDungeonPortalSystem> DungeonPortalSystem;

    /** Noise generator for deterministic world generation */
    TUniquePtr<FNoiseGenerator> NoiseGenerator;

    /** Current player anchor for streaming */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Generation", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<AActor> PlayerAnchor;

    /** Whether the manager has been initialized */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Generation", meta = (AllowPrivateAccess = "true"))
    bool bIsInitialized;

    /** Whether all systems are ready for world generation */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Generation", meta = (AllowPrivateAccess = "true"))
    bool bIsReady;

    /** Whether to automatically find and track player pawn */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Generation", meta = (AllowPrivateAccess = "true"))
    bool bAutoTrackPlayer;

    /** Interval for automatic player anchor updates (in seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Generation", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", ClampMax = "5.0"))
    float PlayerTrackingInterval;

    /** Timer for player anchor tracking */
    float PlayerTrackingTimer;

    /** Whether to enable debug logging */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (AllowPrivateAccess = "true"))
    bool bEnableDebugLogging;

    /** Interval for debug status logging (in seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", ClampMax = "60.0"))
    float DebugLoggingInterval;

    /** Timer for debug logging */
    float DebugLoggingTimer;
};