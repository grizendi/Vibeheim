#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Materials/MaterialInterface.h"
#include "VoxelGenerators/VoxelGenerator.h"
#include "WorldGenSettings.generated.h"

/**
 * Configuration structure for world generation parameters
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FWorldGenSettings
{
    GENERATED_BODY()

    /** Deterministic seed for world generation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Generation", meta = (ClampMin = "0"))
    int64 Seed = 1337;

    /** World generation version for compatibility tracking */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Generation")
    int32 WorldGenVersion = 1;

    /** Plugin SHA for determinism validation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Generation")
    FString PluginSHA = "";

    /** Size of each voxel in centimeters */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel Settings", meta = (ClampMin = "1.0", ClampMax = "200.0"))
    float VoxelSizeCm = 50.0f;

    /** Size of each chunk in voxels (cubic) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel Settings", meta = (ClampMin = "8", ClampMax = "128"))
    int32 ChunkSize = 32;

    /** Maximum level of detail */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming", meta = (ClampMin = "1", ClampMax = "5"))
    int32 MaxLOD = 3;

    /** Radius in chunks for LOD0 (full detail + collision) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming", meta = (ClampMin = "1", ClampMax = "10"))
    int32 LOD0Radius = 2;

    /** Radius in chunks for LOD1 (collision enabled) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming", meta = (ClampMin = "1", ClampMax = "15"))
    int32 LOD1Radius = 4;

    /** Radius in chunks for LOD2 (visual only) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming", meta = (ClampMin = "1", ClampMax = "20"))
    int32 LOD2Radius = 6;

    /** Whether to enable collision only up to LOD1 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming")
    bool bCollisionUpToLOD1 = true;

    /** Biome blend distance in meters */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biomes", meta = (ClampMin = "1.0", ClampMax = "100.0"))
    float BiomeBlendMeters = 24.0f;

    /** Auto-flush timer for edit operations in milliseconds */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Persistence", meta = (ClampMin = "1000", ClampMax = "10000"))
    int32 SaveFlushMs = 3000;

    // Enhanced terrain parameters
    /** Height threshold above which mountain biome dominates */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enhanced Terrain", meta = (ClampMin = "50.0", ClampMax = "500.0"))
    float MountainHeightThreshold = 200.0f;

    /** Height threshold below which water biome dominates */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enhanced Terrain", meta = (ClampMin = "-50.0", ClampMax = "50.0"))
    float WaterHeightThreshold = -10.0f;

    /** Scale factor for ridged multifractal noise */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enhanced Terrain", meta = (ClampMin = "0.0001", ClampMax = "0.01"))
    float RidgedNoiseScale = 0.001f;

    /** Strength of domain warping effect */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enhanced Terrain", meta = (ClampMin = "10.0", ClampMax = "200.0"))
    float DomainWarpStrength = 50.0f;

    /** Enable river network generation using flow accumulation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enhanced Terrain")
    bool bEnableRiverGeneration = true;

    /** Flow threshold for river formation (0.0-1.0) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enhanced Terrain", meta = (ClampMin = "0.1", ClampMax = "0.9"))
    float RiverFlowThreshold = 0.3f;

    /** Base terrain scale for heightmap generation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enhanced Terrain", meta = (ClampMin = "0.0001", ClampMax = "0.01"))
    float BaseTerrainScale = 0.002f;

    // Biome noise parameters
    /** Noise scale for Meadows biome */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome Noise", meta = (ClampMin = "0.0001", ClampMax = "0.01"))
    float MeadowsScale = 0.0025f;

    /** Noise scale for BlackForest biome */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome Noise", meta = (ClampMin = "0.0001", ClampMax = "0.01"))
    float BlackForestScale = 0.0030f;

    /** Noise scale for Swamp biome */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome Noise", meta = (ClampMin = "0.0001", ClampMax = "0.01"))
    float SwampScale = 0.0020f;

    /** Material to use for voxel rendering */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel Settings")
    TSoftObjectPtr<UMaterialInterface> VoxelMaterial;

    /** Generator class to use for terrain generation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel Settings")
    TSoftClassPtr<UVoxelGenerator> GeneratorClass;

    FWorldGenSettings()
    {
        // Default constructor with default values already set above
        // Set default material to the simple color material from VoxelPluginLegacy
        VoxelMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/VoxelFree/Examples/Shared/VoxelExamples_SimpleColorMaterial.VoxelExamples_SimpleColorMaterial")));
        // Set default generator to a basic flat world generator
        GeneratorClass = TSoftClassPtr<UVoxelGenerator>(FSoftClassPath(TEXT("/Script/Voxel.VoxelFlatGenerator")));
    }

    /**
     * Load settings from a JSON configuration file
     * @param FilePath Path to the JSON configuration file
     * @return True if loading was successful
     */
    bool LoadFromJSON(const FString& FilePath);

    /**
     * Save current settings to a JSON configuration file
     * @param FilePath Path where to save the JSON configuration file
     * @return True if saving was successful
     */
    bool SaveToJSON(const FString& FilePath) const;

    /**
     * Validate all configuration parameters are within acceptable ranges
     * @return True if all settings are valid
     */
    bool ValidateSettings() const;
};