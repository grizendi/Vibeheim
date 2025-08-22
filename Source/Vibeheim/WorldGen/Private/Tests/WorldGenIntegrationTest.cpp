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
#include "Misc/FileHelper.h"
#include "Engine/Engine.h"

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
		HandleTestFailure(TEXT("Initialization"), TEXT("Test environment setup failed"));
		return TestSuite;
	}

	// Validate test environment before running tests
	if (!ValidateTestEnvironment())
	{
		FIntegrationTestResult ValidationFailure(TEXT("Test Environment Validation"));
		ValidationFailure.SetFailed(TEXT("Test environment validation failed"));
		TestSuite.AddTestResult(ValidationFailure);
		CleanupTestData();
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
		
		// Execute test with error handling
		FIntegrationTestResult TestResult;
		try
		{
			TestResult = ExecuteTestCategory(TestIndex + 1);
		}
		catch (const std::exception& e)
		{
			TestResult = FIntegrationTestResult(TestNames[TestIndex]);
			TestResult.SetFailed(FString::Printf(TEXT("Exception during test execution: %s"), ANSI_TO_TCHAR(e.what())));
			HandleTestFailure(TestNames[TestIndex], TestResult.ErrorMessage);
		}
		
		TestSuite.AddTestResult(TestResult);

		// Log test result with detailed information
		if (TestResult.bPassed)
		{
			WORLDGEN_LOG(Log, TEXT("✓ %s (%.2fms)"), *TestNames[TestIndex], TestResult.ExecutionTimeMs);
		}
		else
		{
			WORLDGEN_LOG(Error, TEXT("✗ %s (%.2fms)"), *TestNames[TestIndex], TestResult.ExecutionTimeMs);
			WORLDGEN_LOG(Error, TEXT("  Error: %s"), *TestResult.ErrorMessage);
			
			// Log detailed information if available
			for (const auto& DetailPair : TestResult.DetailedInfo)
			{
				WORLDGEN_LOG(Error, TEXT("  %s: %s"), *DetailPair.Key, *DetailPair.Value);
			}
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
	
	// Cleanup service instances (includes state restoration)
	CleanupServiceInstances();
	
	// Remove temporary directory and all test files
	RemoveTempDirectory();
	
	// Reset initialization state
	bIsInitialized = false;
	
	// Force garbage collection to ensure all test objects are cleaned up
	if (GEngine)
	{
		GEngine->ForceGarbageCollection(true);
	}
	
	WORLDGEN_LOG(Log, TEXT("Integration test cleanup completed"));
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

	// Create service instances and initialize them
	if (!CreateServiceInstances())
	{
		WORLDGEN_LOG(Error, TEXT("Failed to create service instances"));
		return false;
	}

	// Initialize services with dependencies
	if (!InitializeServices())
	{
		WORLDGEN_LOG(Error, TEXT("Failed to initialize services"));
		CleanupServiceInstances();
		return false;
	}

	bIsInitialized = true;
	WORLDGEN_LOG(Log, TEXT("Integration test environment initialized successfully"));
	return true;
}

void UWorldGenIntegrationTest::CreateTempDirectory()
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	
	// Ensure we have a unique temporary directory path for this test session
	FString SessionId = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
	TempDirectoryPath = FPaths::ProjectDir() / TestConfig.TempDataPath / SessionId;
	
	// Clean up any existing directory first
	if (PlatformFile.DirectoryExists(*TempDirectoryPath))
	{
		WORLDGEN_LOG(Warning, TEXT("Temporary directory already exists, cleaning up: %s"), *TempDirectoryPath);
		PlatformFile.DeleteDirectoryRecursively(*TempDirectoryPath);
	}
	
	// Create the directory tree
	if (PlatformFile.CreateDirectoryTree(*TempDirectoryPath))
	{
		WORLDGEN_LOG(Log, TEXT("Created temporary test directory: %s"), *TempDirectoryPath);
		
		// Create subdirectories for different test data types
		FString TerrainDataPath = TempDirectoryPath / TEXT("TerrainData");
		FString PCGDataPath = TempDirectoryPath / TEXT("PCGData");
		FString POIDataPath = TempDirectoryPath / TEXT("POIData");
		
		PlatformFile.CreateDirectoryTree(*TerrainDataPath);
		PlatformFile.CreateDirectoryTree(*PCGDataPath);
		PlatformFile.CreateDirectoryTree(*POIDataPath);
	}
	else
	{
		WORLDGEN_LOG(Error, TEXT("Failed to create temporary test directory: %s"), *TempDirectoryPath);
	}
}

void UWorldGenIntegrationTest::RemoveTempDirectory()
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	
	if (PlatformFile.DirectoryExists(*TempDirectoryPath))
	{
		// Attempt to remove the directory multiple times if needed (files might be locked)
		int32 RetryCount = 3;
		bool bRemoved = false;
		
		for (int32 i = 0; i < RetryCount && !bRemoved; i++)
		{
			if (PlatformFile.DeleteDirectoryRecursively(*TempDirectoryPath))
			{
				WORLDGEN_LOG(Log, TEXT("Removed temporary test directory: %s"), *TempDirectoryPath);
				bRemoved = true;
			}
			else if (i < RetryCount - 1)
			{
				WORLDGEN_LOG(Warning, TEXT("Failed to remove temporary test directory (attempt %d/%d): %s"), i + 1, RetryCount, *TempDirectoryPath);
				// Wait a bit before retrying
				FPlatformProcess::Sleep(0.1f);
			}
		}
		
		if (!bRemoved)
		{
			WORLDGEN_LOG(Error, TEXT("Failed to remove temporary test directory after %d attempts: %s"), RetryCount, *TempDirectoryPath);
		}
	}
}

bool UWorldGenIntegrationTest::CreateServiceInstances()
{
	WORLDGEN_LOG(Log, TEXT("Creating service instances for integration testing..."));

	try
	{
		// Get or create WorldGenSettings instance
		WorldGenSettings = UWorldGenSettings::GetWorldGenSettings();
		if (!WorldGenSettings)
		{
			WORLDGEN_LOG(Error, TEXT("Failed to get WorldGenSettings instance"));
			return false;
		}

		// Create service instances with proper object ownership
		NoiseSystem = NewObject<UNoiseSystem>(this, UNoiseSystem::StaticClass(), NAME_None, RF_Transient);
		if (!NoiseSystem)
		{
			WORLDGEN_LOG(Error, TEXT("Failed to create NoiseSystem instance"));
			return false;
		}

		ClimateSystem = NewObject<UClimateSystem>(this, UClimateSystem::StaticClass(), NAME_None, RF_Transient);
		if (!ClimateSystem)
		{
			WORLDGEN_LOG(Error, TEXT("Failed to create ClimateSystem instance"));
			return false;
		}

		HeightfieldService = NewObject<UHeightfieldService>(this, UHeightfieldService::StaticClass(), NAME_None, RF_Transient);
		if (!HeightfieldService)
		{
			WORLDGEN_LOG(Error, TEXT("Failed to create HeightfieldService instance"));
			return false;
		}

		BiomeService = NewObject<UBiomeService>(this, UBiomeService::StaticClass(), NAME_None, RF_Transient);
		if (!BiomeService)
		{
			WORLDGEN_LOG(Error, TEXT("Failed to create BiomeService instance"));
			return false;
		}

		PCGService = NewObject<UPCGWorldService>(this, UPCGWorldService::StaticClass(), NAME_None, RF_Transient);
		if (!PCGService)
		{
			WORLDGEN_LOG(Error, TEXT("Failed to create PCGWorldService instance"));
			return false;
		}

		POIService = NewObject<UPOIService>(this, UPOIService::StaticClass(), NAME_None, RF_Transient);
		if (!POIService)
		{
			WORLDGEN_LOG(Error, TEXT("Failed to create POIService instance"));
			return false;
		}

		TileStreamingService = NewObject<UTileStreamingService>(this, UTileStreamingService::StaticClass(), NAME_None, RF_Transient);
		if (!TileStreamingService)
		{
			WORLDGEN_LOG(Error, TEXT("Failed to create TileStreamingService instance"));
			return false;
		}

		WORLDGEN_LOG(Log, TEXT("All service instances created successfully"));
		return true;
	}
	catch (const std::exception& e)
	{
		WORLDGEN_LOG(Error, TEXT("Exception during service instance creation: %s"), ANSI_TO_TCHAR(e.what()));
		return false;
	}
}

bool UWorldGenIntegrationTest::InitializeServices()
{
	WORLDGEN_LOG(Log, TEXT("Initializing services with test configuration..."));

	// Load and validate WorldGenSettings
	if (WorldGenSettings)
	{
		// Load settings from default configuration
		if (!WorldGenSettings->LoadFromJSON())
		{
			WORLDGEN_LOG(Warning, TEXT("Failed to load WorldGenSettings from JSON, using defaults"));
		}

		// Validate settings
		TArray<FString> ValidationErrors;
		if (!WorldGenSettings->ValidateSettings(ValidationErrors))
		{
			WORLDGEN_LOG(Error, TEXT("WorldGenSettings validation failed:"));
			for (const FString& Error : ValidationErrors)
			{
				WORLDGEN_LOG(Error, TEXT("  - %s"), *Error);
			}
			return false;
		}
	}

	// Initialize services with dependency injection
	// Note: In a real implementation, services would have Initialize() methods
	// For now, we'll just verify they exist and are valid
	
	if (!NoiseSystem)
	{
		WORLDGEN_LOG(Error, TEXT("NoiseSystem is null"));
		return false;
	}

	if (!ClimateSystem)
	{
		WORLDGEN_LOG(Error, TEXT("ClimateSystem is null"));
		return false;
	}

	if (!HeightfieldService)
	{
		WORLDGEN_LOG(Error, TEXT("HeightfieldService is null"));
		return false;
	}

	if (!BiomeService)
	{
		WORLDGEN_LOG(Error, TEXT("BiomeService is null"));
		return false;
	}

	if (!PCGService)
	{
		WORLDGEN_LOG(Error, TEXT("PCGService is null"));
		return false;
	}

	if (!POIService)
	{
		WORLDGEN_LOG(Error, TEXT("POIService is null"));
		return false;
	}

	if (!TileStreamingService)
	{
		WORLDGEN_LOG(Error, TEXT("TileStreamingService is null"));
		return false;
	}

	WORLDGEN_LOG(Log, TEXT("All services initialized successfully"));
	return true;
}

void UWorldGenIntegrationTest::CleanupServiceInstances()
{
	WORLDGEN_LOG(Log, TEXT("Cleaning up service instances..."));

	// Restore system state before cleanup
	RestoreSystemState();

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

void UWorldGenIntegrationTest::RestoreSystemState()
{
	WORLDGEN_LOG(Log, TEXT("Restoring system state after testing..."));

	// Clear any test-specific state that might affect other systems
	// This ensures test isolation and prevents side effects
	
	// Note: In a real implementation, this would restore any global state
	// that was modified during testing, such as:
	// - Resetting singleton instances to their original state
	// - Clearing any cached data that was modified
	// - Restoring original configuration values
	// - Cleaning up any temporary world objects or actors
}

bool UWorldGenIntegrationTest::ValidateTestEnvironment()
{
	WORLDGEN_LOG(Log, TEXT("Validating test environment..."));

	// Check if all required services are available
	if (!WorldGenSettings || !NoiseSystem || !ClimateSystem || !HeightfieldService ||
		!BiomeService || !PCGService || !POIService || !TileStreamingService)
	{
		WORLDGEN_LOG(Error, TEXT("One or more required services are not available"));
		return false;
	}

	// Check if temporary directory exists and is writable
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*TempDirectoryPath))
	{
		WORLDGEN_LOG(Error, TEXT("Temporary directory does not exist: %s"), *TempDirectoryPath);
		return false;
	}

	// Test write permissions by creating a test file
	FString TestFilePath = TempDirectoryPath / TEXT("write_test.tmp");
	if (!FFileHelper::SaveStringToFile(TEXT("test"), *TestFilePath))
	{
		WORLDGEN_LOG(Error, TEXT("Cannot write to temporary directory: %s"), *TempDirectoryPath);
		return false;
	}

	// Clean up test file
	PlatformFile.DeleteFile(*TestFilePath);

	WORLDGEN_LOG(Log, TEXT("Test environment validation passed"));
	return true;
}

void UWorldGenIntegrationTest::HandleTestFailure(const FString& TestName, const FString& ErrorMessage)
{
	WORLDGEN_LOG(Error, TEXT("Test failure in %s: %s"), *TestName, *ErrorMessage);

	// Attempt to recover from test failure by cleaning up any partial state
	// This prevents cascade failures in subsequent tests
	
	// Force garbage collection to clean up any leaked objects
	if (GEngine)
	{
		GEngine->ForceGarbageCollection(true);
	}

	// Log system state for debugging
	WORLDGEN_LOG(Log, TEXT("System state after test failure:"));
	WORLDGEN_LOG(Log, TEXT("  - WorldGenSettings: %s"), WorldGenSettings ? TEXT("Valid") : TEXT("Invalid"));
	WORLDGEN_LOG(Log, TEXT("  - NoiseSystem: %s"), NoiseSystem ? TEXT("Valid") : TEXT("Invalid"));
	WORLDGEN_LOG(Log, TEXT("  - ClimateSystem: %s"), ClimateSystem ? TEXT("Valid") : TEXT("Invalid"));
	WORLDGEN_LOG(Log, TEXT("  - HeightfieldService: %s"), HeightfieldService ? TEXT("Valid") : TEXT("Invalid"));
	WORLDGEN_LOG(Log, TEXT("  - BiomeService: %s"), BiomeService ? TEXT("Valid") : TEXT("Invalid"));
	WORLDGEN_LOG(Log, TEXT("  - PCGService: %s"), PCGService ? TEXT("Valid") : TEXT("Invalid"));
	WORLDGEN_LOG(Log, TEXT("  - POIService: %s"), POIService ? TEXT("Valid") : TEXT("Invalid"));
	WORLDGEN_LOG(Log, TEXT("  - TileStreamingService: %s"), TileStreamingService ? TEXT("Valid") : TEXT("Invalid"));
}

FString UWorldGenIntegrationTest::GetTempDataPath(const FString& SubDirectory) const
{
	if (SubDirectory.IsEmpty())
	{
		return TempDirectoryPath;
	}
	return TempDirectoryPath / SubDirectory;
}

bool UWorldGenIntegrationTest::EnsureDirectoryExists(const FString& DirectoryPath) const
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	
	if (!PlatformFile.DirectoryExists(*DirectoryPath))
	{
		return PlatformFile.CreateDirectoryTree(*DirectoryPath);
	}
	
	return true;
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