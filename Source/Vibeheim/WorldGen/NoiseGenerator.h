#pragma once

#include "CoreMinimal.h"
#include "Data/WorldGenSettings.h"

/**
 * Feature tags for seed mixing in deterministic PRNG
 */
enum class ENoiseFeatureTag : uint32
{
    Terrain = 0x12345678,
    BiomeMeadows = 0x87654321,
    BiomeBlackForest = 0xABCDEF00,
    BiomeSwamp = 0x11223344,
    POI = 0x55667788,
    Dungeon = 0x99AABBCC,
    RidgedTerrain = 0xDDEEFF00,
    DomainWarp = 0x22334455,
    Rivers = 0x66778899,
    Mountains = 0xAABBCCDD,
    Valleys = 0xEEFF0011,
    Vegetation = 0x33445566
};

/**
 * Noise algorithm types for different terrain features
 */
enum class ENoiseAlgorithm : uint8
{
    Perlin,           // Standard Perlin noise
    RidgedMultifractal, // Ridged multifractal for sharp terrain features
    DomainWarped,     // Domain-warped noise for organic variation
    FlowAccumulation  // Flow accumulation for river networks
};

/**
 * Terrain feature types for noise algorithm selection
 */
enum class ETerrainFeature : uint8
{
    Base,       // Base terrain heightmap
    Mountains,  // Mountain ranges and peaks
    Valleys,    // Valley systems and depressions
    Rivers,     // River networks and channels
    Erosion     // Erosion patterns and weathering
};

/**
 * Deterministic noise generator with seed-mixed PRNG system
 * Implements Perlin noise generation for terrain and biome masks
 * Supports versioning for deterministic generation across multiple runs
 */
class VIBEHEIM_API FNoiseGenerator
{
public:
    /**
     * Initialize the noise generator with world generation settings
     * @param InSettings World generation settings containing seed and version info
     */
    void Initialize(const FWorldGenSettings& InSettings);

    /**
     * Generate Perlin noise at specified coordinates
     * @param X X coordinate in world space
     * @param Y Y coordinate in world space
     * @param Scale Noise scale factor
     * @param FeatureTag Feature tag for seed mixing
     * @param ChunkCoord Optional chunk coordinate for additional seed mixing
     * @return Noise value in range [0.0, 1.0]
     */
    float GeneratePerlinNoise(float X, float Y, float Scale, ENoiseFeatureTag FeatureTag, const FIntVector& ChunkCoord = FIntVector::ZeroValue) const;

    /**
     * Generate multi-octave Perlin noise
     * @param X X coordinate in world space
     * @param Y Y coordinate in world space
     * @param Scale Base noise scale factor
     * @param Octaves Number of octaves to combine
     * @param Persistence Amplitude multiplier for each octave
     * @param Lacunarity Frequency multiplier for each octave
     * @param FeatureTag Feature tag for seed mixing
     * @param ChunkCoord Optional chunk coordinate for additional seed mixing
     * @return Noise value in range [0.0, 1.0]
     */
    float GenerateOctaveNoise(float X, float Y, float Scale, int32 Octaves, float Persistence, float Lacunarity, ENoiseFeatureTag FeatureTag, const FIntVector& ChunkCoord = FIntVector::ZeroValue) const;

    /**
     * Generate deterministic random float in range [0.0, 1.0]
     * @param X X coordinate
     * @param Y Y coordinate
     * @param FeatureTag Feature tag for seed mixing
     * @param ChunkCoord Optional chunk coordinate for additional seed mixing
     * @return Random float value in range [0.0, 1.0]
     */
    float GenerateRandomFloat(int32 X, int32 Y, ENoiseFeatureTag FeatureTag, const FIntVector& ChunkCoord = FIntVector::ZeroValue) const;

    /**
     * Generate deterministic random integer
     * @param X X coordinate
     * @param Y Y coordinate
     * @param FeatureTag Feature tag for seed mixing
     * @param ChunkCoord Optional chunk coordinate for additional seed mixing
     * @return Random integer value
     */
    int32 GenerateRandomInt(int32 X, int32 Y, ENoiseFeatureTag FeatureTag, const FIntVector& ChunkCoord = FIntVector::ZeroValue) const;

    /**
     * Get the current world generation version
     * @return World generation version
     */
    int32 GetWorldGenVersion() const { return WorldGenVersion; }

    /**
     * Get the plugin SHA for determinism validation
     * @return Plugin SHA string
     */
    const FString& GetPluginSHA() const { return PluginSHA; }

    /**
     * Get the base seed value
     * @return Base seed value
     */
    int64 GetSeed() const { return BaseSeed; }

    /**
     * Generate ridged multifractal noise for sharp terrain features
     * @param X X coordinate in world space
     * @param Y Y coordinate in world space
     * @param Scale Base noise scale factor
     * @param Octaves Number of octaves to combine
     * @param Persistence Amplitude multiplier for each octave
     * @param Lacunarity Frequency multiplier for each octave
     * @param FeatureTag Feature tag for seed mixing
     * @param ChunkCoord Optional chunk coordinate for additional seed mixing
     * @return Ridged noise value in range [0.0, 1.0]
     */
    float GenerateRidgedNoise(float X, float Y, float Scale, int32 Octaves, float Persistence, float Lacunarity, ENoiseFeatureTag FeatureTag, const FIntVector& ChunkCoord = FIntVector::ZeroValue) const;

    /**
     * Generate domain-warped noise for organic terrain variation
     * @param X X coordinate in world space
     * @param Y Y coordinate in world space
     * @param Scale Base noise scale factor
     * @param WarpStrength Strength of domain warping effect
     * @param FeatureTag Feature tag for seed mixing
     * @param ChunkCoord Optional chunk coordinate for additional seed mixing
     * @return Domain-warped noise value in range [0.0, 1.0]
     */
    float GenerateDomainWarpedNoise(float X, float Y, float Scale, float WarpStrength, ENoiseFeatureTag FeatureTag, const FIntVector& ChunkCoord = FIntVector::ZeroValue) const;

    /**
     * Calculate flow accumulation for river network generation
     * @param X X coordinate in world space
     * @param Y Y coordinate in world space
     * @param HeightmapScale Scale for heightmap sampling
     * @param FlowThreshold Minimum flow accumulation for river formation
     * @param FeatureTag Feature tag for seed mixing
     * @param ChunkCoord Optional chunk coordinate for additional seed mixing
     * @return Flow accumulation value (higher values indicate river channels)
     */
    float CalculateFlowAccumulation(float X, float Y, float HeightmapScale, float FlowThreshold, ENoiseFeatureTag FeatureTag, const FIntVector& ChunkCoord = FIntVector::ZeroValue) const;

    /**
     * Generate terrain height using combined noise algorithms
     * @param X X coordinate in world space
     * @param Y Y coordinate in world space
     * @param BaseScale Base terrain scale
     * @param RidgedScale Ridged noise scale for mountains
     * @param WarpStrength Domain warp strength
     * @param FeatureTag Feature tag for seed mixing
     * @param ChunkCoord Optional chunk coordinate for additional seed mixing
     * @return Combined terrain height value
     */
    float GenerateTerrainHeight(float X, float Y, float BaseScale, float RidgedScale, float WarpStrength, ENoiseFeatureTag FeatureTag, const FIntVector& ChunkCoord = FIntVector::ZeroValue) const;

    /**
     * Select appropriate noise algorithm for terrain feature
     * @param Feature Terrain feature type
     * @return Recommended noise algorithm for the feature
     */
    ENoiseAlgorithm SelectNoiseAlgorithm(ETerrainFeature Feature) const;

    /**
     * Generate noise using specified algorithm
     * @param X X coordinate in world space
     * @param Y Y coordinate in world space
     * @param Scale Noise scale factor
     * @param Algorithm Noise algorithm to use
     * @param FeatureTag Feature tag for seed mixing
     * @param ChunkCoord Optional chunk coordinate for additional seed mixing
     * @return Noise value appropriate for the selected algorithm
     */
    float GenerateNoiseWithAlgorithm(float X, float Y, float Scale, ENoiseAlgorithm Algorithm, ENoiseFeatureTag FeatureTag, const FIntVector& ChunkCoord = FIntVector::ZeroValue) const;

    /**
     * Validate determinism by comparing generation results
     * @param TestX X coordinate for test
     * @param TestY Y coordinate for test
     * @param TestScale Scale for test
     * @param FeatureTag Feature tag for test
     * @param ExpectedValue Expected noise value
     * @param Tolerance Acceptable tolerance for comparison
     * @return True if generated value matches expected within tolerance
     */
    bool ValidateDeterminism(float TestX, float TestY, float TestScale, ENoiseFeatureTag FeatureTag, float ExpectedValue, float Tolerance = 0.0001f) const;

private:
    /** Base seed for world generation */
    int64 BaseSeed;

    /** World generation version for compatibility tracking */
    int32 WorldGenVersion;

    /** Plugin SHA for determinism validation */
    FString PluginSHA;

    /**
     * Create mixed seed using base seed, chunk coordinate, and feature tag
     * Formula: Seed ^ Hash(ChunkCoord) ^ FeatureTag
     * @param FeatureTag Feature tag for seed mixing
     * @param ChunkCoord Chunk coordinate for additional mixing
     * @return Mixed seed value
     */
    uint32 CreateMixedSeed(ENoiseFeatureTag FeatureTag, const FIntVector& ChunkCoord) const;

    /**
     * Hash function for chunk coordinates
     * @param ChunkCoord Chunk coordinate to hash
     * @return Hash value
     */
    uint32 HashChunkCoord(const FIntVector& ChunkCoord) const;

    /**
     * Deterministic hash function for integer coordinates
     * @param X X coordinate
     * @param Y Y coordinate
     * @param Seed Seed value for hashing
     * @return Hash value
     */
    uint32 Hash2D(int32 X, int32 Y, uint32 Seed) const;

    /**
     * Generate gradient vector for Perlin noise
     * @param Hash Hash value for gradient selection
     * @param X X offset from grid point
     * @param Y Y offset from grid point
     * @return Dot product of gradient and offset
     */
    float GenerateGradient(uint32 Hash, float X, float Y) const;

    /**
     * Smooth interpolation function (smoothstep)
     * @param t Input value in range [0.0, 1.0]
     * @return Smoothed value using 6t^5 - 15t^4 + 10t^3
     */
    float SmoothStep(float t) const;

    /**
     * Linear interpolation
     * @param a First value
     * @param b Second value
     * @param t Interpolation factor [0.0, 1.0]
     * @return Interpolated value
     */
    float Lerp(float a, float b, float t) const;

    /**
     * Normalize hash value to float in range [0.0, 1.0]
     * @param Hash Hash value to normalize
     * @return Normalized float value
     */
    float NormalizeHash(uint32 Hash) const;

    /**
     * Calculate gradient at a point for flow direction
     * @param X X coordinate
     * @param Y Y coordinate
     * @param HeightmapScale Scale for heightmap sampling
     * @param FeatureTag Feature tag for seed mixing
     * @param ChunkCoord Optional chunk coordinate for additional seed mixing
     * @return Gradient vector (X, Y components)
     */
    FVector2D CalculateGradient(float X, float Y, float HeightmapScale, ENoiseFeatureTag FeatureTag, const FIntVector& ChunkCoord = FIntVector::ZeroValue) const;

    /**
     * Sample heightmap at specific coordinates
     * @param X X coordinate
     * @param Y Y coordinate
     * @param Scale Heightmap scale
     * @param FeatureTag Feature tag for seed mixing
     * @param ChunkCoord Optional chunk coordinate for additional seed mixing
     * @return Height value at coordinates
     */
    float SampleHeightmap(float X, float Y, float Scale, ENoiseFeatureTag FeatureTag, const FIntVector& ChunkCoord = FIntVector::ZeroValue) const;

    /**
     * Apply ridged transformation to noise value
     * @param NoiseValue Input noise value in range [0.0, 1.0]
     * @return Ridged noise value with sharp peaks
     */
    float ApplyRidgedTransform(float NoiseValue) const;

    /**
     * Calculate domain warp offset
     * @param X X coordinate
     * @param Y Y coordinate
     * @param Scale Warp noise scale
     * @param Strength Warp strength multiplier
     * @param FeatureTag Feature tag for seed mixing
     * @param ChunkCoord Optional chunk coordinate for additional seed mixing
     * @return Warp offset vector (X, Y components)
     */
    FVector2D CalculateDomainWarp(float X, float Y, float Scale, float Strength, ENoiseFeatureTag FeatureTag, const FIntVector& ChunkCoord = FIntVector::ZeroValue) const;
};