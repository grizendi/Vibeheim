#pragma once

#include "CoreMinimal.h"
#include "BiomeSystem.h"

/**
 * Interface for accessing biome system functionality
 * This provides a clean API for other world generation systems to use
 */
class VIBEHEIM_API IBiomeSystemInterface
{
public:
    virtual ~IBiomeSystemInterface() = default;

    /**
     * Evaluate biome at a specific world location
     * @param WorldX X coordinate in world space
     * @param WorldY Y coordinate in world space
     * @return Biome evaluation result
     */
    virtual FBiomeEvaluation EvaluateBiome(float WorldX, float WorldY) const = 0;

    /**
     * Get biome data for a specific biome type
     * @param BiomeType The biome type to get data for
     * @return Biome data structure
     */
    virtual const FBiomeData& GetBiomeData(EBiomeType BiomeType) const = 0;

    /**
     * Get all biome data
     * @return Array of all biome data
     */
    virtual const TArray<FBiomeData>& GetAllBiomeData() const = 0;
};

/**
 * Concrete implementation of the biome system interface
 */
class VIBEHEIM_API FBiomeSystemWrapper : public IBiomeSystemInterface
{
public:
    /**
     * Initialize the biome system with world generation settings
     * @param InSettings World generation settings
     */
    void Initialize(const FWorldGenSettings& InSettings);

    // IBiomeSystemInterface implementation
    virtual FBiomeEvaluation EvaluateBiome(float WorldX, float WorldY) const override;
    virtual const FBiomeData& GetBiomeData(EBiomeType BiomeType) const override;
    virtual const TArray<FBiomeData>& GetAllBiomeData() const override;

private:
    /** Internal biome system implementation */
    FBiomeSystem BiomeSystem;
};