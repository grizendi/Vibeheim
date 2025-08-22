#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/WorldGenTypes.h"
#include "Utils/WorldGenLogging.h"
#include "WorldGenIntegrationTest.generated.h"

// Forward declarations
class UWorldGenSettings;
class UNoiseSystem;
class UClimateSystem;
class UHeightfieldService;
class UBiomeService;
class UPCGWorldService;
class UPOIService;
class UTileStreamingService;

/**
 * Test execution result for individual integration tests
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FIntegrationTestResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test Result")
	bool bPassed = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test Result")
	FString TestName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test Result")
	FString ErrorMessage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test Result")
	float ExecutionTimeMs = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test Result")
	TMap<FString, FString> DetailedInfo;

	FIntegrationTestResult() = default;

	FIntegrationTestResult(const FString& InTestName)
		: TestName(InTestName)
	{
	}

	void SetPassed(float InExecutionTimeMs = 0.0f)
	{
		bPassed = true;
		ExecutionTimeMs = InExecutionTimeMs;
		ErrorMessage.Empty();
	}

	void SetFailed(const FString& InErrorMessage, float InExecutionTimeMs = 0.0f)
	{
		bPassed = false;
		ErrorMessage = InErrorMessage;
		ExecutionTimeMs = InExecutionTimeMs;
	}

	void AddDetailedInfo(const FString& Key, const FString& Value)
	{
		DetailedInfo.Add(Key, Value);
	}
};

/**
 * Complete integration test suite results
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FIntegrationTestSuite
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test Suite")
	TArray<FIntegrationTestResult> TestResults;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test Suite")
	int32 PassedTests = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test Suite")
	int32 TotalTests = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test Suite")
	float TotalExecutionTimeMs = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test Suite")
	bool bAllTestsPassed = false;

	void AddTestResult(const FIntegrationTestResult& Result)
	{
		TestResults.Add(Result);
		TotalTests++;
		TotalExecutionTimeMs += Result.ExecutionTimeMs;
		
		if (Result.bPassed)
		{
			PassedTests++;
		}
		
		bAllTestsPassed = (PassedTests == TotalTests);
	}

	void Reset()
	{
		TestResults.Empty();
		PassedTests = 0;
		TotalTests = 0;
		TotalExecutionTimeMs = 0.0f;
		bAllTestsPassed = false;
	}
};

/**
 * Configuration parameters for integration tests
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FTestConfiguration
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test Config")
	int32 TestSeed = 12345;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test Config")
	FTileCoord TestTileCoord = FTileCoord(0, 0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test Config")
	FString TempDataPath = TEXT("Temp/IntegrationTest/");

	// Performance thresholds (slightly higher than targets for test tolerance)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance Thresholds")
	float MaxTileGenTimeMs = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance Thresholds")
	float MaxPCGGenTimeMs = 2.0f;

	// Test data sizes
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test Parameters")
	int32 ConsistencyTestIterations = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test Parameters")
	int32 PerformanceTestTiles = 5;

	// Terrain editing test parameters
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain Editing")
	float TerrainEditRadius = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain Editing")
	float TerrainEditStrength = 5.0f;

	FTestConfiguration() = default;
};

/**
 * System validation data for tracking service initialization status
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FSystemValidationData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "System Validation")
	bool bWorldGenSettingsValid = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "System Validation")
	bool bNoiseSystemValid = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "System Validation")
	bool bClimateSystemValid = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "System Validation")
	bool bHeightfieldServiceValid = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "System Validation")
	bool bBiomeServiceValid = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "System Validation")
	bool bPCGServiceValid = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "System Validation")
	bool bPOIServiceValid = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "System Validation")
	bool bTileStreamingServiceValid = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "System Validation")
	TArray<FString> InitializationErrors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "System Validation")
	TMap<FString, float> ServiceInitTimes;

	bool AreAllServicesValid() const
	{
		return bWorldGenSettingsValid && bNoiseSystemValid && bClimateSystemValid &&
			   bHeightfieldServiceValid && bBiomeServiceValid && bPCGServiceValid &&
			   bPOIServiceValid && bTileStreamingServiceValid;
	}

	void Reset()
	{
		bWorldGenSettingsValid = false;
		bNoiseSystemValid = false;
		bClimateSystemValid = false;
		bHeightfieldServiceValid = false;
		bBiomeServiceValid = false;
		bPCGServiceValid = false;
		bPOIServiceValid = false;
		bTileStreamingServiceValid = false;
		InitializationErrors.Empty();
		ServiceInitTimes.Empty();
	}
};

/**
 * Main integration test class for World Generation system
 * Provides comprehensive testing of all world generation components
 */
UCLASS(BlueprintType)
class VIBEHEIM_API UWorldGenIntegrationTest : public UObject
{
	GENERATED_BODY()

public:
	UWorldGenIntegrationTest();

	/**
	 * Execute the complete integration test suite
	 * @return Test suite results with pass/fail status and detailed information
	 */
	UFUNCTION(BlueprintCallable, Category = "Integration Test")
	FIntegrationTestSuite ExecuteIntegrationTest();

	/**
	 * Execute a specific test category
	 * @param TestCategory The specific test to run (1-7)
	 * @return Individual test result
	 */
	UFUNCTION(BlueprintCallable, Category = "Integration Test")
	FIntegrationTestResult ExecuteTestCategory(int32 TestCategory);

	/**
	 * Get the current test configuration
	 */
	UFUNCTION(BlueprintCallable, Category = "Integration Test")
	FTestConfiguration GetTestConfiguration() const { return TestConfig; }

	/**
	 * Set custom test configuration
	 */
	UFUNCTION(BlueprintCallable, Category = "Integration Test")
	void SetTestConfiguration(const FTestConfiguration& InConfig) { TestConfig = InConfig; }

	/**
	 * Clean up temporary test data and restore system state
	 */
	UFUNCTION(BlueprintCallable, Category = "Integration Test")
	void CleanupTestData();

protected:
	// Test configuration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
	FTestConfiguration TestConfig;

	// Test execution methods (one for each test category)
	FIntegrationTestResult RunSystemInitializationTest();
	FIntegrationTestResult RunTerrainConsistencyTest();
	FIntegrationTestResult RunPersistenceTest();
	FIntegrationTestResult RunBiomeIntegrationTest();
	FIntegrationTestResult RunPCGIntegrationTest();
	FIntegrationTestResult RunPOIIntegrationTest();
	FIntegrationTestResult RunPerformanceTest();

	// Initialization and cleanup helpers
	bool Initialize();
	void CreateTempDirectory();
	void RemoveTempDirectory();

	// Service instance management
	bool CreateServiceInstances();
	bool InitializeServices();
	void CleanupServiceInstances();
	
	// State restoration
	void RestoreSystemState();
	
	// Error recovery and validation
	bool ValidateTestEnvironment();
	void HandleTestFailure(const FString& TestName, const FString& ErrorMessage);

private:
	// Service instances for testing
	UPROPERTY()
	TObjectPtr<UWorldGenSettings> WorldGenSettings;

	UPROPERTY()
	TObjectPtr<UNoiseSystem> NoiseSystem;

	UPROPERTY()
	TObjectPtr<UClimateSystem> ClimateSystem;

	UPROPERTY()
	TObjectPtr<UHeightfieldService> HeightfieldService;

	UPROPERTY()
	TObjectPtr<UBiomeService> BiomeService;

	UPROPERTY()
	TObjectPtr<UPCGWorldService> PCGService;

	UPROPERTY()
	TObjectPtr<UPOIService> POIService;

	UPROPERTY()
	TObjectPtr<UTileStreamingService> TileStreamingService;

	// Test state tracking
	bool bIsInitialized = false;
	FString TempDirectoryPath;
	
	// Helper methods for test data management
	FString GetTempDataPath(const FString& SubDirectory = TEXT("")) const;
	bool EnsureDirectoryExists(const FString& DirectoryPath) const;
};