#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "../WorldGenManager.h"
#include "WorldGenIntegrationTestController.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogWorldGenIntegrationTest, Log, All);

/**
 * Integration test controller for automated world generation validation
 * Can be placed in levels to run comprehensive integration tests
 */
UCLASS(BlueprintType, Blueprintable)
class VIBEHEIM_API AWorldGenIntegrationTestController : public AActor
{
    GENERATED_BODY()

public:
    AWorldGenIntegrationTestController();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

public:
    /**
     * Run all integration tests
     * @return True if all tests passed
     */
    UFUNCTION(BlueprintCallable, Category = "Integration Test")
    bool RunAllTests();

    /**
     * Test biome transition smoothness at specified locations
     * @param TestLocations Array of world locations to test
     * @param SampleRadius Radius around each location to sample
     * @param SampleCount Number of samples per location
     * @return True if all transitions are smooth
     */
    UFUNCTION(BlueprintCallable, Category = "Integration Test")
    bool TestBiomeTransitionSmoothness(const TArray<FVector>& TestLocations, 
                                     float SampleRadius = 500.0f, 
                                     int32 SampleCount = 20);

    /**
     * Test POI and portal functionality end-to-end
     * @param ChunkCoordinates Array of chunk coordinates to test
     * @return True if POI and portal systems work correctly
     */
    UFUNCTION(BlueprintCallable, Category = "Integration Test")
    bool TestPOIPortalFunctionality(const TArray<FIntVector>& ChunkCoordinates);

    /**
     * Validate visual quality through automated sampling
     * @param SampleLocations Locations to sample for visual validation
     * @return True if visual quality meets standards
     */
    UFUNCTION(BlueprintCallable, Category = "Integration Test")
    bool ValidateVisualQuality(const TArray<FVector>& SampleLocations);

    /**
     * Test complete world generation workflow
     * @return True if workflow completes successfully
     */
    UFUNCTION(BlueprintCallable, Category = "Integration Test")
    bool TestCompleteWorkflow();

    /**
     * Get test results summary
     * @param OutPassedTests Number of tests that passed
     * @param OutFailedTests Number of tests that failed
     * @param OutTotalTests Total number of tests run
     */
    UFUNCTION(BlueprintCallable, Category = "Integration Test")
    void GetTestResults(int32& OutPassedTests, int32& OutFailedTests, int32& OutTotalTests) const;

    /**
     * Reset test results
     */
    UFUNCTION(BlueprintCallable, Category = "Integration Test")
    void ResetTestResults();

    /**
     * Enable or disable automatic testing on BeginPlay
     * @param bEnable Whether to enable automatic testing
     */
    UFUNCTION(BlueprintCallable, Category = "Integration Test")
    void SetAutoTestEnabled(bool bEnable) { bAutoTestOnBeginPlay = bEnable; }

    /**
     * Get the WorldGenManager reference
     * @return Pointer to WorldGenManager, nullptr if not found
     */
    UFUNCTION(BlueprintCallable, Category = "Integration Test")
    AWorldGenManager* GetWorldGenManager() const { return WorldGenManager; }

protected:
    /**
     * Find and cache WorldGenManager reference
     * @return True if WorldGenManager was found
     */
    bool FindWorldGenManager();

    /**
     * Log test result
     * @param TestName Name of the test
     * @param bPassed Whether the test passed
     * @param Details Additional details about the test result
     */
    void LogTestResult(const FString& TestName, bool bPassed, const FString& Details = TEXT(""));

    /**
     * Validate biome evaluation at a specific location
     * @param Location World location to test
     * @param OutBiomeEval Biome evaluation result
     * @return True if biome evaluation is valid
     */
    bool ValidateBiomeEvaluation(const FVector& Location, FBiomeEvaluation& OutBiomeEval);

    /**
     * Check if two biome evaluations represent a smooth transition
     * @param Eval1 First biome evaluation
     * @param Eval2 Second biome evaluation
     * @param MaxBlendChange Maximum allowed blend weight change
     * @return True if transition is smooth
     */
    bool IsSmoothTransition(const FBiomeEvaluation& Eval1, const FBiomeEvaluation& Eval2, 
                           float MaxBlendChange = 0.1f);

private:
    /** Reference to WorldGenManager in the level */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Integration Test", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<AWorldGenManager> WorldGenManager;

    /** Whether to automatically run tests on BeginPlay */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Integration Test", meta = (AllowPrivateAccess = "true"))
    bool bAutoTestOnBeginPlay;

    /** Whether to enable detailed logging */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Integration Test", meta = (AllowPrivateAccess = "true"))
    bool bEnableDetailedLogging;

    /** Test results tracking */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Integration Test", meta = (AllowPrivateAccess = "true"))
    int32 PassedTests;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Integration Test", meta = (AllowPrivateAccess = "true"))
    int32 FailedTests;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Integration Test", meta = (AllowPrivateAccess = "true"))
    int32 TotalTests;

    /** Test settings */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Integration Test", meta = (AllowPrivateAccess = "true"))
    float BiomeTransitionTolerance;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Integration Test", meta = (AllowPrivateAccess = "true"))
    float VisualQualityThreshold;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Integration Test", meta = (AllowPrivateAccess = "true"))
    int32 DefaultSampleCount;
};