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
	float CalculateBiomeSuitability(EBiomeType BiomeType, const FClimateData& ClimateData, float Altitude) const;

private:
	UPROPERTY()
	UClimateSystem* ClimateSystem;

	UPROPERTY()
	FWorldGenConfig WorldGenSettings;

	UPROPERTY()
	TMap<EBiomeType, FBiomeDefinition> BiomeDefinitions;

	/**
	 * Initialize default biome definitions
	 */
	void InitializeDefaultBiomes();

	/**
	 * Calculate biome weights based on climate suitability
	 */
	TMap<EBiomeType, float> CalculateBiomeWeights(const FClimateData& ClimateData, float Altitude) const;

	/**
	 * Apply biome blending based on distance
	 */
	FBiomeResult ApplyBiomeBlending(const TMap<EBiomeType, float>& BiomeWeights, FVector2D WorldPosition) const;

	/**
	 * Get biome color for PNG export
	 */
	FColor GetBiomeColor(EBiomeType BiomeType) const;
};
