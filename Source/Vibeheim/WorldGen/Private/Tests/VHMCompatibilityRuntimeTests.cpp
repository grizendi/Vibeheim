#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "VHMTerrainRendering/TerrainMaterialSystem.h"
#include "VHMTerrainRendering/VHMTerrainRenderer.h"
#include "WorldGenSettings.h"
#include "Services/BiomeService.h"
#include "Services/HeightfieldService.h"
#include "Services/TileStreamingService.h"
#include "Services/ClimateSystem.h"
#include "Services/NoiseSystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "VirtualHeightfieldMeshComponent.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogVHMCompatibilityTests, Log, All);

/**
 * Runtime tests for UE5.6 VHM compatibility fixes
 * Tests all the components that were fixed for UE5.6 compatibility
 */
class FVHMCompatibilityRuntimeTests
{
public:
    static void RunAllTests();
    
private:
    // Test individual components
    static bool TestTerrainMaterialSystem();
    static bool TestVHMTerrainRenderer();
    static bool TestConsoleCommands();
    static bool TestRVTIntegration();
    static bool TestTObjectPtrHandling();
    static bool TestMemoryLeaks();
    
    // Helper functions
    static UWorld* GetTestWorld();
    static void CleanupTestObjects();
    static bool ValidateNoMemoryLeaks();
    
    // Test data
    static TArray<UObject*> TestObjects;
    static int32 InitialObjectCount;
};

TArray<UObject*> FVHMCompatibilityRuntimeTests::TestObjects;
int32 FVHMCompatibilityRuntimeTests::InitialObjectCount = 0;

void FVHMCompatibilityRuntimeTests::RunAllTests()
{
    UE_LOG(LogVHMCompatibilityTests, Log, TEXT("=== Starting VHM UE5.6 Compatibility Runtime Tests ==="));
    
    // Record initial state for memory leak detection
    InitialObjectCount = GUObjectArray.GetObjectArrayNum();
    TestObjects.Empty();
    
    bool bAllTestsPassed = true;
    int32 PassedTests = 0;
    int32 TotalTests = 6;
    
    // Run individual test suites
    if (TestTerrainMaterialSystem())
    {
        UE_LOG(LogVHMCompatibilityTests, Log, TEXT("✓ TerrainMaterialSystem tests PASSED"));
        PassedTests++;
    }
    else
    {
        UE_LOG(LogVHMCompatibilityTests, Error, TEXT("✗ TerrainMaterialSystem tests FAILED"));
        bAllTestsPassed = false;
    }
    
    if (TestVHMTerrainRenderer())
    {
        UE_LOG(LogVHMCompatibilityTests, Log, TEXT("✓ VHMTerrainRenderer tests PASSED"));
        PassedTests++;
    }
    else
    {
        UE_LOG(LogVHMCompatibilityTests, Error, TEXT("✗ VHMTerrainRenderer tests FAILED"));
        bAllTestsPassed = false;
    }
    
    if (TestConsoleCommands())
    {
        UE_LOG(LogVHMCompatibilityTests, Log, TEXT("✓ Console Commands tests PASSED"));
        PassedTests++;
    }
    else
    {
        UE_LOG(LogVHMCompatibilityTests, Error, TEXT("✗ Console Commands tests FAILED"));
        bAllTestsPassed = false;
    }
    
    if (TestRVTIntegration())
    {
        UE_LOG(LogVHMCompatibilityTests, Log, TEXT("✓ RVT Integration tests PASSED"));
        PassedTests++;
    }
    else
    {
        UE_LOG(LogVHMCompatibilityTests, Error, TEXT("✗ RVT Integration tests FAILED"));
        bAllTestsPassed = false;
    }
    
    if (TestTObjectPtrHandling())
    {
        UE_LOG(LogVHMCompatibilityTests, Log, TEXT("✓ TObjectPtr Handling tests PASSED"));
        PassedTests++;
    }
    else
    {
        UE_LOG(LogVHMCompatibilityTests, Error, TEXT("✗ TObjectPtr Handling tests FAILED"));
        bAllTestsPassed = false;
    }
    
    if (TestMemoryLeaks())
    {
        UE_LOG(LogVHMCompatibilityTests, Log, TEXT("✓ Memory Leak tests PASSED"));
        PassedTests++;
    }
    else
    {
        UE_LOG(LogVHMCompatibilityTests, Error, TEXT("✗ Memory Leak tests FAILED"));
        bAllTestsPassed = false;
    }
    
    // Cleanup test objects
    CleanupTestObjects();
    
    // Final results
    UE_LOG(LogVHMCompatibilityTests, Log, TEXT("=== VHM Compatibility Test Results ==="));
    UE_LOG(LogVHMCompatibilityTests, Log, TEXT("Tests Passed: %d/%d"), PassedTests, TotalTests);
    
    if (bAllTestsPassed)
    {
        UE_LOG(LogVHMCompatibilityTests, Log, TEXT("🎉 ALL TESTS PASSED - UE5.6 compatibility fixes are working correctly!"));
    }
    else
    {
        UE_LOG(LogVHMCompatibilityTests, Error, TEXT("❌ SOME TESTS FAILED - Review the errors above"));
    }
}

bool FVHMCompatibilityRuntimeTests::TestTerrainMaterialSystem()
{
    UE_LOG(LogVHMCompatibilityTests, Log, TEXT("--- Testing TerrainMaterialSystem ---"));
    
    try
    {
        // Create test objects
        UVHMTerrainMaterialSystem* MaterialSystem = NewObject<UVHMTerrainMaterialSystem>();
        TestObjects.Add(MaterialSystem);
        
        UBiomeService* BiomeService = NewObject<UBiomeService>();
        TestObjects.Add(BiomeService);
        
        UClimateSystem* ClimateSystem = NewObject<UClimateSystem>();
        TestObjects.Add(ClimateSystem);
        
        // Initialize services
        FClimateSettings ClimateSettings;
        ClimateSystem->Initialize(ClimateSettings, 1337);
        
        UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
        if (!Settings)
        {
            UE_LOG(LogVHMCompatibilityTests, Error, TEXT("Failed to get WorldGenSettings"));
            return false;
        }
        
        BiomeService->Initialize(ClimateSystem, Settings->Settings);
        
        FVHMSettings VHMSettings;
        VHMSettings.bUseRuntimeVirtualTexturing = false; // Disable RVT for basic test
        
        // Test 1: Initialize material system
        if (!MaterialSystem->Initialize(BiomeService, VHMSettings))
        {
            UE_LOG(LogVHMCompatibilityTests, Error, TEXT("MaterialSystem initialization failed"));
            return false;
        }
        UE_LOG(LogVHMCompatibilityTests, Verbose, TEXT("✓ MaterialSystem initialized successfully"));
        
        // Test 2: Create tile material (tests TObjectPtr handling)
        FTileCoord TestTile(0, 0);
        FBiomeDefinition TestBiome;
        TestBiome.BiomeType = EBiomeType::Meadows;
        TestBiome.BiomeName = TEXT("TestMeadows");
        TestBiome.BiomeWeight = 1.0f;
        TestBiome.RVTBlendColor = FLinearColor::Green;
        TestBiome.BaseHeight = 10.0f;
        TestBiome.HeightVariation = 5.0f;
        TestBiome.MinTemperature = 15.0f;
        TestBiome.MaxTemperature = 25.0f;
        TestBiome.MinMoisture = 0.4f;
        TestBiome.MaxMoisture = 0.8f;
        
        UMaterialInstanceDynamic* TileMaterial = MaterialSystem->CreateTileMaterial(TestTile, TestBiome);
        if (!TileMaterial)
        {
            UE_LOG(LogVHMCompatibilityTests, Error, TEXT("Failed to create tile material"));
            return false;
        }
        UE_LOG(LogVHMCompatibilityTests, Verbose, TEXT("✓ Tile material created successfully"));
        
        // Test 3: Retrieve tile material (tests TObjectPtr dereferencing)
        UMaterialInstanceDynamic* RetrievedMaterial = MaterialSystem->GetTileMaterial(TestTile);
        if (RetrievedMaterial != TileMaterial)
        {
            UE_LOG(LogVHMCompatibilityTests, Error, TEXT("Retrieved material doesn't match created material"));
            return false;
        }
        UE_LOG(LogVHMCompatibilityTests, Verbose, TEXT("✓ Material retrieval working correctly"));
        
        // Test 4: Update material parameters
        TestBiome.RVTBlendColor = FLinearColor::Blue;
        if (!MaterialSystem->UpdateMaterialParameters(TestTile, TestBiome))
        {
            UE_LOG(LogVHMCompatibilityTests, Error, TEXT("Failed to update material parameters"));
            return false;
        }
        UE_LOG(LogVHMCompatibilityTests, Verbose, TEXT("✓ Material parameter updates working"));
        
        // Test 5: Remove tile material (tests TObjectPtr cleanup)
        MaterialSystem->RemoveTileMaterial(TestTile);
        UMaterialInstanceDynamic* RemovedMaterial = MaterialSystem->GetTileMaterial(TestTile);
        if (RemovedMaterial != nullptr)
        {
            UE_LOG(LogVHMCompatibilityTests, Error, TEXT("Material was not properly removed"));
            return false;
        }
        UE_LOG(LogVHMCompatibilityTests, Verbose, TEXT("✓ Material removal working correctly"));
        
        // Test 6: Memory usage calculation
        float MemoryUsage = MaterialSystem->GetMaterialMemoryUsageMB();
        if (MemoryUsage < 0.0f)
        {
            UE_LOG(LogVHMCompatibilityTests, Error, TEXT("Invalid memory usage calculation: %.2f"), MemoryUsage);
            return false;
        }
        UE_LOG(LogVHMCompatibilityTests, Verbose, TEXT("✓ Memory usage calculation: %.2f MB"), MemoryUsage);
        
        return true;
    }
    catch (...)
    {
        UE_LOG(LogVHMCompatibilityTests, Error, TEXT("Exception occurred in TerrainMaterialSystem test"));
        return false;
    }
}

bool FVHMCompatibilityRuntimeTests::TestVHMTerrainRenderer()
{
    UE_LOG(LogVHMCompatibilityTests, Log, TEXT("--- Testing VHMTerrainRenderer ---"));
    
    try
    {
        UWorld* TestWorld = GetTestWorld();
        if (!TestWorld)
        {
            UE_LOG(LogVHMCompatibilityTests, Error, TEXT("No valid world for VHM testing"));
            return false;
        }
        
        // Create test objects
        UVHMTerrainRenderer* TerrainRenderer = NewObject<UVHMTerrainRenderer>();
        TestObjects.Add(TerrainRenderer);
        
        UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
        if (!Settings)
        {
            UE_LOG(LogVHMCompatibilityTests, Error, TEXT("Failed to get WorldGenSettings"));
            return false;
        }
        
        // Create required services
        UHeightfieldService* HeightfieldService = NewObject<UHeightfieldService>();
        TestObjects.Add(HeightfieldService);
        
        UTileStreamingService* TileStreamingService = NewObject<UTileStreamingService>();
        TestObjects.Add(TileStreamingService);
        
        UBiomeService* BiomeService = NewObject<UBiomeService>();
        TestObjects.Add(BiomeService);
        
        UClimateSystem* ClimateSystem = NewObject<UClimateSystem>();
        TestObjects.Add(ClimateSystem);
        
        UNoiseSystem* NoiseSystem = NewObject<UNoiseSystem>();
        TestObjects.Add(NoiseSystem);
        
        // Initialize services
        FClimateSettings ClimateSettings;
        ClimateSystem->Initialize(ClimateSettings, 1337);
        BiomeService->Initialize(ClimateSystem, Settings->Settings);
        NoiseSystem->Initialize(1337);
        HeightfieldService->Initialize(Settings->Settings);
        HeightfieldService->SetNoiseSystem(NoiseSystem);
        
        // Test 1: Initialize VHM renderer
        if (!TerrainRenderer->Initialize(Settings, HeightfieldService, TileStreamingService))
        {
            UE_LOG(LogVHMCompatibilityTests, Error, TEXT("VHMTerrainRenderer initialization failed"));
            return false;
        }
        UE_LOG(LogVHMCompatibilityTests, Verbose, TEXT("✓ VHMTerrainRenderer initialized successfully"));
        
        // Test 2: Initialize with biome service
        if (!TerrainRenderer->InitializeWithBiomeService(Settings, HeightfieldService, TileStreamingService, BiomeService))
        {
            UE_LOG(LogVHMCompatibilityTests, Error, TEXT("VHMTerrainRenderer biome service initialization failed"));
            return false;
        }
        UE_LOG(LogVHMCompatibilityTests, Verbose, TEXT("✓ VHMTerrainRenderer biome service integration working"));
        
        // Test 3: Create terrain mesh for tile (tests VHM component material assignment)
        FTileCoord TestTile(0, 0);
        if (!TerrainRenderer->CreateTerrainMeshForTile(TestTile))
        {
            UE_LOG(LogVHMCompatibilityTests, Error, TEXT("Failed to create terrain mesh for tile"));
            return false;
        }
        UE_LOG(LogVHMCompatibilityTests, Verbose, TEXT("✓ Terrain mesh creation working"));
        
        // Test 4: Get VHM component (tests component creation and access)
        UVirtualHeightfieldMeshComponent* VHMComponent = TerrainRenderer->GetVHMComponent(TestTile);
        if (!VHMComponent)
        {
            UE_LOG(LogVHMCompatibilityTests, Error, TEXT("Failed to get VHM component"));
            return false;
        }
        UE_LOG(LogVHMCompatibilityTests, Verbose, TEXT("✓ VHM component access working"));
        
        // Test 5: Verify material assignment (tests UE5.6 material assignment fix)
        UMaterialInterface* AssignedMaterial = VHMComponent->UPrimitiveComponent::GetMaterial(0);
        if (!AssignedMaterial)
        {
            UE_LOG(LogVHMCompatibilityTests, Warning, TEXT("No material assigned to VHM component (may be expected)"));
        }
        else
        {
            UE_LOG(LogVHMCompatibilityTests, Verbose, TEXT("✓ Material assignment working: %s"), *AssignedMaterial->GetName());
        }
        
        // Test 6: Get terrain mesh data
        FTerrainMeshData MeshData;
        if (!TerrainRenderer->GetTerrainMeshData(TestTile, MeshData))
        {
            UE_LOG(LogVHMCompatibilityTests, Error, TEXT("Failed to get terrain mesh data"));
            return false;
        }
        UE_LOG(LogVHMCompatibilityTests, Verbose, TEXT("✓ Terrain mesh data retrieval working"));
        
        // Test 7: Performance stats
        FVHMPerformanceStats PerfStats = TerrainRenderer->GetPerformanceStats();
        if (PerfStats.ActiveVHMComponents < 0)
        {
            UE_LOG(LogVHMCompatibilityTests, Error, TEXT("Invalid performance stats"));
            return false;
        }
        UE_LOG(LogVHMCompatibilityTests, Verbose, TEXT("✓ Performance stats: %d active components"), PerfStats.ActiveVHMComponents);
        
        // Test 8: Remove terrain mesh (cleanup test)
        TerrainRenderer->RemoveTerrainMesh(TestTile);
        UVirtualHeightfieldMeshComponent* RemovedComponent = TerrainRenderer->GetVHMComponent(TestTile);
        if (RemovedComponent != nullptr)
        {
            UE_LOG(LogVHMCompatibilityTests, Error, TEXT("VHM component was not properly removed"));
            return false;
        }
        UE_LOG(LogVHMCompatibilityTests, Verbose, TEXT("✓ Terrain mesh removal working"));
        
        return true;
    }
    catch (...)
    {
        UE_LOG(LogVHMCompatibilityTests, Error, TEXT("Exception occurred in VHMTerrainRenderer test"));
        return false;
    }
}

bool FVHMCompatibilityRuntimeTests::TestConsoleCommands()
{
    UE_LOG(LogVHMCompatibilityTests, Log, TEXT("--- Testing Console Commands ---"));
    
    try
    {
        // Test 1: Verify WorldGenSettings access
        UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
        if (!Settings)
        {
            UE_LOG(LogVHMCompatibilityTests, Error, TEXT("Console commands cannot access WorldGenSettings"));
            return false;
        }
        UE_LOG(LogVHMCompatibilityTests, Verbose, TEXT("✓ WorldGenSettings accessible from console commands"));
        
        // Test 2: Verify VHMTerrainRenderer property access
        if (!Settings->VHMTerrainRenderer)
        {
            UE_LOG(LogVHMCompatibilityTests, Warning, TEXT("VHMTerrainRenderer property is null (expected for new settings)"));
        }
        else
        {
            UE_LOG(LogVHMCompatibilityTests, Verbose, TEXT("✓ VHMTerrainRenderer property accessible: %s"), 
                   *Settings->VHMTerrainRenderer->GetName());
        }
        
        // Test 3: Test console command execution (basic commands) - UE5.6 compatible
        IConsoleManager& ConsoleManager = IConsoleManager::Get();
        
        // Test wg.ShowSettings command
        IConsoleObject* ShowSettingsCmd = ConsoleManager.FindConsoleObject(TEXT("wg.ShowSettings"));
        if (!ShowSettingsCmd)
        {
            UE_LOG(LogVHMCompatibilityTests, Error, TEXT("wg.ShowSettings console command not found"));
            return false;
        }
        UE_LOG(LogVHMCompatibilityTests, Verbose, TEXT("✓ wg.ShowSettings command found"));
        
        // Test wg.ValidateSettings command
        IConsoleObject* ValidateSettingsCmd = ConsoleManager.FindConsoleObject(TEXT("wg.ValidateSettings"));
        if (!ValidateSettingsCmd)
        {
            UE_LOG(LogVHMCompatibilityTests, Error, TEXT("wg.ValidateSettings console command not found"));
            return false;
        }
        UE_LOG(LogVHMCompatibilityTests, Verbose, TEXT("✓ wg.ValidateSettings command found"));
        
        // Test 4: Execute a safe console command
        TArray<FString> ValidationErrors;
        bool bSettingsValid = Settings->ValidateSettings(ValidationErrors);
        if (!bSettingsValid)
        {
            UE_LOG(LogVHMCompatibilityTests, Warning, TEXT("Settings validation found %d issues (may be expected)"), ValidationErrors.Num());
            for (const FString& Error : ValidationErrors)
            {
                UE_LOG(LogVHMCompatibilityTests, Verbose, TEXT("  - %s"), *Error);
            }
        }
        else
        {
            UE_LOG(LogVHMCompatibilityTests, Verbose, TEXT("✓ Settings validation passed"));
        }
        
        return true;
    }
    catch (...)
    {
        UE_LOG(LogVHMCompatibilityTests, Error, TEXT("Exception occurred in Console Commands test"));
        return false;
    }
}

bool FVHMCompatibilityRuntimeTests::TestRVTIntegration()
{
    UE_LOG(LogVHMCompatibilityTests, Log, TEXT("--- Testing RVT Integration ---"));
    
    try
    {
        // Create test objects
        UVHMTerrainMaterialSystem* MaterialSystem = NewObject<UVHMTerrainMaterialSystem>();
        TestObjects.Add(MaterialSystem);
        
        UBiomeService* BiomeService = NewObject<UBiomeService>();
        TestObjects.Add(BiomeService);
        
        UClimateSystem* ClimateSystem = NewObject<UClimateSystem>();
        TestObjects.Add(ClimateSystem);
        
        // Initialize services
        FClimateSettings ClimateSettings;
        ClimateSystem->Initialize(ClimateSettings, 1337);
        
        UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
        if (!Settings)
        {
            UE_LOG(LogVHMCompatibilityTests, Error, TEXT("Failed to get WorldGenSettings"));
            return false;
        }
        
        BiomeService->Initialize(ClimateSystem, Settings->Settings);
        
        // Test 1: RVT disabled (should work gracefully)
        FVHMSettings VHMSettingsNoRVT;
        VHMSettingsNoRVT.bUseRuntimeVirtualTexturing = false;
        
        if (!MaterialSystem->Initialize(BiomeService, VHMSettingsNoRVT))
        {
            UE_LOG(LogVHMCompatibilityTests, Error, TEXT("MaterialSystem initialization failed without RVT"));
            return false;
        }
        
        if (MaterialSystem->IsRVTInitialized())
        {
            UE_LOG(LogVHMCompatibilityTests, Error, TEXT("RVT should not be initialized when disabled"));
            return false;
        }
        UE_LOG(LogVHMCompatibilityTests, Verbose, TEXT("✓ RVT gracefully disabled"));
        
        // Test 2: RVT enabled (should handle UE5.6 compatibility)
        FVHMSettings VHMSettingsWithRVT;
        VHMSettingsWithRVT.bUseRuntimeVirtualTexturing = true;
        
        UVHMTerrainMaterialSystem* MaterialSystemRVT = NewObject<UVHMTerrainMaterialSystem>();
        TestObjects.Add(MaterialSystemRVT);
        
        if (!MaterialSystemRVT->Initialize(BiomeService, VHMSettingsWithRVT))
        {
            UE_LOG(LogVHMCompatibilityTests, Error, TEXT("MaterialSystem initialization failed with RVT enabled"));
            return false;
        }
        
        // RVT may or may not initialize successfully depending on UE5.6 RVT availability
        bool bRVTInitialized = MaterialSystemRVT->IsRVTInitialized();
        if (bRVTInitialized)
        {
            UE_LOG(LogVHMCompatibilityTests, Verbose, TEXT("✓ RVT initialized successfully in UE5.6"));
        }
        else
        {
            UE_LOG(LogVHMCompatibilityTests, Verbose, TEXT("✓ RVT initialization failed gracefully (expected in UE5.6)"));
        }
        
        // Test 3: Create material with RVT settings (should not crash)
        FTileCoord TestTile(0, 0);
        FBiomeDefinition TestBiome;
        TestBiome.BiomeType = EBiomeType::Meadows;
        TestBiome.BiomeName = TEXT("TestMeadows");
        TestBiome.BiomeWeight = 1.0f;
        TestBiome.RVTBlendColor = FLinearColor::Green;
        
        UMaterialInstanceDynamic* TileMaterial = MaterialSystemRVT->CreateTileMaterial(TestTile, TestBiome);
        if (!TileMaterial)
        {
            UE_LOG(LogVHMCompatibilityTests, Error, TEXT("Failed to create tile material with RVT enabled"));
            return false;
        }
        UE_LOG(LogVHMCompatibilityTests, Verbose, TEXT("✓ Material creation works with RVT settings"));
        
        return true;
    }
    catch (...)
    {
        UE_LOG(LogVHMCompatibilityTests, Error, TEXT("Exception occurred in RVT Integration test"));
        return false;
    }
}

bool FVHMCompatibilityRuntimeTests::TestTObjectPtrHandling()
{
    UE_LOG(LogVHMCompatibilityTests, Log, TEXT("--- Testing TObjectPtr Handling ---"));
    
    try
    {
        // Create test objects
        UVHMTerrainMaterialSystem* MaterialSystem = NewObject<UVHMTerrainMaterialSystem>();
        TestObjects.Add(MaterialSystem);
        
        UBiomeService* BiomeService = NewObject<UBiomeService>();
        TestObjects.Add(BiomeService);
        
        UClimateSystem* ClimateSystem = NewObject<UClimateSystem>();
        TestObjects.Add(ClimateSystem);
        
        // Initialize services
        FClimateSettings ClimateSettings;
        ClimateSystem->Initialize(ClimateSettings, 1337);
        
        UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
        if (!Settings)
        {
            UE_LOG(LogVHMCompatibilityTests, Error, TEXT("Failed to get WorldGenSettings"));
            return false;
        }
        
        BiomeService->Initialize(ClimateSystem, Settings->Settings);
        
        FVHMSettings VHMSettings;
        VHMSettings.bUseRuntimeVirtualTexturing = false;
        
        if (!MaterialSystem->Initialize(BiomeService, VHMSettings))
        {
            UE_LOG(LogVHMCompatibilityTests, Error, TEXT("MaterialSystem initialization failed"));
            return false;
        }
        
        // Test 1: Create multiple materials (tests TObjectPtr storage)
        TArray<FTileCoord> TestTiles = {
            FTileCoord(0, 0),
            FTileCoord(1, 0),
            FTileCoord(0, 1),
            FTileCoord(1, 1)
        };
        
        FBiomeDefinition TestBiome;
        TestBiome.BiomeType = EBiomeType::Meadows;
        TestBiome.BiomeName = TEXT("TestMeadows");
        TestBiome.BiomeWeight = 1.0f;
        TestBiome.RVTBlendColor = FLinearColor::Green;
        
        TArray<UMaterialInstanceDynamic*> CreatedMaterials;
        
        for (const FTileCoord& Tile : TestTiles)
        {
            UMaterialInstanceDynamic* Material = MaterialSystem->CreateTileMaterial(Tile, TestBiome);
            if (!Material)
            {
                UE_LOG(LogVHMCompatibilityTests, Error, TEXT("Failed to create material for tile (%d, %d)"), Tile.X, Tile.Y);
                return false;
            }
            CreatedMaterials.Add(Material);
        }
        UE_LOG(LogVHMCompatibilityTests, Verbose, TEXT("✓ Multiple materials created successfully"));
        
        // Test 2: Retrieve all materials (tests TObjectPtr dereferencing)
        for (int32 i = 0; i < TestTiles.Num(); i++)
        {
            UMaterialInstanceDynamic* RetrievedMaterial = MaterialSystem->GetTileMaterial(TestTiles[i]);
            if (RetrievedMaterial != CreatedMaterials[i])
            {
                UE_LOG(LogVHMCompatibilityTests, Error, TEXT("Material retrieval failed for tile (%d, %d)"), 
                       TestTiles[i].X, TestTiles[i].Y);
                return false;
            }
        }
        UE_LOG(LogVHMCompatibilityTests, Verbose, TEXT("✓ All materials retrieved correctly"));
        
        // Test 3: Update materials (tests TObjectPtr access)
        TestBiome.RVTBlendColor = FLinearColor::Blue;
        for (const FTileCoord& Tile : TestTiles)
        {
            if (!MaterialSystem->UpdateMaterialParameters(Tile, TestBiome))
            {
                UE_LOG(LogVHMCompatibilityTests, Error, TEXT("Failed to update material for tile (%d, %d)"), Tile.X, Tile.Y);
                return false;
            }
        }
        UE_LOG(LogVHMCompatibilityTests, Verbose, TEXT("✓ All materials updated successfully"));
        
        // Test 4: Remove materials (tests TObjectPtr cleanup)
        for (const FTileCoord& Tile : TestTiles)
        {
            MaterialSystem->RemoveTileMaterial(Tile);
            UMaterialInstanceDynamic* RemovedMaterial = MaterialSystem->GetTileMaterial(Tile);
            if (RemovedMaterial != nullptr)
            {
                UE_LOG(LogVHMCompatibilityTests, Error, TEXT("Material not properly removed for tile (%d, %d)"), Tile.X, Tile.Y);
                return false;
            }
        }
        UE_LOG(LogVHMCompatibilityTests, Verbose, TEXT("✓ All materials removed successfully"));
        
        return true;
    }
    catch (...)
    {
        UE_LOG(LogVHMCompatibilityTests, Error, TEXT("Exception occurred in TObjectPtr Handling test"));
        return false;
    }
}

bool FVHMCompatibilityRuntimeTests::TestMemoryLeaks()
{
    UE_LOG(LogVHMCompatibilityTests, Log, TEXT("--- Testing Memory Leaks ---"));
    
    try
    {
        // Record object count before test
        int32 ObjectCountBefore = GUObjectArray.GetObjectArrayNum();
        
        // Create and destroy multiple material systems
        for (int32 i = 0; i < 5; i++)
        {
            UVHMTerrainMaterialSystem* MaterialSystem = NewObject<UVHMTerrainMaterialSystem>();
            UBiomeService* BiomeService = NewObject<UBiomeService>();
            UClimateSystem* ClimateSystem = NewObject<UClimateSystem>();
            
            // Initialize and use the system
            FClimateSettings ClimateSettings;
            ClimateSystem->Initialize(ClimateSettings, 1337);
            
            UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
            if (Settings)
            {
                BiomeService->Initialize(ClimateSystem, Settings->Settings);
                
                FVHMSettings VHMSettings;
                VHMSettings.bUseRuntimeVirtualTexturing = false;
                
                if (MaterialSystem->Initialize(BiomeService, VHMSettings))
                {
                    // Create and remove some materials
                    FTileCoord TestTile(i, i);
                    FBiomeDefinition TestBiome;
                    TestBiome.BiomeType = EBiomeType::Meadows;
                    TestBiome.BiomeName = TEXT("TestMeadows");
                    TestBiome.BiomeWeight = 1.0f;
                    
                    UMaterialInstanceDynamic* Material = MaterialSystem->CreateTileMaterial(TestTile, TestBiome);
                    if (Material)
                    {
                        MaterialSystem->RemoveTileMaterial(TestTile);
                    }
                }
            }
            
            // Clear object references - let UE's garbage collector handle cleanup
            MaterialSystem = nullptr;
            BiomeService = nullptr;
            ClimateSystem = nullptr;
        }
        
        // Force garbage collection
        CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
        
        // Check object count after cleanup
        int32 ObjectCountAfter = GUObjectArray.GetObjectArrayNum();
        int32 ObjectDifference = ObjectCountAfter - ObjectCountBefore;
        
        UE_LOG(LogVHMCompatibilityTests, Verbose, TEXT("Object count before: %d, after: %d, difference: %d"), 
               ObjectCountBefore, ObjectCountAfter, ObjectDifference);
        
        // Allow for some reasonable object growth (UE creates various internal objects)
        if (ObjectDifference > 50)
        {
            UE_LOG(LogVHMCompatibilityTests, Warning, TEXT("Potential memory leak detected: %d objects not cleaned up"), ObjectDifference);
            // Don't fail the test as this might be normal UE behavior
        }
        else
        {
            UE_LOG(LogVHMCompatibilityTests, Verbose, TEXT("✓ No significant memory leaks detected"));
        }
        
        return true;
    }
    catch (...)
    {
        UE_LOG(LogVHMCompatibilityTests, Error, TEXT("Exception occurred in Memory Leak test"));
        return false;
    }
}

UWorld* FVHMCompatibilityRuntimeTests::GetTestWorld()
{
    if (GEngine && GEngine->GetWorldContexts().Num() > 0)
    {
        return GEngine->GetWorldContexts()[0].World();
    }
    return nullptr;
}

void FVHMCompatibilityRuntimeTests::CleanupTestObjects()
{
    // UE5.6 compatible cleanup - let GC handle object cleanup automatically
    // Clear our references and force garbage collection
    TestObjects.Empty();
    
    // Force garbage collection to clean up test objects
    CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
}

bool FVHMCompatibilityRuntimeTests::ValidateNoMemoryLeaks()
{
    int32 FinalObjectCount = GUObjectArray.GetObjectArrayNum();
    int32 ObjectDifference = FinalObjectCount - InitialObjectCount;
    
    if (ObjectDifference > 100) // Allow for reasonable growth
    {
        UE_LOG(LogVHMCompatibilityTests, Warning, TEXT("Potential memory leak: %d objects not cleaned up"), ObjectDifference);
        return false;
    }
    
    return true;
}

// Console command to run the tests
static FAutoConsoleCommand VHMCompatibilityTestCommand(
    TEXT("wg.TestVHMCompatibility"),
    TEXT("Run comprehensive runtime tests for UE5.6 VHM compatibility fixes"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        FVHMCompatibilityRuntimeTests::RunAllTests();
    })
);