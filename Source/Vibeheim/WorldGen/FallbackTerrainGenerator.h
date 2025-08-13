#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/WorldGenSettings.h"
#include "NoiseGenerator.h"
#include "FallbackTerrainGenerator.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogFallbackTerrain, Log, All);

/**
 * Fallback terrain generator for when primary generation fails
 * Provides simple single-octave heightmap generation as a safety net
 */
UCLASS(BlueprintType, Blueprintable)
class VIBEHEIM_API UFallbackTerrainGenerator : public UObject
{
    GENERATED_BODY()

public:
    UFallbackTerrainGenerator();

    /**
     * Initialize the fallback generator with settings
     * @param Settings World generation settings
     * @return True if initialization was successful
     */
    bool Initialize(const FWorldGenSettings& Settings);

    /**
     * Generate fallback heightmap for a chunk
     * @param ChunkCoordinate The chunk coordinate to generate
     * @param OutHeightData Array to fill with height values (ChunkSize^2 elements)
     * @param Seed The seed to use for generation
     * @param ChunkCoordinate The chunk coordinate for logging context
     * @return True if generation was successful
     */
    bool GenerateHeightmapForChunk(const FIntVector& ChunkCoordinate, TArray<float>& OutHeightData, 
                                  int64 Seed, const FIntVector& LoggingChunkCoord);

    /**
     * Generate a gray proxy mesh for completely failed chunks
     * @param ChunkCoordinate The chunk coordinate
     * @param Seed The seed for logging context
     * @param ChunkCoordinate The chunk coordinate for logging context
     * @return True if proxy generation was successful
     */
    bool GenerateProxyMesh(const FIntVector& ChunkCoordinate, int64 Seed, const FIntVector& LoggingChunkCoord);

    /**
     * Check if the fallback generator is initialized
     * @return True if initialized
     */
    bool IsInitialized() const { return bIsInitialized; }

protected:
    /**
     * Generate simple single-octave noise for heightmap
     * @param X World X coordinate
     * @param Y World Y coordinate
     * @param Seed Generation seed
     * @return Height value
     */
    float GenerateSimpleHeight(float X, float Y, int64 Seed);

private:
    /** Current world generation settings */
    UPROPERTY()
    FWorldGenSettings CurrentSettings;

    /** Simple noise generator for fallback terrain */
    TUniquePtr<FNoiseGenerator> SimpleNoiseGenerator;

    /** Whether the generator is initialized */
    bool bIsInitialized;

    /** Base height for fallback terrain */
    float BaseHeight;

    /** Height variation for fallback terrain */
    float HeightVariation;
};