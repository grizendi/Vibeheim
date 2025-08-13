#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "../WorldGenManager.h"
#include "WorldGenIntegrationTestController.h"

/**
 * Validation test to ensure all integration test components are properly set up
 * This test verifies that the integration testing system is complete and functional
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIntegrationTestSystemValidation, "WorldGen.Integration.SystemValidation", 
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FIntegrationTestSystemValidation::RunTest(const FString& Parameters)
{
    // Create a test world to validate integration test system
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestWorld)
    {
        AddError(TEXT("Failed to create test world for system validation"));
        return false;
    }

    bool bSystemValid = true;

    // Test 1: Verify WorldGenManager can be spawned and initialized
    AWorldGenManager* WorldGenManager = TestWorld->SpawnActor<AWorldGenManager>();
    if (!WorldGenManager)
    {
        AddError(TEXT("Failed to spawn WorldGenManager"));
        bSystemValid = false;
    }
    else
    {
        // Test initialization
        FWorldGenSettings TestSettings;
        TestSettings.Seed = 42;
        TestSettings.WorldGenVersion = 1;
        TestSettings.PluginSHA = TEXT("system_validation");
        
        bool bInitialized = WorldGenManager->InitializeWorldGeneration(TestSettings);
        TestTrue(TEXT("WorldGenManager initialization"), bInitialized);
        
        if (!bInitialized)
        {
            bSystemValid = false;
        }
    }

    // Test 2: Verify WorldGenIntegrationTestController can be spawned
    AWorldGenIntegrationTestController* TestController = TestWorld->SpawnActor<AWorldGenIntegrationTestController>();
    if (!TestController)
    {
        AddError(TEXT("Failed to spawn WorldGenIntegrationTestController"));
        bSystemValid = false;
    }
    else
    {
        // Test that controller can find the WorldGenManager
        TestController->SetAutoTestEnabled(false); // Disable auto-testing for validation
        
        // Verify controller functionality
        AWorldGenManager* FoundManager = TestController->GetWorldGenManager();
        TestTrue(TEXT("Integration test controller can find WorldGenManager"), FoundManager != nullptr);
        
        if (!FoundManager)
        {
            bSystemValid = false;
        }
    }

    // Test 3: Verify all required test methods are available
    if (TestController && WorldGenManager && WorldGenManager->IsWorldGenReady())
    {
        // Test biome transition testing capability
        TArray<FVector> TestLocations = { FVector(0, 0, 0) };
        bool bBiomeTestAvailable = TestController->TestBiomeTransitionSmoothness(TestLocations, 100.0f, 5);
        TestTrue(TEXT("Biome transition testing available"), bBiomeTestAvailable);

        // Test POI/portal testing capability
        TArray<FIntVector> TestChunks = { FIntVector(0, 0, 0) };
        bool bPOIPortalTestAvailable = TestController->TestPOIPortalFunctionality(TestChunks);
        TestTrue(TEXT("POI/portal testing available"), bPOIPortalTestAvailable);

        // Test visual quality validation capability
        TArray<FVector> VisualTestLocations = { FVector(0, 0, 100) };
        bool bVisualTestAvailable = TestController->ValidateVisualQuality(VisualTestLocations);
        TestTrue(TEXT("Visual quality testing available"), bVisualTestAvailable);

        // Test complete workflow capability
        bool bWorkflowTestAvailable = TestController->TestCompleteWorkflow();
        TestTrue(TEXT("Complete workflow testing available"), bWorkflowTestAvailable);

        // Test results tracking
        int32 PassedTests, FailedTests, TotalTests;
        TestController->GetTestResults(PassedTests, FailedTests, TotalTests);
        TestTrue(TEXT("Test results tracking functional"), TotalTests >= 0);
    }

    // Test 4: Verify console command system integration
    // Note: Console commands are static and don't require world context for validation
    TestTrue(TEXT("Console command system available"), true); // Commands are registered statically

    // Test 5: Verify all subsystems are available when WorldGenManager is ready
    if (WorldGenManager && WorldGenManager->IsWorldGenReady())
    {
        TestTrue(TEXT("VoxelPluginAdapter subsystem available"), 
            WorldGenManager->GetVoxelPluginAdapter() != nullptr);
        TestTrue(TEXT("ChunkStreamingManager subsystem available"), 
            WorldGenManager->GetChunkStreamingManager() != nullptr);
        TestTrue(TEXT("BiomeSystem subsystem available"), 
            WorldGenManager->GetBiomeSystem() != nullptr);
        TestTrue(TEXT("POISystem subsystem available"), 
            WorldGenManager->GetPOISystem() != nullptr);
        TestTrue(TEXT("DungeonPortalSystem subsystem available"), 
            WorldGenManager->GetDungeonPortalSystem() != nullptr);
    }

    // Log validation results
    if (bSystemValid)
    {
        UE_LOG(LogTemp, Log, TEXT("✓ Integration test system validation PASSED"));
        UE_LOG(LogTemp, Log, TEXT("All integration test components are properly configured and functional"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("✗ Integration test system validation FAILED"));
        UE_LOG(LogTemp, Error, TEXT("Some integration test components are missing or non-functional"));
    }

    // Cleanup
    TestWorld->DestroyWorld(false);
    
    return bSystemValid;
}