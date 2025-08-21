#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Services/IPCGWorldService.h"
#include "Data/WorldGenTypes.h"
#include "Engine/StaticMesh.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "PCGWorldService.generated.h"

// Forward declarations
class UStaticMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;
class AActor;
class UPCGGraph;
class UInstancePersistenceManager;

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
	
	friend uint32 GetTypeHash(const FHISMComponentArray& Array)
	{
		return GetTypeHash(Array.Components.Num());
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
	virtual bool ValidatePCGGraph(const FString& GraphPath, TArray<FString>& OutErrors) override;

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

private:
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

	// Performance and LOD settings
	UPROPERTY()
	int32 MaxInstancesPerTile;

	UPROPERTY()
	TArray<float> LODDistances;

	// PCG-related properties (always declared but only used when WITH_PCG is true)
	UPROPERTY()
	TObjectPtr<UObject> CurrentPCGGraph; // UPCGGraph* when WITH_PCG is available

	UPROPERTY()
	TMap<EBiomeType, TObjectPtr<UObject>> BiomePCGComponents; // UPCGComponent* when WITH_PCG is available

	// Instance persistence manager
	UPROPERTY()
	TObjectPtr<UInstancePersistenceManager> PersistenceManager;

	/**
	 * Generate content using PCG if available, otherwise use fallback
	 */
	FPCGGenerationData GenerateContentInternal(FTileCoord TileCoord, EBiomeType BiomeType, const TArray<float>& HeightData);

	/**
	 * Generate content using PCG system
	 */
	FPCGGenerationData GeneratePCGContent(FTileCoord TileCoord, EBiomeType BiomeType, const TArray<float>& HeightData, UPCGGraph* PCGGraph);

	/**
	 * Fallback content generation without PCG
	 */
	FPCGGenerationData GenerateFallbackContent(FTileCoord TileCoord, EBiomeType BiomeType, const TArray<float>& HeightData);

	/**
	 * Create HISM component for vegetation
	 */
	UHierarchicalInstancedStaticMeshComponent* CreateHISMComponent(FTileCoord TileCoord, UStaticMesh* Mesh);

	/**
	 * Generate vegetation instances for a tile
	 */
	TArray<FPCGInstanceData> GenerateVegetationInstances(FTileCoord TileCoord, const FBiomeDefinition& BiomeDef, const TArray<float>& HeightData);

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
	 * Get deterministic random seed for a tile
	 */
	uint32 GetTileRandomSeed(FTileCoord TileCoord) const;

	/**
	 * Generate Poisson disc sample point for better vegetation distribution
	 */
	FVector2D GeneratePoissonSample(FRandomStream& RandomStream, FVector2D TileStart, float TileSize, float MinDistance);

	/**
	 * Calculate slope at given heightfield position
	 */
	float CalculateSlope(const TArray<float>& HeightData, int32 X, int32 Y, int32 GridSize);

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
};
