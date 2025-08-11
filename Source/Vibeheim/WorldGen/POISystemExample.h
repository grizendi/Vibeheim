#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "POISystem.h"
#include "NoiseGenerator.h"
#include "BiomeSystem.h"
#include "Data/WorldGenSettings.h"
#include "POISystemExample.generated.h"

/**
 * Example actor demonstrating POI system usage
 * Shows how to initialize and use the POI placement system
 */
UCLASS(BlueprintType, Blueprintable)
class VIBEHEIM_API APOISystemExample : public AActor
{
    GENERATED_BODY()

public:
    APOISystemExample();

protected:
    virtual void BeginPlay() override;

public:
    /**
     * Generate POIs for a test chunk
     * @param ChunkX X coordinate of the chunk
     * @param ChunkY Y coordinate of the chunk
     * @param ChunkZ Z coordinate of the chunk
     */
    UFUNCTION(BlueprintCallable, Category = "POI Example")
    void GenerateTestPOIs(int32 ChunkX = 0, int32 ChunkY = 0, int32 ChunkZ = 0);

    /**
     * Clear all generated POIs
     */
    UFUNCTION(BlueprintCallable, Category = "POI Example")
    void ClearAllPOIs();

    /**
     * Add a custom POI spawn rule for testing
     * @param POITypeName Name of the POI type
     * @param MinSpacing Minimum spacing between POIs
     * @param SpawnProbability Probability of spawning (0.0 to 1.0)
     * @param AllowedBiome Biome where this POI can spawn
     */
    UFUNCTION(BlueprintCallable, Category = "POI Example")
    void AddCustomPOIRule(const FString& POITypeName, float MinSpacing = 150.0f, 
                         float SpawnProbability = 0.1f, const FString& AllowedBiome = TEXT("Meadows"));

    /**
     * Get placement statistics
     * @param OutTotalAttempts Total placement attempts
     * @param OutSuccessful Successful placements
     * @param OutFailed Failed placements
     * @param OutAverageAttempts Average attempts per POI
     */
    UFUNCTION(BlueprintCallable, Category = "POI Example")
    void GetPlacementStats(int32& OutTotalAttempts, int32& OutSuccessful, 
                          int32& OutFailed, float& OutAverageAttempts);

    /**
     * Test POI location validation
     * @param TestLocation Location to test
     * @param POITypeName POI type to test against
     * @return True if location is valid
     */
    UFUNCTION(BlueprintCallable, Category = "POI Example")
    bool TestPOILocationValidation(const FVector& TestLocation, const FString& POITypeName);

private:
    /** POI system instance */
    TUniquePtr<FPOISystem> POISystem;

    /** Noise generator for deterministic placement */
    TUniquePtr<FNoiseGenerator> NoiseGenerator;

    /** Biome system for biome evaluation */
    TUniquePtr<FBiomeSystem> BiomeSystem;

    /** World generation settings */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (AllowPrivateAccess = "true"))
    FWorldGenSettings WorldGenSettings;

    /** Whether the systems have been initialized */
    bool bIsInitialized;
};