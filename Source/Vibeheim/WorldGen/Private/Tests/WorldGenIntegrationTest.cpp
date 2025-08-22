#include "Tests/WorldGenIntegrationTest.h"
#include "WorldGenSettings.h"
#include "Services/NoiseSystem.h"
#include "Services/ClimateSystem.h"
#include "Services/HeightfieldService.h"
#include "Services/BiomeService.h"
#include "Services/PCGWorldService.h"
#include "Services/POIService.h"
#include "Services/TileStreamingService.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"

UWorldGenIntegrationTest::UWorldGenIntegrationTest()
{
	// Initialize default test configuration
	TestConfig = FTestConfiguration();
	TempDirectoryPath = FPaths::ProjectDir() / TestConfig.TempDataPath;
}

FIntegrationTestSuite UWorldGenIntegrationTest::ExecuteIntegrationTest()
{
	WORLDGEN_LOG(Warning, TEXT("=== WORLD GENERATION INTEGRATION TEST ==="));
	
	FIntegrationTestSuite TestSuite;
	TestSuite.Reset();

	// Initialize test environment
	if (!Initialize())
	{
		FIntegrationTestResult InitFailure(TEXT("Test Environment Initialization"));
		InitFailure.SetFailed(TEXT("Failed to initialize test environment"));
		TestSuite.AddTestResult(InitFailure);
		return TestSuite;
	}

	// Execute all test categories in sequence
	TArray<FString> TestNames = {
		TEXT("System Initialization"),
		TEXT("Terrain Generation Consistency"),
		TEXT("Terrain Editing and Persistence"),
		TEXT("Biome System Integration"),
		TEXT("PCG Content Generation"),
		TEXT("POI Generation and Placement"),
		TEXT("Performance Validation")
	};

	for (int32 TestIndex = 0; TestIndex < TestNames.Num(); TestIndex++)
	{
		WORLDGEN_LOG(Log, TEXT("--- Test %d: %s ---"), TestIndex + 1, *TestNames[TestIndex]);
		
		FIntegrationTestResult TestResult = ExecuteTestCategory(TestIndex + 1);
		TestSuite.AddTestResult(TestResult);

		// Log test result
		if (TestResult.bPassed)
		{
			WORLDGEN_LOG(Log, TEXT("✓ %s"), *TestNames[TestIndex]);
		}
		else
		{
			WORLDGEN_LOG(Error, TEXT("✗ %s"), *TestNames[TestIndex]);
			WORLDGEN_LOG(Error, TEXT("  Error: %s"), *TestResult.ErrorMessage);
		}
	}

	// Cleanup test environment
	CleanupTestData();

	// Log final results
	WORLDGEN_LOG(Warning, TEXT("=== INTEGRATION TEST RESULTS ==="));
	WORLDGEN_LOG(Warning, TEXT("Tests Passed: %d/%d"), TestSuite.PassedTests, TestSuite.TotalTests);
	WORLDGEN_LOG(Warning, TEXT("Total Execution Time: %.2fms"), TestSuite.TotalExecutionTimeMs);

	if (TestSuite.bAllTestsPassed)
	{
		WORLDGEN_LOG(Warning, TEXT("✓ ALL INTEGRATION TESTS PASSED"));
	}
	else
	{
		WORLDGEN_LOG(Error, TEXT("✗ SOME INTEGRATION TESTS FAILED"));
		WORLDGEN_LOG(Error, TEXT("Error: System requires fixes before gameplay testing"));
	}

	return TestSuite;
}

FIntegrationTestResult UWorldGenIntegrationTest::ExecuteTestCategory(int32 TestCategory)
{
	switch (TestCategory)
	{
		case 1: return RunSystemInitializationTest();
		case 2: return RunTerrainConsistencyTest();
		case 3: return RunPersistenceTest();
		case 4: return RunBiomeIntegrationTest();
		case 5: return RunPCGIntegrationTest();
		case 6: return RunPOIIntegrationTest();
		case 7: return RunPerformanceTest();
		default:
		{
			FIntegrationTestResult InvalidTest(TEXT("Invalid Test Category"));
			InvalidTest.SetFailed(FString::Printf(TEXT("Test category %d does not exist"), TestCategory));
			return InvalidTest;
		}
	}
}

void UWorldGenIntegrationTest::CleanupTestData()
{
	WORLDGEN_LOG(Log, TEXT("Cleaning up integration test data..."));
	
	// Cleanup service instances
	CleanupServiceInstances();
	
	// Remove temporary directory
	RemoveTempDirectory();
	
	bIsInitialized = false;
}

bool UWorldGenIntegrationTest::Initialize()
{
	if (bIsInitialized)
	{
		return true;
	}

	WORLDGEN_LOG(Log, TEXT("Initializing integration test environment..."));

	// Create temporary directory for test data
	CreateTempDirectory();

	// Create service instances
	CreateServiceInstances();

	bIsInitialized = true;
	return true;
}

void UWorldGenIntegrationTest::CreateTempDirectory()
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	
	if (!PlatformFile.DirectoryExists(*TempDirectoryPath))
	{
		if (PlatformFile.CreateDirectoryTree(*TempDirectoryPath))
		{
			WORLDGEN_LOG(Log, TEXT("Created temporary test directory: %s"), *TempDirectoryPath);
		}
		else
		{
			WORLDGEN_LOG(Error, TEXT("Failed to create temporary test directory: %s"), *TempDirectoryPath);
		}
	}
}

void UWorldGenIntegrationTest::RemoveTempDirectory()
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	
	if (PlatformFile.DirectoryExists(*TempDirectoryPath))
	{
		if (PlatformFile.DeleteDirectoryRecursively(*TempDirectoryPath))
		{
			WORLDGEN_LOG(Log, TEXT("Removed temporary test directory: %s"), *TempDirectoryPath);
		}
		else
		{
			WORLDGEN_LOG(Warning, TEXT("Failed to remove temporary test directory: %s"), *TempDirectoryPath);
		}
	}
}

void UWorldGenIntegrationTest::CreateServiceInstances()
{
	// Create service instances for testing
	WorldGenSettings = UWorldGenSettings::GetWorldGenSettings();
	NoiseSystem = NewObject<UNoiseSystem>(this);
	ClimateSystem = NewObject<UClimateSystem>(this);
	HeightfieldService = NewObject<UHeightfieldService>(this);
	BiomeService = NewObject<UBiomeService>(this);
	PCGService = NewObject<UPCGWorldService>(this);
	POIService = NewObject<UPOIService>(this);
	TileStreamingService = NewObject<UTileStreamingService>(this);
}

void UWorldGenIntegrationTest::CleanupServiceInstances()
{
	// Clear service references (they will be garbage collected)
	WorldGenSettings = nullptr;
	NoiseSystem = nullptr;
	ClimateSystem = nullptr;
	HeightfieldService = nullptr;
	BiomeService = nullptr;
	PCGService = nullptr;
	POIService = nullptr;
	TileStreamingService = nullptr;
}

// Test implementation stubs - these will be implemented in subsequent tasks
FIntegrationTestResult UWorldGenIntegrationTest::RunSystemInitializationTest()
{
	FIntegrationTestResult Result(TEXT("System Initialization"));
	
	// TODO: Implement in task 3
	Result.SetFailed(TEXT("Test not yet implemented"));
	
	return Result;
}

FIntegrationTestResult UWorldGenIntegrationTest::RunTerrainConsistencyTest()
{
	FIntegrationTestResult Result(TEXT("Terrain Generation Consistency"));
	
	// TODO: Implement in task 4
	Result.SetFailed(TEXT("Test not yet implemented"));
	
	return Result;
}

FIntegrationTestResult UWorldGenIntegrationTest::RunPersistenceTest()
{
	FIntegrationTestResult Result(TEXT("Terrain Editing and Persistence"));
	
	// TODO: Implement in task 5
	Result.SetFailed(TEXT("Test not yet implemented"));
	
	return Result;
}

FIntegrationTestResult UWorldGenIntegrationTest::RunBiomeIntegrationTest()
{
	FIntegrationTestResult Result(TEXT("Biome System Integration"));
	
	// TODO: Implement in task 6
	Result.SetFailed(TEXT("Test not yet implemented"));
	
	return Result;
}

FIntegrationTestResult UWorldGenIntegrationTest::RunPCGIntegrationTest()
{
	FIntegrationTestResult Result(TEXT("PCG Content Generation"));
	
	// TODO: Implement in task 7
	Result.SetFailed(TEXT("Test not yet implemented"));
	
	return Result;
}

FIntegrationTestResult UWorldGenIntegrationTest::RunPOIIntegrationTest()
{
	FIntegrationTestResult Result(TEXT("POI Generation and Placement"));
	
	// TODO: Implement in task 8
	Result.SetFailed(TEXT("Test not yet implemented"));
	
	return Result;
}

FIntegrationTestResult UWorldGenIntegrationTest::RunPerformanceTest()
{
	FIntegrationTestResult Result(TEXT("Performance Validation"));
	
	// TODO: Implement in task 9
	Result.SetFailed(TEXT("Test not yet implemented"));
	
	return Result;
}