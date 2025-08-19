#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Data/WorldGenTypes.h"
#include "IPCGWorldService.generated.h"

// Forward declarations (always declared for compatibility)
class UPCGGraph;
class UPCGComponent;

/**
 * PCG content generation data
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FPCGGenerationData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCG")
	FTileCoord TileCoord;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCG")
	EBiomeType BiomeType = EBiomeType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCG")
	TArray<FPCGInstanceData> GeneratedInstances;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCG")
	int32 TotalInstanceCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCG")
	float GenerationTimeMs = 0.0f;
};

UINTERFACE(MinimalAPI, Blueprintable)
class UPCGWorldServiceInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for PCG-based world content generation
 * Handles biome-specific content, POI placement, and HISM management
 */
class VIBEHEIM_API IPCGWorldServiceInterface
{
	GENERATED_BODY()

public:
	/**
	 * Initialize PCG system with world generation settings
	 */
	virtual bool Initialize(const FWorldGenConfig& Settings) = 0;

	/**
	 * Initialize PCG graph for biome generation (UObject* for compatibility)
	 */
	virtual bool InitializePCGGraph(UObject* BiomeGraph) = 0;

	/**
	 * Generate biome-specific content for a tile
	 */
	virtual FPCGGenerationData GenerateBiomeContent(FTileCoord TileCoord, EBiomeType BiomeType, const TArray<float>& HeightData) = 0;

	/**
	 * Spawn a point of interest at specified location
	 */
	virtual bool SpawnPOI(FVector Location, const FPOIData& POIData) = 0;

	/**
	 * Update HISM instances for performance optimization
	 */
	virtual bool UpdateHISMInstances(FTileCoord TileCoord) = 0;

	/**
	 * Remove all content in specified area
	 */
	virtual bool RemoveContentInArea(FBox Area) = 0;

	/**
	 * Get PCG generation statistics
	 */
	virtual FPCGPerformanceStats GetPerformanceStats() = 0;

	/**
	 * Enable/disable runtime PCG operations
	 */
	virtual void SetRuntimeOperationsEnabled(bool bEnabled) = 0;

	/**
	 * Clear all cached PCG data
	 */
	virtual void ClearPCGCache() = 0;

	/**
	 * Validate PCG graph compatibility
	 */
	virtual bool ValidatePCGGraph(const FString& GraphPath, TArray<FString>& OutErrors) = 0;
};
