#include "WorldGenConsoleCommands.h"
#include "WorldGenManager.h"
#include "VoxelPluginAdapter.h"
#include "ChunkStreamingManager.h"
#include "Tests/WorldGenIntegrationTestController.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"

// Define static members
TAutoConsoleVariable<int32> FWorldGenConsoleCommands::CVarSeed(
    TEXT("wg.Seed"),
    1337,
    TEXT("World generation seed value"),
    ECVF_Default
);

TAutoConsoleVariable<int32> FWorldGenConsoleCommands::CVarStreamRadius(
    TEXT("wg.StreamRadius"),
    6,
    TEXT("Streaming radius in chunks"),
    ECVF_Default
);

TAutoConsoleVariable<bool> FWorldGenConsoleCommands::CVarDebugMasks(
    TEXT("wg.DebugMasks"),
    false,
    TEXT("Enable biome mask visualization"),
    ECVF_Default
);

TAutoConsoleVariable<bool> FWorldGenConsoleCommands::CVarDebugChunks(
    TEXT("wg.DebugChunks"),
    false,
    TEXT("Show chunk boundaries and LOD levels"),
    ECVF_Default
);

FAutoConsoleCommand FWorldGenConsoleCommands::TestErrorLoggingCommand(
    TEXT("wg.TestErrorLogging"),
    TEXT("Test structured error logging system"),
    FConsoleCommandWithArgsDelegate::CreateStatic(&FWorldGenConsoleCommands::TestErrorLogging)
);

FAutoConsoleCommand FWorldGenConsoleCommands::TestFallbackGenerationCommand(
    TEXT("wg.TestFallbackGeneration"),
    TEXT("Test fallback terrain generation for a specific chunk (usage: wg.TestFallbackGeneration X Y Z)"),
    FConsoleCommandWithArgsDelegate::CreateStatic(&FWorldGenConsoleCommands::TestFallbackGeneration)
);

FAutoConsoleCommand FWorldGenConsoleCommands::ShowStreamingStatsCommand(
    TEXT("wg.ShowStreamingStats"),
    TEXT("Display current streaming statistics"),
    FConsoleCommandWithArgsDelegate::CreateStatic(&FWorldGenConsoleCommands::ShowStreamingStats)
);

void FWorldGenConsoleCommands::RegisterCommands()
{
    UE_LOG(LogTemp, Log, TEXT("WorldGen console commands registered"));
}

void FWorldGenConsoleCommands::UnregisterCommands()
{
    UE_LOG(LogTemp, Log, TEXT("WorldGen console commands unregistered"));
}

void FWorldGenConsoleCommands::TestErrorLogging(const TArray<FString>& Args)
{
    AWorldGenManager* WorldGenManager = FindWorldGenManager();
    if (!WorldGenManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("No WorldGenManager found in current world"));
        return;
    }
    
    // Test structured error logging
    UVoxelPluginAdapter* Adapter = WorldGenManager->GetVoxelPluginAdapter();
    if (Adapter)
    {
        // Test chunk coordinate logging
        FIntVector TestChunk(10, 20, 30);
        UE_LOG(LogTemp, Warning, TEXT("Testing structured error logging..."));
        
        // This will trigger structured error logging
        Adapter->TestStructuredErrorLogging(TEXT("Test error message for console command"), TestChunk, 
                                           TEXT("TestContext: Console command execution"));
        
        UE_LOG(LogTemp, Log, TEXT("Structured error logging test completed - check logs for [STRUCTURED_ERROR] entries"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("VoxelPluginAdapter not available for error logging test"));
    }
}

void FWorldGenConsoleCommands::TestFallbackGeneration(const TArray<FString>& Args)
{
    if (Args.Num() < 3)
    {
        UE_LOG(LogTemp, Warning, TEXT("Usage: wg.TestFallbackGeneration X Y Z (chunk coordinates)"));
        return;
    }
    
    AWorldGenManager* WorldGenManager = FindWorldGenManager();
    if (!WorldGenManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("No WorldGenManager found in current world"));
        return;
    }
    
    // Parse chunk coordinates
    int32 X = FCString::Atoi(*Args[0]);
    int32 Y = FCString::Atoi(*Args[1]);
    int32 Z = FCString::Atoi(*Args[2]);
    FIntVector ChunkCoord(X, Y, Z);
    
    UE_LOG(LogTemp, Log, TEXT("Testing fallback generation for chunk (%d, %d, %d)"), X, Y, Z);
    
    UVoxelPluginAdapter* Adapter = WorldGenManager->GetVoxelPluginAdapter();
    if (Adapter)
    {
        // Simulate a chunk generation failure to test fallback
        bool bFallbackResult = Adapter->TestFallbackGeneration(ChunkCoord, 
                                                               TEXT("Console command test failure"), true);
        
        UE_LOG(LogTemp, Log, TEXT("Fallback generation test result: %s"), bFallbackResult ? TEXT("Success") : TEXT("Failed"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("VoxelPluginAdapter not available for fallback generation test"));
    }
}

void FWorldGenConsoleCommands::ShowStreamingStats(const TArray<FString>& Args)
{
    AWorldGenManager* WorldGenManager = FindWorldGenManager();
    if (!WorldGenManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("No WorldGenManager found in current world"));
        return;
    }
    
    int32 LoadedChunks, GeneratingChunks;
    float AvgTime, P95Time;
    WorldGenManager->GetStreamingStats(LoadedChunks, GeneratingChunks, AvgTime, P95Time);
    
    const FWorldGenSettings& Settings = WorldGenManager->GetWorldGenSettings();
    
    UE_LOG(LogTemp, Log, TEXT("=== World Generation Streaming Stats ==="));
    UE_LOG(LogTemp, Log, TEXT("Seed: %lld"), Settings.Seed);
    UE_LOG(LogTemp, Log, TEXT("Loaded Chunks: %d"), LoadedChunks);
    UE_LOG(LogTemp, Log, TEXT("Generating Chunks: %d"), GeneratingChunks);
    UE_LOG(LogTemp, Log, TEXT("Average Generation Time: %.2f ms"), AvgTime);
    UE_LOG(LogTemp, Log, TEXT("P95 Generation Time: %.2f ms"), P95Time);
    UE_LOG(LogTemp, Log, TEXT("Ready: %s"), WorldGenManager->IsWorldGenReady() ? TEXT("Yes") : TEXT("No"));
    
    if (WorldGenManager->GetPlayerAnchor())
    {
        FVector PlayerPos = WorldGenManager->GetPlayerAnchor()->GetActorLocation();
        UE_LOG(LogTemp, Log, TEXT("Player Anchor: %s at (%.2f, %.2f, %.2f)"), 
               *WorldGenManager->GetPlayerAnchor()->GetName(), PlayerPos.X, PlayerPos.Y, PlayerPos.Z);
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("Player Anchor: None"));
    }
    
    UE_LOG(LogTemp, Log, TEXT("========================================"));
}

AWorldGenManager* FWorldGenConsoleCommands::FindWorldGenManager()
{
    if (!GEngine || !GEngine->GetCurrentPlayWorld())
    {
        return nullptr;
    }
    
    UWorld* World = GEngine->GetCurrentPlayWorld();
    return Cast<AWorldGenManager>(UGameplayStatics::GetActorOfClass(World, AWorldGenManager::StaticClass()));
}

// Integration test console commands
FAutoConsoleCommand FWorldGenConsoleCommands::RunIntegrationTestsCommand(
    TEXT("wg.RunIntegrationTests"),
    TEXT("Run all integration tests for world generation system"),
    FConsoleCommandWithArgsDelegate::CreateStatic(&FWorldGenConsoleCommands::RunIntegrationTests)
);

FAutoConsoleCommand FWorldGenConsoleCommands::TestBiomeTransitionsCommand(
    TEXT("wg.TestBiomeTransitions"),
    TEXT("Test biome transition smoothness at specified locations (usage: wg.TestBiomeTransitions [X Y] [radius] [samples])"),
    FConsoleCommandWithArgsDelegate::CreateStatic(&FWorldGenConsoleCommands::TestBiomeTransitions)
);

FAutoConsoleCommand FWorldGenConsoleCommands::TestPOIPortalSystemCommand(
    TEXT("wg.TestPOIPortalSystem"),
    TEXT("Test POI and portal system functionality (usage: wg.TestPOIPortalSystem [ChunkX ChunkY ChunkZ])"),
    FConsoleCommandWithArgsDelegate::CreateStatic(&FWorldGenConsoleCommands::TestPOIPortalSystem)
);

FAutoConsoleCommand FWorldGenConsoleCommands::ValidateVisualQualityCommand(
    TEXT("wg.ValidateVisualQuality"),
    TEXT("Validate visual quality through automated sampling (usage: wg.ValidateVisualQuality [X Y Z])"),
    FConsoleCommandWithArgsDelegate::CreateStatic(&FWorldGenConsoleCommands::ValidateVisualQuality)
);

void FWorldGenConsoleCommands::RunIntegrationTests(const TArray<FString>& Args)
{
    AWorldGenManager* WorldGenManager = FindWorldGenManager();
    if (!WorldGenManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("No WorldGenManager found in current world"));
        return;
    }

    if (!WorldGenManager->IsWorldGenReady())
    {
        UE_LOG(LogTemp, Warning, TEXT("WorldGenManager is not ready for testing"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("Starting comprehensive integration tests..."));

    // Find integration test controller in the level
    UWorld* World = GEngine->GetCurrentPlayWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Warning, TEXT("No valid world found"));
        return;
    }

    // Look for integration test controller
    TArray<AActor*> TestControllers;
    UGameplayStatics::GetAllActorsOfClass(World, AWorldGenIntegrationTestController::StaticClass(), TestControllers);

    AWorldGenIntegrationTestController* TestController = nullptr;
    if (TestControllers.Num() > 0)
    {
        TestController = Cast<AWorldGenIntegrationTestController>(TestControllers[0]);
    }
    else
    {
        // Spawn a temporary test controller
        TestController = World->SpawnActor<AWorldGenIntegrationTestController>();
        UE_LOG(LogTemp, Log, TEXT("Spawned temporary integration test controller"));
    }

    if (TestController)
    {
        bool bAllTestsPassed = TestController->RunAllTests();
        
        int32 PassedTests, FailedTests, TotalTests;
        TestController->GetTestResults(PassedTests, FailedTests, TotalTests);
        
        UE_LOG(LogTemp, Log, TEXT("Integration tests completed: %d passed, %d failed, %d total"), 
            PassedTests, FailedTests, TotalTests);
        
        if (bAllTestsPassed)
        {
            UE_LOG(LogTemp, Log, TEXT("✓ All integration tests PASSED"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("✗ Some integration tests FAILED"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create integration test controller"));
    }
}

void FWorldGenConsoleCommands::TestBiomeTransitions(const TArray<FString>& Args)
{
    AWorldGenManager* WorldGenManager = FindWorldGenManager();
    if (!WorldGenManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("No WorldGenManager found in current world"));
        return;
    }

    // Parse arguments
    FVector TestLocation = FVector(0, 0, 0);
    float SampleRadius = 500.0f;
    int32 SampleCount = 20;

    if (Args.Num() >= 2)
    {
        TestLocation.X = FCString::Atof(*Args[0]);
        TestLocation.Y = FCString::Atof(*Args[1]);
    }
    if (Args.Num() >= 3)
    {
        SampleRadius = FCString::Atof(*Args[2]);
    }
    if (Args.Num() >= 4)
    {
        SampleCount = FCString::Atoi(*Args[3]);
    }

    UE_LOG(LogTemp, Log, TEXT("Testing biome transitions at (%.1f, %.1f) with radius %.1f and %d samples"), 
        TestLocation.X, TestLocation.Y, SampleRadius, SampleCount);

    // Test biome transitions in a circle around the test location
    bool bAllTransitionsSmooth = true;
    float MaxBlendChange = 0.1f;

    for (int32 SampleIdx = 0; SampleIdx < SampleCount; ++SampleIdx)
    {
        float Angle = (2.0f * PI * SampleIdx) / SampleCount;
        FVector SampleLocation1 = TestLocation + FVector(
            FMath::Cos(Angle) * SampleRadius,
            FMath::Sin(Angle) * SampleRadius,
            0
        );

        float NextAngle = (2.0f * PI * (SampleIdx + 1)) / SampleCount;
        FVector SampleLocation2 = TestLocation + FVector(
            FMath::Cos(NextAngle) * SampleRadius,
            FMath::Sin(NextAngle) * SampleRadius,
            0
        );

        FBiomeEvaluation Eval1 = WorldGenManager->EvaluateBiomeAtLocation(SampleLocation1);
        FBiomeEvaluation Eval2 = WorldGenManager->EvaluateBiomeAtLocation(SampleLocation2);

        // Check blend weight smoothness
        for (int32 BiomeIdx = 0; BiomeIdx < static_cast<int32>(EBiomeType::Count); ++BiomeIdx)
        {
            float WeightChange = FMath::Abs(Eval2.BiomeWeights.Weights[BiomeIdx] - Eval1.BiomeWeights.Weights[BiomeIdx]);
            if (WeightChange > MaxBlendChange)
            {
                bAllTransitionsSmooth = false;
                UE_LOG(LogTemp, Warning, TEXT("Rough transition detected at sample %d: biome %d changed by %.3f"), 
                    SampleIdx, BiomeIdx, WeightChange);
            }
        }
    }

    if (bAllTransitionsSmooth)
    {
        UE_LOG(LogTemp, Log, TEXT("✓ All biome transitions are smooth"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("✗ Some biome transitions are rough"));
    }
}

void FWorldGenConsoleCommands::TestPOIPortalSystem(const TArray<FString>& Args)
{
    AWorldGenManager* WorldGenManager = FindWorldGenManager();
    if (!WorldGenManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("No WorldGenManager found in current world"));
        return;
    }

    // Parse chunk coordinates
    FIntVector ChunkCoord = FIntVector(0, 0, 0);
    if (Args.Num() >= 3)
    {
        ChunkCoord.X = FCString::Atoi(*Args[0]);
        ChunkCoord.Y = FCString::Atoi(*Args[1]);
        ChunkCoord.Z = FCString::Atoi(*Args[2]);
    }

    UE_LOG(LogTemp, Log, TEXT("Testing POI and portal system for chunk (%d, %d, %d)"), 
        ChunkCoord.X, ChunkCoord.Y, ChunkCoord.Z);

    // Test POI generation
    TArray<FPOIPlacementResult> POIResults = WorldGenManager->GeneratePOIsForChunk(ChunkCoord);
    TArray<FPOIInstance> ChunkPOIs = WorldGenManager->GetPOIsInChunk(ChunkCoord);
    
    UE_LOG(LogTemp, Log, TEXT("Generated %d POI placement results, found %d POI instances"), 
        POIResults.Num(), ChunkPOIs.Num());

    // Test portal generation
    TArray<FPortalPlacementResult> PortalResults = WorldGenManager->GeneratePortalsForChunk(ChunkCoord);
    TArray<FDungeonPortal> ChunkPortals = WorldGenManager->GetPortalsInChunk(ChunkCoord);
    
    UE_LOG(LogTemp, Log, TEXT("Generated %d portal placement results, found %d portal instances"), 
        PortalResults.Num(), ChunkPortals.Num());

    // Validate POI properties
    bool bPOIsValid = true;
    for (const FPOIInstance& POI : ChunkPOIs)
    {
        if (POI.WorldLocation.IsZero())
        {
            bPOIsValid = false;
            UE_LOG(LogTemp, Warning, TEXT("POI %s has invalid (zero) location"), *POI.POITypeName);
        }
    }

    // Validate portal properties
    bool bPortalsValid = true;
    for (const FDungeonPortal& Portal : ChunkPortals)
    {
        if (Portal.WorldLocation.IsZero())
        {
            bPortalsValid = false;
            UE_LOG(LogTemp, Warning, TEXT("Portal %s has invalid (zero) location"), *Portal.PortalTypeName);
        }
    }

    // Test statistics
    int32 TotalAttempts, SuccessfulPlacements, FailedPlacements;
    float AvgAttemptsPerPOI;
    WorldGenManager->GetPOIPlacementStats(TotalAttempts, SuccessfulPlacements, FailedPlacements, AvgAttemptsPerPOI);
    UE_LOG(LogTemp, Log, TEXT("POI Stats - Attempts: %d, Successful: %d, Failed: %d, Avg per POI: %.2f"), 
        TotalAttempts, SuccessfulPlacements, FailedPlacements, AvgAttemptsPerPOI);

    WorldGenManager->GetPortalPlacementStats(TotalAttempts, SuccessfulPlacements, FailedPlacements, AvgAttemptsPerPOI);
    UE_LOG(LogTemp, Log, TEXT("Portal Stats - Attempts: %d, Successful: %d, Failed: %d, Avg per Portal: %.2f"), 
        TotalAttempts, SuccessfulPlacements, FailedPlacements, AvgAttemptsPerPOI);

    if (bPOIsValid && bPortalsValid)
    {
        UE_LOG(LogTemp, Log, TEXT("✓ POI and portal system test PASSED"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("✗ POI and portal system test FAILED"));
    }
}

void FWorldGenConsoleCommands::ValidateVisualQuality(const TArray<FString>& Args)
{
    AWorldGenManager* WorldGenManager = FindWorldGenManager();
    if (!WorldGenManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("No WorldGenManager found in current world"));
        return;
    }

    // Parse test location
    FVector TestLocation = FVector(0, 0, 100);
    if (Args.Num() >= 3)
    {
        TestLocation.X = FCString::Atof(*Args[0]);
        TestLocation.Y = FCString::Atof(*Args[1]);
        TestLocation.Z = FCString::Atof(*Args[2]);
    }

    UE_LOG(LogTemp, Log, TEXT("Validating visual quality at location (%.1f, %.1f, %.1f)"), 
        TestLocation.X, TestLocation.Y, TestLocation.Z);

    // Test biome evaluation
    FBiomeEvaluation BiomeEval = WorldGenManager->EvaluateBiomeAtLocation(TestLocation);
    
    bool bQualityValid = true;

    // Check primary biome validity
    if (BiomeEval.DominantBiome == TEXT("None"))
    {
        bQualityValid = false;
        UE_LOG(LogTemp, Warning, TEXT("Primary biome is None at test location"));
    }

    // Check blend weight validity
    float TotalWeight = 0.0f;
    for (int32 BiomeIdx = 0; BiomeIdx < static_cast<int32>(EBiomeType::Count); ++BiomeIdx)
    {
        float Weight = BiomeEval.BiomeWeights.Weights[BiomeIdx];
        if (Weight < 0.0f)
        {
            bQualityValid = false;
            UE_LOG(LogTemp, Warning, TEXT("Negative blend weight for biome %d: %.3f"), BiomeIdx, Weight);
        }
        TotalWeight += Weight;
    }

    // Check that blend weights sum to approximately 1.0
    if (FMath::Abs(TotalWeight - 1.0f) > 0.1f)
    {
        bQualityValid = false;
        UE_LOG(LogTemp, Warning, TEXT("Blend weights don't sum to 1.0: %.3f"), TotalWeight);
    }

    // Log biome information
    UE_LOG(LogTemp, Log, TEXT("Primary biome: %s, Total blend weight: %.3f"), 
        *BiomeEval.DominantBiome, TotalWeight);

    if (bQualityValid)
    {
        UE_LOG(LogTemp, Log, TEXT("✓ Visual quality validation PASSED"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("✗ Visual quality validation FAILED"));
    }
}