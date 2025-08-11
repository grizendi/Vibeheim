#pragma once

#include "CoreMinimal.h"
#include "Data/BiomeData.h"
#include "Data/WorldGenSettings.h"
#include "NoiseGenerator.h"
#include "BiomeSystem.generated.h"

/**
 * Enum for biome types
 */
UENUM(BlueprintType)
enum class EBiomeType : uint8
{
    Meadows = 0,
    BlackForest = 1,
    Swamp = 2,
    Count UMETA(Hidden)
};

/**
 * Structure representing biome weights at a specific location
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FBiomeWeights
{
    GENERATED_BODY()

    /** Weight for each biome type */
    float Weights[static_cast<int32>(EBiomeType::Count)];

    /** Dominant biome (highest weight) */
    EBiomeType DominantBiome;

    /** Normalized weights (sum to 1.0) */
    float NormalizedWeights[static_cast<int32>(EBiomeType::Count)];

    FBiomeWeights()
    {
        // Initialize all weights to zero
        for (int32 i = 0; i < static_cast<int32>(EBiomeType::Count); ++i)
        {
            Weights[i] = 0.0f;
            NormalizedWeights[i] = 0.0f;
        }
        DominantBiome = EBiomeType::Meadows;
    }
};

/**
 * Result of biome evaluation at a specific location
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FBiomeEvaluation
{
    GENERATED_BODY()

    /** Biome weights at this location */
    FBiomeWeights BiomeWeights;

    /** Final height offset after blending */
    float BlendedHeightOffset;

    /** Debug color for visualization */
    FLinearColor BlendedDebugColor;

    /** Dominant biome at this location */
    FString DominantBiome;

    /** Terrain height at this location */
    float TerrainHeight;

    FBiomeEvaluation()
        : BlendedHeightOffset(0.0f)
        , BlendedDebugColor(FLinearColor::White)
        , DominantBiome(TEXT("Unknown"))
        , TerrainHeight(0.0f)
    {
    }
};

/**
 * System responsible for generating and blending biomes
 */
class VIBEHEIM_API FBiomeSystem
{
public:
    /**
     * Initialize the biome system with world generation settings
     * @param InSettings World generation settings
     */
    void Initialize(const FWorldGenSettings& InSettings);

    /**
     * Evaluate biome at a specific world location
     * @param WorldX X coordinate in world space
     * @param WorldY Y coordinate in world space
     * @param ChunkCoord Optional chunk coordinate for additional seed mixing
     * @return Biome evaluation result
     */
    FBiomeEvaluation EvaluateBiome(float WorldX, float WorldY, const FIntVector& ChunkCoord = FIntVector::ZeroValue) const;

    /**
     * Get biome data for a specific biome type
     * @param BiomeType The biome type to get data for
     * @return Biome data structure
     */
    const FBiomeData& GetBiomeData(EBiomeType BiomeType) const;

    /**
     * Get all biome data
     * @return Array of all biome data
     */
    const TArray<FBiomeData>& GetAllBiomeData() const { return BiomeDataArray; }

private:
    /** World generation settings */
    FWorldGenSettings Settings;

    /** Array of biome data for each biome type */
    TArray<FBiomeData> BiomeDataArray;

    /** Deterministic noise generator */
    FNoiseGenerator NoiseGenerator;

    /**
     * Generate noise value for a specific biome at given coordinates
     * @param BiomeType The biome type
     * @param WorldX X coordinate in world space
     * @param WorldY Y coordinate in world space
     * @param ChunkCoord Optional chunk coordinate for additional seed mixing
     * @return Noise value (0.0 to 1.0)
     */
    float GenerateBiomeNoise(EBiomeType BiomeType, float WorldX, float WorldY, const FIntVector& ChunkCoord = FIntVector::ZeroValue) const;

    /**
     * Calculate biome weights at a specific location
     * @param WorldX X coordinate in world space
     * @param WorldY Y coordinate in world space
     * @param ChunkCoord Optional chunk coordinate for additional seed mixing
     * @return Biome weights structure
     */
    FBiomeWeights CalculateBiomeWeights(float WorldX, float WorldY, const FIntVector& ChunkCoord = FIntVector::ZeroValue) const;

    /**
     * Normalize biome weights so they sum to 1.0
     * @param InOutWeights Biome weights to normalize
     */
    void NormalizeBiomeWeights(FBiomeWeights& InOutWeights) const;

    /**
     * Apply biome blending with 24m blend zones
     * @param InOutWeights Biome weights to apply blending to
     * @param WorldX X coordinate in world space
     * @param WorldY Y coordinate in world space
     * @param ChunkCoord Optional chunk coordinate for additional seed mixing
     */
    void ApplyBiomeBlending(FBiomeWeights& InOutWeights, float WorldX, float WorldY, const FIntVector& ChunkCoord = FIntVector::ZeroValue) const;

    /**
     * Calculate blended height offset from biome weights
     * @param BiomeWeights The biome weights
     * @return Blended height offset
     */
    float CalculateBlendedHeightOffset(const FBiomeWeights& BiomeWeights) const;

    /**
     * Calculate blended debug color from biome weights
     * @param BiomeWeights The biome weights
     * @return Blended debug color
     */
    FLinearColor CalculateBlendedDebugColor(const FBiomeWeights& BiomeWeights) const;

    /**
     * Initialize default biome data
     */
    void InitializeDefaultBiomeData();

    /**
     * Get the appropriate noise feature tag for a biome type
     * @param BiomeType The biome type
     * @return Corresponding noise feature tag
     */
    ENoiseFeatureTag GetBiomeFeatureTag(EBiomeType BiomeType) const;
};