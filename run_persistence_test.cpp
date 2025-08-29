// Simple function to run just the persistence test
#include "Tests/WorldGenIntegrationTest.h"
#include "Engine/Engine.h"

// Console command to run persistence test
static FAutoConsoleCommand RunPersistenceTestCommand(
    TEXT("wg.TestPersistence"),
    TEXT("Run the terrain persistence integration test"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        UE_LOG(LogTemp, Warning, TEXT("=== Running Terrain Persistence Test ==="));
        
        // Create integration test instance
        UWorldGenIntegrationTest* IntegrationTest = NewObject<UWorldGenIntegrationTest>();
        
        // Run just the persistence test (category 3)
        FIntegrationTestResult Result = IntegrationTest->ExecuteTestCategory(3);
        
        // Log results
        if (Result.bPassed)
        {
            UE_LOG(LogTemp, Warning, TEXT("✓ Terrain Persistence Test PASSED"));
            UE_LOG(LogTemp, Warning, TEXT("Execution time: %.2fms"), Result.ExecutionTimeMs);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("✗ Terrain Persistence Test FAILED"));
            UE_LOG(LogTemp, Error, TEXT("Error: %s"), *Result.ErrorMessage);
        }
        
        // Log detailed info
        for (const auto& DetailPair : Result.DetailedInfo)
        {
            if (Result.bPassed)
            {
                UE_LOG(LogTemp, Warning, TEXT("  %s: %s"), *DetailPair.Key, *DetailPair.Value);
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("  %s: %s"), *DetailPair.Key, *DetailPair.Value);
            }
        }
        
        // Cleanup
        IntegrationTest->CleanupTestData();
        
        UE_LOG(LogTemp, Warning, TEXT("=== Terrain Persistence Test Complete ==="));
    })
);