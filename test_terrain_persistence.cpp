// Simple test to verify terrain persistence checksum matching
#include "Services/HeightfieldService.h"
#include "Tests/WorldGenIntegrationTest.h"
#include "Data/WorldGenTypes.h"
#include "Utils/WorldGenLogging.h"
#include "Misc/CRC.h"

void TestTerrainPersistence()
{
    UE_LOG(LogTemp, Warning, TEXT("=== Testing Terrain Persistence Checksums ==="));
    
    // Create test objects
    UWorldGenIntegrationTest* IntegrationTest = NewObject<UWorldGenIntegrationTest>();
    
    // Run just the persistence test
    FIntegrationTestResult Result = IntegrationTest->ExecuteTestCategory(3); // Persistence test is category 3
    
    if (Result.bPassed)
    {
        UE_LOG(LogTemp, Warning, TEXT("✓ Terrain persistence test PASSED"));
        UE_LOG(LogTemp, Warning, TEXT("Execution time: %.2fms"), Result.ExecutionTimeMs);
        
        // Log detailed info
        for (const auto& DetailPair : Result.DetailedInfo)
        {
            UE_LOG(LogTemp, Warning, TEXT("  %s: %s"), *DetailPair.Key, *DetailPair.Value);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("✗ Terrain persistence test FAILED"));
        UE_LOG(LogTemp, Error, TEXT("Error: %s"), *Result.ErrorMessage);
        
        // Log detailed info
        for (const auto& DetailPair : Result.DetailedInfo)
        {
            UE_LOG(LogTemp, Error, TEXT("  %s: %s"), *DetailPair.Key, *DetailPair.Value);
        }
    }
    
    // Cleanup
    IntegrationTest->CleanupTestData();
}