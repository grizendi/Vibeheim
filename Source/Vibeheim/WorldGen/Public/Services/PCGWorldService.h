#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Services/IPCGWorldService.h"
#include "Services/PCGWorldServiceTypes.h"
#include "Data/WorldGenTypes.h"
#include "Engine/StaticMesh.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Data/SerializationShims.h"
#include "HAL/IConsoleManager.h"
#include "PCGWorldService.generated.h"

UE_DECLARE_LOG_CATEGORY_EXTERN(LogPCGWorldService, Log, All);

// Forward declarations
class UStaticMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;
class AActor;
class UPCGGraph;
class UInstancePersistenceManager;
#if VHM_PCG_ENABLED
class UPCGComponent;
class UPCGParamData;
class UPCGPointData;
class FPCGSchedulerExecutor;
#endif

/**
 * Wrapper struct for HISM components to make it compatible with TMap
 */
USTRUCT()
struct FHISMComponentArray
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Components;

	FHISMComponentArray() = default;
	
	bool operator==(const FHISMComponentArray& Other) const
	{
		return Components == Other.Components;
	}
};


/**
 * PCG World Service implementation
 * Handles PCG-based content generation with fallback for non-PCG builds
 */
UCLASS(BlueprintType)
class VIBEHEIM_API UPCGWorldService : public UObject, public IPCGWorldServiceInterface
{
	GENERATED_BODY()

public:
        UPCGWorldService();

        virtual void BeginDestroy() override;

	// IPCGWorldServiceInterface interface
	virtual bool Initialize(const FWorldGenConfig& Settings) override;

	virtual bool InitializePCGGraph(UObject* BiomeGraph) override;

	virtual FPCGGenerationData GenerateBiomeContent(FTileCoord TileCoord, EBiomeType BiomeType, const TArray<float>& HeightData) override;
	virtual bool SpawnPOI(FVector Location, const FPOIData& POIData) override;
	virtual bool UpdateHISMInstances(FTileCoord TileCoord) override;
	virtual bool RemoveContentInArea(FBox Area) override;
	virtual FPCGPerformanceStats GetPerformanceStats() override;
	virtual void SetRuntimeOperationsEnabled(bool bEnabled) override;
        virtual void ClearPCGCache() override;
        virtual FPCGGraphValidationResult ValidatePCGGraph(const FString& GraphPath) override;

	/**
	 * Get biome weight for spawn parameters (returns 1.0f when bForceBiome = true)
	 */
	float GetBiomeWeightForSpawn(const FPCGSpawnParams& SpawnParams, EBiomeType BiomeType) const;

	/**
	 * Set biome definitions for PCG generation
	 */
	UFUNCTION(BlueprintCallable, Category = "PCG")
	void SetBiomeDefinitions(const TMap<EBiomeType, FBiomeDefinition>& InBiomeDefinitions);

	/**
	 * Set instance persistence manager for saving/loading modifications
	 */
	UFUNCTION(BlueprintCallable, Category = "PCG")
	void SetPersistenceManager(UInstancePersistenceManager* InPersistenceManager);

	/**
	 * Remove specific instance and persist the change
	 */
	UFUNCTION(BlueprintCallable, Category = "PCG")
	bool RemoveInstance(FTileCoord TileCoord, FGuid InstanceId);

	/**
	 * Add new instance and persist the change
	 */
	UFUNCTION(BlueprintCallable, Category = "PCG")
	bool AddInstance(FTileCoord TileCoord, const FPCGInstanceData& InstanceData);

	/**
	 * Remove specific POI and persist the change
	 */
	UFUNCTION(BlueprintCallable, Category = "PCG")
	bool RemovePOI(FGuid POIId);

	/**
	 * Add new POI and persist the change
	 */
	UFUNCTION(BlueprintCallable, Category = "PCG")
	bool AddPOI(const FPOIData& POIData);

	/**
	 * Load tile with persistence reconciliation
	 */
        UFUNCTION(BlueprintCallable, Category = "PCG")
        bool LoadTileWithPersistence(FTileCoord TileCoord, EBiomeType BiomeType, const TArray<float>& HeightData);

        void AbandonTasksForTile(FTileCoord TileCoord);

        FString GetBiomeGraphAssetPath(EBiomeType BiomeType) const;

private:
#if VHM_PCG_ENABLED
        struct FAttributeValidationResult
        {
                bool bIsValid = true;
                TArray<FString> Errors;
                TArray<FString> Warnings;
        };
#endif
        UPROPERTY()
        bool bHeadless = false;

        UPROPERTY()
        bool bDedicatedServer = false;

	UPROPERTY()
	FWorldGenConfig WorldGenSettings;

	UPROPERTY()
	TMap<EBiomeType, FBiomeDefinition> BiomeDefinitions;

	UPROPERTY()
	TMap<FTileCoord, FPCGGenerationData> GenerationCache;

	UPROPERTY()
	TMap<FTileCoord, FHISMComponentArray> HISMComponents;

	UPROPERTY()
	TMap<FGuid, FPOIData> SpawnedPOIs;

	UPROPERTY()
	TMap<FGuid, TObjectPtr<AActor>> SpawnedPOIActors;

	UPROPERTY()
	FPCGPerformanceStats PerformanceStats;

	UPROPERTY()
	bool bRuntimeOperationsEnabled;

	UPROPERTY()
	TObjectPtr<AActor> TileActor;

	UPROPERTY()
	TMap<EBiomeType, TSoftObjectPtr<UPCGGraph>> BiomePCGGraphs;

#if WITH_PCG
	TMap<EBiomeType, TWeakObjectPtr<UPCGGraph>> ResolvedBiomeGraphs;
#endif

	// Performance and LOD settings
	UPROPERTY()
	int32 MaxInstancesPerTile;

	UPROPERTY()
	TArray<float> LODDistances;

	// PCG-related properties (always declared but only used when WITH_PCG is true)
	UPROPERTY()
	TObjectPtr<UObject> CurrentPCGGraph; // UPCGGraph* when WITH_PCG is available

        UPROPERTY()
#if VHM_PCG_ENABLED
        TMap<EBiomeType, TObjectPtr<UPCGComponent>> BiomePCGComponents;
#else
        TMap<EBiomeType, TObjectPtr<UObject>> BiomePCGComponents;
#endif

	// Instance persistence manager
        UPROPERTY()
        TObjectPtr<UInstancePersistenceManager> PersistenceManager;

#if VHM_PCG_ENABLED
        TWeakObjectPtr<AActor> PCGAnchorActor;
        TUniquePtr<FPCGSchedulerExecutor> SchedulerExecutor;
        TMap<FPCGTaskId, FPCGTaskContext> ActiveTasks;
        TMap<FPCGTaskId, FPCGTaskTelemetry> ActiveTelemetry;
        TMap<EBiomeType, TArray<double>> BiomeLatencySamples;
        TMap<FString, TArray<double>> GraphLatencySamples;
        bool bTelemetryCsvHeaderWritten = false;
        double LastTelemetryFlushSeconds = 0.0;
        FDelegateHandle WorldCleanupHandle;
        FConsoleVariableSinkHandle ConsoleSinkHandle;
#endif

        /**
         * Generate content using PCG if available, otherwise use fallback
         */
        FPCGGenerationData GenerateContentInternal(FTileCoord TileCoord, EBiomeType BiomeType, const TArray<float>& HeightData);

	/**
	 * Attempt PCG graph-based content generation; returns true when graph execution succeeds
	 */
	bool TryGeneratePCGGraphContent(FTileCoord TileCoord, EBiomeType BiomeType, const TArray<float>& HeightData, const FPCGTileMetrics& TileMetrics, FPCGGenerationData& OutData);
#if VHM_PCG_ENABLED
	FAttributeValidationResult ValidateInputAttributes(const UPCGParamData* ParameterData, const UPCGPointData* PointData) const;
#endif
	/**
	 * Generate content using PCG system
	 */
	FPCGGenerationData GeneratePCGContent(FTileCoord TileCoord, EBiomeType BiomeType, const TArray<float>& HeightData, UPCGGraph* PCGGraph, const FPCGTileMetrics* TileMetrics);

	/**
	 * Fallback content generation without PCG
	 */
	FPCGGenerationData GenerateFallbackContent(FTileCoord TileCoord, EBiomeType BiomeType, const TArray<float>& HeightData, bool bUsePCGHeuristics = false, const FPCGTileMetrics* TileMetrics = nullptr);

	/**
	 * Create HISM component for vegetation
	 */
	UHierarchicalInstancedStaticMeshComponent* CreateHISMComponent(FTileCoord TileCoord, UStaticMesh* Mesh);

	/**
	 * Generate vegetation instances for a tile
	 */
	TArray<FPCGInstanceData> GenerateVegetationInstances(FTileCoord TileCoord, const FBiomeDefinition& BiomeDef, const TArray<float>& HeightData, const FPCGTileMetrics* TileMetrics = nullptr, bool bUsePCGHeuristics = false);

	/**
	 * Generate vegetation instances for a tile with spawn parameters (forced biome mode support)
	 */
	TArray<FPCGInstanceData> GenerateVegetationInstances(FTileCoord TileCoord, const FBiomeDefinition& BiomeDef, const TArray<float>& HeightData, const FPCGSpawnParams& SpawnParams, const FPCGTileMetrics* TileMetrics = nullptr, bool bUsePCGHeuristics = false);

	/**
	 * Generate POI instances for a tile
	 */
	TArray<FPOIData> GeneratePOIInstances(FTileCoord TileCoord, const FBiomeDefinition& BiomeDef, const TArray<float>& HeightData);

	/**
	 * Update performance statistics
	 */
	void UpdatePerformanceStats(float GenerationTimeMs, int32 InstanceCount);

	/**
	 * Initialize default biome definitions
	 */
	void InitializeDefaultBiomes();

	/**
	 * Initialize default biome definitions with vegetation rules (for merging)
	 */
	void InitializeDefaultBiomes(TMap<EBiomeType, FBiomeDefinition>& OutDefaultBiomes);

	/**
	 * Get deterministic random seed for a tile
	 */
	uint32 GetTileRandomSeed(FTileCoord TileCoord) const;

	/**
	 * Generate Poisson disc sample point for better vegetation distribution
	 */
	FVector2D GeneratePoissonSample(FRandomStream& RandomStream, FVector2D TileStart, float TileSize, float MinDistance) const;
	void GenerateClusteredSamples(FRandomStream& RandomStream, int32 InstanceCount, FVector2D TileStart, float TileSize, float MinDistance, TArray<FVector2D>& OutSamples) const;

	FPCGTileMetrics AnalyzeTileMetrics(const TArray<float>& HeightData) const;

	float ComputeEnvironmentScale(const FPCGTileMetrics& TileMetrics, const FPCGVegetationRule& VegRule) const;

	UPCGGraph* ResolveBiomePCGGraph(EBiomeType BiomeType);

	/**
	 * Calculate slope at given heightfield position
	 */
	float CalculateSlope(const TArray<float>& HeightData, int32 X, int32 Y, int32 GridSize) const;

	/**
	 * Check if POI placement meets spacing requirements
	 */
	bool CheckPOISpacingRequirements(FVector Location, float MinDistance);

	/**
	 * Apply density limiting to reduce instance count if needed
	 */
	void ApplyDensityLimiting(FPCGGenerationData& GenerationData);

	/**
	 * Create HISM component containers for a tile
	 */
	void CreateHISMComponentsForTile(FTileCoord TileCoord);

	/**
	 * Get or create HISM component for specific mesh and tile
	 */
	UHierarchicalInstancedStaticMeshComponent* GetOrCreateHISMComponent(FTileCoord TileCoord, UStaticMesh* Mesh);

	/**
	 * Estimate current memory usage for performance tracking
	 */
	float EstimateMemoryUsage();

	/**
	 * Find suitable POI location using stratified sampling
	 */
	bool FindPOILocationStratified(FTileCoord TileCoord, const FPOISpawnRule& POIRule, const TArray<float>& HeightData, FRandomStream& RandomStream, FVector& OutLocation);

        /**
         * Apply terrain modification stamp for POI placement
         */
        void ApplyPOITerrainStamp(FVector Location, float Radius);

#if VHM_PCG_ENABLED
        void RegisterTelemetry(FPCGTaskId TaskId, EBiomeType BiomeType, const UPCGGraph& Graph, const FTileCoord& TileCoord);
        void MarkTelemetryStart(FPCGTaskId TaskId);
        void MarkTelemetryCompletion(FPCGTaskId TaskId, bool bSuccess, int32 PointsGenerated, double ExecutionTimeMs, bool bFallback, const FString& StatusLabel);
        void MarkTelemetryFallback(FPCGTaskId TaskId, const FString& Reason, const UPCGGraph* Graph, EBiomeType BiomeType, const FTileCoord& TileCoord);
        void EmitTelemetryLog(const FPCGTaskTelemetry& Telemetry, const FString& StatusLabel, const FString& Reason = FString());
        void UpdateLatencySamples(const FString& GraphKey, EBiomeType BiomeType, double DurationMs);
        double ComputePercentile(const TArray<double>& Samples, double Percent) const;
        void FlushTelemetryCsv();
        void AppendTelemetryCsvRow(const FPCGTaskTelemetry& Telemetry, const FString& StatusLabel, const FString& Reason);
        AActor* EnsurePCGAnchor(UWorld* World);
        UPCGComponent* GetOrCreateBiomeComponent(EBiomeType BiomeType, UPCGGraph& Graph);
        void DestroyBiomeComponent(EBiomeType BiomeType);
        void CleanupAllComponents();
        void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
        void HandleConsoleVariablesChanged();
        void RefreshRuntimeSettingsFromCVars();
        bool CanScheduleNewTask() const;
        void TrackTask(const FPCGTaskContext& Context);
        void ReleaseTrackedTask(FPCGTaskId TaskId);
        void AbandonTasksForWorld(UWorld* World);
        float ResolveFrustumMargin(const UPCGComponent& Component, EBiomeType BiomeType) const;
#endif
};








