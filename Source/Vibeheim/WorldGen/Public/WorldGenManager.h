#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/Object.h"
#include "Data/WorldGenTypes.h"
#include "WorldGenManager.generated.h"

// Forward declarations
class UWorldGenSettings;
class UHeightfieldService;
class UClimateSystem;
class UBiomeService;
class UPCGWorldService;
class UTileStreamingService;
class UPOIService;
class UVHMTerrainRenderer;
class UVHMDebugSystem;
class UWaterSystemService;
class URiverFlowService;
class UWorldGenSettingsAsset;
class UBiomeDefinitionsAsset;
class UWorldGenBuildStateAsset;
struct FTileCoord;

/**
 * Derived runtime behavior based on build mode and build state.
 */
struct FWorldGenRuntimeDecision
{
    bool bUseRuntimeStreaming = true;
    bool bTreatWorldAsBaked = false;
    bool bRequireRebuild = false;
    bool bHasValidBuildState = false;
    bool bBuildStateMatchesConfig = false;
};

VIBEHEIM_API FWorldGenRuntimeDecision EvaluateWorldGenRuntimeDecision(
    const FWorldGenConfig& Config,
    const FWorldBuildState& BuildState,
    bool bHasBuildStateAsset,
    EWorldBuildStatePolicy StalePolicy);

/**
 * World Generation Manager responsible for coordinating all world generation systems
 * Handles initialization, streaming, and performance monitoring
 */
UCLASS(Blueprintable, BlueprintType)
class VIBEHEIM_API AWorldGenManager : public AActor
{
    GENERATED_BODY()

public:
#if WITH_AUTOMATION_TESTS
    friend struct FWorldGenManagerTestAccessor;
#endif

    AWorldGenManager();

    // AActor interface
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    /**
     * Initialize all world generation systems
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    bool InitializeWorldGenSystems();

    /**
     * Update streaming based on player position
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    void UpdateWorldStreaming();

    /**
     * Get current world generation performance statistics
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    void GetWorldGenPerformanceStats(
        UPARAM(ref) float& OutTileGenerationTimeMs,
        UPARAM(ref) float& OutPCGGenerationTimeMs,
        UPARAM(ref) int32& OutLoadedTiles,
        UPARAM(ref) int32& OutPendingLoads
    );

    UPCGWorldService* GetPCGWorldService() const { return PCGWorldService; }

    UHeightfieldService* GetHeightfieldService() const { return HeightfieldService; }

    /**
     * Handle world generation error fallback
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    void HandleWorldGenerationError(const FString& ErrorMessage);

    /**
     * Get VHM terrain renderer
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    UVHMTerrainRenderer* GetVHMTerrainRenderer() const { return VHMTerrainRenderer; }

    /**
     * Get VHM debug system
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    UVHMDebugSystem* GetVHMDebugSystem() const { return VHMDebugSystem; }

    /** Seed used by runtime systems (prefers baked state when valid). */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    int32 GetRuntimeSeed() const;

    /**
     * Get Tile Streaming Service
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    UTileStreamingService* GetTileStreamingService() const { return TileStreamingService; }

    /** Access biome service (for validation/exports) */
    UBiomeService* GetBiomeService() const { return BiomeService; }

    /** Access POI service for validation tools */
    UPOIService* GetPOIService() const { return POIService; }

    /** Access river flow service for debug exports */
    URiverFlowService* GetRiverFlowService() const { return RiverFlowService; }

    /**
     * Data Assets selected for world generation
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Generation|Assets")
    TSoftObjectPtr<UWorldGenSettingsAsset> WorldGenSettingsAsset;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Generation|Assets")
    TSoftObjectPtr<UBiomeDefinitionsAsset> BiomeDefinitionsAsset;

    /** Companion build state asset to validate baked worlds. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Generation|Assets")
    TSoftObjectPtr<UWorldGenBuildStateAsset> WorldBuildStateAsset;

    /** Reload selected data assets (resolve soft refs and validate) */
    UFUNCTION(BlueprintCallable, Category = "World Generation|Assets")
    void ReloadWorldGenAssets();

protected:
    // World generation configuration
    UPROPERTY(BlueprintReadOnly, Category = "World Generation")
    UWorldGenSettings* WorldGenSettings;

    // Core world generation services
    UPROPERTY(BlueprintReadOnly, Category = "World Generation")
    UHeightfieldService* HeightfieldService;

    UPROPERTY(BlueprintReadOnly, Category = "World Generation")
    UClimateSystem* ClimateSystem;

    UPROPERTY(BlueprintReadOnly, Category = "World Generation")
    UBiomeService* BiomeService;

    UPROPERTY(BlueprintReadOnly, Category = "World Generation")
    UPCGWorldService* PCGWorldService;

    UPROPERTY(BlueprintReadOnly, Category = "World Generation")
    UPOIService* POIService;

    UPROPERTY(BlueprintReadOnly, Category = "World Generation")
    UTileStreamingService* TileStreamingService;

    UPROPERTY(BlueprintReadOnly, Category = "World Generation")
    UVHMTerrainRenderer* VHMTerrainRenderer;

    UPROPERTY(BlueprintReadOnly, Category = "World Generation")
    UVHMDebugSystem* VHMDebugSystem;

    UPROPERTY(BlueprintReadOnly, Category = "World Generation")
    UWaterSystemService* WaterSystemService;

    UPROPERTY(BlueprintReadOnly, Category = "World Generation")
    URiverFlowService* RiverFlowService;

    // Streaming parameters
    UPROPERTY(BlueprintReadOnly, Category = "World Generation")
    FVector LastPlayerPosition;

    UPROPERTY(BlueprintReadOnly, Category = "World Generation")
    float StreamingUpdateInterval;

    UPROPERTY(BlueprintReadOnly, Category = "World Generation")
    float LastStreamingUpdateTime;

    // Performance tracking
    UPROPERTY(BlueprintReadOnly, Category = "World Generation")
    float TotalTileGenerationTime;

    UPROPERTY(BlueprintReadOnly, Category = "World Generation")
    float TotalPCGGenerationTime;

    UPROPERTY(BlueprintReadOnly, Category = "World Generation")
    int32 TotalTilesGenerated;

    /** Loaded build state to validate baked worlds. */
    UPROPERTY(BlueprintReadOnly, Category = "World Generation|Build")
    FWorldBuildState ActiveBuildState;

    /** Derived runtime decision for the current build mode/state. */
    FWorldGenRuntimeDecision RuntimeDecision;

private:
    /** Resolve and load data assets with defaults when unset */
    void ResolveWorldGenAssets();

    /** Load and cache the build state asset if provided. */
    void LoadBuildStateAsset();

    /** Log build state evaluation and policy outcomes. */
    void LogBuildStateStatus(const FWorldGenConfig& Config) const;

    /** Determine whether the current world has World Partition enabled. */
    bool IsWorldPartitionAvailable() const;

    /**
     * Calculate player's current tile coordinate
     */
    FTileCoord GetPlayerTileCoordinate() const;

    /**
     * Determine tiles that need to be generated or loaded
     */
    TArray<FTileCoord> CalculateTilesToGenerate(const FTileCoord& PlayerTileCoord);

    /**
     * Generate tiles around the player
     */
    void GenerateSurroundingTiles(const TArray<FTileCoord>& TilesToGenerate);

    /**
     * Update performance tracking metrics
     */
    void UpdatePerformanceMetrics(float TileGenTime, float PCGGenTime);

    /** Log once when runtime generation is invoked in baked mode. */
    bool bLoggedRuntimeGenerationWarning = false;

    /** Track whether a build state asset was present. */
    bool bHasBuildStateAsset = false;

    /** Whether World Partition streaming is active for this world. */
    bool bWorldPartitionStreamingActive = false;

    /** Skip runtime streaming when World Partition owns baked content. */
    bool bSuppressRuntimeStreamingForWorldPartition = false;

    /** Log once when runtime streaming is suppressed for World Partition. */
    bool bLoggedWorldPartitionSuppression = false;
};

#if WITH_AUTOMATION_TESTS
struct FWorldGenManagerTestAccessor
{
    static void SetRuntimeDecision(AWorldGenManager* Manager, const FWorldGenRuntimeDecision& Decision);
    static void SetTileStreamingService(AWorldGenManager* Manager, UTileStreamingService* Service);
    static void SetWorldGenSettings(AWorldGenManager* Manager, UWorldGenSettings* Settings);
    static void SetWorldPartitionSuppression(AWorldGenManager* Manager, bool bSuppress);
};
#endif // WITH_AUTOMATION_TESTS
