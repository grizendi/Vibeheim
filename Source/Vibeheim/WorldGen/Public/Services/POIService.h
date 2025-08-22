#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Services/IPOIService.h"
#include "Data/WorldGenTypes.h"
#include "POIService.generated.h"

// Forward declarations
class UBiomeService;
class UHeightfieldService;

/**
 * Stratified sampling grid configuration for POI placement
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FStratifiedSamplingConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sampling")
	int32 GridSize = 4; // 4x4 grid for stratified sampling

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sampling")
	float CellSize = 16.0f; // Size of each sampling cell in meters

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sampling")
	int32 MaxAttemptsPerCell = 3; // Maximum placement attempts per cell

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sampling")
	float MinCellSpacing = 8.0f; // Minimum spacing within a cell
};

/**
 * POI placement validation settings
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FPOIValidationSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation")
	float FlatGroundCheckRadius = 3.0f; // Radius for 3x3 flat ground validation

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation")
	float FlatGroundTolerance = 2.0f; // Maximum height variation for flat ground

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation")
	float TerrainStampRadius = 5.0f; // Default radius for terrain flattening

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation")
	float TerrainStampStrength = 0.8f; // Strength of terrain flattening
};

/**
 * Wrapper struct for POI arrays to enable TMap storage
 */
USTRUCT()
struct VIBEHEIM_API FPOITileData
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FPOIData> POIs;

	FPOITileData() = default;
	FPOITileData(const TArray<FPOIData>& InPOIs) : POIs(InPOIs) {}
};

/**
 * POI service implementation with stratified placement and advanced filtering
 * Handles biome-specific POI rules, terrain validation, and persistence
 */
UCLASS(BlueprintType)
class VIBEHEIM_API UPOIService : public UObject, public IPOIServiceInterface
{
	GENERATED_BODY()

public:
	UPOIService();

	// IPOIServiceInterface interface
	virtual bool Initialize(const FWorldGenConfig& Settings) override;
	virtual TArray<FPOIData> GenerateTilePOIs(FTileCoord TileCoord, EBiomeType BiomeType, const TArray<float>& HeightData) override;
	virtual bool ValidatePOIPlacement(FVector Location, const FPOISpawnRule& Rule, const TArray<float>& HeightData, FTileCoord TileCoord) override;
	virtual bool CheckPOIDistanceRequirements(FVector Location, const FPOISpawnRule& Rule, const TArray<FPOIData>& ExistingPOIs) override;
	virtual bool ApplyTerrainStamp(FVector Location, float Radius, TArray<float>& HeightData, FTileCoord TileCoord) override;
	virtual TArray<FPOIData> GetPOIsInArea(FVector Center, float Radius) override;
	virtual bool SavePOIData() override;
	virtual bool LoadPOIData() override;
	virtual void GetPerformanceStats(float& OutAverageGenerationTimeMs, int32& OutTotalPOIs) override;

	/**
	 * Set biome service for accessing biome-specific POI rules
	 */
	UFUNCTION(BlueprintCallable, Category = "POI")
	void SetBiomeService(UBiomeService* InBiomeService);

	/**
	 * Set heightfield service for terrain data access
	 */
	UFUNCTION(BlueprintCallable, Category = "POI")
	void SetHeightfieldService(UHeightfieldService* InHeightfieldService);

	/**
	 * Update stratified sampling configuration
	 */
	UFUNCTION(BlueprintCallable, Category = "POI")
	void UpdateSamplingConfig(const FStratifiedSamplingConfig& NewConfig);

	/**
	 * Update POI validation settings
	 */
	UFUNCTION(BlueprintCallable, Category = "POI")
	void UpdateValidationSettings(const FPOIValidationSettings& NewSettings);

	/**
	 * Get all POIs for a specific tile
	 */
	UFUNCTION(BlueprintCallable, Category = "POI")
	TArray<FPOIData> GetTilePOIs(FTileCoord TileCoord) const;

	/**
	 * Remove POI by ID (for gameplay interactions)
	 */
	UFUNCTION(BlueprintCallable, Category = "POI")
	bool RemovePOI(const FGuid& POIId);

	/**
	 * Add custom POI at specific location
	 */
	UFUNCTION(BlueprintCallable, Category = "POI")
	bool AddCustomPOI(const FPOIData& POIData);

private:
	UPROPERTY()
	FWorldGenConfig WorldGenSettings;

	UPROPERTY()
	FStratifiedSamplingConfig SamplingConfig;

	UPROPERTY()
	FPOIValidationSettings ValidationSettings;

	UPROPERTY()
	UBiomeService* BiomeService;

	UPROPERTY()
	UHeightfieldService* HeightfieldService;

	// POI storage and tracking
	UPROPERTY()
	TMap<FTileCoord, FPOITileData> TilePOIs;

	UPROPERTY()
	TMap<FGuid, FPOIData> AllPOIs;

	// Performance tracking
	TArray<float> GenerationTimes;
	float TotalGenerationTime;
	int32 GenerationCount;

	// Persistence
	FString PersistenceDirectory;

	/**
	 * Generate stratified sampling points for a tile
	 */
	TArray<FVector2D> GenerateStratifiedSamplePoints(FTileCoord TileCoord, int32 Seed) const;

	/**
	 * Get biome-specific POI rules from biome service
	 */
	TArray<FPOISpawnRule> GetBiomePOIRules(EBiomeType BiomeType) const;

	/**
	 * Check slope requirements for POI placement
	 */
	bool CheckSlopeRequirements(FVector Location, float SlopeLimit, const TArray<float>& HeightData, FTileCoord TileCoord) const;

	/**
	 * Validate flat ground requirements using 3x3 area check
	 */
	bool ValidateFlatGround(FVector Location, const TArray<float>& HeightData, FTileCoord TileCoord) const;

	/**
	 * Get height at specific location within tile
	 */
	float GetHeightAtTileLocation(FVector2D LocalPosition, const TArray<float>& HeightData, FTileCoord TileCoord) const;

	/**
	 * Calculate slope at specific location
	 */
	float CalculateSlopeAtLocation(FVector2D LocalPosition, const TArray<float>& HeightData, FTileCoord TileCoord) const;

	/**
	 * Generate deterministic random value for POI placement
	 */
	float GenerateSeededRandom(int32 Seed, FTileCoord TileCoord, int32 SampleIndex) const;

	/**
	 * Apply terrain flattening stamp to heightfield data
	 */
	void ApplyFlatteningStamp(FVector2D Center, float Radius, float Strength, TArray<float>& HeightData, FTileCoord TileCoord) const;

	/**
	 * Update performance statistics
	 */
	void UpdatePerformanceStats(float GenerationTimeMs);

	/**
	 * Get file path for POI persistence
	 */
	FString GetPOIPersistencePath(FTileCoord TileCoord) const;

	/**
	 * Serialize POI data to binary format
	 */
	bool SerializePOIData(const TArray<FPOIData>& POIs, TArray<uint8>& OutData) const;

	/**
	 * Deserialize POI data from binary format
	 */
	bool DeserializePOIData(const TArray<uint8>& InData, TArray<FPOIData>& OutPOIs) const;

	/**
	 * Convert world position to tile-local coordinates
	 */
	FVector2D WorldToTileLocal(FVector WorldPosition, FTileCoord TileCoord) const;

	/**
	 * Convert tile-local coordinates to world position
	 */
	FVector TileLocalToWorld(FVector2D LocalPosition, FTileCoord TileCoord) const;

	/**
	 * Hash function for deterministic POI placement
	 */
	uint32 HashTilePosition(FTileCoord TileCoord, int32 SampleIndex, int32 Seed) const;
};