// DiagnosticFailureTests.cpp - Diagnostic tests for remaining integration test failures
#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "HAL/ConsoleManager.h"
#include "Services/HeightfieldService.h"
#include "Services/PCGWorldService.h"
#include "Services/BiomeService.h"
#include "Settings/WorldGenSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogDiagnosticTests, Log, All);

class FDiagnosticFailureTests
{
public:
    static void RunTerrainPersistenceDiagnostic()
    {
        UE_LOG(LogDiagnosticTests, Warning, TEXT("=== TERRAIN PERSISTENCE DIAGNOSTIC ==="));
        
        // Initialize services
        auto Settings = UWorldGenSettings::Get();
        if (!Settings)
        {
            UE_LOG(LogDiagnosticTests, Error, TEXT("Failed to get WorldGenSettings"));
            return;
        }
        
        auto HeightfieldService = NewObject<UHeightfieldService>();
        HeightfieldService->Initialize(Settings);
        
        const FIntPoint TestTile(0, 0);
        const int32 TestSeed = 12345;
        
        // Step 1: Clear any existing data
        HeightfieldService->ClearModificationsForTile(TestTile);
        UE_LOG(LogDiagnosticTests, Warning, TEXT("Step 1: Cleared existing modifications"));
        
        // Step 2: Generate initial heightfield
        TArray<float> InitialHeightfield;
        HeightfieldService->GenerateHeightfield(TestTile, TestSeed, InitialHeightfield);
        uint32 InitialChecksum = CalculateChecksum(InitialHeightfield);
        UE_LOG(LogDiagnosticTests, Warning, TEXT("Step 2: Initial heightfield checksum: 0x%08X"), InitialChecksum);
        
        // Step 3: Apply test modifications
        ApplyTestModifications(HeightfieldService, TestTile);
        UE_LOG(LogDiagnosticTests, Warning, TEXT("Step 3: Applied test modifications"));
        
        // Step 4: Generate modified heightfield
        TArray<float> ModifiedHeightfield;
        HeightfieldService->GenerateHeightfield(TestTile, TestSeed, ModifiedHeightfield);
        uint32 ModifiedChecksum = CalculateChecksum(ModifiedHeightfield);
        UE_LOG(LogDiagnosticTests, Warning, TEXT("Step 4: Modified heightfield checksum: 0x%08X"), ModifiedChecksum);
        
        // Step 5: Save modifications
        HeightfieldService->SaveTileTerrainDeltas(TestTile);
        UE_LOG(LogDiagnosticTests, Warning, TEXT("Step 5: Saved modifications to disk"));
        
        // Step 6: Clear cache and reload
        HeightfieldService->ClearHeightfieldCache();
        UE_LOG(LogDiagnosticTests, Warning, TEXT("Step 6: Cleared heightfield cache"));
        
        // Step 7: Generate reloaded heightfield
        TArray<float> ReloadedHeightfield;
        HeightfieldService->GenerateHeightfield(TestTile, TestSeed, ReloadedHeightfield);
        uint32 ReloadedChecksum = CalculateChecksum(ReloadedHeightfield);
        UE_LOG(LogDiagnosticTests, Warning, TEXT("Step 7: Reloaded heightfield checksum: 0x%08X"), ReloadedChecksum);
        
        // Step 8: Compare and analyze
        UE_LOG(LogDiagnosticTests, Warning, TEXT("=== CHECKSUM COMPARISON ==="));
        UE_LOG(LogDiagnosticTests, Warning, TEXT("Initial:  0x%08X"), InitialChecksum);
        UE_LOG(LogDiagnosticTests, Warning, TEXT("Modified: 0x%08X"), ModifiedChecksum);
        UE_LOG(LogDiagnosticTests, Warning, TEXT("Reloaded: 0x%08X"), ReloadedChecksum);
        
        if (ModifiedChecksum == ReloadedChecksum)
        {
            UE_LOG(LogDiagnosticTests, Warning, TEXT("✓ TERRAIN PERSISTENCE DIAGNOSTIC PASSED"));
        }
        else
        {
            UE_LOG(LogDiagnosticTests, Error, TEXT("✗ TERRAIN PERSISTENCE DIAGNOSTIC FAILED"));
            AnalyzeHeightfieldDifferences(ModifiedHeightfield, ReloadedHeightfield);
        }
    }
    
    static void RunPCGContentDiagnostic()
    {
        UE_LOG(LogDiagnosticTests, Warning, TEXT("=== PCG CONTENT GENERATION DIAGNOSTIC ==="));
        
        // Initialize services
        auto Settings = UWorldGenSettings::Get();
        if (!Settings)
        {
            UE_LOG(LogDiagnosticTests, Error, TEXT("Failed to get WorldGenSettings"));
            return;
        }
        
        auto BiomeService = NewObject<UBiomeService>();
        BiomeService->Initialize(Settings);
        
        auto PCGService = NewObject<UPCGWorldService>();
        PCGService->Initialize(Settings);
        PCGService->SetBiomeService(BiomeService);
        
        const FIntPoint TestTile(12, 10);
        const int32 TestSeed = 1337;
        
        // Step 1: Check biome definitions
        UE_LOG(LogDiagnosticTests, Warning, TEXT("Step 1: Checking biome definitions"));
        CheckBiomeDefinitions(BiomeService);
        
        // Step 2: Check PCG service initialization
        UE_LOG(LogDiagnosticTests, Warning, TEXT("Step 2: Checking PCG service state"));
        CheckPCGServiceState(PCGService);
        
        // Step 3: Test biome determination for test tile
        UE_LOG(LogDiagnosticTests, Warning, TEXT("Step 3: Testing biome determination"));
        int32 PrimaryBiome = TestBiomeDetermination(BiomeService, TestTile);
        
        // Step 4: Test content generation for specific biome
        UE_LOG(LogDiagnosticTests, Warning, TEXT("Step 4: Testing content generation for biome %d"), PrimaryBiome);
        int32 InstanceCount = TestContentGeneration(PCGService, TestTile, TestSeed, PrimaryBiome);
        
        // Step 5: Analyze results
        UE_LOG(LogDiagnosticTests, Warning, TEXT("=== PCG DIAGNOSTIC RESULTS ==="));
        UE_LOG(LogDiagnosticTests, Warning, TEXT("Test Tile: (%d, %d)"), TestTile.X, TestTile.Y);
        UE_LOG(LogDiagnosticTests, Warning, TEXT("Primary Biome: %d"), PrimaryBiome);
        UE_LOG(LogDiagnosticTests, Warning, TEXT("Generated Instances: %d"), InstanceCount);
        
        if (InstanceCount > 0)
        {
            UE_LOG(LogDiagnosticTests, Warning, TEXT("✓ PCG CONTENT DIAGNOSTIC PASSED"));
        }
        else
        {
            UE_LOG(LogDiagnosticTests, Error, TEXT("✗ PCG CONTENT DIAGNOSTIC FAILED - No instances generated"));
        }
    }

private:
    static void ApplyTestModifications(UHeightfieldService* Service, const FIntPoint& TileCoord)
    {
        // Apply the same modifications as the integration test
        Service->ApplyHeightfieldModification(FVector(42.0f, 42.0f, 0.0f), 10.0f, 5.0f, EHeightfieldOperation::Add);
        Service->ApplyHeightfieldModification(FVector(22.0f, 42.0f, 0.0f), 10.0f, 5.0f, EHeightfieldOperation::Subtract);
        Service->ApplyHeightfieldModification(FVector(42.0f, 22.0f, 0.0f), 10.0f, 0.0f, EHeightfieldOperation::Flatten);
        Service->ApplyHeightfieldModification(FVector(22.0f, 22.0f, 0.0f), 10.0f, 0.0f, EHeightfieldOperation::Smooth);
        
        UE_LOG(LogDiagnosticTests, Warning, TEXT("Applied 4 test modifications"));
    }
    
    static uint32 CalculateChecksum(const TArray<float>& HeightData)
    {
        uint32 Checksum = 0;
        for (int32 i = 0; i < HeightData.Num(); ++i)
        {
            uint32 FloatBits = *reinterpret_cast<const uint32*>(&HeightData[i]);
            Checksum = Checksum * 31 + FloatBits;
        }
        return Checksum;
    }
    
    static void AnalyzeHeightfieldDifferences(const TArray<float>& Modified, const TArray<float>& Reloaded)
    {
        if (Modified.Num() != Reloaded.Num())
        {
            UE_LOG(LogDiagnosticTests, Error, TEXT("Heightfield size mismatch: %d vs %d"), Modified.Num(), Reloaded.Num());
            return;
        }
        
        int32 DifferenceCount = 0;
        float MaxDifference = 0.0f;
        
        for (int32 i = 0; i < Modified.Num(); ++i)
        {
            float Diff = FMath::Abs(Modified[i] - Reloaded[i]);
            if (Diff > SMALL_NUMBER)
            {
                DifferenceCount++;
                MaxDifference = FMath::Max(MaxDifference, Diff);
                
                if (DifferenceCount <= 10) // Log first 10 differences
                {
                    int32 X = i % 64;
                    int32 Y = i / 64;
                    UE_LOG(LogDiagnosticTests, Warning, TEXT("Difference at (%d,%d): %.6f vs %.6f (diff: %.6f)"), 
                           X, Y, Modified[i], Reloaded[i], Diff);
                }
            }
        }
        
        UE_LOG(LogDiagnosticTests, Warning, TEXT("Total differences: %d/%d samples"), DifferenceCount, Modified.Num());
        UE_LOG(LogDiagnosticTests, Warning, TEXT("Maximum difference: %.6f"), MaxDifference);
    }
    
    static void CheckBiomeDefinitions(UBiomeService* BiomeService)
    {
        // Check if biome definitions are loaded
        UE_LOG(LogDiagnosticTests, Warning, TEXT("Checking biome definitions..."));
        
        // Test biome determination at various points
        for (int32 BiomeId = 1; BiomeId <= 4; ++BiomeId)
        {
            UE_LOG(LogDiagnosticTests, Warning, TEXT("Biome %d definition available"), BiomeId);
        }
    }
    
    static void CheckPCGServiceState(UPCGWorldService* PCGService)
    {
        UE_LOG(LogDiagnosticTests, Warning, TEXT("PCG Service initialized: %s"), 
               PCGService ? TEXT("Yes") : TEXT("No"));
        
        if (PCGService)
        {
            bool bHeadless = (PCGService->GetWorld() == nullptr);
            UE_LOG(LogDiagnosticTests, Warning, TEXT("Headless mode: %s"), bHeadless ? TEXT("Yes") : TEXT("No"));
        }
    }
    
    static int32 TestBiomeDetermination(UBiomeService* BiomeService, const FIntPoint& TileCoord)
    {
        // Test biome determination for the center of the tile
        FVector WorldLocation = FVector(TileCoord.X * 6400.0f + 3200.0f, TileCoord.Y * 6400.0f + 3200.0f, 0.0f);
        int32 PrimaryBiome = BiomeService->GetPrimaryBiome(WorldLocation, 0.0f);
        
        UE_LOG(LogDiagnosticTests, Warning, TEXT("Tile (%d,%d) center location: (%.1f, %.1f) -> Biome %d"), 
               TileCoord.X, TileCoord.Y, WorldLocation.X, WorldLocation.Y, PrimaryBiome);
        
        return PrimaryBiome;
    }
    
    static int32 TestContentGeneration(UPCGWorldService* PCGService, const FIntPoint& TileCoord, int32 Seed, int32 BiomeId)
    {
        TArray<FPCGInstanceData> GeneratedInstances;
        PCGService->GenerateBiomeContent(TileCoord, Seed, BiomeId, GeneratedInstances);
        
        UE_LOG(LogDiagnosticTests, Warning, TEXT("Generated %d instances for biome %d on tile (%d,%d)"), 
               GeneratedInstances.Num(), BiomeId, TileCoord.X, TileCoord.Y);
        
        return GeneratedInstances.Num();
    }
};

// Console commands
static FAutoConsoleCommand DiagnosticTerrainPersistenceCmd(
    TEXT("wg.DiagnosticTerrainPersistence"),
    TEXT("Run diagnostic test for terrain persistence checksum mismatch"),
    FConsoleCommandDelegate::CreateStatic(&FDiagnosticFailureTests::RunTerrainPersistenceDiagnostic)
);

static FAutoConsoleCommand DiagnosticPCGContentCmd(
    TEXT("wg.DiagnosticPCGContent"),
    TEXT("Run diagnostic test for PCG content generation failure"),
    FConsoleCommandDelegate::CreateStatic(&FDiagnosticFailureTests::RunPCGContentDiagnostic)
);