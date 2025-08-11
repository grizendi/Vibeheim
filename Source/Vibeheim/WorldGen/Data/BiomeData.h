#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "BiomeData.generated.h"

/**
 * Forward declaration for POI spawn rules (will be defined in POI system)
 */
USTRUCT(BlueprintType)
struct FPOISpawnRule
{
    GENERATED_BODY()

    // Placeholder structure - will be expanded in POI system implementation
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI")
    FString POIType = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI")
    float SpawnChance = 0.1f;
};

/**
 * Defines characteristics and generation parameters for each biome
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FBiomeData : public FTableRowBase
{
    GENERATED_BODY()

    /** Display name of the biome */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
    FString BiomeName = "";

    /** Height offset applied to terrain in this biome */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain", meta = (ClampMin = "-1000.0", ClampMax = "1000.0"))
    float HeightOffset = 0.0f;

    /** Scale factor for noise generation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise", meta = (ClampMin = "0.0001", ClampMax = "0.01"))
    float NoiseScale = 0.0025f;

    /** Number of octaves for noise generation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise", meta = (ClampMin = "1.0", ClampMax = "8.0"))
    float NoiseOctaves = 4.0f;

    /** POI spawn rules for this biome */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI")
    TArray<FPOISpawnRule> POIRules;

    /** Debug color for biome visualization */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    FLinearColor DebugColor = FLinearColor::White;

    FBiomeData()
    {
        // Default constructor with default values already set above
    }

    FBiomeData(const FString& InBiomeName, float InHeightOffset, float InNoiseScale, const FLinearColor& InDebugColor)
        : BiomeName(InBiomeName)
        , HeightOffset(InHeightOffset)
        , NoiseScale(InNoiseScale)
        , NoiseOctaves(4.0f)
        , DebugColor(InDebugColor)
    {
    }
};