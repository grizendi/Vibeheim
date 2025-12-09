#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WorldGenBuildState.generated.h"

struct FWorldGenConfig;

/**
 * Tracks baked world metadata to validate runtime compatibility.
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FWorldBuildState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WorldGen")
    int32 BuiltSeed = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WorldGen")
    int32 WorldGenVersion = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WorldGen")
    FDateTime LastBuildTime;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WorldGen")
    FString PCGBuildHash;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WorldGen")
    bool bIsBaked = false;

    /** True when all required metadata is present and the world is baked. */
    bool IsValid() const;

    /** True when the build state matches the provided worldgen config. */
    bool IsCompatibleWith(const FWorldGenConfig& Config) const;

    friend FArchive& operator<<(FArchive& Ar, FWorldBuildState& State);
};

/**
 * Companion asset that stores world build state alongside a map.
 */
UCLASS(BlueprintType)
class VIBEHEIM_API UWorldGenBuildStateAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WorldGen")
    FWorldBuildState BuildState;

    UFUNCTION(BlueprintCallable, Category = "WorldGen")
    void UpdateFromBuild(int32 Seed, int32 Version, const FString& PCGHash);
};
