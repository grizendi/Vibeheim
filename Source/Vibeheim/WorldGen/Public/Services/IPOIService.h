#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Data/WorldGenTypes.h"
#include "IPOIService.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UPOIServiceInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for POI (Point of Interest) placement and management services
 * Handles stratified placement, filtering, and terrain stamping
 */
class VIBEHEIM_API IPOIServiceInterface
{
	GENERATED_BODY()

public:
	/**
	 * Initialize the POI service with world generation settings
	 */
	virtual bool Initialize(const FWorldGenConfig& Settings) = 0;

	/**
	 * Generate POIs for a specific tile using stratified placement
	 */
	virtual TArray<FPOIData> GenerateTilePOIs(FTileCoord TileCoord, EBiomeType BiomeType, const TArray<float>& HeightData) = 0;

	/**
	 * Validate POI placement location with slope and flat ground checks
	 */
	virtual bool ValidatePOIPlacement(FVector Location, const FPOISpawnRule& Rule, const TArray<float>& HeightData, FTileCoord TileCoord) = 0;

	/**
	 * Check distance requirements between POIs
	 */
	virtual bool CheckPOIDistanceRequirements(FVector Location, const FPOISpawnRule& Rule, const TArray<FPOIData>& ExistingPOIs) = 0;

	/**
	 * Apply terrain flattening/clearing stamp for POI placement
	 */
	virtual bool ApplyTerrainStamp(FVector Location, float Radius, TArray<float>& HeightData, FTileCoord TileCoord) = 0;

	/**
	 * Get all POIs within a specific area
	 */
	virtual TArray<FPOIData> GetPOIsInArea(FVector Center, float Radius) = 0;

	/**
	 * Save POI data to persistence system
	 */
	virtual bool SavePOIData() = 0;

	/**
	 * Load POI data from persistence system
	 */
	virtual bool LoadPOIData() = 0;

	/**
	 * Get performance statistics for POI generation
	 */
	virtual void GetPerformanceStats(float& OutAverageGenerationTimeMs, int32& OutTotalPOIs) = 0;
};