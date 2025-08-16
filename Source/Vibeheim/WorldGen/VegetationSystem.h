#pragma once

#include "CoreMinimal.h"
#include "Data/EnhancedBiomeData.h"
#include "Data/WorldGenSettings.h"
#include "NoiseGenerator.h"
#include "BiomeSystem.h"
#include "VegetationSystem.generated.h"

/**
 * Structure representing vegetation density at a specific location
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FVegetationDensity
{
    GENERATED_BODY()

    /** Overall vegetation density (0.0 to 1.0) */
    UPROPERTY(BlueprintReadOnly, Category = "Vegetation")
    float OverallDensity = 0.0f;

    /** Tree density at this location */
    UPROPERTY(BlueprintReadOnly, Category = "Vegetation")
    float TreeDensity = 0.0f;

    /** Foliage density at this location */
    UPROPERTY(BlueprintReadOnly, Category = "Vegetation")
    float FoliageDensity = 0.0f;

    /** Resource density at this location */
    UPROPERTY(BlueprintReadOnly, Category = "Vegetation")
    float ResourceDensity = 0.0f;

    /** Dominant biome affecting vegetation at this location */
    UPROPERTY(BlueprintReadOnly, Category = "Vegetation")
    EBiomeType DominantBiome = EBiomeType::Meadows;

    FVegetationDensity()
        : OverallDensity(0.0f)
        , TreeDensity(0.0f)
        , FoliageDensity(0.0f)
        , ResourceDensity(0.0f)
        , DominantBiome(EBiomeType::Meadows)
    {
    }
};

/**
 * Structure representing available vegetation at a specific location
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FVegetationAvailability
{
    GENERATED_BODY()

    /** Available tree species with spawn probabilities */
    UPROPERTY(BlueprintReadOnly, Category = "Vegetation")
    TArray<FTreeSpeciesData> AvailableTreeSpecies;

    /** Available foliage types with spawn probabilities */
    UPROPERTY(BlueprintReadOnly, Category = "Vegetation")
    TArray<FFoliageTypeData> AvailableFoliageTypes;

    /** Available harvestable resources */
    UPROPERTY(BlueprintReadOnly, Category = "Vegetation")
    TArray<FResourceData> AvailableResources;

    /** Blended vegetation profile from multiple biomes */
    UPROPERTY(BlueprintReadOnly, Category = "Vegetation")
    FVegetationProfile BlendedProfile;

    FVegetationAvailability()
    {
    }
};

/**
 * Structure for persisting vegetation data per chunk
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FChunkVegetationData
{
    GENERATED_BODY()

    /** Chunk coordinate this data belongs to */
    UPROPERTY(BlueprintReadOnly, Category = "Persistence")
    FIntVector ChunkCoord = FIntVector::ZeroValue;

    /** Vegetation density map for this chunk (sampled at regular intervals) */
    UPROPERTY(BlueprintReadOnly, Category = "Persistence")
    TArray<FVegetationDensity> DensityMap;

    /** Available vegetation types in this chunk */
    UPROPERTY(BlueprintReadOnly, Category = "Persistence")
    FVegetationAvailability VegetationAvailability;

    /** Timestamp when this data was generated */
    UPROPERTY(BlueprintReadOnly, Category = "Persistence")
    FDateTime GenerationTimestamp;

    /** World generation version used to create this data */
    UPROPERTY(BlueprintReadOnly, Category = "Persistence")
    int32 WorldGenVersion = 0;

    /** Seed used for vegetation generation */
    UPROPERTY(BlueprintReadOnly, Category = "Persistence")
    int32 GenerationSeed = 0;

    FChunkVegetationData()
        : ChunkCoord(FIntVector::ZeroValue)
        , GenerationTimestamp(FDateTime::Now())
        , WorldGenVersion(0)
        , GenerationSeed(0)
    {
    }
};

/**
 * System responsible for managing biome-specific vegetation data and resource availability
 */
class VIBEHEIM_API FVegetationSystem
{
public:
    /**
     * Initialize the vegetation system with world generation settings
     * @param InSettings World generation settings
     * @param InBiomeSystem Reference to the biome system for biome data access
     */
    void Initialize(const FWorldGenSettings& InSettings, const FBiomeSystem* InBiomeSystem);

    /**
     * Get vegetation data for a specific biome type
     * @param BiomeType The biome type to get vegetation data for
     * @return Vegetation profile for the specified biome
     */
    const FVegetationProfile& GetVegetationDataForBiome(EBiomeType BiomeType) const;

    /**
     * Calculate vegetation density at a specific world location
     * @param WorldX X coordinate in world space
     * @param WorldY Y coordinate in world space
     * @param TerrainHeight Height of terrain at this location
     * @param ChunkCoord Optional chunk coordinate for additional seed mixing
     * @return Vegetation density information
     */
    FVegetationDensity CalculateVegetationDensity(float WorldX, float WorldY, float TerrainHeight = 0.0f, const FIntVector& ChunkCoord = FIntVector::ZeroValue) const;

    /**
     * Get available vegetation at a specific world location (blended from multiple biomes)
     * @param WorldX X coordinate in world space
     * @param WorldY Y coordinate in world space
     * @param TerrainHeight Height of terrain at this location
     * @param ChunkCoord Optional chunk coordinate for additional seed mixing
     * @return Available vegetation information
     */
    FVegetationAvailability GetVegetationAvailability(float WorldX, float WorldY, float TerrainHeight = 0.0f, const FIntVector& ChunkCoord = FIntVector::ZeroValue) const;

    /**
     * Generate vegetation data for an entire chunk
     * @param ChunkCoord Chunk coordinate to generate data for
     * @param SampleResolution Number of samples per chunk edge (default: 8x8 = 64 samples per chunk)
     * @return Complete vegetation data for the chunk
     */
    FChunkVegetationData GenerateChunkVegetationData(const FIntVector& ChunkCoord, int32 SampleResolution = 8) const;

    /**
     * Save vegetation data for a chunk to persistent storage
     * @param ChunkVegetationData The vegetation data to save
     * @return True if save was successful
     */
    bool SaveChunkVegetationData(const FChunkVegetationData& ChunkVegetationData) const;

    /**
     * Load vegetation data for a chunk from persistent storage
     * @param ChunkCoord Chunk coordinate to load data for
     * @param OutChunkVegetationData Output vegetation data
     * @return True if load was successful
     */
    bool LoadChunkVegetationData(const FIntVector& ChunkCoord, FChunkVegetationData& OutChunkVegetationData) const;

    /**
     * Check if vegetation data exists for a chunk
     * @param ChunkCoord Chunk coordinate to check
     * @return True if vegetation data exists for this chunk
     */
    bool HasChunkVegetationData(const FIntVector& ChunkCoord) const;

    /**
     * Clear vegetation data for a chunk (useful for regeneration)
     * @param ChunkCoord Chunk coordinate to clear data for
     * @return True if clear was successful
     */
    bool ClearChunkVegetationData(const FIntVector& ChunkCoord) const;

    /**
     * Get resource availability for a specific resource type at a location
     * @param ResourceName Name of the resource to check
     * @param WorldX X coordinate in world space
     * @param WorldY Y coordinate in world space
     * @param TerrainHeight Height of terrain at this location
     * @param ChunkCoord Optional chunk coordinate for additional seed mixing
     * @return Resource availability (0.0 to 1.0)
     */
    float GetResourceAvailability(const FString& ResourceName, float WorldX, float WorldY, float TerrainHeight = 0.0f, const FIntVector& ChunkCoord = FIntVector::ZeroValue) const;

    /**
     * Get all available resource types at a location
     * @param WorldX X coordinate in world space
     * @param WorldY Y coordinate in world space
     * @param TerrainHeight Height of terrain at this location
     * @param ChunkCoord Optional chunk coordinate for additional seed mixing
     * @return Array of available resource data
     */
    TArray<FResourceData> GetAvailableResources(float WorldX, float WorldY, float TerrainHeight = 0.0f, const FIntVector& ChunkCoord = FIntVector::ZeroValue) const;

private:
    /** World generation settings */
    FWorldGenSettings Settings;

    /** Reference to the biome system for accessing biome data */
    const FBiomeSystem* BiomeSystem;

    /** Deterministic noise generator for vegetation placement */
    FNoiseGenerator NoiseGenerator;

    /** Cache of vegetation profiles per biome type */
    TMap<EBiomeType, FVegetationProfile> BiomeVegetationProfiles;

    /**
     * Initialize default vegetation profiles for each biome type
     */
    void InitializeDefaultVegetationProfiles();

    /**
     * Create vegetation profile for Meadows biome
     * @return Vegetation profile for Meadows
     */
    FVegetationProfile CreateMeadowsVegetationProfile() const;

    /**
     * Create vegetation profile for BlackForest biome
     * @return Vegetation profile for BlackForest
     */
    FVegetationProfile CreateBlackForestVegetationProfile() const;

    /**
     * Create vegetation profile for Swamp biome
     * @return Vegetation profile for Swamp
     */
    FVegetationProfile CreateSwampVegetationProfile() const;

    /**
     * Create vegetation profile for Mountains biome
     * @return Vegetation profile for Mountains
     */
    FVegetationProfile CreateMountainsVegetationProfile() const;

    /**
     * Create vegetation profile for Ocean biome
     * @return Vegetation profile for Ocean
     */
    FVegetationProfile CreateOceanVegetationProfile() const;

    /**
     * Blend vegetation profiles from multiple biomes based on weights
     * @param BiomeWeights Weights for each biome
     * @return Blended vegetation profile
     */
    FVegetationProfile BlendVegetationProfiles(const FBiomeWeights& BiomeWeights) const;

    /**
     * Calculate base vegetation density using noise
     * @param WorldX X coordinate in world space
     * @param WorldY Y coordinate in world space
     * @param ChunkCoord Optional chunk coordinate for additional seed mixing
     * @return Base vegetation density (0.0 to 1.0)
     */
    float CalculateBaseVegetationDensity(float WorldX, float WorldY, const FIntVector& ChunkCoord = FIntVector::ZeroValue) const;

    /**
     * Apply terrain height modifiers to vegetation density
     * @param BaseDensity Base vegetation density
     * @param TerrainHeight Height of terrain
     * @param BiomeType Dominant biome type
     * @return Modified vegetation density
     */
    float ApplyHeightModifiers(float BaseDensity, float TerrainHeight, EBiomeType BiomeType) const;

    /**
     * Get the file path for chunk vegetation data
     * @param ChunkCoord Chunk coordinate
     * @return File path for vegetation data storage
     */
    FString GetChunkVegetationDataFilePath(const FIntVector& ChunkCoord) const;

    /**
     * Ensure the vegetation data directory exists
     * @return True if directory exists or was created successfully
     */
    bool EnsureVegetationDataDirectory() const;

    /**
     * Convert chunk coordinate to world position (center of chunk)
     * @param ChunkCoord Chunk coordinate
     * @return World position of chunk center
     */
    FVector2D ChunkCoordToWorldPosition(const FIntVector& ChunkCoord) const;

    /**
     * Sample vegetation density at regular intervals within a chunk
     * @param ChunkCoord Chunk coordinate
     * @param SampleResolution Number of samples per chunk edge
     * @return Array of vegetation density samples
     */
    TArray<FVegetationDensity> SampleChunkVegetationDensity(const FIntVector& ChunkCoord, int32 SampleResolution) const;
};