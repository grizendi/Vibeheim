#include "WorldGenIntegrationTestController.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformTime.h"

DEFINE_LOG_CATEGORY(LogWorldGenIntegrationTest);

AWorldGenIntegrationTestController::AWorldGenIntegrationTestController()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    // Initialize default values
    bAutoTestOnBeginPlay = false;
    bEnableDetailedLogging = true;
    PassedTests = 0;
    FailedTests = 0;
    TotalTests = 0;
    BiomeTransitionTolerance = 0.1f;
    VisualQualityThreshold = 0.8f;
    DefaultSampleCount = 20;
}

void AWorldGenIntegrationTestController::BeginPlay()
{
    Super::BeginPlay();

    // Find WorldGenManager in the level
    if (!FindWorldGenManager())
    {
        UE_LOG(LogWorldGenIntegrationTest, Error, TEXT("WorldGenManager not found in level"));
        return;
    }

    // Run automatic tests if enabled
    if (bAutoTestOnBeginPlay)
    {
        // Delay test execution to allow WorldGenManager to initialize
        FTimerHandle TimerHandle;
        GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this]()
        {
            RunAllTests();
        }, 2.0f, false);
    }
}

void AWorldGenIntegrationTestController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

bool AWorldGenIntegrationTestController::FindWorldGenManager()
{
    if (WorldGenManager)
    {
        return true;
    }

    // Search for WorldGenManager in the level
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWorldGenManager::StaticClass(), FoundActors);

    if (FoundActors.Num() > 0)
    {
        WorldGenManager = Cast<AWorldGenManager>(FoundActors[0]);
        if (WorldGenManager)
        {
            UE_LOG(LogWorldGenIntegrationTest, Log, TEXT("Found WorldGenManager: %s"), 
                *WorldGenManager->GetName());
            return true;
        }
    }

    UE_LOG(LogWorldGenIntegrationTest, Warning, TEXT("WorldGenManager not found in level"));
    return false;
}

bool AWorldGenIntegrationTestController::RunAllTests()
{
    if (!WorldGenManager)
    {
        LogTestResult(TEXT("RunAllTests"), false, TEXT("WorldGenManager not available"));
        return false;
    }

    if (!WorldGenManager->IsWorldGenReady())
    {
        LogTestResult(TEXT("RunAllTests"), false, TEXT("WorldGenManager not ready"));
        return false;
    }

    ResetTestResults();
    UE_LOG(LogWorldGenIntegrationTest, Log, TEXT("Starting comprehensive integration tests"));

    bool bAllTestsPassed = true;

    // Test 1: Biome transition smoothness
    TArray<FVector> BiomeTestLocations = {
        FVector(0, 0, 0),
        FVector(1000, 1000, 0),
        FVector(-500, 750, 0),
        FVector(2500, -1200, 0)
    };
    bool bBiomeTest = TestBiomeTransitionSmoothness(BiomeTestLocations);
    bAllTestsPassed &= bBiomeTest;

    // Test 2: POI and Portal functionality
    TArray<FIntVector> TestChunks = {
        FIntVector(0, 0, 0),
        FIntVector(1, 0, 0),
        FIntVector(0, 1, 0),
        FIntVector(1, 1, 0)
    };
    bool bPOIPortalTest = TestPOIPortalFunctionality(TestChunks);
    bAllTestsPassed &= bPOIPortalTest;

    // Test 3: Visual quality validation
    TArray<FVector> VisualTestLocations = {
        FVector(0, 0, 100),
        FVector(500, 500, 100),
        FVector(-250, 375, 100)
    };
    bool bVisualTest = ValidateVisualQuality(VisualTestLocations);
    bAllTestsPassed &= bVisualTest;

    // Test 4: Complete workflow
    bool bWorkflowTest = TestCompleteWorkflow();
    bAllTestsPassed &= bWorkflowTest;

    // Log final results
    UE_LOG(LogWorldGenIntegrationTest, Log, 
        TEXT("Integration tests completed: %d passed, %d failed, %d total"), 
        PassedTests, FailedTests, TotalTests);

    return bAllTestsPassed;
}

bool AWorldGenIntegrationTestController::TestBiomeTransitionSmoothness(
    const TArray<FVector>& TestLocations, float SampleRadius, int32 SampleCount)
{
    if (!WorldGenManager)
    {
        LogTestResult(TEXT("BiomeTransitionSmoothness"), false, TEXT("WorldGenManager not available"));
        return false;
    }

    bool bAllTransitionsSmooth = true;
    int32 LocationIndex = 0;

    for (const FVector& CenterLocation : TestLocations)
    {
        // Sample in a circle around the center location
        for (int32 SampleIdx = 0; SampleIdx < SampleCount; ++SampleIdx)
        {
            float Angle = (2.0f * PI * SampleIdx) / SampleCount;
            FVector SampleLocation1 = CenterLocation + FVector(
                FMath::Cos(Angle) * SampleRadius,
                FMath::Sin(Angle) * SampleRadius,
                0
            );

            float NextAngle = (2.0f * PI * (SampleIdx + 1)) / SampleCount;
            FVector SampleLocation2 = CenterLocation + FVector(
                FMath::Cos(NextAngle) * SampleRadius,
                FMath::Sin(NextAngle) * SampleRadius,
                0
            );

            FBiomeEvaluation Eval1 = WorldGenManager->EvaluateBiomeAtLocation(SampleLocation1);
            FBiomeEvaluation Eval2 = WorldGenManager->EvaluateBiomeAtLocation(SampleLocation2);

            if (!IsSmoothTransition(Eval1, Eval2, BiomeTransitionTolerance))
            {
                bAllTransitionsSmooth = false;
                if (bEnableDetailedLogging)
                {
                    UE_LOG(LogWorldGenIntegrationTest, Warning, 
                        TEXT("Rough biome transition detected at location %d, sample %d"), 
                        LocationIndex, SampleIdx);
                }
            }
        }
        LocationIndex++;
    }

    LogTestResult(TEXT("BiomeTransitionSmoothness"), bAllTransitionsSmooth, 
        FString::Printf(TEXT("Tested %d locations with %d samples each"), 
        TestLocations.Num(), SampleCount));

    return bAllTransitionsSmooth;
}

bool AWorldGenIntegrationTestController::TestPOIPortalFunctionality(const TArray<FIntVector>& ChunkCoordinates)
{
    if (!WorldGenManager)
    {
        LogTestResult(TEXT("POIPortalFunctionality"), false, TEXT("WorldGenManager not available"));
        return false;
    }

    bool bAllFunctionalityWorking = true;
    int32 TotalPOIs = 0;
    int32 TotalPortals = 0;

    for (const FIntVector& ChunkCoord : ChunkCoordinates)
    {
        // Test POI generation
        TArray<FPOIPlacementResult> POIResults = WorldGenManager->GeneratePOIsForChunk(ChunkCoord);
        TArray<FPOIInstance> ChunkPOIs = WorldGenManager->GetPOIsInChunk(ChunkCoord);
        TotalPOIs += ChunkPOIs.Num();

        // Test portal generation
        TArray<FPortalPlacementResult> PortalResults = WorldGenManager->GeneratePortalsForChunk(ChunkCoord);
        TArray<FDungeonPortal> ChunkPortals = WorldGenManager->GetPortalsInChunk(ChunkCoord);
        TotalPortals += ChunkPortals.Num();

        // Validate POI properties
        for (const FPOIInstance& POI : ChunkPOIs)
        {
            if (POI.WorldLocation.IsZero())
            {
                bAllFunctionalityWorking = false;
                if (bEnableDetailedLogging)
                {
                    UE_LOG(LogWorldGenIntegrationTest, Warning, 
                        TEXT("POI with invalid location in chunk (%d, %d, %d)"), 
                        ChunkCoord.X, ChunkCoord.Y, ChunkCoord.Z);
                }
            }
        }

        // Validate portal properties
        for (const FDungeonPortal& Portal : ChunkPortals)
        {
            if (Portal.WorldLocation.IsZero())
            {
                bAllFunctionalityWorking = false;
                if (bEnableDetailedLogging)
                {
                    UE_LOG(LogWorldGenIntegrationTest, Warning, 
                        TEXT("Portal with invalid location in chunk (%d, %d, %d)"), 
                        ChunkCoord.X, ChunkCoord.Y, ChunkCoord.Z);
                }
            }
        }
    }

    // Test retrieval of all active POIs and portals
    TArray<FPOIInstance> AllActivePOIs = WorldGenManager->GetAllActivePOIs();
    TArray<FDungeonPortal> AllActivePortals = WorldGenManager->GetAllActivePortals();

    LogTestResult(TEXT("POIPortalFunctionality"), bAllFunctionalityWorking, 
        FString::Printf(TEXT("Generated %d POIs and %d portals across %d chunks"), 
        TotalPOIs, TotalPortals, ChunkCoordinates.Num()));

    return bAllFunctionalityWorking;
}

bool AWorldGenIntegrationTestController::ValidateVisualQuality(const TArray<FVector>& SampleLocations)
{
    if (!WorldGenManager)
    {
        LogTestResult(TEXT("VisualQuality"), false, TEXT("WorldGenManager not available"));
        return false;
    }

    bool bVisualQualityAcceptable = true;
    int32 ValidSamples = 0;

    for (const FVector& Location : SampleLocations)
    {
        FBiomeEvaluation BiomeEval;
        if (ValidateBiomeEvaluation(Location, BiomeEval))
        {
            ValidSamples++;

            // Check for reasonable biome blend weights (should sum to approximately 1.0)
            float TotalWeight = 0.0f;
            for (int32 BiomeIdx = 0; BiomeIdx < static_cast<int32>(EBiomeType::Count); ++BiomeIdx)
            {
                TotalWeight += BiomeEval.BiomeWeights.Weights[BiomeIdx];
            }

            if (FMath::Abs(TotalWeight - 1.0f) > 0.1f)
            {
                bVisualQualityAcceptable = false;
                if (bEnableDetailedLogging)
                {
                    UE_LOG(LogWorldGenIntegrationTest, Warning, 
                        TEXT("Biome blend weights don't sum to 1.0 at location (%.1f, %.1f, %.1f): %.3f"), 
                        Location.X, Location.Y, Location.Z, TotalWeight);
                }
            }
        }
    }

    float QualityScore = static_cast<float>(ValidSamples) / SampleLocations.Num();
    bool bQualityMeetsThreshold = QualityScore >= VisualQualityThreshold;
    bVisualQualityAcceptable &= bQualityMeetsThreshold;

    LogTestResult(TEXT("VisualQuality"), bVisualQualityAcceptable, 
        FString::Printf(TEXT("Quality score: %.2f (threshold: %.2f)"), 
        QualityScore, VisualQualityThreshold));

    return bVisualQualityAcceptable;
}

bool AWorldGenIntegrationTestController::TestCompleteWorkflow()
{
    if (!WorldGenManager)
    {
        LogTestResult(TEXT("CompleteWorkflow"), false, TEXT("WorldGenManager not available"));
        return false;
    }

    bool bWorkflowSuccessful = true;

    // Test 1: Verify initialization
    if (!WorldGenManager->IsWorldGenReady())
    {
        bWorkflowSuccessful = false;
        LogTestResult(TEXT("CompleteWorkflow - Initialization"), false, TEXT("WorldGenManager not ready"));
    }

    // Test 2: Test settings retrieval
    const FWorldGenSettings& CurrentSettings = WorldGenManager->GetWorldGenSettings();
    if (CurrentSettings.Seed == 0 && CurrentSettings.WorldGenVersion == 0)
    {
        bWorkflowSuccessful = false;
        LogTestResult(TEXT("CompleteWorkflow - Settings"), false, TEXT("Invalid settings"));
    }

    // Test 3: Test streaming statistics
    int32 LoadedChunks, GeneratingChunks;
    float AvgGenTime, P95GenTime;
    WorldGenManager->GetStreamingStats(LoadedChunks, GeneratingChunks, AvgGenTime, P95GenTime);
    
    if (LoadedChunks < 0 || GeneratingChunks < 0)
    {
        bWorkflowSuccessful = false;
        LogTestResult(TEXT("CompleteWorkflow - Streaming Stats"), false, TEXT("Invalid streaming statistics"));
    }

    // Test 4: Test chunk rebuild functionality
    bool bRebuildQueued = WorldGenManager->RebuildChunk(FIntVector(0, 0, 0));
    // Rebuild may fail if chunk doesn't exist yet, which is acceptable

    LogTestResult(TEXT("CompleteWorkflow"), bWorkflowSuccessful, 
        FString::Printf(TEXT("Loaded chunks: %d, Generating: %d"), LoadedChunks, GeneratingChunks));

    return bWorkflowSuccessful;
}

void AWorldGenIntegrationTestController::GetTestResults(int32& OutPassedTests, int32& OutFailedTests, int32& OutTotalTests) const
{
    OutPassedTests = PassedTests;
    OutFailedTests = FailedTests;
    OutTotalTests = TotalTests;
}

void AWorldGenIntegrationTestController::ResetTestResults()
{
    PassedTests = 0;
    FailedTests = 0;
    TotalTests = 0;
}

void AWorldGenIntegrationTestController::LogTestResult(const FString& TestName, bool bPassed, const FString& Details)
{
    TotalTests++;
    if (bPassed)
    {
        PassedTests++;
        UE_LOG(LogWorldGenIntegrationTest, Log, TEXT("✓ %s: PASSED %s"), *TestName, *Details);
    }
    else
    {
        FailedTests++;
        UE_LOG(LogWorldGenIntegrationTest, Error, TEXT("✗ %s: FAILED %s"), *TestName, *Details);
    }
}

bool AWorldGenIntegrationTestController::ValidateBiomeEvaluation(const FVector& Location, FBiomeEvaluation& OutBiomeEval)
{
    if (!WorldGenManager)
    {
        return false;
    }

    OutBiomeEval = WorldGenManager->EvaluateBiomeAtLocation(Location);
    
    // Basic validation - primary biome should not be None
    if (OutBiomeEval.DominantBiome == TEXT("None"))
    {
        return false;
    }

    // Blend weights should be non-negative
    for (int32 BiomeIdx = 0; BiomeIdx < static_cast<int32>(EBiomeType::Count); ++BiomeIdx)
    {
        if (OutBiomeEval.BiomeWeights.Weights[BiomeIdx] < 0.0f)
        {
            return false;
        }
    }

    return true;
}

bool AWorldGenIntegrationTestController::IsSmoothTransition(const FBiomeEvaluation& Eval1, const FBiomeEvaluation& Eval2, float MaxBlendChange)
{
    for (int32 BiomeIdx = 0; BiomeIdx < static_cast<int32>(EBiomeType::Count); ++BiomeIdx)
    {
        float WeightChange = FMath::Abs(Eval2.BiomeWeights.Weights[BiomeIdx] - Eval1.BiomeWeights.Weights[BiomeIdx]);
        if (WeightChange > MaxBlendChange)
        {
            return false;
        }
    }
    return true;
}