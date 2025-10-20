#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Services/IHeightfieldService.h"
#include "Services/ClimateSystem.h"
#include "Services/NoiseSystem.h"
#include "Data/WorldGenTypes.h"
#include "HeightfieldService.generated.h"

class URiverFlowService;

/**
 * Noise generation settings for heightfield creation
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FNoiseSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
	float Scale = 0.01f; // Noise scale

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
	float Amplitude = 50.0f; // Noise amplitude

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
	int32 Octaves = 4; // Number of noise octaves

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
	float Persistence = 0.5f; // Amplitude multiplier per octave

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
	float Lacunarity = 2.0f; // Frequency multiplier per octave

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
	FVector2D DomainWarp = FVector2D::ZeroVector; // Domain warping offset

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
	float DomainWarpStrength = 0.0f; // Strength of domain warping
};

/**
 * Heightfield generation settings
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FHeightfieldGenerationSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	FNoiseSettings BaseNoise; // Primary terrain noise

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	FNoiseSettings DetailNoise; // Detail/texture noise

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	FNoiseSettings RidgeNoise; // Ridge/mountain noise

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	float BaseHeight = 0.0f; // Base terrain height

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	float HeightMultiplier = 1.0f; // Overall height scaling

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	bool bEnableThermalSmoothing = true; // Apply thermal erosion

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	int32 ThermalSmoothingIterations = 2; // Thermal smoothing passes

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	float ThermalSmoothingStrength = 0.1f; // Thermal smoothing intensity
};

USTRUCT()
struct VIBEHEIM_API FHeightfieldModificationList
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FHeightfieldModification> Modifications; // or Mods, name it as you like

	UPROPERTY()
	int32 JournalVersion = 1;

	UPROPERTY()
	int32 AuthoredWorldGenVersion = 0;

	UPROPERTY()
	uint32 MacroConfigSignature = 0;

	UPROPERTY()
	bool bRequiresReplay = false;
};


/**
 * CPU-based heightfield service implementation
 * Provides deterministic heightfield generation with noise and climate integration
 */
UCLASS(BlueprintType)
class VIBEHEIM_API UHeightfieldService : public UObject, public IHeightfieldServiceInterface
{
	GENERATED_BODY()

public:
	UHeightfieldService();

	// IHeightfieldServiceInterface interface
	virtual bool Initialize(const FWorldGenConfig& Settings) override;
	virtual FHeightfieldData GenerateHeightfield(int32 Seed, FTileCoord TileCoord) override;
	virtual bool ModifyHeightfield(FVector Location, float Radius, float Strength, EHeightfieldOperation Operation) override;
        virtual float GetHeightAtLocation(FVector2D WorldPos) override;
        virtual FVector GetNormalAtLocation(FVector2D WorldPos) override;
        virtual float GetSlopeAtLocation(FVector2D WorldPos) override;
	virtual bool SaveHeightfieldModifications() override;
	virtual bool LoadHeightfieldModifications() override;
	virtual bool UploadHeightfieldToGPU(const FHeightfieldData& HeightfieldData) override;
	virtual void ApplyThermalSmoothing(FHeightfieldData& HeightfieldData, int32 Iterations = 1) override;
	virtual void CalculateNormalsAndSlopes(FHeightfieldData& HeightfieldData) override;
	virtual bool ExportHeightfieldPNG(const FHeightfieldData& HeightfieldData, const FString& OutputPath) override;
	virtual bool GetCachedHeightfield(FTileCoord TileCoord, FHeightfieldData& OutHeightfieldData) override;
	virtual void CacheHeightfield(const FHeightfieldData& HeightfieldData) override;
	virtual void ClearHeightfieldCache() override;
    virtual void GetPerformanceStats(float& OutAverageGenerationTimeMs, int32& OutCachedTiles) override;

    /**
     * Generate pristine heightfield data for determinism testing (ignores persisted terrain deltas)
     */
    UFUNCTION(BlueprintCallable, Category = "Heightfield")
    FHeightfieldData GenerateHeightfieldPristine(int32 Seed, FTileCoord TileCoord);

	/**
	 * Update heightfield generation settings
	 */
	UFUNCTION(BlueprintCallable, Category = "Heightfield")
	void UpdateGenerationSettings(const FHeightfieldGenerationSettings& NewSettings);

	/** Inject the river flow service used for carving rivers and stamping lakes */
	void SetRiverFlowService(URiverFlowService* InRiverFlowService);

	/** Update river system configuration (unset disables river carving) */
	void SetRiverSystemConfig(const TOptional<FRiverSystemConfig>& InRiverConfig);

	/**
	 * Notify the service that macro world parameters have changed and terrain journals must be replayed
	 */
	UFUNCTION(BlueprintCallable, Category = "Heightfield")
	void HandleWorldConfigUpdated();

	/**
	 * Get current generation settings
	 */
	UFUNCTION(BlueprintCallable, Category = "Heightfield")
	const FHeightfieldGenerationSettings& GetGenerationSettings() const { return GenerationSettings; }

	/**
	 * Set climate system for biome-aware height generation
	 */
	UFUNCTION(BlueprintCallable, Category = "Heightfield")
	void SetClimateSystem(UClimateSystem* InClimateSystem);

	/**
	 * Set noise system for advanced noise generation
	 */
	UFUNCTION(BlueprintCallable, Category = "Heightfield")
	void SetNoiseSystem(UNoiseSystem* InNoiseSystem);

	/**
	 * Save terrain modifications for a specific tile
	 */
	UFUNCTION(BlueprintCallable, Category = "Persistence")
	bool SaveTileTerrainDeltas(FTileCoord TileCoord);

	/**
	 * Load terrain modifications for a specific tile
	 */
	UFUNCTION(BlueprintCallable, Category = "Persistence")
        bool LoadTileTerrainDeltas(FTileCoord TileCoord);

	/**
	 * Get all modifications affecting a specific tile
	 */
	UFUNCTION(BlueprintCallable, Category = "Persistence")
	TArray<FHeightfieldModification> GetTileModifications(FTileCoord TileCoord) const;

	/**
	 * Clear all modifications for a specific tile (for testing)
	 */
	UFUNCTION(BlueprintCallable, Category = "Persistence")
	void ClearTileModifications(FTileCoord TileCoord);

	/**
	 * Get file path for .terra file
	 */
	UFUNCTION(BlueprintCallable, Category = "Persistence")
	FString GetTerraDeltaPath(FTileCoord TileCoord) const;

        /**
         * Sample height at world coordinates with consistent coordinate conversion (cm → sample index)
         */
        float SampleHeightAt(FVector2D WorldXY, const TArray<float>& HeightData, FTileCoord TileCoord) const;

        /**
         * Sample height directly from cached terrain data using world-space XY coordinates in meters.
         */
        float SampleHeightWorldXY(FVector2D WorldXY) const;

private:
	UPROPERTY()
	FWorldGenConfig WorldGenSettings;

	UPROPERTY()
	FHeightfieldGenerationSettings GenerationSettings;

	UPROPERTY()
	UClimateSystem* ClimateSystem;

	UPROPERTY()
	UNoiseSystem* NoiseSystem;

	UPROPERTY()
	URiverFlowService* RiverFlowService;

	// Heightfield cache
	UPROPERTY()
	TMap<FTileCoord, FHeightfieldData> HeightfieldCache;

	// Heightfield modifications
	UPROPERTY()
	TArray<FHeightfieldModification> PendingModifications;

	// Persistence system
	FString PersistenceDirectory;

	UPROPERTY()
	TMap<FTileCoord, FHeightfieldModificationList> TileModifications;

	TSet<FTileCoord> DirtyTiles;

	// Order tracking for deterministic modification ordering
	TMap<FTileCoord, uint32> NextOrderIndexPerTile;

	// Performance tracking
	TArray<float> GenerationTimes;
	float TotalGenerationTime;
	int32 GenerationCount;

	/**
	 * Generate base terrain height using noise
	 */
	float GenerateBaseHeight(FVector2D WorldPosition, int32 Seed) const;

	/**
	 * Generate noise value with multiple octaves
	 */
	float GenerateOctaveNoise(FVector2D Position, const FNoiseSettings& Settings, int32 Seed) const;

	/**
	 * Generate single octave of noise
	 */
	float GenerateNoise(FVector2D Position, float Scale, int32 Seed) const;

	/**
	 * Apply domain warping to position
	 */
	FVector2D ApplyDomainWarp(FVector2D Position, const FNoiseSettings& Settings, int32 Seed) const;

	/**
	 * Hash function for deterministic noise
	 */
	uint32 HashPosition(FVector2D Position, int32 Seed) const;

	/**
	 * Interpolate height between cached tiles
	 */
	float InterpolateHeight(FVector2D WorldPos) const;

	/**
	 * Apply heightfield modification to cached data
	 */
	void ApplyModificationToCache(const FHeightfieldModification& Modification);

	/**
	 * Calculate thermal erosion for a single iteration
	 */
	void ApplyThermalErosionIteration(FHeightfieldData& HeightfieldData, float Strength) const;

	/**
	 * Calculate surface normal from height samples
	 */
	FVector CalculateNormal(const TArray<float>& HeightData, int32 X, int32 Y, int32 Resolution, float SampleSpacing) const;

	/**
	 * Calculate slope angle from normal vector
	 */
	float CalculateSlope(const FVector& Normal) const;

	/**
	 * Update performance statistics
	 */
	void UpdatePerformanceStats(float GenerationTimeMs);

	/**
	 * Clear vegetation in the specified area (integration with PCGWorldService)
	 */
	void ClearVegetationInArea(FVector2D Center, float Radius);



	/**
	 * Serialize terrain delta data to binary format
	 */
	bool SerializeTerrainDeltas(const FHeightfieldModificationList& List, TArray<uint8>& OutData) const;

	/**
	 * Deserialize terrain delta data from binary format
	 */
	bool DeserializeTerrainDeltas(const TArray<uint8>& InData, FHeightfieldModificationList& OutList) const;

	/**
	* Apply heightfield modification directly to a heightfield data object
	*/
	void ApplyModificationToHeightfield(FHeightfieldData& HeightfieldData, const FHeightfieldModification& Modification);

	/**
	 * Apply all modifications to a tile's height data array
	 */
	void ApplyModificationsToTile(FTileCoord TileCoord, TArray<float>& HeightData);

	/** Carve river channels into the heightfield based on flow data */
	void ApplyRiverCarving(FHeightfieldData& HeightfieldData, const FRiverFlowTileData& FlowData, const TArray<float>& OriginalHeights);

	/** Stamp lake basins at flow minima */
	void ApplyLakePlacement(FHeightfieldData& HeightfieldData, const FRiverFlowTileData& FlowData, const TArray<float>& OriginalHeights);

	/** River channel carving helper */
	void CarveRiverChannelAtPoint(const FVector2D& LocalPointMeters, const FVector2D& FlowDir, float ChannelWidthMeters, float ChannelDepthMeters, FHeightfieldData& HeightfieldData, float& InOutMinHeight, float& InOutMaxHeight, const TArray<float>& OriginalHeights);

	/** Lake stamping helper */
	void StampLakeAtPoint(const FVector2D& LocalPointMeters, float LakeRadiusMeters, float LakeDepthMeters, FHeightfieldData& HeightfieldData, float& InOutMinHeight, float& InOutMaxHeight, const TArray<float>& OriginalHeights);

	/** Active river system configuration */
	TOptional<FRiverSystemConfig> RiverSystemConfig;

	/** Terrain delta serialization helpers */
	uint32 ComputeMacroConfigSignature() const;
	void EnsureJournalMetadata(const FTileCoord& TileCoord, FHeightfieldModificationList& List);
	void EnsureJournalCompatibility(const FTileCoord& TileCoord);
	void MarkJournalReplayed(const FTileCoord& TileCoord);
	void NormalizeModificationList(FHeightfieldModificationList& List) const;
};
