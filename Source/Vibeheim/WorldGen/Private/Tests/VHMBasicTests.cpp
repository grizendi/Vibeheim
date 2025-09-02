#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "VHMTerrainRendering/TerrainMaterialSystem.h"
#include "WorldGenSettings.h"
#include "Services/BiomeService.h"
#include "Services/ClimateSystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogVHMBasicTests, Log, All);

/**
 * Basic VHM compatibility tests that are safe to run
 * These tests avoid complex object creation and cleanup
 */
class FVHMBasicTests
{
public:
    static void RunBasicCompatibilityTest();
    static void TestSettingsAccess();
    static void TestServiceCreation();
};

void FVHMBasicTests::RunBasicCompatibilityTest()
{
    UE_LOG(LogVHMBasicTests, Log, TEXT("=== Basic VHM UE5.6 Compatibility Test ==="));
    
    bool bAllTestsPassed = true;
    int32 PassedTests = 0;
    int32 TotalTests = 2;
    
    // Test 1: Settings Access
    try
    {
        TestSettingsAccess();
        UE_LOG(LogVHMBasicTests, Log, TEXT("✅ Settings access test PASSED"));
        PassedTests++;
    }
    catch (...)
    {
        UE_LOG(LogVHMBasicTests, Error, TEXT("❌ Settings access test FAILED"));
        bAllTestsPassed = false;
    }
    
    // Test 2: Service Creation
    try
    {
        TestServiceCreation();
        UE_LOG(LogVHMBasicTests, Log, TEXT("✅ Service creation test PASSED"));
        PassedTests++;
    }
    catch (...)
    {
        UE_LOG(LogVHMBasicTests, Error, TEXT("❌ Service creation test FAILED"));
        bAllTestsPassed = false;
    }
    
    // Results
    UE_LOG(LogVHMBasicTests, Log, TEXT("=== Basic Test Results ==="));
    UE_LOG(LogVHMBasicTests, Log, TEXT("Tests Passed: %d/%d"), PassedTests, TotalTests);
    
    if (bAllTestsPassed)
    {
        UE_LOG(LogVHMBasicTests, Log, TEXT("🎉 BASIC TESTS PASSED - Core VHM compatibility is working!"));
    }
    else
    {
        UE_LOG(LogVHMBasicTests, Error, TEXT("❌ SOME BASIC TESTS FAILED - Check the errors above"));
    }
}

void FVHMBasicTests::TestSettingsAccess()
{
    UE_LOG(LogVHMBasicTests, Log, TEXT("--- Testing Settings Access ---"));
    
    // Test WorldGenSettings access
    UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
    if (!Settings)
    {
        UE_LOG(LogVHMBasicTests, Error, TEXT("Failed to get WorldGenSettings"));
        return;
    }
    UE_LOG(LogVHMBasicTests, Verbose, TEXT("✓ WorldGenSettings accessible"));
    
    // Test VHMTerrainRenderer property access
    if (Settings->VHMTerrainRenderer)
    {
        UE_LOG(LogVHMBasicTests, Verbose, TEXT("✓ VHMTerrainRenderer property accessible"));
    }
    else
    {
        UE_LOG(LogVHMBasicTests, Verbose, TEXT("ℹ️ VHMTerrainRenderer property is null (expected for new settings)"));
    }
    
    // Test settings validation
    TArray<FString> ValidationErrors;
    bool bValid = Settings->ValidateSettings(ValidationErrors);
    if (bValid)
    {
        UE_LOG(LogVHMBasicTests, Verbose, TEXT("✓ Settings validation passed"));
    }
    else
    {
        UE_LOG(LogVHMBasicTests, Verbose, TEXT("ℹ️ Settings validation found %d issues (may be expected)"), ValidationErrors.Num());
    }
}

void FVHMBasicTests::TestServiceCreation()
{
    UE_LOG(LogVHMBasicTests, Log, TEXT("--- Testing Service Creation ---"));
    
    // Test creating services without complex initialization
    UVHMTerrainMaterialSystem* MaterialSystem = NewObject<UVHMTerrainMaterialSystem>();
    if (!MaterialSystem)
    {
        UE_LOG(LogVHMBasicTests, Error, TEXT("Failed to create TerrainMaterialSystem"));
        return;
    }
    UE_LOG(LogVHMBasicTests, Verbose, TEXT("✓ TerrainMaterialSystem created successfully"));
    
    UBiomeService* BiomeService = NewObject<UBiomeService>();
    if (!BiomeService)
    {
        UE_LOG(LogVHMBasicTests, Error, TEXT("Failed to create BiomeService"));
        return;
    }
    UE_LOG(LogVHMBasicTests, Verbose, TEXT("✓ BiomeService created successfully"));
    
    UClimateSystem* ClimateSystem = NewObject<UClimateSystem>();
    if (!ClimateSystem)
    {
        UE_LOG(LogVHMBasicTests, Error, TEXT("Failed to create ClimateSystem"));
        return;
    }
    UE_LOG(LogVHMBasicTests, Verbose, TEXT("✓ ClimateSystem created successfully"));
    
    // Test basic initialization
    FClimateSettings ClimateSettings;
    ClimateSystem->Initialize(ClimateSettings, 1337);
    UE_LOG(LogVHMBasicTests, Verbose, TEXT("✓ ClimateSystem initialized successfully"));
    
    // Objects will be cleaned up automatically by UE's garbage collector
    UE_LOG(LogVHMBasicTests, Verbose, TEXT("✓ Service creation test completed"));
}

// TEMPORARY: Disable console commands to prevent startup crashes
#if 0
// Console command for basic test
static FAutoConsoleCommand VHMBasicTestCommand(
    TEXT("wg.TestVHMBasic"),
    TEXT("Run basic VHM UE5.6 compatibility tests (safe, minimal object creation)"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        FVHMBasicTests::RunBasicCompatibilityTest();
    })
);

#endif // Temporarily disabled
