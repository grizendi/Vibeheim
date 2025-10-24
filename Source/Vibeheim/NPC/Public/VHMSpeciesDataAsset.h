#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VHMNPCTypes.h"
#include "VHMSpeciesDataAsset.generated.h"

class UNavigationQueryFilter;

USTRUCT(BlueprintType)
struct VIBEHEIM_API FVHMSpeciesConfig
{
    GENERATED_BODY()

    // Need configuration per species
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Needs")
    TMap<EVHMNeed, FNeedStat> DefaultNeeds;

    // Search behavior
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Search")
    float SearchRadius = 2500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Search")
    float SearchRadiusMax = 5000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Search")
    float SearchCooldown = 1.0f;

    // Performance LOD
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    float EvaluationInterval = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    float LODEvaluationInterval = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    float LODDistance = 3000.0f;

    // Health penalty when critical needs are at zero
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Needs")
    float ZeroNeedPenaltyDelay = 3.0f; // seconds below zero before health penalty starts

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Needs")
    float HealthPenaltyPerSec = 5.0f; // health loss per second when starving/dehydrated

    // Cooldown after a need exits low state to prevent immediate re-selection
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Needs")
    float PostSatiationCooldown = 1.5f; // seconds

    // Navigation
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation")
    TSubclassOf<UNavigationQueryFilter> NavQueryFilter;

    // Failure handling
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Failure")
    int32 MaxRetries = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Failure")
    float BlacklistDuration = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Failure")
    float StuckThreshold = 50.0f; // cm

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Failure")
    float StuckTimeout = 5.0f; // seconds
};

UCLASS(BlueprintType)
class VIBEHEIM_API UVHMSpeciesDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, Category = "Species")
    FVHMSpeciesConfig Config;
};
