#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "BiomeData.h"
#include "EnhancedBiomeData.generated.h"

/**
 * Data structure for tree species within a biome
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FTreeSpeciesData
{
    GENERATED_BODY()

    /** Name of the tree species */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree Species")
    FString SpeciesName = "";

    /** Static mesh for this tree species */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree Species")
    TSoftObjectPtr<UStaticMesh> TreeMesh;

    /** Probability of spawning this species (0.0-1.0) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree Species", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float SpawnProbability = 1.0f;

    /** Size variation range for this species */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree Species")
    FVector2D SizeRange = FVector2D(0.8f, 1.2f);

    /** List of harvestable resources from this tree */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree Species")
    TArray<FString> HarvestableResources;

    FTreeSpeciesData()
    {
        HarvestableResources.Add(TEXT("Wood"));
    }
};

/**
 * Data structure for foliage types within a biome
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FFoliageTypeData
{
    GENERATED_BODY()

    /** Name of the foliage type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage")
    FString FoliageTypeName = "";

    /** Static mesh for this foliage type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage")
    TSoftObjectPtr<UStaticMesh> FoliageMesh;

    /** Probability of spawning this foliage type (0.0-1.0) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float SpawnProbability = 1.0f;

    /** Density multiplier for this foliage type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage", meta = (ClampMin = "0.1", ClampMax = "5.0"))
    float DensityMultiplier = 1.0f;
};

/**
 * Data structure for harvestable resources within a biome
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FResourceData
{
    GENERATED_BODY()

    /** Name of the resource */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
    FString ResourceName = "";

    /** Type category of the resource */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
    FString ResourceType = "Material";

    /** Spawn rate for this resource (0.0-1.0) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float SpawnRate = 1.0f;

    /** Minimum quantity when harvested */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource", meta = (ClampMin = "1"))
    int32 MinQuantity = 1;

    /** Maximum quantity when harvested */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource", meta = (ClampMin = "1"))
    int32 MaxQuantity = 3;
};

/**
 * Vegetation profile for a biome containing all vegetation data
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FVegetationProfile
{
    GENERATED_BODY()

    /** Tree species available in this biome */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vegetation")
    TArray<FTreeSpeciesData> TreeSpecies;

    /** Foliage types available in this biome */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vegetation")
    TArray<FFoliageTypeData> FoliageTypes;

    /** Harvestable resources available in this biome */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vegetation")
    TArray<FResourceData> HarvestableResources;

    /** Overall tree density multiplier for this biome */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vegetation", meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float TreeDensity = 1.0f;

    /** Overall foliage density multiplier for this biome */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vegetation", meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float FoliageDensity = 1.0f;

    /** Overall resource density multiplier for this biome */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vegetation", meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float ResourceDensity = 1.0f;
};

/**
 * Enhanced biome data structure with material properties and vegetation profiles
 * Extends the base FBiomeData with additional features for enhanced world generation
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FEnhancedBiomeData : public FBiomeData
{
    GENERATED_BODY()

    // Material system properties
    /** Material interface for this biome's terrain */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
    TSoftObjectPtr<UMaterialInterface> BiomeMaterial;

    /** Fallback color if no material is assigned */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
    FLinearColor BiomeColor = FLinearColor::White;

    /** Material roughness parameter */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaterialRoughness = 0.5f;

    /** Material metallic parameter */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaterialMetallic = 0.0f;

    // Vegetation system properties
    /** Vegetation profile containing all vegetation data for this biome */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vegetation")
    FVegetationProfile VegetationProfile;

    // Height-based biome properties
    /** Whether this biome is distributed based on height rather than noise */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Height Distribution")
    bool bIsHeightBasedBiome = false;

    /** Height threshold for this biome (used if bIsHeightBasedBiome is true) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Height Distribution")
    float HeightThreshold = 0.0f;

    /** Whether this biome overrides other biomes when height conditions are met */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Height Distribution")
    bool bOverrideOtherBiomes = false;

    /** Blend distance for height-based biome transitions */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Height Distribution", meta = (ClampMin = "1.0", ClampMax = "100.0"))
    float HeightBlendDistance = 20.0f;

    FEnhancedBiomeData()
        : FBiomeData()
    {
        // Initialize with default values
        BiomeColor = FLinearColor::White;
        MaterialRoughness = 0.5f;
        MaterialMetallic = 0.0f;
        bIsHeightBasedBiome = false;
        HeightThreshold = 0.0f;
        bOverrideOtherBiomes = false;
        HeightBlendDistance = 20.0f;
    }

    FEnhancedBiomeData(const FString& InBiomeName, float InHeightOffset, float InNoiseScale, const FLinearColor& InDebugColor)
        : FBiomeData(InBiomeName, InHeightOffset, InNoiseScale, InDebugColor)
    {
        // Initialize enhanced properties with defaults
        BiomeColor = InDebugColor;
        MaterialRoughness = 0.5f;
        MaterialMetallic = 0.0f;
        bIsHeightBasedBiome = false;
        HeightThreshold = 0.0f;
        bOverrideOtherBiomes = false;
        HeightBlendDistance = 20.0f;
    }

    /**
     * Create a height-based biome with specified threshold
     */
    static FEnhancedBiomeData CreateHeightBasedBiome(const FString& InBiomeName, float InHeightThreshold, 
        bool bOverride, const FLinearColor& InColor, const FLinearColor& InDebugColor)
    {
        FEnhancedBiomeData BiomeData(InBiomeName, 0.0f, 0.0f, InDebugColor);
        BiomeData.bIsHeightBasedBiome = true;
        BiomeData.HeightThreshold = InHeightThreshold;
        BiomeData.bOverrideOtherBiomes = bOverride;
        BiomeData.BiomeColor = InColor;
        return BiomeData;
    }
};