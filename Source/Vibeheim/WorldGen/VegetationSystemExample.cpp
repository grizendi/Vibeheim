#include "VegetationSystemExample.h"
#include "VegetationSystem.h"
#include "BiomeSystem.h"
#include "Data/WorldGenSettings.h"
#include "Engine/Engine.h"

DEFINE_LOG_CATEGORY_STATIC(LogVegetationSystemExample, Log, All);

void FVegetationSystemExample::RunExample()
{
    UE_LOG(LogVegetationSystemExample, Log, TEXT("Running VegetationSystem example"));
    
    // Create example world generation settings
    FWorldGenSettings ExampleSettings;
    ExampleSettings.Seed = 12345;
    ExampleSettings.WorldGenVersion = 2;
    ExampleSettings.VoxelSizeCm = 50.0f;
    ExampleSettings.ChunkSize = 32;
    ExampleSettings.BiomeBlendMeters = 24.0f;
    
    // Initialize biome system (required for vegetation system)
    FBiomeSystem BiomeSystem;
    BiomeSystem.Initialize(ExampleSettings);
    
    // Initialize vegetation system
    FVegetationSystem VegetationSystem;
    VegetationSystem.Initialize(ExampleSettings, &BiomeSystem);
    
    // Example 1: Get vegetation data for different biomes
    UE_LOG(LogVegetationSystemExample, Log, TEXT("=== Biome Vegetation Profiles ==="));
    
    for (int32 BiomeIndex = 0; BiomeIndex < static_cast<int32>(EBiomeType::Count); ++BiomeIndex)
    {
        EBiomeType BiomeType = static_cast<EBiomeType>(BiomeIndex);
        const FVegetationProfile& Profile = VegetationSystem.GetVegetationDataForBiome(BiomeType);
        
        UE_LOG(LogVegetationSystemExample, Log, TEXT("Biome %d: TreeDensity=%.2f, FoliageDensity=%.2f, ResourceDensity=%.2f, TreeSpecies=%d, Resources=%d"),
            BiomeIndex, Profile.TreeDensity, Profile.FoliageDensity, Profile.ResourceDensity,
            Profile.TreeSpecies.Num(), Profile.HarvestableResources.Num());
    }
    
    // Example 2: Calculate vegetation density at various locations
    UE_LOG(LogVegetationSystemExample, Log, TEXT("=== Vegetation Density Sampling ==="));
    
    TArray<FVector> TestLocations = {
        FVector(0.0f, 0.0f, 0.0f),        // Origin
        FVector(1000.0f, 0.0f, 0.0f),     // East
        FVector(0.0f, 1000.0f, 0.0f),     // North
        FVector(2000.0f, 2000.0f, 0.0f),  // Far northeast
        FVector(-1000.0f, -1000.0f, 0.0f) // Southwest
    };
    
    for (const FVector& Location : TestLocations)
    {
        FIntVector ChunkCoord(
            FMath::FloorToInt(Location.X / (ExampleSettings.ChunkSize * ExampleSettings.VoxelSizeCm)),
            FMath::FloorToInt(Location.Y / (ExampleSettings.ChunkSize * ExampleSettings.VoxelSizeCm)),
            0
        );
        
        FVegetationDensity Density = VegetationSystem.CalculateVegetationDensity(Location.X, Location.Y, Location.Z, ChunkCoord);
        
        UE_LOG(LogVegetationSystemExample, Log, TEXT("Location (%.0f, %.0f): Overall=%.3f, Tree=%.3f, Foliage=%.3f, Resource=%.3f, DominantBiome=%d"),
            Location.X, Location.Y, Density.OverallDensity, Density.TreeDensity, 
            Density.FoliageDensity, Density.ResourceDensity, static_cast<int32>(Density.DominantBiome));
    }
    
    // Example 3: Get vegetation availability (blended from multiple biomes)
    UE_LOG(LogVegetationSystemExample, Log, TEXT("=== Vegetation Availability ==="));
    
    FVector TestLocation(500.0f, 500.0f, 0.0f);
    FIntVector TestChunk(0, 0, 0);
    FVegetationAvailability Availability = VegetationSystem.GetVegetationAvailability(TestLocation.X, TestLocation.Y, TestLocation.Z, TestChunk);
    
    UE_LOG(LogVegetationSystemExample, Log, TEXT("At location (%.0f, %.0f): TreeSpecies=%d, FoliageTypes=%d, Resources=%d"),
        TestLocation.X, TestLocation.Y, Availability.AvailableTreeSpecies.Num(),
        Availability.AvailableFoliageTypes.Num(), Availability.AvailableResources.Num());
    
    // Log available resources
    for (const FResourceData& Resource : Availability.AvailableResources)
    {
        UE_LOG(LogVegetationSystemExample, Log, TEXT("  Resource: %s (Type: %s, SpawnRate: %.2f, Quantity: %d-%d)"),
            *Resource.ResourceName, *Resource.ResourceType, Resource.SpawnRate, Resource.MinQuantity, Resource.MaxQuantity);
    }
    
    // Example 4: Generate and save chunk vegetation data
    UE_LOG(LogVegetationSystemExample, Log, TEXT("=== Chunk Vegetation Data ==="));
    
    FIntVector ExampleChunk(0, 0, 0);
    FChunkVegetationData ChunkData = VegetationSystem.GenerateChunkVegetationData(ExampleChunk, 4); // 4x4 samples
    
    UE_LOG(LogVegetationSystemExample, Log, TEXT("Generated chunk data for (%d, %d, %d): %d density samples, Seed=%llu, Version=%d"),
        ChunkData.ChunkCoord.X, ChunkData.ChunkCoord.Y, ChunkData.ChunkCoord.Z,
        ChunkData.DensityMap.Num(), ChunkData.GenerationSeed, ChunkData.WorldGenVersion);
    
    // Save the chunk data
    bool bSaveSuccess = VegetationSystem.SaveChunkVegetationData(ChunkData);
    UE_LOG(LogVegetationSystemExample, Log, TEXT("Save chunk vegetation data: %s"), bSaveSuccess ? TEXT("SUCCESS") : TEXT("FAILED"));
    
    // Load it back
    FChunkVegetationData LoadedData;
    bool bLoadSuccess = VegetationSystem.LoadChunkVegetationData(ExampleChunk, LoadedData);
    UE_LOG(LogVegetationSystemExample, Log, TEXT("Load chunk vegetation data: %s"), bLoadSuccess ? TEXT("SUCCESS") : TEXT("FAILED"));
    
    if (bLoadSuccess)
    {
        UE_LOG(LogVegetationSystemExample, Log, TEXT("Loaded data matches: Seed=%s, Version=%s, Samples=%s"),
            (LoadedData.GenerationSeed == ChunkData.GenerationSeed) ? TEXT("YES") : TEXT("NO"),
            (LoadedData.WorldGenVersion == ChunkData.WorldGenVersion) ? TEXT("YES") : TEXT("NO"),
            (LoadedData.DensityMap.Num() == ChunkData.DensityMap.Num()) ? TEXT("YES") : TEXT("NO"));
    }
    
    // Example 5: Resource availability queries
    UE_LOG(LogVegetationSystemExample, Log, TEXT("=== Resource Availability Queries ==="));
    
    TArray<FString> TestResources = {TEXT("Wood"), TEXT("Berries"), TEXT("Stone"), TEXT("Mushrooms"), TEXT("Herbs")};
    
    for (const FString& ResourceName : TestResources)
    {
        float ResourceAvailability = VegetationSystem.GetResourceAvailability(ResourceName, TestLocation.X, TestLocation.Y, TestLocation.Z, TestChunk);
        UE_LOG(LogVegetationSystemExample, Log, TEXT("Resource '%s' availability at (%.0f, %.0f): %.3f"),
            *ResourceName, TestLocation.X, TestLocation.Y, ResourceAvailability);
    }
    
    // Clean up test data
    VegetationSystem.ClearChunkVegetationData(ExampleChunk);
    
    UE_LOG(LogVegetationSystemExample, Log, TEXT("VegetationSystem example completed successfully"));
}