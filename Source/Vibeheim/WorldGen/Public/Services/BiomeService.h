#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/WorldGenTypes.h"
#include "Services/ClimateSystem.h"
#include "BiomeService.generated.h"

/**
 * Biome determination result containing biome type and blend weights
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FBiomeResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
	EBiomeType PrimaryBiome = EBiomeType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
	TMap<EBiomeType, float> BiomeWeights;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
	float BlendRadius = 0.0f;

	FBiomeResult()
	{
		PrimaryBiome = EBiomeType::None;
		BlendRadius = 0.0f;
	}

	/**
	 * Get the weight of a specific biome
	 */
	float GetBiomeWeight(EBiomeType BiomeType) const
	{
		if (const float* Weight = BiomeWeights.Find(BiomeType))
		{
			return *Weight;
		}
		return 0.0f;
	}

	/**
	 * Check if this result contains multiple biomes (blended)
	 */
	bool IsBlended() const
	{
		return BiomeWeights.Num() > 1;
	}
};

/**
 * Biome service responsible for determining biome types based on climate data
 * Integrates with climate system to provide biome-aware world generation
 */
UCLASS(BlueprintType)
class VIBEHEIM_API UBiomeService : public UObject
{
	GENERATED_BODY()

public:
	UBiomeService();

	/**
	 * Initialize the biome service with climate system
	 */
	UFUNCTION(BlueprintCallable, Category = "Biome")
	void Initialize(UClimateSystem* InClimateSystem, const FWorldGenConfig& Settings);

	/**
	 * Determine biome at a specific world position
	 */
	UFUNCTION(BlueprintCallable, Category = "Biome")
	FBiomeResult DetermineBiome(FVector2D WorldPosition, float Altitude = 0.0f) const;

	
	UFUNCTION(BlueprintCallable, Category = "Biome")
	EBiomeType DetermineTileBiome(FTileCoord Tile, const TArray<float>& HeightData) const;
	
	/**
	 * Generate biome data for an entire tile
	 */
	UFUNCTION(BlueprintCallable, Category = "Biome")
	TArray<FBiomeResult> GenerateTileBiomeData(FTileCoord TileCoord, const TArray<float>& HeightData) const;

	/**
	 * Export biome data as PNG for debugging
	 */
	UFUNCTION(BlueprintCallable, Category = "Biome")
	bool ExportBiomePNG(FTileCoord TileCoord, const TArray<float>& HeightData, const FString& OutputPath) const;

	/**
	 * Get biome definition for a specific biome type
	 */
	UFUNCTION(BlueprintCallable, Category = "Biome")
	bool GetBiomeDefinition(EBiomeType BiomeType, FBiomeDefinition& OutDefinition) const;

	/**
	 * Set biome definitions (loaded from data table)
	 */
	UFUNCTION(BlueprintCallable, Category = "Biome")
	void SetBiomeDefinitions(const TMap<EBiomeType, FBiomeDefinition>& InBiomeDefinitions);

	/**
	 * Set biome ring definitions for radial progression
	 */
	void SetBiomeRingDefinitions(const TArray<FBiomeRingDefinition>& InBiomeRings);

    /**
     * Override the world center used for ring calculations
     */
    void SetRingWorldCenter(const FVector2D& InWorldCenter);

    /** Retrieve current ring definitions for validation tooling */
    const TArray<FBiomeRingDefinition>& GetBiomeRingDefinitions() const { return BiomeRingDefinitions; }

    /**
     * Load biome definitions from JSON configuration file
     */
    UFUNCTION(BlueprintCallable, Category = "Biome")
    bool LoadBiomesFromJSON(const FString& ConfigPath = TEXT("Config/BiomeDefinitions.json"));

	/**
	 * Save biome definitions to JSON configuration file
	 */
	UFUNCTION(BlueprintCallable, Category = "Biome")
	bool SaveBiomesToJSON(const FString& ConfigPath = TEXT("Config/BiomeDefinitions.json")) const;

	/**
	 * Calculate biome suitability based on climate conditions
	 */
	UFUNCTION(BlueprintCallable, Category = "Biome")
	float CalculateBiomeSuitability(EBiomeType BiomeType, const FClimateData& ClimateData, float Altitude, FVector2D WorldPosition = FVector2D::ZeroVector) const;

	/**
	 * Get all biome definitions (for PCG integration)
	 */
	const TMap<EBiomeType, FBiomeDefinition>& GetBiomeDefinitions() const { return BiomeDefinitions; }

private:
	UPROPERTY()
	UClimateSystem* ClimateSystem;

	UPROPERTY()
	FWorldGenConfig WorldGenSettings;
	// Ring configuration supplied by data assets
	TArray<FBiomeRingDefinition> BiomeRingDefinitions;

	// Cached world center used for distance-based ring weighting
	FVector2D RingWorldCenter = FVector2D::ZeroVector;

	UPROPERTY()
	TMap<EBiomeType, FBiomeDefinition> BiomeDefinitions;

	/**
	 * Initialize default biome definitions
	 */
	void InitializeDefaultBiomes();

	/**
	 * Calculate biome weights based on climate suitability
	 */
	TMap<EBiomeType, float> CalculateBiomeWeights(const FClimateData& ClimateData, float Altitude, FVector2D WorldPosition = FVector2D::ZeroVector) const;

	/**
	 * Apply biome blending based on distance
	 */
	FBiomeResult ApplyBiomeBlending(const TMap<EBiomeType, float>& BiomeWeights, FVector2D WorldPosition) const;

	/**
	 * Get biome color for PNG export
	 */
	FColor GetBiomeColor(EBiomeType BiomeType) const;

	/**
	 * Determine if ring progression data should be used
	 */
	bool HasRingDefinitions() const;

	/**
	 * Compute the ring weight for a biome at the provided distance
	 */
	float ComputeRingWeight(EBiomeType BiomeType, float DistanceFromCenter, const FBiomeRingDefinition** OutRingDefinition) const;
};
