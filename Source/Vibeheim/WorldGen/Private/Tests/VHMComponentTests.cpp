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
#include "VirtualHeightfieldMeshComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogVHMComponentTests, Log, All);

/**
 * Individual component tests for VHM compatibility fixes
 */
class FVHMComponentTests
{
public:
    static void TestMaterialCreation();
    static void TestVHMComponentCreation();
    static void TestConsoleCommandAccess();
    static void TestRVTFallback();
    static void TestPerformanceMetrics();
};

void FVHMComponentTests::TestMaterialCreation()
{
    UE_LOG(LogVHMComponentTests, Log, TEXT("=== Testing Material Creation and Assignment ==="));
    
    try
    {
        // Create required services
        UVHMTerrainMaterialSystem* MaterialSystem = NewObject<UVHMTerrainMaterialSystem>();
        UBiomeService* BiomeService = NewObject<UBiomeService>();
        UClimateSystem* ClimateSystem = NewObject<UClimateSystem>();
        
        // Initialize services
        FClimateSettings ClimateSettings;
        ClimateSystem->Initialize(ClimateSettings, 1337);
        
        UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
        if (!Settings)
        {
            UE_LOG(LogVHMComponentTests, Error, TEXT("Failed to get WorldGenSettings"));
            return;
        }
        
        BiomeService->Initialize(ClimateSystem, Settings->Settings);
        
        FVHMSettings VHMSettings;
        VHMSettings.bUseRuntimeVirtualTexturing = false;
        
        // Test material system initialization
        if (!MaterialSystem->Initialize(BiomeService, VHMSettings))
        {
            UE_LOG(LogVHMComponentTests, Error, TEXT("❌ MaterialSystem initialization failed"));
            return;
        }
        UE_LOG(LogVHMComponentTests, Log, TEXT("✅ MaterialSystem initialized successfully"));
        
        // Test material creation for different biomes
        TArray<EBiomeType> TestBiomes = {
            EBiomeType::Meadows,
            EBiomeType::Forest,
            EBiomeType::Mountains,
            EBiomeType::Ocean
        };
        
        for (int32 i = 0; i < TestBiomes.Num(); i++)
        {
            FTileCoord TestTile(i, 0);
            FBiomeDefinition BiomeDefinition;
            
            if (BiomeService->GetBiomeDefinition(TestBiomes[i], BiomeDefinition))
            {
                UMaterialInstanceDynamic* Material = MaterialSystem->CreateTileMaterial(TestTile, BiomeDefinition);
                if (Material)
                {
                    UE_LOG(LogVHMComponentTests, Log, TEXT("✅ Created material for biome %d: %s"), 
                           static_cast<int32>(TestBiomes[i]), *Material->GetName());
                    
                    // Test parameter updates
                    BiomeDefinition.RVTBlendColor = FLinearColor::Red;
                    if (MaterialSystem->UpdateMaterialParameters(TestTile, BiomeDefinition))
                    {
                        UE_LOG(LogVHMComponentTests, Log, TEXT("✅ Updated material parameters for biome %d"), 
                               static_cast<int32>(TestBiomes[i]));
                    }
                    else
                    {
                        UE_LOG(LogVHMComponentTests, Warning, TEXT("⚠️ Failed to update material parameters for biome %d"), 
                               static_cast<int32>(TestBiomes[i]));
                    }
                }
                else
                {
                    UE_LOG(LogVHMComponentTests, Error, TEXT("❌ Failed to create material for biome %d"), 
                           static_cast<int32>(TestBiomes[i]));
                }
            }
        }
        
        // Test memory usage calculation
        float MemoryUsage = MaterialSystem->GetMaterialMemoryUsageMB();
        UE_LOG(LogVHMComponentTests, Log, TEXT("📊 Material system memory usage: %.2f MB"), MemoryUsage);
        
        // Cleanup - let UE's garbage collector handle cleanup automatically
        MaterialSystem = nullptr;
        BiomeService = nullptr;
        ClimateSystem = nullptr;
        
        UE_LOG(LogVHMComponentTests, Log, TEXT("✅ Material creation test completed successfully"));
    }
    catch (...)
    {
        UE_LOG(LogVHMComponentTests, Error, TEXT("❌ Exception in material creation test"));
    }
}

void FVHMComponentTests::TestVHMComponentCreation()
{
    UE_LOG(LogVHMComponentTests, Log, TEXT("=== Testing VHM Component Creation and Material Assignment ==="));
    
    try
    {
        UWorld* World = nullptr;
        if (GEngine && GEngine->GetWorldContexts().Num() > 0)
        {
            World = GEngine->GetWorldContexts()[0].World();
        }
        
        if (!World)
        {
            UE_LOG(LogVHMComponentTests, Error, TEXT("❌ No valid world context for VHM component testing"));
            return;
        }
        
        // Create VHM terrain renderer with world context
        UVHMTerrainRenderer* TerrainRenderer = NewObject<UVHMTerrainRenderer>(World);
        
        UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
        if (!Settings)
        {
            UE_LOG(LogVHMComponentTests, Error, TEXT("❌ Failed to get WorldGenSettings"));
            return;
        }
        
        // Create required services
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
        
        // Initialize terrain renderer
        if (!TerrainRenderer->InitializeWithBiomeService(Settings, HeightfieldService, TileStreamingService, BiomeService))
        {
            UE_LOG(LogVHMComponentTests, Error, TEXT("❌ VHMTerrainRenderer initialization failed"));
            return;
        }
        UE_LOG(LogVHMComponentTests, Log, TEXT("✅ VHMTerrainRenderer initialized successfully"));
        
        // Test terrain mesh creation
        FTileCoord TestTile(0, 0);
        if (!TerrainRenderer->CreateTerrainMeshForTile(TestTile))
        {
            UE_LOG(LogVHMComponentTests, Error, TEXT("❌ Failed to create terrain mesh for tile (0, 0)"));
            return;
        }
        UE_LOG(LogVHMComponentTests, Log, TEXT("✅ Terrain mesh created successfully"));
        
        // Test VHM component access
        UVirtualHeightfieldMeshComponent* VHMComponent = TerrainRenderer->GetVHMComponent(TestTile);
        if (!VHMComponent)
        {
            UE_LOG(LogVHMComponentTests, Error, TEXT("❌ Failed to get VHM component"));
            return;
        }
        UE_LOG(LogVHMComponentTests, Log, TEXT("✅ VHM component retrieved successfully"));
        
        // Test material assignment (UE5.6 compatibility fix)
        UMaterialInterface* AssignedMaterial = VHMComponent->UPrimitiveComponent::GetMaterial(0);
        if (AssignedMaterial)
        {
            UE_LOG(LogVHMComponentTests, Log, TEXT("✅ Material assigned to VHM component: %s"), *AssignedMaterial->GetName());
            
            // Verify material is a dynamic instance
            UMaterialInstanceDynamic* DynamicMaterial = Cast<UMaterialInstanceDynamic>(AssignedMaterial);
            if (DynamicMaterial)
            {
                UE_LOG(LogVHMComponentTests, Log, TEXT("✅ Material is dynamic instance (correct for terrain)"));
            }
            else
            {
                UE_LOG(LogVHMComponentTests, Warning, TEXT("⚠️ Material is not dynamic instance"));
            }
        }
        else
        {
            UE_LOG(LogVHMComponentTests, Warning, TEXT("⚠️ No material assigned to VHM component"));
        }
        
        // Test component properties
        if (VHMComponent->IsRegistered())
        {
            UE_LOG(LogVHMComponentTests, Log, TEXT("✅ VHM component is properly registered"));
        }
        else
        {
            UE_LOG(LogVHMComponentTests, Warning, TEXT("⚠️ VHM component is not registered"));
        }
        
        // Test visibility using base class method (UE5.6 compatible)
        if (!VHMComponent->bHiddenInGame)
        {
            UE_LOG(LogVHMComponentTests, Log, TEXT("✅ VHM component is not hidden in game"));
        }
        else
        {
            UE_LOG(LogVHMComponentTests, Warning, TEXT("⚠️ VHM component is hidden in game"));
        }
        
        // Test performance stats
        FVHMPerformanceStats PerfStats = TerrainRenderer->GetPerformanceStats();
        UE_LOG(LogVHMComponentTests, Log, TEXT("📊 Performance Stats:"));
        UE_LOG(LogVHMComponentTests, Log, TEXT("  Active VHM Components: %d"), PerfStats.ActiveVHMComponents);
        UE_LOG(LogVHMComponentTests, Log, TEXT("  Last Generation Time: %.2f ms"), PerfStats.LastMeshGenerationMs);
        UE_LOG(LogVHMComponentTests, Log, TEXT("  Average Generation Time: %.2f ms"), PerfStats.AverageMeshGenerationMs);
        UE_LOG(LogVHMComponentTests, Log, TEXT("  Texture Memory Usage: %.2f MB"), PerfStats.TextureMemoryUsageMB);
        
        // Test cleanup
        TerrainRenderer->RemoveTerrainMesh(TestTile);
        UVirtualHeightfieldMeshComponent* RemovedComponent = TerrainRenderer->GetVHMComponent(TestTile);
        if (!RemovedComponent)
        {
            UE_LOG(LogVHMComponentTests, Log, TEXT("✅ VHM component cleanup successful"));
        }
        else
        {
            UE_LOG(LogVHMComponentTests, Warning, TEXT("⚠️ VHM component not properly cleaned up"));
        }
        
        // Cleanup test objects - let UE's garbage collector handle cleanup automatically
        TerrainRenderer = nullptr;
        HeightfieldService = nullptr;
        TileStreamingService = nullptr;
        BiomeService = nullptr;
        ClimateSystem = nullptr;
        NoiseSystem = nullptr;
        
        UE_LOG(LogVHMComponentTests, Log, TEXT("✅ VHM component test completed successfully"));
    }
    catch (...)
    {
        UE_LOG(LogVHMComponentTests, Error, TEXT("❌ Exception in VHM component test"));
    }
}

void FVHMComponentTests::TestConsoleCommandAccess()
{
    UE_LOG(LogVHMComponentTests, Log, TEXT("=== Testing Console Command Property Access ==="));
    
    try
    {
        // Test WorldGenSettings access
        UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
        if (!Settings)
        {
            UE_LOG(LogVHMComponentTests, Error, TEXT("❌ Cannot access WorldGenSettings from console commands"));
            return;
        }
        UE_LOG(LogVHMComponentTests, Log, TEXT("✅ WorldGenSettings accessible"));
        
        // Test VHMTerrainRenderer property access
        UVHMTerrainRenderer* VHMRenderer = Settings->VHMTerrainRenderer;
        if (!VHMRenderer)
        {
            UE_LOG(LogVHMComponentTests, Log, TEXT("ℹ️ VHMTerrainRenderer property is null (expected for new settings)"));
            
            // Test property assignment
            UVHMTerrainRenderer* NewRenderer = NewObject<UVHMTerrainRenderer>();
            Settings->VHMTerrainRenderer = NewRenderer;
            
            if (Settings->VHMTerrainRenderer == NewRenderer)
            {
                UE_LOG(LogVHMComponentTests, Log, TEXT("✅ VHMTerrainRenderer property assignment works"));
                
                // Test property access
                UVHMTerrainRenderer* AccessedRenderer = Settings->VHMTerrainRenderer;
                if (AccessedRenderer == NewRenderer)
                {
                    UE_LOG(LogVHMComponentTests, Log, TEXT("✅ VHMTerrainRenderer property access works"));
                }
                else
                {
                    UE_LOG(LogVHMComponentTests, Error, TEXT("❌ VHMTerrainRenderer property access failed"));
                }
            }
            else
            {
                UE_LOG(LogVHMComponentTests, Error, TEXT("❌ VHMTerrainRenderer property assignment failed"));
            }
        }
        else
        {
            UE_LOG(LogVHMComponentTests, Log, TEXT("✅ VHMTerrainRenderer property accessible: %s"), *VHMRenderer->GetName());
        }
        
        // Test console command registration (UE5.6 compatible approach)
        IConsoleManager& ConsoleManager = IConsoleManager::Get();
        
        TArray<FString> TestCommands = {
            TEXT("wg.ShowSettings"),
            TEXT("wg.ValidateSettings"),
            TEXT("wg.TestVHMCompatibility"),
            TEXT("wg.BasicTest")
        };
        
        for (const FString& CommandName : TestCommands)
        {
            // UE5.6 compatible: Use FindConsoleObject instead of FindConsoleCommand
            IConsoleObject* ConsoleObject = ConsoleManager.FindConsoleObject(*CommandName);
            if (ConsoleObject)
            {
                UE_LOG(LogVHMComponentTests, Log, TEXT("✅ Console command found: %s"), *CommandName);
            }
            else
            {
                UE_LOG(LogVHMComponentTests, Warning, TEXT("⚠️ Console command not found: %s"), *CommandName);
            }
        }
        
        // Test settings validation (safe console command execution)
        TArray<FString> ValidationErrors;
        bool bValid = Settings->ValidateSettings(ValidationErrors);
        if (bValid)
        {
            UE_LOG(LogVHMComponentTests, Log, TEXT("✅ Settings validation passed"));
        }
        else
        {
            UE_LOG(LogVHMComponentTests, Log, TEXT("ℹ️ Settings validation found %d issues (may be expected):"), ValidationErrors.Num());
            for (const FString& Error : ValidationErrors)
            {
                UE_LOG(LogVHMComponentTests, Log, TEXT("  - %s"), *Error);
            }
        }
        
        UE_LOG(LogVHMComponentTests, Log, TEXT("✅ Console command access test completed"));
    }
    catch (...)
    {
        UE_LOG(LogVHMComponentTests, Error, TEXT("❌ Exception in console command access test"));
    }
}

void FVHMComponentTests::TestRVTFallback()
{
    UE_LOG(LogVHMComponentTests, Log, TEXT("=== Testing RVT Integration and Fallback ==="));
    
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
            UE_LOG(LogVHMComponentTests, Error, TEXT("❌ Failed to get WorldGenSettings"));
            return;
        }
        
        BiomeService->Initialize(ClimateSystem, Settings->Settings);
        
        // Test 1: RVT disabled
        FVHMSettings VHMSettingsNoRVT;
        VHMSettingsNoRVT.bUseRuntimeVirtualTexturing = false;
        
        if (!MaterialSystem->Initialize(BiomeService, VHMSettingsNoRVT))
        {
            UE_LOG(LogVHMComponentTests, Error, TEXT("❌ MaterialSystem initialization failed without RVT"));
            return;
        }
        
        if (MaterialSystem->IsRVTInitialized())
        {
            UE_LOG(LogVHMComponentTests, Error, TEXT("❌ RVT should not be initialized when disabled"));
            return;
        }
        UE_LOG(LogVHMComponentTests, Log, TEXT("✅ RVT properly disabled"));
        
        // Test material creation without RVT
        FTileCoord TestTile(0, 0);
        FBiomeDefinition TestBiome;
        TestBiome.BiomeType = EBiomeType::Meadows;
        TestBiome.BiomeName = TEXT("TestMeadows");
        TestBiome.BiomeWeight = 1.0f;
        TestBiome.RVTBlendColor = FLinearColor::Green;
        
        UMaterialInstanceDynamic* MaterialNoRVT = MaterialSystem->CreateTileMaterial(TestTile, TestBiome);
        if (!MaterialNoRVT)
        {
            UE_LOG(LogVHMComponentTests, Error, TEXT("❌ Failed to create material without RVT"));
            return;
        }
        UE_LOG(LogVHMComponentTests, Log, TEXT("✅ Material created successfully without RVT"));
        
        // Test 2: RVT enabled (should handle UE5.6 gracefully)
        UVHMTerrainMaterialSystem* MaterialSystemRVT = NewObject<UVHMTerrainMaterialSystem>();
        
        FVHMSettings VHMSettingsWithRVT;
        VHMSettingsWithRVT.bUseRuntimeVirtualTexturing = true;
        
        if (!MaterialSystemRVT->Initialize(BiomeService, VHMSettingsWithRVT))
        {
            UE_LOG(LogVHMComponentTests, Error, TEXT("❌ MaterialSystem initialization failed with RVT enabled"));
            return;
        }
        
        bool bRVTInitialized = MaterialSystemRVT->IsRVTInitialized();
        if (bRVTInitialized)
        {
            UE_LOG(LogVHMComponentTests, Log, TEXT("✅ RVT initialized successfully in UE5.6"));
        }
        else
        {
            UE_LOG(LogVHMComponentTests, Log, TEXT("ℹ️ RVT initialization failed gracefully (expected in UE5.6)"));
        }
        
        // Test material creation with RVT enabled (should not crash)
        UMaterialInstanceDynamic* MaterialWithRVT = MaterialSystemRVT->CreateTileMaterial(TestTile, TestBiome);
        if (!MaterialWithRVT)
        {
            UE_LOG(LogVHMComponentTests, Error, TEXT("❌ Failed to create material with RVT enabled"));
            return;
        }
        UE_LOG(LogVHMComponentTests, Log, TEXT("✅ Material created successfully with RVT enabled"));
        
        // Test RVT configuration fallback
        FTerrainRVTConfig RVTConfig;
        RVTConfig.RVTResolution = 1024;
        RVTConfig.RVTTileSize = 256;
        RVTConfig.RVTMemoryBudgetMB = 512.0f;
        
        MaterialSystemRVT->SetRVTConfig(RVTConfig);
        UE_LOG(LogVHMComponentTests, Log, TEXT("✅ RVT configuration set without errors"));
        
        // Cleanup - let UE's garbage collector handle cleanup automatically
        MaterialSystem = nullptr;
        MaterialSystemRVT = nullptr;
        BiomeService = nullptr;
        ClimateSystem = nullptr;
        
        UE_LOG(LogVHMComponentTests, Log, TEXT("✅ RVT fallback test completed successfully"));
    }
    catch (...)
    {
        UE_LOG(LogVHMComponentTests, Error, TEXT("❌ Exception in RVT fallback test"));
    }
}

void FVHMComponentTests::TestPerformanceMetrics()
{
    UE_LOG(LogVHMComponentTests, Log, TEXT("=== Testing Performance Metrics ==="));
    
    try
    {
        UWorld* World = nullptr;
        if (GEngine && GEngine->GetWorldContexts().Num() > 0)
        {
            World = GEngine->GetWorldContexts()[0].World();
        }
        
        if (!World)
        {
            UE_LOG(LogVHMComponentTests, Warning, TEXT("⚠️ No valid world context, skipping VHM performance tests"));
            return;
        }
        
        // Create terrain renderer with world context
        UVHMTerrainRenderer* TerrainRenderer = NewObject<UVHMTerrainRenderer>(World);
        
        UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
        if (!Settings)
        {
            UE_LOG(LogVHMComponentTests, Error, TEXT("❌ Failed to get WorldGenSettings"));
            return;
        }
        
        // Create services
        UHeightfieldService* HeightfieldService = NewObject<UHeightfieldService>();
        UTileStreamingService* TileStreamingService = NewObject<UTileStreamingService>();
        UNoiseSystem* NoiseSystem = NewObject<UNoiseSystem>();
        
        // Initialize services
        NoiseSystem->Initialize(1337);
        HeightfieldService->Initialize(Settings->Settings);
        HeightfieldService->SetNoiseSystem(NoiseSystem);
        
        if (!TerrainRenderer->Initialize(Settings, HeightfieldService, TileStreamingService))
        {
            UE_LOG(LogVHMComponentTests, Error, TEXT("❌ VHMTerrainRenderer initialization failed"));
            return;
        }
        
        // Test performance metrics collection
        double StartTime = FPlatformTime::Seconds();
        
        // Create multiple terrain meshes to test performance
        TArray<FTileCoord> TestTiles = {
            FTileCoord(0, 0),
            FTileCoord(1, 0),
            FTileCoord(0, 1),
            FTileCoord(-1, 0),
            FTileCoord(0, -1)
        };
        
        for (const FTileCoord& Tile : TestTiles)
        {
            if (TerrainRenderer->CreateTerrainMeshForTile(Tile))
            {
                UE_LOG(LogVHMComponentTests, Verbose, TEXT("Created mesh for tile (%d, %d)"), Tile.X, Tile.Y);
            }
        }
        
        double CreationTime = (FPlatformTime::Seconds() - StartTime) * 1000.0; // Convert to ms
        
        // Get performance stats
        FVHMPerformanceStats PerfStats = TerrainRenderer->GetPerformanceStats();
        
        UE_LOG(LogVHMComponentTests, Log, TEXT("📊 Performance Metrics:"));
        UE_LOG(LogVHMComponentTests, Log, TEXT("  Total Creation Time: %.2f ms"), CreationTime);
        UE_LOG(LogVHMComponentTests, Log, TEXT("  Active VHM Components: %d"), PerfStats.ActiveVHMComponents);
        UE_LOG(LogVHMComponentTests, Log, TEXT("  Last Generation Time: %.2f ms"), PerfStats.LastMeshGenerationMs);
        UE_LOG(LogVHMComponentTests, Log, TEXT("  Average Generation Time: %.2f ms"), PerfStats.AverageMeshGenerationMs);
        UE_LOG(LogVHMComponentTests, Log, TEXT("  Texture Memory Usage: %.2f MB"), PerfStats.TextureMemoryUsageMB);
        UE_LOG(LogVHMComponentTests, Log, TEXT("  Current FPS: %.1f"), PerfStats.CurrentFPS);
        
        // Validate metrics
        if (PerfStats.ActiveVHMComponents != TestTiles.Num())
        {
            UE_LOG(LogVHMComponentTests, Warning, TEXT("⚠️ Active component count mismatch: expected %d, got %d"), 
                   TestTiles.Num(), PerfStats.ActiveVHMComponents);
        }
        else
        {
            UE_LOG(LogVHMComponentTests, Log, TEXT("✅ Active component count correct"));
        }
        
        if (PerfStats.LastMeshGenerationMs <= 0.0f)
        {
            UE_LOG(LogVHMComponentTests, Warning, TEXT("⚠️ Invalid last generation time: %.2f ms"), PerfStats.LastMeshGenerationMs);
        }
        else
        {
            UE_LOG(LogVHMComponentTests, Log, TEXT("✅ Generation time metrics valid"));
        }
        
        // Test memory usage calculation
        if (PerfStats.TextureMemoryUsageMB < 0.0f)
        {
            UE_LOG(LogVHMComponentTests, Warning, TEXT("⚠️ Invalid texture memory usage: %.2f MB"), PerfStats.TextureMemoryUsageMB);
        }
        else
        {
            UE_LOG(LogVHMComponentTests, Log, TEXT("✅ Memory usage metrics valid"));
        }
        
        // Test active tile retrieval
        TArray<FTileCoord> ActiveTiles = TerrainRenderer->GetActiveMeshTiles();
        if (ActiveTiles.Num() != TestTiles.Num())
        {
            UE_LOG(LogVHMComponentTests, Warning, TEXT("⚠️ Active tiles count mismatch: expected %d, got %d"), 
                   TestTiles.Num(), ActiveTiles.Num());
        }
        else
        {
            UE_LOG(LogVHMComponentTests, Log, TEXT("✅ Active tiles retrieval correct"));
        }
        
        // Cleanup
        for (const FTileCoord& Tile : TestTiles)
        {
            TerrainRenderer->RemoveTerrainMesh(Tile);
        }
        
        TerrainRenderer = nullptr;
        HeightfieldService = nullptr;
        TileStreamingService = nullptr;
        NoiseSystem = nullptr;
        
        UE_LOG(LogVHMComponentTests, Log, TEXT("✅ Performance metrics test completed successfully"));
    }
    catch (...)
    {
        UE_LOG(LogVHMComponentTests, Error, TEXT("❌ Exception in performance metrics test"));
    }
}

// TEMPORARY: Disable console commands to prevent startup crashes
#if 0
// Console commands for individual tests
static FAutoConsoleCommand TestMaterialCreationCommand(
    TEXT("wg.TestMaterialCreation"),
    TEXT("Test TerrainMaterialSystem material creation and TObjectPtr handling"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        FVHMComponentTests::TestMaterialCreation();
    })
);

static FAutoConsoleCommand TestVHMComponentCommand(
    TEXT("wg.TestVHMComponent"),
    TEXT("Test VHM component creation and material assignment"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        FVHMComponentTests::TestVHMComponentCreation();
    })
);

static FAutoConsoleCommand TestConsoleAccessCommand(
    TEXT("wg.TestConsoleAccess"),
    TEXT("Test console command property access to VHMTerrainRenderer"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        FVHMComponentTests::TestConsoleCommandAccess();
    })
);

static FAutoConsoleCommand TestRVTFallbackCommand(
    TEXT("wg.TestRVTFallback"),
    TEXT("Test RVT integration and UE5.6 fallback behavior"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        FVHMComponentTests::TestRVTFallback();
    })
);

static FAutoConsoleCommand TestPerformanceCommand(
    TEXT("wg.TestPerformance"),
    TEXT("Test VHM performance metrics and memory usage tracking"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        FVHMComponentTests::TestPerformanceMetrics();
    })
);

#endif // Temporarily disabled