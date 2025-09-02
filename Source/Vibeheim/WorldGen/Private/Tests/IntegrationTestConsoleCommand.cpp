#if 0 // DISABLED FOR TEST DIET - Non-essential integration test console command
#include "Tests/WorldGenIntegrationTest.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"

// Console command to run the full integration test suite
static FAutoConsoleCommand RunIntegrationTestCommand(
    TEXT("wg.IntegrationTest"),
    TEXT("Run the complete world generation integration test suite"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        UE_LOG(LogTemp, Warning, TEXT("=== Running World Generation Integration Test Suite ==="));
        
        // Create integration test instance
        UWorldGenIntegrationTest* IntegrationTest = NewObject<UWorldGenIntegrationTest>();
        
        // Run the complete test suite
        FIntegrationTestSuite TestSuite = IntegrationTest->ExecuteIntegrationTest();
        
        // Log overall results
        UE_LOG(LogTemp, Warning, TEXT("=== INTEGRATION TEST RESULTS ==="));
        UE_LOG(LogTemp, Warning, TEXT("Tests Passed: %d/%d"), TestSuite.PassedTests, TestSuite.TotalTests);
        UE_LOG(LogTemp, Warning, TEXT("Total Execution Time: %.2fms"), TestSuite.TotalExecutionTimeMs);
        
        if (TestSuite.bAllTestsPassed)
        {
            UE_LOG(LogTemp, Warning, TEXT("✓ ALL INTEGRATION TESTS PASSED"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("✗ SOME INTEGRATION TESTS FAILED"));
        }
        
        // Log individual test results
        for (const FIntegrationTestResult& Result : TestSuite.TestResults)
        {
            if (Result.bPassed)
            {
                UE_LOG(LogTemp, Warning, TEXT("✓ %s (%.2fms)"), *Result.TestName, Result.ExecutionTimeMs);
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("✗ %s (%.2fms)"), *Result.TestName, Result.ExecutionTimeMs);
                UE_LOG(LogTemp, Error, TEXT("  Error: %s"), *Result.ErrorMessage);
                
                // Log detailed information for failed tests
                for (const auto& DetailPair : Result.DetailedInfo)
                {
                    UE_LOG(LogTemp, Error, TEXT("  %s: %s"), *DetailPair.Key, *DetailPair.Value);
                }
            }
        }
        
        UE_LOG(LogTemp, Warning, TEXT("=== Integration Test Suite Complete ==="));
    })
);

// Console command to run just the persistence test
static FAutoConsoleCommand RunPersistenceTestCommand(
    TEXT("wg.TestPersistence"),
    TEXT("Run only the terrain persistence integration test"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        UE_LOG(LogTemp, Warning, TEXT("=== Running Terrain Persistence Integration Test ==="));
        
        // Create integration test instance
        UWorldGenIntegrationTest* IntegrationTest = NewObject<UWorldGenIntegrationTest>();
        
        // Run just the persistence test (category 3)
        FIntegrationTestResult Result = IntegrationTest->ExecuteTestCategory(3);
        
        // Log results
        if (Result.bPassed)
        {
            UE_LOG(LogTemp, Warning, TEXT("✓ Terrain Persistence Test PASSED"));
            UE_LOG(LogTemp, Warning, TEXT("Execution time: %.2fms"), Result.ExecutionTimeMs);
            
            // Log detailed info for passed test
            for (const auto& DetailPair : Result.DetailedInfo)
            {
                UE_LOG(LogTemp, Warning, TEXT("  %s: %s"), *DetailPair.Key, *DetailPair.Value);
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("✗ Terrain Persistence Test FAILED"));
            UE_LOG(LogTemp, Error, TEXT("Error: %s"), *Result.ErrorMessage);
            
            // Log detailed info for failed test
            for (const auto& DetailPair : Result.DetailedInfo)
            {
                UE_LOG(LogTemp, Error, TEXT("  %s: %s"), *DetailPair.Key, *DetailPair.Value);
            }
        }
        
        // Cleanup
        IntegrationTest->CleanupTestData();
        
        UE_LOG(LogTemp, Warning, TEXT("=== Terrain Persistence Test Complete ==="));
    })
);#e
ndif // DISABLED FOR TEST DIET