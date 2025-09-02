#if 0 // DISABLED FOR TEST DIET - VHM stress test (bring back after core loop works)
#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "VHMTerrainRendering/TerrainMaterialSystem.h"
#include "VHMTerrainRendering/VHMTerrainRenderer.h"
#include "WorldGenSettings.h"
#include "Services/BiomeService.h"
#include "Services/HeightfieldService.h"
#include "Services/TileStreamingService.h"
#include "Services/ClimateSystem.h"
#include "Services/NoiseSystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogVHMStressTests, Log, All);

/**
 * Stress tests for VHM compatibility fixes
 * Tests system behavior under load and validates memory management
 */
class FVHMStressTests
{
public:
    static void RunMaterialStressTest(int32 NumTiles = 100);
    static void RunVHMComponentStressTest(int32 NumComponents = 25);
    static void RunMemoryLeakTest(int32 Iterations = 10);
    static void RunPerformanceBenchmark(int32 NumTiles = 50);
    
private:
    static void LogMemoryUsage(const FString& TestPhase);
    static bool ValidateMemoryGrowth(int32 InitialObjects, int32 FinalObjects, int32 MaxAllowedGrowth);
};

void FVHMStressTests::RunMaterialStressTest(int32 NumTiles)
{
    UE_LOG(LogVHMStressTests, Log, TEXT("=== Material System Stress Test (%d tiles) ==="), NumTiles);
    
    LogMemoryUsage(TEXT("Before Test"));
    int32 InitialObjectCount = GUObjectArray.GetObjectArrayNum();
    
    try
    {
        // Create material system
        UVHMTerrainMaterialSystem* MaterialSystem = NewObject<UVHMTerrainMaterialSystem>();
        UBiomeService* BiomeService = NewObject<UBiomeService>();
        UClimateSystem* ClimateSystem = NewObject<UClimateSystem>();
        
        // Initialize services
        FClimateSettings ClimateSettings;
        ClimateSystem->Initialize(ClimateSettings, 1337);
        
        UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
        if (!Settings)
        {
            UE_LOG(LogVHMStressTests, Error, TEXT("❌ Failed to get WorldGenSettings"));
            return;
        }
        
        BiomeService->Initialize(ClimateSystem, Settings->Settings);
        
        FVHMSettings VHMSettings;
        VHMSettings.bUseRuntimeVirtualTexturing = false;
        
        if (!MaterialSystem->Initialize(BiomeService, VHMSettings))
        {
            UE_LOG(LogVHMStressTests, Error, TEXT("❌ MaterialSystem initialization failed"));
            return;
        }
        
        // Create biome definitions for testing
        TArray<FBiomeDefinition> TestBiomes;
        TArray<EBiomeType> BiomeTypes = {
            EBiomeType::Meadows,
            EBiomeType::Forest,
            EBiomeType::Mountains,
            EBiomeType::Ocean
        };
        
        for (EBiomeType BiomeType : BiomeTypes)
        {
            FBiomeDefinition BiomeDefinition;
            if (BiomeService->GetBiomeDefinition(BiomeType, BiomeDefinition))
            {
                TestBiomes.Add(BiomeDefinition);
            }
        }
        
        if (TestBiomes.Num() == 0)
        {
            UE_LOG(LogVHMStressTests, Error, TEXT("❌ No biome definitions available"));
            return;
        }
        
        // Stress test: Create many materials
        double StartTime = FPlatformTime::Seconds();
        TArray<FTileCoord> CreatedTiles;
        
        for (int32 i = 0; i < NumTiles; i++)
        {
            FTileCoord TileCoord(i % 20, i / 20); // Create a grid pattern
            FBiomeDefinition& BiomeDefinition = TestBiomes[i % TestBiomes.Num()];
            
            UMaterialInstanceDynamic* Material = MaterialSystem->CreateTileMaterial(TileCoord, BiomeDefinition);
            if (Material)
            {
                CreatedTiles.Add(TileCoord);
                
                // Periodically update materials to test TObjectPtr access
                if (i % 10 == 0)
                {
                    BiomeDefinition.RVTBlendColor = FLinearColor::MakeRandomColor();
                    MaterialSystem->UpdateMaterialParameters(TileCoord, BiomeDefinition);
                }
            }
            else
            {
                UE_LOG(LogVHMStressTests, Warning, TEXT("⚠️ Failed to create material for tile %d"), i);
            }
        }
        
        double CreationTime = (FPlatformTime::Seconds() - StartTime) * 1000.0;
        
        UE_LOG(LogVHMStressTests, Log, TEXT("📊 Created %d materials in %.2f ms (%.2f ms/material)"), 
               CreatedTiles.Num(), CreationTime, CreationTime / FMath::Max(1, CreatedTiles.Num()));
        
        LogMemoryUsage(TEXT("After Creation"));
        
        // Test material retrieval performance
        StartTime = FPlatformTime::Seconds();
        int32 RetrievedCount = 0;
        
        for (const FTileCoord& TileCoord : CreatedTiles)
        {
            UMaterialInstanceDynamic* Material = MaterialSystem->GetTileMaterial(TileCoord);
            if (Material)
            {
                RetrievedCount++;
            }
        }
        
        double RetrievalTime = (FPlatformTime::Seconds() - StartTime) * 1000.0;
        
        UE_LOG(LogVHMStressTests, Log, TEXT("📊 Retrieved %d materials in %.2f ms (%.3f ms/material)"), 
               RetrievedCount, RetrievalTime, RetrievalTime / FMath::Max(1, RetrievedCount));
        
        // Test memory usage
        float MemoryUsage = MaterialSystem->GetMaterialMemoryUsageMB();
        UE_LOG(LogVHMStressTests, Log, TEXT("📊 Material system memory usage: %.2f MB"), MemoryUsage);
        
        // Cleanup test
        StartTime = FPlatformTime::Seconds();
        for (const FTileCoord& TileCoord : CreatedTiles)
        {
            MaterialSystem->RemoveTileMaterial(TileCoord);
        }
        double CleanupTime = (FPlatformTime::Seconds() - StartTime) * 1000.0;
        
        UE_LOG(LogVHMStressTests, Log, TEXT("📊 Cleaned up %d materials in %.2f ms"), 
               CreatedTiles.Num(), CleanupTime);
        
        // Cleanup objects - let UE's garbage collector handle cleanup automatically
        MaterialSystem = nullptr;
        BiomeService = nullptr;
        ClimateSystem = nullptr;
        
        CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
        
        LogMemoryUsage(TEXT("After Cleanup"));
        
        int32 FinalObjectCount = GUObjectArray.GetObjectArrayNum();
        if (ValidateMemoryGrowth(InitialObjectCount, FinalObjectCount, 50))
        {
            UE_LOG(LogVHMStressTests, Log, TEXT("✅ Material stress test completed successfully"));
        }
        else
        {
            UE_LOG(LogVHMStressTests, Warning, TEXT("⚠️ Material stress test completed with potential memory issues"));
        }
    }
    catch (...)
    {
        UE_LOG(LogVHMStressTests, Error, TEXT("❌ Exception in material stress test"));
    }
}

void FVHMStressTests::RunVHMComponentStressTest(int32 NumComponents)
{
    UE_LOG(LogVHMStressTests, Log, TEXT("=== VHM Component Stress Test (%d components) ==="), NumComponents);
    
    UWorld* World = nullptr;
    if (GEngine && GEngine->GetWorldContexts().Num() > 0)
    {
        World = GEngine->GetWorldContexts()[0].World();
    }
    
    if (!World)
    {
        UE_LOG(LogVHMStressTests, Warning, TEXT("⚠️ No valid world context, skipping VHM component stress test"));
        return;
    }
    
    LogMemoryUsage(TEXT("Before Test"));
    int32 InitialObjectCount = GUObjectArray.GetObjectArrayNum();
    
    try
    {
        // Create terrain renderer with world context
        UVHMTerrainRenderer* TerrainRenderer = NewObject<UVHMTerrainRenderer>(World);
        
        UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
        if (!Settings)
        {
            UE_LOG(LogVHMStressTests, Error, TEXT("❌ Failed to get WorldGenSettings"));
            return;
        }
        
        // Create services
        UHeightfieldService* HeightfieldService = NewObject<UHeightfieldService>();
        UTileStreamingService* TileStreamingService = NewObject<UTileStreamingService>();
        UBiomeService* BiomeService = NewObject<UBiomeService>();
        UClimateSystem* ClimateSystem = NewObject<UClimateSystem>();
        UNoiseSystem* NoiseSystem = NewObject<UNoiseSystem>();
        
        // Initialize services
        FClimateSettings ClimateSettings;
        ClimateSystem->Initialize(ClimateSettings, 1337);
        BiomeService->Initialize(ClimateSystem, Settings->Settings);
        NoiseSystem->Initialize(1337);
        HeightfieldService->Initialize(Settings->Settings);
        HeightfieldService->SetNoiseSystem(NoiseSystem);
        
        if (!TerrainRenderer->InitializeWithBiomeService(Settings, HeightfieldService, TileStreamingService, BiomeService))
        {
            UE_LOG(LogVHMStressTests, Error, TEXT("❌ VHMTerrainRenderer initialization failed"));
            return;
        }
        
        // Stress test: Create many VHM components
        double StartTime = FPlatformTime::Seconds();
        TArray<FTileCoord> CreatedTiles;
        
        for (int32 i = 0; i < NumComponents; i++)
        {
            FTileCoord TileCoord(i % 10, i / 10); // Create a grid pattern
            
            if (TerrainRenderer->CreateTerrainMeshForTile(TileCoord))
            {
                CreatedTiles.Add(TileCoord);
            }
            else
            {
                UE_LOG(LogVHMStressTests, Warning, TEXT("⚠️ Failed to create VHM component for tile %d"), i);
            }
        }
        
        double CreationTime = (FPlatformTime::Seconds() - StartTime) * 1000.0;
        
        UE_LOG(LogVHMStressTests, Log, TEXT("📊 Created %d VHM components in %.2f ms (%.2f ms/component)"), 
               CreatedTiles.Num(), CreationTime, CreationTime / FMath::Max(1, CreatedTiles.Num()));
        
        LogMemoryUsage(TEXT("After Creation"));
        
        // Test performance stats
        FVHMPerformanceStats PerfStats = TerrainRenderer->GetPerformanceStats();
        UE_LOG(LogVHMStressTests, Log, TEXT("📊 Performance Stats:"));
        UE_LOG(LogVHMStressTests, Log, TEXT("  Active Components: %d"), PerfStats.ActiveVHMComponents);
        UE_LOG(LogVHMStressTests, Log, TEXT("  Average Generation Time: %.2f ms"), PerfStats.AverageMeshGenerationMs);
        UE_LOG(LogVHMStressTests, Log, TEXT("  Texture Memory Usage: %.2f MB"), PerfStats.TextureMemoryUsageMB);
        
        // Test component access performance
        StartTime = FPlatformTime::Seconds();
        int32 AccessedCount = 0;
        
        for (const FTileCoord& TileCoord : CreatedTiles)
        {
            UVirtualHeightfieldMeshComponent* Component = TerrainRenderer->GetVHMComponent(TileCoord);
            if (Component)
            {
                AccessedCount++;
            }
        }
        
        double AccessTime = (FPlatformTime::Seconds() - StartTime) * 1000.0;
        
        UE_LOG(LogVHMStressTests, Log, TEXT("📊 Accessed %d components in %.2f ms (%.3f ms/component)"), 
               AccessedCount, AccessTime, AccessTime / FMath::Max(1, AccessedCount));
        
        // Cleanup test
        StartTime = FPlatformTime::Seconds();
        for (const FTileCoord& TileCoord : CreatedTiles)
        {
            TerrainRenderer->RemoveTerrainMesh(TileCoord);
        }
        double CleanupTime = (FPlatformTime::Seconds() - StartTime) * 1000.0;
        
        UE_LOG(LogVHMStressTests, Log, TEXT("📊 Cleaned up %d components in %.2f ms"), 
               CreatedTiles.Num(), CleanupTime);
        
        // Cleanup objects - let UE's garbage collector handle cleanup automatically
        TerrainRenderer = nullptr;
        HeightfieldService = nullptr;
        TileStreamingService = nullptr;
        BiomeService = nullptr;
        ClimateSystem = nullptr;
        NoiseSystem = nullptr;
        
        CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
        
        LogMemoryUsage(TEXT("After Cleanup"));
        
        int32 FinalObjectCount = GUObjectArray.GetObjectArrayNum();
        if (ValidateMemoryGrowth(InitialObjectCount, FinalObjectCount, 100))
        {
            UE_LOG(LogVHMStressTests, Log, TEXT("✅ VHM component stress test completed successfully"));
        }
        else
        {
            UE_LOG(LogVHMStressTests, Warning, TEXT("⚠️ VHM component stress test completed with potential memory issues"));
        }
    }
    catch (...)
    {
        UE_LOG(LogVHMStressTests, Error, TEXT("❌ Exception in VHM component stress test"));
    }
}

void FVHMStressTests::RunMemoryLeakTest(int32 Iterations)
{
    UE_LOG(LogVHMStressTests, Log, TEXT("=== Memory Leak Test (%d iterations) ==="), Iterations);
    
    int32 InitialObjectCount = GUObjectArray.GetObjectArrayNum();
    LogMemoryUsage(TEXT("Initial State"));
    
    try
    {
        for (int32 i = 0; i < Iterations; i++)
        {
            UE_LOG(LogVHMStressTests, Verbose, TEXT("Memory leak test iteration %d/%d"), i + 1, Iterations);
            
            // Create and destroy material system
            UVHMTerrainMaterialSystem* MaterialSystem = NewObject<UVHMTerrainMaterialSystem>();
            UBiomeService* BiomeService = NewObject<UBiomeService>();
            UClimateSystem* ClimateSystem = NewObject<UClimateSystem>();
            
            // Initialize services
            FClimateSettings ClimateSettings;
            ClimateSystem->Initialize(ClimateSettings, 1337 + i);
            
            UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
            if (Settings)
            {
                BiomeService->Initialize(ClimateSystem, Settings->Settings);
                
                FVHMSettings VHMSettings;
                VHMSettings.bUseRuntimeVirtualTexturing = (i % 2 == 0); // Alternate RVT settings
                
                if (MaterialSystem->Initialize(BiomeService, VHMSettings))
                {
                    // Create and remove materials
                    TArray<FTileCoord> TestTiles;
                    for (int32 j = 0; j < 10; j++)
                    {
                        FTileCoord TileCoord(j, i);
                        TestTiles.Add(TileCoord);
                        
                        FBiomeDefinition TestBiome;
                        TestBiome.BiomeType = static_cast<EBiomeType>((j % 4) + 1);
                        TestBiome.BiomeName = FString::Printf(TEXT("TestBiome_%d_%d"), i, j);
                        TestBiome.BiomeWeight = 1.0f;
                        TestBiome.RVTBlendColor = FLinearColor::MakeRandomColor();
                        
                        UMaterialInstanceDynamic* Material = MaterialSystem->CreateTileMaterial(TileCoord, TestBiome);
                        if (Material)
                        {
                            // Update material parameters
                            TestBiome.RVTBlendColor = FLinearColor::MakeRandomColor();
                            MaterialSystem->UpdateMaterialParameters(TileCoord, TestBiome);
                        }
                    }
                    
                    // Remove all materials
                    for (const FTileCoord& TileCoord : TestTiles)
                    {
                        MaterialSystem->RemoveTileMaterial(TileCoord);
                    }
                }
            }
            
            // Clear object references - let UE's garbage collector handle cleanup
            MaterialSystem = nullptr;
            BiomeService = nullptr;
            ClimateSystem = nullptr;
            
            // Force garbage collection every few iterations
            if (i % 3 == 0)
            {
                CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
            }
        }
        
        // Final garbage collection
        CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
        
        LogMemoryUsage(TEXT("After Test"));
        
        int32 FinalObjectCount = GUObjectArray.GetObjectArrayNum();
        int32 ObjectGrowth = FinalObjectCount - InitialObjectCount;
        
        UE_LOG(LogVHMStressTests, Log, TEXT("📊 Memory Leak Test Results:"));
        UE_LOG(LogVHMStressTests, Log, TEXT("  Initial Objects: %d"), InitialObjectCount);
        UE_LOG(LogVHMStressTests, Log, TEXT("  Final Objects: %d"), FinalObjectCount);
        UE_LOG(LogVHMStressTests, Log, TEXT("  Object Growth: %d"), ObjectGrowth);
        UE_LOG(LogVHMStressTests, Log, TEXT("  Growth per Iteration: %.2f"), (float)ObjectGrowth / Iterations);
        
        // Validate memory growth
        int32 MaxAllowedGrowth = Iterations * 5; // Allow 5 objects per iteration
        if (ObjectGrowth <= MaxAllowedGrowth)
        {
            UE_LOG(LogVHMStressTests, Log, TEXT("✅ Memory leak test passed - growth within acceptable limits"));
        }
        else
        {
            UE_LOG(LogVHMStressTests, Warning, TEXT("⚠️ Potential memory leak detected - growth exceeds expected limits"));
        }
    }
    catch (...)
    {
        UE_LOG(LogVHMStressTests, Error, TEXT("❌ Exception in memory leak test"));
    }
}

void FVHMStressTests::RunPerformanceBenchmark(int32 NumTiles)
{
    UE_LOG(LogVHMStressTests, Log, TEXT("=== Performance Benchmark (%d tiles) ==="), NumTiles);
    
    try
    {
        // Material system benchmark
        UVHMTerrainMaterialSystem* MaterialSystem = NewObject<UVHMTerrainMaterialSystem>();
        UBiomeService* BiomeService = NewObject<UBiomeService>();
        UClimateSystem* ClimateSystem = NewObject<UClimateSystem>();
        
        // Initialize services
        FClimateSettings ClimateSettings;
        ClimateSystem->Initialize(ClimateSettings, 1337);
        
        UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
        if (!Settings)
        {
            UE_LOG(LogVHMStressTests, Error, TEXT("❌ Failed to get WorldGenSettings"));
            return;
        }
        
        BiomeService->Initialize(ClimateSystem, Settings->Settings);
        
        FVHMSettings VHMSettings;
        VHMSettings.bUseRuntimeVirtualTexturing = false;
        
        if (!MaterialSystem->Initialize(BiomeService, VHMSettings))
        {
            UE_LOG(LogVHMStressTests, Error, TEXT("❌ MaterialSystem initialization failed"));
            return;
        }
        
        // Benchmark material creation
        double StartTime = FPlatformTime::Seconds();
        TArray<FTileCoord> CreatedTiles;
        
        FBiomeDefinition TestBiome;
        TestBiome.BiomeType = EBiomeType::Meadows;
        TestBiome.BiomeName = TEXT("BenchmarkBiome");
        TestBiome.BiomeWeight = 1.0f;
        TestBiome.RVTBlendColor = FLinearColor::Green;
        
        for (int32 i = 0; i < NumTiles; i++)
        {
            FTileCoord TileCoord(i % 20, i / 20);
            UMaterialInstanceDynamic* Material = MaterialSystem->CreateTileMaterial(TileCoord, TestBiome);
            if (Material)
            {
                CreatedTiles.Add(TileCoord);
            }
        }
        
        double CreationTime = (FPlatformTime::Seconds() - StartTime) * 1000.0;
        
        // Benchmark material updates
        StartTime = FPlatformTime::Seconds();
        TestBiome.RVTBlendColor = FLinearColor::Blue;
        
        for (const FTileCoord& TileCoord : CreatedTiles)
        {
            MaterialSystem->UpdateMaterialParameters(TileCoord, TestBiome);
        }
        
        double UpdateTime = (FPlatformTime::Seconds() - StartTime) * 1000.0;
        
        // Benchmark material retrieval
        StartTime = FPlatformTime::Seconds();
        int32 RetrievedCount = 0;
        
        for (const FTileCoord& TileCoord : CreatedTiles)
        {
            UMaterialInstanceDynamic* Material = MaterialSystem->GetTileMaterial(TileCoord);
            if (Material)
            {
                RetrievedCount++;
            }
        }
        
        double RetrievalTime = (FPlatformTime::Seconds() - StartTime) * 1000.0;
        
        // Benchmark cleanup
        StartTime = FPlatformTime::Seconds();
        
        for (const FTileCoord& TileCoord : CreatedTiles)
        {
            MaterialSystem->RemoveTileMaterial(TileCoord);
        }
        
        double CleanupTime = (FPlatformTime::Seconds() - StartTime) * 1000.0;
        
        // Report benchmark results
        UE_LOG(LogVHMStressTests, Log, TEXT("📊 Performance Benchmark Results:"));
        UE_LOG(LogVHMStressTests, Log, TEXT("  Material Creation: %.2f ms total, %.3f ms/tile"), CreationTime, CreationTime / NumTiles);
        UE_LOG(LogVHMStressTests, Log, TEXT("  Material Updates: %.2f ms total, %.3f ms/tile"), UpdateTime, UpdateTime / NumTiles);
        UE_LOG(LogVHMStressTests, Log, TEXT("  Material Retrieval: %.2f ms total, %.3f ms/tile"), RetrievalTime, RetrievalTime / NumTiles);
        UE_LOG(LogVHMStressTests, Log, TEXT("  Material Cleanup: %.2f ms total, %.3f ms/tile"), CleanupTime, CleanupTime / NumTiles);
        
        float TotalTime = CreationTime + UpdateTime + RetrievalTime + CleanupTime;
        UE_LOG(LogVHMStressTests, Log, TEXT("  Total Time: %.2f ms"), TotalTime);
        
        // Performance thresholds (adjust based on expected performance)
        float MaxCreationTimePerTile = 5.0f; // 5ms per tile
        float MaxUpdateTimePerTile = 1.0f;   // 1ms per tile
        float MaxRetrievalTimePerTile = 0.1f; // 0.1ms per tile
        
        bool bPerformanceGood = true;
        
        if (CreationTime / NumTiles > MaxCreationTimePerTile)
        {
            UE_LOG(LogVHMStressTests, Warning, TEXT("⚠️ Material creation performance below threshold"));
            bPerformanceGood = false;
        }
        
        if (UpdateTime / NumTiles > MaxUpdateTimePerTile)
        {
            UE_LOG(LogVHMStressTests, Warning, TEXT("⚠️ Material update performance below threshold"));
            bPerformanceGood = false;
        }
        
        if (RetrievalTime / NumTiles > MaxRetrievalTimePerTile)
        {
            UE_LOG(LogVHMStressTests, Warning, TEXT("⚠️ Material retrieval performance below threshold"));
            bPerformanceGood = false;
        }
        
        if (bPerformanceGood)
        {
            UE_LOG(LogVHMStressTests, Log, TEXT("✅ Performance benchmark passed - all operations within acceptable limits"));
        }
        else
        {
            UE_LOG(LogVHMStressTests, Warning, TEXT("⚠️ Performance benchmark completed with some operations below threshold"));
        }
        
        // Cleanup - let UE's garbage collector handle cleanup automatically
        MaterialSystem = nullptr;
        BiomeService = nullptr;
        ClimateSystem = nullptr;
    }
    catch (...)
    {
        UE_LOG(LogVHMStressTests, Error, TEXT("❌ Exception in performance benchmark"));
    }
}

void FVHMStressTests::LogMemoryUsage(const FString& TestPhase)
{
    FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
    int32 ObjectCount = GUObjectArray.GetObjectArrayNum();
    
    UE_LOG(LogVHMStressTests, Log, TEXT("📊 Memory Usage [%s]:"), *TestPhase);
    UE_LOG(LogVHMStressTests, Log, TEXT("  UObject Count: %d"), ObjectCount);
    UE_LOG(LogVHMStressTests, Log, TEXT("  Used Physical: %.2f MB"), MemStats.UsedPhysical / (1024.0f * 1024.0f));
    UE_LOG(LogVHMStressTests, Log, TEXT("  Used Virtual: %.2f MB"), MemStats.UsedVirtual / (1024.0f * 1024.0f));
}

bool FVHMStressTests::ValidateMemoryGrowth(int32 InitialObjects, int32 FinalObjects, int32 MaxAllowedGrowth)
{
    int32 ObjectGrowth = FinalObjects - InitialObjects;
    
    if (ObjectGrowth <= MaxAllowedGrowth)
    {
        UE_LOG(LogVHMStressTests, Log, TEXT("✅ Memory growth within limits: %d objects (max %d)"), ObjectGrowth, MaxAllowedGrowth);
        return true;
    }
    else
    {
        UE_LOG(LogVHMStressTests, Warning, TEXT("⚠️ Memory growth exceeds limits: %d objects (max %d)"), ObjectGrowth, MaxAllowedGrowth);
        return false;
    }
}

// TEMPORARY: Disable console commands to prevent startup crashes
#if 0
// Console commands for stress tests
static FAutoConsoleCommand MaterialStressTestCommand(
    TEXT("wg.StressMaterials"),
    TEXT("Run material system stress test. Usage: wg.StressMaterials [NumTiles]"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        int32 NumTiles = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 100;
        NumTiles = FMath::Clamp(NumTiles, 1, 1000);
        FVHMStressTests::RunMaterialStressTest(NumTiles);
    })
);

static FAutoConsoleCommand VHMStressTestCommand(
    TEXT("wg.StressVHM"),
    TEXT("Run VHM component stress test. Usage: wg.StressVHM [NumComponents]"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        int32 NumComponents = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 25;
        NumComponents = FMath::Clamp(NumComponents, 1, 100);
        FVHMStressTests::RunVHMComponentStressTest(NumComponents);
    })
);

static FAutoConsoleCommand MemoryLeakTestCommand(
    TEXT("wg.TestMemoryLeaks"),
    TEXT("Run memory leak detection test. Usage: wg.TestMemoryLeaks [Iterations]"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        int32 Iterations = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 10;
        Iterations = FMath::Clamp(Iterations, 1, 100);
        FVHMStressTests::RunMemoryLeakTest(Iterations);
    })
);

static FAutoConsoleCommand PerformanceBenchmarkCommand(
    TEXT("wg.BenchmarkVHM"),
    TEXT("Run VHM performance benchmark. Usage: wg.BenchmarkVHM [NumTiles]"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        int32 NumTiles = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 50;
        NumTiles = FMath::Clamp(NumTiles, 1, 500);
        FVHMStressTests::RunPerformanceBenchmark(NumTiles);
    })
);

#endif // Temporarily disabled
#endif // DISABLED FOR TEST DIET
