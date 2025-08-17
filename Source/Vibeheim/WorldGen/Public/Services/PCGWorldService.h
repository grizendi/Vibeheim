#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Services/IPCGWorldService.h"
#include "Data/WorldGenTypes.h"
#include "PCGWorldService.generated.h"

// Forward declarations
class UStaticMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;

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

private:
	UPROPERTY()
	FWorldGenConfig WorldGenSettings;

	UPROPERTY()
	TMap<EBiomeType, FBiomeDefinition> BiomeDefinitions;

	UPROPERTY()
	TMap<FTileCoord, FPCGGenerationData> GenerationCache;

	UPROPERTY()
	TMap<FTileCoord, UHierarchicalInstancedStaticMeshComponent*> HISMComponents;

	UPROPERTY()
	FPCGPerformanceStats PerformanceStats;

	UPROPERTY()
	bool bRuntimeOperationsEnabled;

	// PCG-related properties (always declared but only used when WITH_PCG is true)
	UPROPERTY()
	TObjectPtr<UObject> CurrentPCGGraph; // UPCGGraph* when WITH_PCG is available

	UPROPERTY()
	TMap<EBiomeType, TObjectPtr<UObject>> BiomePCGComponents; // UPCGComponent* when WITH_PCG is available

	/**
	 * Generate content using PCG if available, otherwise use fallback
	 */
	FPCGGenerationData GenerateContentInternal(FTileCoord TileCoord, EBiomeType BiomeType, const TArray<float>& HeightData);

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
};
