#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/WorldGenTypes.h"
#include "WorldGenAssets.generated.h"

/**
 * Primary data asset for world generation configuration
 */
UCLASS(BlueprintType)
class VIBEHEIM_API UWorldGenSettingsAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WorldGen", meta = (ShowOnlyInnerProperties))
    FWorldGenConfig CoreSettings;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WorldGen")
    FMacroWorldConfig MacroWorld;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WorldGen")
    FWaterSystemConfig WaterSystem;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WorldGen")
    FRiverSystemConfig RiverSystem;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WorldGen")
    FStreamingBudgetsConfig StreamingBudgets;

    // Reference to biome definitions asset
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WorldGen")
    TSoftObjectPtr<class UBiomeDefinitionsAsset> BiomeDefinitions;

    UFUNCTION(BlueprintCallable, Category = "WorldGen")
    bool ValidateAsset(TArray<FString>& OutErrors) const;
};

/**
 * Primary data asset for biome definitions and ring configuration
 */
UCLASS(BlueprintType)
class VIBEHEIM_API UBiomeDefinitionsAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biomes")
    TMap<EBiomeType, FBiomeDefinition> Biomes;

    // Ring-based biome progression
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biomes")
    TArray<FBiomeRingDefinition> BiomeRings;

    UFUNCTION(BlueprintCallable, Category = "Biomes")
    bool ValidateAsset(TArray<FString>& OutErrors) const;
};
