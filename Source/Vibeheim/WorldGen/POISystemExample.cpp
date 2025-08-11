#include "POISystemExample.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

APOISystemExample::APOISystemExample()
{
    PrimaryActorTick.bCanEverTick = false;
    
    // Initialize default settings
    WorldGenSettings.Seed = 12345;
    WorldGenSettings.ChunkSize = 32;
    WorldGenSettings.VoxelSizeCm = 50.0f;
    WorldGenSettings.WorldGenVersion = 1;
    
    bIsInitialized = false;
}

void APOISystemExample::BeginPlay()
{
    Super::BeginPlay();
    
    UE_LOG(LogPOISystem, Log, TEXT("POISystemExample BeginPlay - Initializing systems"));
    
    // Create and initialize systems
    NoiseGenerator = MakeUnique<FNoiseGenerator>();
    NoiseGenerator->Initialize(WorldGenSettings);
    
    BiomeSystem = MakeUnique<FBiomeSystem>();
    BiomeSystem->Initialize(WorldGenSettings);
    
    POISystem = MakeUnique<FPOISystem>();
    POISystem->Initialize(WorldGenSettings, NoiseGenerator.Get(), BiomeSystem.Get());
    
    bIsInitialized = true;
    
    UE_LOG(LogPOISystem, Log, TEXT("POISystemExample initialized successfully"));
    
    // Generate some test POIs automatically
    GenerateTestPOIs(0, 0, 0);
    GenerateTestPOIs(1, 0, 0);
    GenerateTestPOIs(0, 1, 0);
}

void APOISystemExample::GenerateTestPOIs(int32 ChunkX, int32 ChunkY, int32 ChunkZ)
{
    if (!bIsInitialized || !POISystem.IsValid())
    {
        UE_LOG(LogPOISystem, Warning, TEXT("POI system not initialized - cannot generate test POIs"));
        return;
    }
    
    FIntVector ChunkCoordinate(ChunkX, ChunkY, ChunkZ);
    
    UE_LOG(LogPOISystem, Log, TEXT("Generating test POIs for chunk %s"), *ChunkCoordinate.ToString());
    
    TArray<FPOIPlacementResult> Results = POISystem->GeneratePOIsForChunk(ChunkCoordinate, GetWorld());
    
    UE_LOG(LogPOISystem, Log, TEXT("Generated %d POI placement results for chunk %s"), 
           Results.Num(), *ChunkCoordinate.ToString());
    
    // Log results
    for (int32 i = 0; i < Results.Num(); ++i)
    {
        const FPOIPlacementResult& Result = Results[i];
        if (Result.bSuccess)
        {
            UE_LOG(LogPOISystem, Log, TEXT("  [%d] SUCCESS: %s at %s (attempts: %d)"), 
                   i, *Result.POIInstance.POITypeName, *Result.POIInstance.WorldLocation.ToString(), Result.AttemptsUsed);
        }
        else
        {
            UE_LOG(LogPOISystem, Log, TEXT("  [%d] FAILED: %s (attempts: %d, reason: %s)"), 
                   i, *Result.POIInstance.POITypeName, Result.AttemptsUsed, *Result.FailureReason);
        }
    }
    
    // Log current statistics
    int32 TotalAttempts, Successful, Failed;
    float AverageAttempts;
    GetPlacementStats(TotalAttempts, Successful, Failed, AverageAttempts);
    
    UE_LOG(LogPOISystem, Log, TEXT("Current stats - Total: %d, Success: %d, Failed: %d, Avg: %.2f"), 
           TotalAttempts, Successful, Failed, AverageAttempts);
}

void APOISystemExample::ClearAllPOIs()
{
    if (!bIsInitialized || !POISystem.IsValid())
    {
        UE_LOG(LogPOISystem, Warning, TEXT("POI system not initialized - cannot clear POIs"));
        return;
    }
    
    POISystem->Reset();
    UE_LOG(LogPOISystem, Log, TEXT("All POIs cleared"));
}

void APOISystemExample::AddCustomPOIRule(const FString& POITypeName, float MinSpacing, 
                                        float SpawnProbability, const FString& AllowedBiome)
{
    if (!bIsInitialized || !POISystem.IsValid())
    {
        UE_LOG(LogPOISystem, Warning, TEXT("POI system not initialized - cannot add custom rule"));
        return;
    }
    
    FPOISpawnRule CustomRule;
    CustomRule.POITypeName = POITypeName;
    CustomRule.MinSpacing = MinSpacing;
    CustomRule.SpawnProbability = SpawnProbability;
    CustomRule.MaxSlope = 20.0f;
    CustomRule.MinAltitude = -50.0f;
    CustomRule.MaxAltitude = 200.0f;
    CustomRule.MinWaterlineClearance = 5.0f;
    CustomRule.AllowedBiomes.Add(AllowedBiome);
    CustomRule.FlattenRadius = 10.0f;
    CustomRule.MaxRetryAttempts = 5;
    
    POISystem->AddPOISpawnRule(CustomRule);
    
    UE_LOG(LogPOISystem, Log, TEXT("Added custom POI rule: %s (spacing: %.1f, probability: %.2f, biome: %s)"), 
           *POITypeName, MinSpacing, SpawnProbability, *AllowedBiome);
}

void APOISystemExample::GetPlacementStats(int32& OutTotalAttempts, int32& OutSuccessful, 
                                         int32& OutFailed, float& OutAverageAttempts)
{
    if (!bIsInitialized || !POISystem.IsValid())
    {
        OutTotalAttempts = OutSuccessful = OutFailed = 0;
        OutAverageAttempts = 0.0f;
        return;
    }
    
    POISystem->GetPlacementStats(OutTotalAttempts, OutSuccessful, OutFailed, OutAverageAttempts);
}

bool APOISystemExample::TestPOILocationValidation(const FVector& TestLocation, const FString& POITypeName)
{
    if (!bIsInitialized || !POISystem.IsValid())
    {
        UE_LOG(LogPOISystem, Warning, TEXT("POI system not initialized - cannot test location validation"));
        return false;
    }
    
    // Find the spawn rule for this POI type
    TArray<FPOISpawnRule> SpawnRules = POISystem->GetPOISpawnRules();
    for (const FPOISpawnRule& Rule : SpawnRules)
    {
        if (Rule.POITypeName == POITypeName)
        {
            FString FailureReason;
            bool bIsValid = POISystem->IsValidPOILocation(TestLocation, Rule, FailureReason);
            
            if (bIsValid)
            {
                UE_LOG(LogPOISystem, Log, TEXT("Location %s is VALID for POI type %s"), 
                       *TestLocation.ToString(), *POITypeName);
            }
            else
            {
                UE_LOG(LogPOISystem, Log, TEXT("Location %s is INVALID for POI type %s: %s"), 
                       *TestLocation.ToString(), *POITypeName, *FailureReason);
            }
            
            return bIsValid;
        }
    }
    
    UE_LOG(LogPOISystem, Warning, TEXT("POI type %s not found in spawn rules"), *POITypeName);
    return false;
}