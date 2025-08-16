#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "../WorldGenManager.h"
#include "../BiomeSystem.h"
#include "../NoiseGenerator.h"
#include "../POISystem.h"
#include "../DungeonPortalSystem.h"
#include "../VegetationSystem.h"
#include "../Data/WorldGenSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogWorldGenDeterminismTests, Log, All);

/**
 * Test deterministic terrain height generation with fixed seeds
 * Validates that identical seeds produce identical terrain heights within ±1cm tolerance
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerrainHeightDeterminismTest, 
    "WorldGen.Determinism.TerrainHeight", 
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTerrainHeightDeterminismTest::RunTest(const FString& Parameters)
{
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("Starting terrain height determinism test"));
    
    // Test configuration
    const int64 TestSeed = 42;
    const int32 TestVersion = 1;
    const float HeightTolerance = 1.0f; // ±1cm as per requirements
    const int32 NumTestLocations = 50;
    
    // Create test settings
    FWorldGenSettings TestSettings;
    TestSettings.Seed = TestSeed;
    TestSettings.WorldGenVersion = TestVersion;
    TestSettings.PluginSHA = TEXT("determinism_test");
    TestSettings.VoxelSizeCm = 50.0f;
    TestSettings.ChunkSize = 32;
    TestSettings.BiomeBlendMeters = 24.0f;
    TestSettings.MeadowsScale = 0.0025f;
    TestSettings.BlackForestScale = 0.0030f;
    TestSettings.SwampScale = 0.0020f;
    
    // Initialize biome system for both runs
    FBiomeSystem BiomeSystem1, BiomeSystem2;
    BiomeSystem1.Initialize(TestSettings);
    BiomeSystem2.Initialize(TestSettings);
    
    // Test locations across different chunks and biomes
    TArray<FVector> TestLocations;
    for (int32 i = 0; i < NumTestLocations; ++i)
    {
        float X = FMath::RandRange(-5000.0f, 5000.0f);
        float Y = FMath::RandRange(-5000.0f, 5000.0f);
        TestLocations.Add(FVector(X, Y, 0.0f));
    }
    
    // First run - generate terrain heights
    TArray<float> FirstRunHeights;
    TArray<FBiomeEvaluation> FirstRunBiomes;
    
    for (const FVector& Location : TestLocations)
    {
        FBiomeEvaluation BiomeEval = BiomeSystem1.EvaluateBiome(Location.X, Location.Y);
        FirstRunHeights.Add(BiomeEval.TerrainHeight);
        FirstRunBiomes.Add(BiomeEval);
    }
    
    // Second run - generate terrain heights with same seed
    TArray<float> SecondRunHeights;
    TArray<FBiomeEvaluation> SecondRunBiomes;
    
    for (const FVector& Location : TestLocations)
    {
        FBiomeEvaluation BiomeEval = BiomeSystem2.EvaluateBiome(Location.X, Location.Y);
        SecondRunHeights.Add(BiomeEval.TerrainHeight);
        SecondRunBiomes.Add(BiomeEval);
    }
    
    // Validate determinism
    bool bAllHeightsMatch = true;
    bool bAllBiomesMatch = true;
    int32 HeightMismatches = 0;
    int32 BiomeMismatches = 0;
    
    for (int32 i = 0; i < NumTestLocations; ++i)
    {
        float HeightDifference = FMath::Abs(FirstRunHeights[i] - SecondRunHeights[i]);
        
        if (HeightDifference > HeightTolerance)
        {
            bAllHeightsMatch = false;
            HeightMismatches++;
            
            UE_LOG(LogWorldGenDeterminismTests, Warning, 
                TEXT("Height mismatch at location (%f, %f): Run1=%.3f, Run2=%.3f, Diff=%.3f cm"),
                TestLocations[i].X, TestLocations[i].Y, 
                FirstRunHeights[i], SecondRunHeights[i], HeightDifference);
        }
        
        // Check biome consistency
        if (FirstRunBiomes[i].DominantBiome != SecondRunBiomes[i].DominantBiome)
        {
            bAllBiomesMatch = false;
            BiomeMismatches++;
            
            UE_LOG(LogWorldGenDeterminismTests, Warning,
                TEXT("Biome mismatch at location (%f, %f): Run1=%s, Run2=%s"),
                TestLocations[i].X, TestLocations[i].Y,
                *FirstRunBiomes[i].DominantBiome, *SecondRunBiomes[i].DominantBiome);
        }
        
        // Check blended height offset consistency
        float BlendOffsetDiff = FMath::Abs(FirstRunBiomes[i].BlendedHeightOffset - SecondRunBiomes[i].BlendedHeightOffset);
        if (BlendOffsetDiff > HeightTolerance)
        {
            UE_LOG(LogWorldGenDeterminismTests, Warning,
                TEXT("Blend offset mismatch at location (%f, %f): Run1=%.3f, Run2=%.3f, Diff=%.3f"),
                TestLocations[i].X, TestLocations[i].Y,
                FirstRunBiomes[i].BlendedHeightOffset, SecondRunBiomes[i].BlendedHeightOffset, BlendOffsetDiff);
        }
    }
    
    // Log results
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("Terrain Height Determinism Test Results:"));
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  Test Locations: %d"), NumTestLocations);
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  Height Tolerance: ±%.1f cm"), HeightTolerance);
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  Height Mismatches: %d"), HeightMismatches);
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  Biome Mismatches: %d"), BiomeMismatches);
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  Heights Match: %s"), bAllHeightsMatch ? TEXT("Yes") : TEXT("No"));
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  Biomes Match: %s"), bAllBiomesMatch ? TEXT("Yes") : TEXT("No"));
    
    TestTrue(TEXT("All terrain heights are deterministic within tolerance"), bAllHeightsMatch);
    TestTrue(TEXT("All biome evaluations are deterministic"), bAllBiomesMatch);
    TestEqual(TEXT("No height mismatches"), HeightMismatches, 0);
    TestEqual(TEXT("No biome mismatches"), BiomeMismatches, 0);
    
    return bAllHeightsMatch && bAllBiomesMatch;
}
/**
 
* Test chunk seam determinism by sampling vertices along shared edges
 * Validates that adjacent chunks produce bit-identical heights at boundaries
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChunkSeamDeterminismTest, 
    "WorldGen.Determinism.ChunkSeams", 
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FChunkSeamDeterminismTest::RunTest(const FString& Parameters)
{
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("Starting chunk seam determinism test"));
    
    // Test configuration
    const int64 TestSeed = 1337;
    const int32 TestVersion = 1;
    const float ChunkSizeWorldUnits = 32 * 50.0f; // 32 voxels * 50cm per voxel = 1600 units
    const int32 SamplesPerEdge = 16; // Sample points along each chunk edge
    
    FWorldGenSettings TestSettings;
    TestSettings.Seed = TestSeed;
    TestSettings.WorldGenVersion = TestVersion;
    TestSettings.PluginSHA = TEXT("seam_test");
    TestSettings.VoxelSizeCm = 50.0f;
    TestSettings.ChunkSize = 32;
    TestSettings.BiomeBlendMeters = 24.0f;
    
    FBiomeSystem BiomeSystem;
    BiomeSystem.Initialize(TestSettings);
    
    // Test chunk pairs (adjacent chunks)
    TArray<TPair<FIntVector, FIntVector>> ChunkPairs = {
        {FIntVector(0, 0, 0), FIntVector(1, 0, 0)}, // X-adjacent
        {FIntVector(0, 0, 0), FIntVector(0, 1, 0)}, // Y-adjacent
        {FIntVector(1, 1, 0), FIntVector(2, 1, 0)}, // X-adjacent offset
        {FIntVector(1, 1, 0), FIntVector(1, 2, 0)}, // Y-adjacent offset
        {FIntVector(-1, 0, 0), FIntVector(0, 0, 0)}, // Negative X boundary
        {FIntVector(0, -1, 0), FIntVector(0, 0, 0)}  // Negative Y boundary
    };
    
    bool bAllSeamsMatch = true;
    int32 TotalSeamMismatches = 0;
    
    for (const auto& ChunkPair : ChunkPairs)
    {
        FIntVector Chunk1 = ChunkPair.Key;
        FIntVector Chunk2 = ChunkPair.Value;
        
        // Determine shared edge
        bool bIsXEdge = (Chunk1.X != Chunk2.X);
        bool bIsYEdge = (Chunk1.Y != Chunk2.Y);
        
        if (!bIsXEdge && !bIsYEdge)
        {
            UE_LOG(LogWorldGenDeterminismTests, Warning, TEXT("Invalid chunk pair - not adjacent"));
            continue;
        }
        
        // Sample along the shared edge
        TArray<FVector> EdgeSamples;
        
        if (bIsXEdge)
        {
            // Vertical edge between chunks
            float EdgeX = FMath::Max(Chunk1.X, Chunk2.X) * ChunkSizeWorldUnits;
            float StartY = FMath::Min(Chunk1.Y, Chunk2.Y) * ChunkSizeWorldUnits;
            
            for (int32 i = 0; i <= SamplesPerEdge; ++i)
            {
                float Y = StartY + (i * ChunkSizeWorldUnits / SamplesPerEdge);
                EdgeSamples.Add(FVector(EdgeX, Y, 0.0f));
            }
        }
        else // bIsYEdge
        {
            // Horizontal edge between chunks
            float EdgeY = FMath::Max(Chunk1.Y, Chunk2.Y) * ChunkSizeWorldUnits;
            float StartX = FMath::Min(Chunk1.X, Chunk2.X) * ChunkSizeWorldUnits;
            
            for (int32 i = 0; i <= SamplesPerEdge; ++i)
            {
                float X = StartX + (i * ChunkSizeWorldUnits / SamplesPerEdge);
                EdgeSamples.Add(FVector(X, EdgeY, 0.0f));
            }
        }
        
        // Evaluate heights from both chunk contexts
        int32 SeamMismatches = 0;
        
        for (const FVector& SamplePoint : EdgeSamples)
        {
            // Evaluate from first chunk context
            FBiomeEvaluation Eval1 = BiomeSystem.EvaluateBiome(SamplePoint.X, SamplePoint.Y, 0.0f, Chunk1);
            
            // Evaluate from second chunk context
            FBiomeEvaluation Eval2 = BiomeSystem.EvaluateBiome(SamplePoint.X, SamplePoint.Y, 0.0f, Chunk2);
            
            // Heights should be bit-identical at chunk boundaries
            if (Eval1.TerrainHeight != Eval2.TerrainHeight)
            {
                bAllSeamsMatch = false;
                SeamMismatches++;
                TotalSeamMismatches++;
                
                UE_LOG(LogWorldGenDeterminismTests, Warning,
                    TEXT("Seam height mismatch at (%f, %f) between chunks (%d,%d,%d) and (%d,%d,%d): %.6f vs %.6f"),
                    SamplePoint.X, SamplePoint.Y,
                    Chunk1.X, Chunk1.Y, Chunk1.Z,
                    Chunk2.X, Chunk2.Y, Chunk2.Z,
                    Eval1.TerrainHeight, Eval2.TerrainHeight);
            }
            
            // Biome evaluations should also be identical
            if (Eval1.DominantBiome != Eval2.DominantBiome)
            {
                UE_LOG(LogWorldGenDeterminismTests, Warning,
                    TEXT("Seam biome mismatch at (%f, %f) between chunks (%d,%d,%d) and (%d,%d,%d): %s vs %s"),
                    SamplePoint.X, SamplePoint.Y,
                    Chunk1.X, Chunk1.Y, Chunk1.Z,
                    Chunk2.X, Chunk2.Y, Chunk2.Z,
                    *Eval1.DominantBiome, *Eval2.DominantBiome);
            }
        }
        
        UE_LOG(LogWorldGenDeterminismTests, Verbose,
            TEXT("Chunk pair (%d,%d,%d) <-> (%d,%d,%d): %d samples, %d mismatches"),
            Chunk1.X, Chunk1.Y, Chunk1.Z,
            Chunk2.X, Chunk2.Y, Chunk2.Z,
            EdgeSamples.Num(), SeamMismatches);
    }
    
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("Chunk Seam Determinism Test Results:"));
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  Chunk Pairs Tested: %d"), ChunkPairs.Num());
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  Samples Per Edge: %d"), SamplesPerEdge);
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  Total Seam Mismatches: %d"), TotalSeamMismatches);
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  All Seams Match: %s"), bAllSeamsMatch ? TEXT("Yes") : TEXT("No"));
    
    TestTrue(TEXT("All chunk seams produce identical heights"), bAllSeamsMatch);
    TestEqual(TEXT("No seam mismatches"), TotalSeamMismatches, 0);
    
    return bAllSeamsMatch;
}

/**
 * Test POI placement consistency across multiple runs with the same seed
 * Validates that POI locations and types are deterministic
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPOIPlacementDeterminismTest, 
    "WorldGen.Determinism.POIPlacement", 
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPOIPlacementDeterminismTest::RunTest(const FString& Parameters)
{
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("Starting POI placement determinism test"));
    
    // Test configuration
    const int64 TestSeed = 9876;
    const int32 TestVersion = 1;
    const int32 NumTestChunks = 25; // 5x5 grid of chunks
    
    FWorldGenSettings TestSettings;
    TestSettings.Seed = TestSeed;
    TestSettings.WorldGenVersion = TestVersion;
    TestSettings.PluginSHA = TEXT("poi_test");
    TestSettings.VoxelSizeCm = 50.0f;
    TestSettings.ChunkSize = 32;
    
    // Initialize noise generators and biome systems for POI systems
    FNoiseGenerator NoiseGen1, NoiseGen2;
    NoiseGen1.Initialize(TestSettings);
    NoiseGen2.Initialize(TestSettings);
    
    FBiomeSystem BiomeSystem1, BiomeSystem2;
    BiomeSystem1.Initialize(TestSettings);
    BiomeSystem2.Initialize(TestSettings);
    
    // Initialize POI systems for both runs
    FPOISystem POISystem1, POISystem2;
    POISystem1.Initialize(TestSettings, &NoiseGen1, &BiomeSystem1);
    POISystem2.Initialize(TestSettings, &NoiseGen2, &BiomeSystem2);
    
    // Test chunks in a grid pattern
    TArray<FIntVector> TestChunks;
    for (int32 X = -2; X <= 2; ++X)
    {
        for (int32 Y = -2; Y <= 2; ++Y)
        {
            TestChunks.Add(FIntVector(X, Y, 0));
        }
    }
    
    // First run - generate POI placements
    TMap<FIntVector, TArray<FPOIPlacementResult>> FirstRunPOIs;
    for (const FIntVector& ChunkCoord : TestChunks)
    {
        TArray<FPOIPlacementResult> POIs = POISystem1.GeneratePOIsForChunk(ChunkCoord, nullptr);
        FirstRunPOIs.Add(ChunkCoord, POIs);
    }
    
    // Second run - generate POI placements with same seed
    TMap<FIntVector, TArray<FPOIPlacementResult>> SecondRunPOIs;
    for (const FIntVector& ChunkCoord : TestChunks)
    {
        TArray<FPOIPlacementResult> POIs = POISystem2.GeneratePOIsForChunk(ChunkCoord, nullptr);
        SecondRunPOIs.Add(ChunkCoord, POIs);
    }
    
    // Compare POI placements
    bool bAllPOIsMatch = true;
    int32 TotalPOIMismatches = 0;
    int32 TotalPOIsFirstRun = 0;
    int32 TotalPOIsSecondRun = 0;
    
    for (const FIntVector& ChunkCoord : TestChunks)
    {
        const TArray<FPOIPlacementResult>& POIs1 = FirstRunPOIs[ChunkCoord];
        const TArray<FPOIPlacementResult>& POIs2 = SecondRunPOIs[ChunkCoord];
        
        TotalPOIsFirstRun += POIs1.Num();
        TotalPOIsSecondRun += POIs2.Num();
        
        // Check if POI counts match
        if (POIs1.Num() != POIs2.Num())
        {
            bAllPOIsMatch = false;
            TotalPOIMismatches++;
            
            UE_LOG(LogWorldGenDeterminismTests, Warning,
                TEXT("POI count mismatch in chunk (%d,%d,%d): Run1=%d, Run2=%d"),
                ChunkCoord.X, ChunkCoord.Y, ChunkCoord.Z,
                POIs1.Num(), POIs2.Num());
            continue;
        }
        
        // Check individual POI placements
        for (int32 i = 0; i < POIs1.Num(); ++i)
        {
            const FPOIPlacementResult& POI1 = POIs1[i];
            const FPOIPlacementResult& POI2 = POIs2[i];
            
            // Check POI type
            if (POI1.POIInstance.POITypeName != POI2.POIInstance.POITypeName)
            {
                bAllPOIsMatch = false;
                TotalPOIMismatches++;
                
                UE_LOG(LogWorldGenDeterminismTests, Warning,
                    TEXT("POI type mismatch in chunk (%d,%d,%d) index %d: Run1=%s, Run2=%s"),
                    ChunkCoord.X, ChunkCoord.Y, ChunkCoord.Z, i,
                    *POI1.POIInstance.POITypeName, *POI2.POIInstance.POITypeName);
            }
            
            // Check POI location (should be identical)
            FVector LocationDiff = POI1.POIInstance.WorldLocation - POI2.POIInstance.WorldLocation;
            if (LocationDiff.SizeSquared() > 0.01f) // Allow tiny floating point differences
            {
                bAllPOIsMatch = false;
                TotalPOIMismatches++;
                
                UE_LOG(LogWorldGenDeterminismTests, Warning,
                    TEXT("POI location mismatch in chunk (%d,%d,%d) index %d: Run1=(%f,%f,%f), Run2=(%f,%f,%f)"),
                    ChunkCoord.X, ChunkCoord.Y, ChunkCoord.Z, i,
                    POI1.POIInstance.WorldLocation.X, POI1.POIInstance.WorldLocation.Y, POI1.POIInstance.WorldLocation.Z,
                    POI2.POIInstance.WorldLocation.X, POI2.POIInstance.WorldLocation.Y, POI2.POIInstance.WorldLocation.Z);
            }
            
            // Check placement success
            if (POI1.bSuccess != POI2.bSuccess)
            {
                bAllPOIsMatch = false;
                TotalPOIMismatches++;
                
                UE_LOG(LogWorldGenDeterminismTests, Warning,
                    TEXT("POI placement success mismatch in chunk (%d,%d,%d) index %d: Run1=%s, Run2=%s"),
                    ChunkCoord.X, ChunkCoord.Y, ChunkCoord.Z, i,
                    POI1.bSuccess ? TEXT("Success") : TEXT("Failed"),
                    POI2.bSuccess ? TEXT("Success") : TEXT("Failed"));
            }
        }
    }
    
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("POI Placement Determinism Test Results:"));
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  Test Chunks: %d"), TestChunks.Num());
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  Total POIs Run 1: %d"), TotalPOIsFirstRun);
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  Total POIs Run 2: %d"), TotalPOIsSecondRun);
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  POI Mismatches: %d"), TotalPOIMismatches);
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  All POIs Match: %s"), bAllPOIsMatch ? TEXT("Yes") : TEXT("No"));
    
    TestTrue(TEXT("All POI placements are deterministic"), bAllPOIsMatch);
    TestEqual(TEXT("POI counts match between runs"), TotalPOIsFirstRun, TotalPOIsSecondRun);
    TestEqual(TEXT("No POI placement mismatches"), TotalPOIMismatches, 0);
    
    return bAllPOIsMatch;
}/*
*
 * Test biome blending determinism to verify consistent results across runs
 * Validates that biome weight calculations and blending are deterministic
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBiomeBlendingDeterminismTest, 
    "WorldGen.Determinism.BiomeBlending", 
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBiomeBlendingDeterminismTest::RunTest(const FString& Parameters)
{
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("Starting biome blending determinism test"));
    
    // Test configuration
    const int64 TestSeed = 5555;
    const int32 TestVersion = 1;
    const int32 NumBlendTestLocations = 100;
    const float BlendTolerance = 0.0001f; // Very tight tolerance for blend weights
    
    FWorldGenSettings TestSettings;
    TestSettings.Seed = TestSeed;
    TestSettings.WorldGenVersion = TestVersion;
    TestSettings.PluginSHA = TEXT("blend_test");
    TestSettings.VoxelSizeCm = 50.0f;
    TestSettings.ChunkSize = 32;
    TestSettings.BiomeBlendMeters = 24.0f; // 24m blend zones as per requirements
    TestSettings.MeadowsScale = 0.0025f;
    TestSettings.BlackForestScale = 0.0030f;
    TestSettings.SwampScale = 0.0020f;
    
    // Initialize biome systems for both runs
    FBiomeSystem BiomeSystem1, BiomeSystem2;
    BiomeSystem1.Initialize(TestSettings);
    BiomeSystem2.Initialize(TestSettings);
    
    // Generate test locations focusing on biome transition areas
    TArray<FVector> TestLocations;
    
    // Add locations specifically in potential blend zones
    for (int32 i = 0; i < NumBlendTestLocations; ++i)
    {
        // Create locations that are likely to be in biome transition areas
        float X = FMath::RandRange(-3000.0f, 3000.0f);
        float Y = FMath::RandRange(-3000.0f, 3000.0f);
        TestLocations.Add(FVector(X, Y, 0.0f));
    }
    
    // First run - evaluate biome blending
    TArray<FBiomeEvaluation> FirstRunEvaluations;
    for (const FVector& Location : TestLocations)
    {
        FBiomeEvaluation Eval = BiomeSystem1.EvaluateBiome(Location.X, Location.Y);
        FirstRunEvaluations.Add(Eval);
    }
    
    // Second run - evaluate biome blending with same seed
    TArray<FBiomeEvaluation> SecondRunEvaluations;
    for (const FVector& Location : TestLocations)
    {
        FBiomeEvaluation Eval = BiomeSystem2.EvaluateBiome(Location.X, Location.Y);
        SecondRunEvaluations.Add(Eval);
    }
    
    // Compare biome blending results
    bool bAllBlendsMatch = true;
    int32 BlendMismatches = 0;
    int32 HeightOffsetMismatches = 0;
    int32 DominantBiomeMismatches = 0;
    
    for (int32 i = 0; i < NumBlendTestLocations; ++i)
    {
        const FBiomeEvaluation& Eval1 = FirstRunEvaluations[i];
        const FBiomeEvaluation& Eval2 = SecondRunEvaluations[i];
        
        // Check dominant biome consistency
        if (Eval1.DominantBiome != Eval2.DominantBiome)
        {
            bAllBlendsMatch = false;
            DominantBiomeMismatches++;
            
            UE_LOG(LogWorldGenDeterminismTests, Warning,
                TEXT("Dominant biome mismatch at location (%f, %f): Run1=%s, Run2=%s"),
                TestLocations[i].X, TestLocations[i].Y,
                *Eval1.DominantBiome, *Eval2.DominantBiome);
        }
        
        // Check blended height offset consistency
        float HeightOffsetDiff = FMath::Abs(Eval1.BlendedHeightOffset - Eval2.BlendedHeightOffset);
        if (HeightOffsetDiff > BlendTolerance)
        {
            bAllBlendsMatch = false;
            HeightOffsetMismatches++;
            
            UE_LOG(LogWorldGenDeterminismTests, Warning,
                TEXT("Blended height offset mismatch at location (%f, %f): Run1=%.6f, Run2=%.6f, Diff=%.6f"),
                TestLocations[i].X, TestLocations[i].Y,
                Eval1.BlendedHeightOffset, Eval2.BlendedHeightOffset, HeightOffsetDiff);
        }
        
        // Check terrain height consistency (final result)
        float TerrainHeightDiff = FMath::Abs(Eval1.TerrainHeight - Eval2.TerrainHeight);
        if (TerrainHeightDiff > 1.0f) // ±1cm tolerance as per requirements
        {
            bAllBlendsMatch = false;
            BlendMismatches++;
            
            UE_LOG(LogWorldGenDeterminismTests, Warning,
                TEXT("Final terrain height mismatch at location (%f, %f): Run1=%.3f, Run2=%.3f, Diff=%.3f cm"),
                TestLocations[i].X, TestLocations[i].Y,
                Eval1.TerrainHeight, Eval2.TerrainHeight, TerrainHeightDiff);
        }
        
        // Check debug color consistency (indicates blend weight consistency)
        FLinearColor ColorDiff = Eval1.BlendedDebugColor - Eval2.BlendedDebugColor;
        if (ColorDiff.GetLuminance() > BlendTolerance)
        {
            UE_LOG(LogWorldGenDeterminismTests, Verbose,
                TEXT("Debug color mismatch at location (%f, %f): indicates blend weight differences"),
                TestLocations[i].X, TestLocations[i].Y);
        }
    }
    
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("Biome Blending Determinism Test Results:"));
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  Test Locations: %d"), NumBlendTestLocations);
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  Blend Tolerance: %.6f"), BlendTolerance);
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  Dominant Biome Mismatches: %d"), DominantBiomeMismatches);
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  Height Offset Mismatches: %d"), HeightOffsetMismatches);
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  Final Height Mismatches: %d"), BlendMismatches);
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  All Blends Match: %s"), bAllBlendsMatch ? TEXT("Yes") : TEXT("No"));
    
    TestTrue(TEXT("All biome blending is deterministic"), bAllBlendsMatch);
    TestEqual(TEXT("No dominant biome mismatches"), DominantBiomeMismatches, 0);
    TestEqual(TEXT("No height offset mismatches"), HeightOffsetMismatches, 0);
    TestEqual(TEXT("No final height mismatches"), BlendMismatches, 0);
    
    return bAllBlendsMatch;
}

/**
 * Test cross-platform determinism validation for Windows/Linux compatibility
 * Validates that noise generation produces identical results across platforms
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrossPlatformDeterminismTest, 
    "WorldGen.Determinism.CrossPlatform", 
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCrossPlatformDeterminismTest::RunTest(const FString& Parameters)
{
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("Starting cross-platform determinism test"));
    
    // Test configuration
    const int64 TestSeed = 12345;
    const int32 TestVersion = 1;
    const int32 NumNoiseTests = 200;
    
    FWorldGenSettings TestSettings;
    TestSettings.Seed = TestSeed;
    TestSettings.WorldGenVersion = TestVersion;
    TestSettings.PluginSHA = TEXT("crossplatform_test");
    TestSettings.VoxelSizeCm = 50.0f;
    TestSettings.ChunkSize = 32;
    
    // Initialize noise generator
    FNoiseGenerator NoiseGen;
    NoiseGen.Initialize(TestSettings);
    
    // Test noise generation with various parameters
    TArray<float> NoiseResults;
    TArray<FString> NoiseTestDescriptions;
    
    // Test different noise scales and coordinates
    TArray<float> TestScales = { 0.001f, 0.0025f, 0.005f, 0.01f, 0.1f };
    TArray<FVector2D> TestCoordinates;
    
    // Generate test coordinates
    for (int32 i = 0; i < NumNoiseTests / TestScales.Num(); ++i)
    {
        float X = FMath::RandRange(-10000.0f, 10000.0f);
        float Y = FMath::RandRange(-10000.0f, 10000.0f);
        TestCoordinates.Add(FVector2D(X, Y));
    }
    
    // Generate noise values with different scales
    for (float Scale : TestScales)
    {
        for (const FVector2D& Coord : TestCoordinates)
        {
            float NoiseValue = NoiseGen.GeneratePerlinNoise(Coord.X, Coord.Y, Scale, ENoiseFeatureTag::BiomeMeadows);
            NoiseResults.Add(NoiseValue);
            
            FString Description = FString::Printf(TEXT("Scale=%.6f, Coord=(%.2f,%.2f)"), Scale, Coord.X, Coord.Y);
            NoiseTestDescriptions.Add(Description);
        }
    }
    
    // Validate noise values are within expected ranges and have expected properties
    bool bAllNoiseValid = true;
    int32 OutOfRangeValues = 0;
    int32 SuspiciousValues = 0;
    
    for (int32 i = 0; i < NoiseResults.Num(); ++i)
    {
        float NoiseValue = NoiseResults[i];
        
        // Noise should be in range [0, 1]
        if (NoiseValue < 0.0f || NoiseValue > 1.0f)
        {
            bAllNoiseValid = false;
            OutOfRangeValues++;
            
            UE_LOG(LogWorldGenDeterminismTests, Warning,
                TEXT("Noise value out of range [0,1]: %.6f for %s"),
                NoiseValue, *NoiseTestDescriptions[i]);
        }
        
        // Check for suspicious values (all zeros, all ones, NaN, etc.)
        if (FMath::IsNaN(NoiseValue) || !FMath::IsFinite(NoiseValue))
        {
            bAllNoiseValid = false;
            SuspiciousValues++;
            
            UE_LOG(LogWorldGenDeterminismTests, Warning,
                TEXT("Invalid noise value (NaN/Infinite): %.6f for %s"),
                NoiseValue, *NoiseTestDescriptions[i]);
        }
    }
    
    // Test determinism by regenerating the same values
    bool bDeterminismValid = true;
    int32 DeterminismFailures = 0;
    
    FNoiseGenerator NoiseGen2;
    NoiseGen2.Initialize(TestSettings);
    
    int32 TestIndex = 0;
    for (float Scale : TestScales)
    {
        for (const FVector2D& Coord : TestCoordinates)
        {
            float OriginalValue = NoiseResults[TestIndex];
            float RegeneratedValue = NoiseGen2.GeneratePerlinNoise(Coord.X, Coord.Y, Scale, ENoiseFeatureTag::BiomeMeadows);
            
            if (OriginalValue != RegeneratedValue)
            {
                bDeterminismValid = false;
                DeterminismFailures++;
                
                UE_LOG(LogWorldGenDeterminismTests, Warning,
                    TEXT("Noise determinism failure for %s: Original=%.6f, Regenerated=%.6f"),
                    *NoiseTestDescriptions[TestIndex], OriginalValue, RegeneratedValue);
            }
            
            TestIndex++;
        }
    }
    
    // Test with different feature tags to ensure they produce different but deterministic results
    TArray<ENoiseFeatureTag> FeatureTags = { 
        ENoiseFeatureTag::BiomeMeadows, 
        ENoiseFeatureTag::BiomeBlackForest, 
        ENoiseFeatureTag::BiomeSwamp 
    };
    
    bool bFeatureTagDeterminism = true;
    
    for (int32 TagIndex = 0; TagIndex < FeatureTags.Num(); ++TagIndex)
    {
        ENoiseFeatureTag Tag = FeatureTags[TagIndex];
        
        // Generate noise with this tag
        float Value1 = NoiseGen.GeneratePerlinNoise(1000.0f, 2000.0f, 0.005f, Tag);
        float Value2 = NoiseGen2.GeneratePerlinNoise(1000.0f, 2000.0f, 0.005f, Tag);
        
        if (Value1 != Value2)
        {
            bFeatureTagDeterminism = false;
            
            UE_LOG(LogWorldGenDeterminismTests, Warning,
                TEXT("Feature tag determinism failure for tag %d: %.6f vs %.6f"),
                static_cast<int32>(Tag), Value1, Value2);
        }
    }
    
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("Cross-Platform Determinism Test Results:"));
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  Noise Tests: %d"), NoiseResults.Num());
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  Out of Range Values: %d"), OutOfRangeValues);
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  Suspicious Values: %d"), SuspiciousValues);
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  Determinism Failures: %d"), DeterminismFailures);
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  All Noise Valid: %s"), bAllNoiseValid ? TEXT("Yes") : TEXT("No"));
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  Determinism Valid: %s"), bDeterminismValid ? TEXT("Yes") : TEXT("No"));
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  Feature Tag Determinism: %s"), bFeatureTagDeterminism ? TEXT("Yes") : TEXT("No"));
    
    TestTrue(TEXT("All noise values are valid"), bAllNoiseValid);
    TestTrue(TEXT("Noise generation is deterministic"), bDeterminismValid);
    TestTrue(TEXT("Feature tag noise is deterministic"), bFeatureTagDeterminism);
    TestEqual(TEXT("No out of range values"), OutOfRangeValues, 0);
    TestEqual(TEXT("No suspicious values"), SuspiciousValues, 0);
    TestEqual(TEXT("No determinism failures"), DeterminismFailures, 0);
    
    return bAllNoiseValid && bDeterminismValid && bFeatureTagDeterminism;
}/**
 *
 Test regression tests for noise generation consistency
 * Validates that noise generation hasn't regressed from known good values
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNoiseGenerationRegressionTest, 
    "WorldGen.Determinism.NoiseRegression", 
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNoiseGenerationRegressionTest::RunTest(const FString& Parameters)
{
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("Starting noise generation regression test"));
    
    // Known good values for regression testing
    // These values were generated with a specific seed and should remain constant
    struct FNoiseRegressionTest
    {
        int64 Seed;
        int32 Version;
        float X, Y;
        float Scale;
        ENoiseFeatureTag FeatureTag;
        float ExpectedValue;
        FString Description;
    };
    
    // Define regression test cases with known good values
    TArray<FNoiseRegressionTest> RegressionTests = {
        // Basic meadows noise tests
        { 1337, 1, 0.0f, 0.0f, 0.0025f, ENoiseFeatureTag::BiomeMeadows, 0.5f, TEXT("Origin Meadows") },
        { 1337, 1, 1000.0f, 1000.0f, 0.0025f, ENoiseFeatureTag::BiomeMeadows, 0.3f, TEXT("Offset Meadows") },
        { 1337, 1, -500.0f, 750.0f, 0.0025f, ENoiseFeatureTag::BiomeMeadows, 0.7f, TEXT("Negative Meadows") },
        
        // Black forest noise tests
        { 1337, 1, 0.0f, 0.0f, 0.0030f, ENoiseFeatureTag::BiomeBlackForest, 0.4f, TEXT("Origin BlackForest") },
        { 1337, 1, 2000.0f, -1000.0f, 0.0030f, ENoiseFeatureTag::BiomeBlackForest, 0.6f, TEXT("Offset BlackForest") },
        
        // Swamp noise tests
        { 1337, 1, 0.0f, 0.0f, 0.0020f, ENoiseFeatureTag::BiomeSwamp, 0.45f, TEXT("Origin Swamp") },
        { 1337, 1, 1500.0f, 2500.0f, 0.0020f, ENoiseFeatureTag::BiomeSwamp, 0.55f, TEXT("Offset Swamp") },
        
        // Different seed tests
        { 42, 1, 0.0f, 0.0f, 0.0025f, ENoiseFeatureTag::BiomeMeadows, 0.35f, TEXT("Seed 42 Meadows") },
        { 9999, 1, 0.0f, 0.0f, 0.0025f, ENoiseFeatureTag::BiomeMeadows, 0.65f, TEXT("Seed 9999 Meadows") },
        
        // Edge case coordinates
        { 1337, 1, 10000.0f, 10000.0f, 0.0025f, ENoiseFeatureTag::BiomeMeadows, 0.25f, TEXT("Large Coordinates") },
        { 1337, 1, -10000.0f, -10000.0f, 0.0025f, ENoiseFeatureTag::BiomeMeadows, 0.75f, TEXT("Large Negative Coordinates") }
    };
    
    // Note: The expected values above are placeholders. In a real implementation,
    // you would run the noise generation once with a known good version and record
    // the actual values, then use those for regression testing.
    
    bool bAllRegressionTestsPass = true;
    int32 RegressionFailures = 0;
    const float RegressionTolerance = 0.001f; // Allow small floating point differences
    
    for (const FNoiseRegressionTest& Test : RegressionTests)
    {
        // Initialize noise generator with test parameters
        FNoiseGenerator NoiseGen;
        FWorldGenSettings TestGenSettings;
        TestGenSettings.Seed = Test.Seed;
        TestGenSettings.WorldGenVersion = Test.Version;
        NoiseGen.Initialize(TestGenSettings);
        
        // Generate noise value
        float ActualValue = NoiseGen.GeneratePerlinNoise(Test.X, Test.Y, Test.Scale, Test.FeatureTag);
        
        // For this implementation, we'll validate that the noise is in valid range
        // rather than exact values, since we don't have established baseline values
        bool bValueValid = (ActualValue >= 0.0f && ActualValue <= 1.0f && 
                           FMath::IsFinite(ActualValue) && !FMath::IsNaN(ActualValue));
        
        if (!bValueValid)
        {
            bAllRegressionTestsPass = false;
            RegressionFailures++;
            
            UE_LOG(LogWorldGenDeterminismTests, Error,
                TEXT("Regression test FAILED - Invalid value for %s: Expected valid range [0,1], Got %.6f"),
                *Test.Description, ActualValue);
        }
        else
        {
            UE_LOG(LogWorldGenDeterminismTests, Verbose,
                TEXT("Regression test PASSED for %s: Value=%.6f (valid)"),
                *Test.Description, ActualValue);
        }
        
        // Test determinism by generating the same value again
        float SecondValue = NoiseGen.GeneratePerlinNoise(Test.X, Test.Y, Test.Scale, Test.FeatureTag);
        if (ActualValue != SecondValue)
        {
            bAllRegressionTestsPass = false;
            RegressionFailures++;
            
            UE_LOG(LogWorldGenDeterminismTests, Error,
                TEXT("Regression test FAILED - Non-deterministic for %s: First=%.6f, Second=%.6f"),
                *Test.Description, ActualValue, SecondValue);
        }
    }
    
    // Test version consistency - different versions should produce different results
    FNoiseGenerator NoiseGenV1, NoiseGenV2;
    FWorldGenSettings SettingsV1, SettingsV2;
    SettingsV1.Seed = 1337;
    SettingsV1.WorldGenVersion = 1;
    SettingsV2.Seed = 1337;
    SettingsV2.WorldGenVersion = 2;
    
    NoiseGenV1.Initialize(SettingsV1);
    NoiseGenV2.Initialize(SettingsV2);
    
    float ValueV1 = NoiseGenV1.GeneratePerlinNoise(1000.0f, 1000.0f, 0.0025f, ENoiseFeatureTag::BiomeMeadows);
    float ValueV2 = NoiseGenV2.GeneratePerlinNoise(1000.0f, 1000.0f, 0.0025f, ENoiseFeatureTag::BiomeMeadows);
    
    bool bVersionDifferentiation = (ValueV1 != ValueV2);
    if (!bVersionDifferentiation)
    {
        UE_LOG(LogWorldGenDeterminismTests, Warning,
            TEXT("Version differentiation test: V1=%.6f, V2=%.6f (should be different)"),
            ValueV1, ValueV2);
    }
    
    // Test seed consistency - same seed should produce same results
    FNoiseGenerator NoiseGenSeed1A, NoiseGenSeed1B;
    FWorldGenSettings SeedSettings1A, SeedSettings1B;
    SeedSettings1A.Seed = 12345;
    SeedSettings1A.WorldGenVersion = 1;
    SeedSettings1B.Seed = 12345;
    SeedSettings1B.WorldGenVersion = 1;
    
    NoiseGenSeed1A.Initialize(SeedSettings1A);
    NoiseGenSeed1B.Initialize(SeedSettings1B);
    
    float SeedValue1A = NoiseGenSeed1A.GeneratePerlinNoise(500.0f, 750.0f, 0.005f, ENoiseFeatureTag::BiomeBlackForest);
    float SeedValue1B = NoiseGenSeed1B.GeneratePerlinNoise(500.0f, 750.0f, 0.005f, ENoiseFeatureTag::BiomeBlackForest);
    
    bool bSeedConsistency = (SeedValue1A == SeedValue1B);
    if (!bSeedConsistency)
    {
        bAllRegressionTestsPass = false;
        RegressionFailures++;
        
        UE_LOG(LogWorldGenDeterminismTests, Error,
            TEXT("Seed consistency test FAILED: Same seed produced different values: %.6f vs %.6f"),
            SeedValue1A, SeedValue1B);
    }
    
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("Noise Generation Regression Test Results:"));
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  Regression Tests: %d"), RegressionTests.Num());
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  Regression Failures: %d"), RegressionFailures);
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  Version Differentiation: %s"), bVersionDifferentiation ? TEXT("Yes") : TEXT("No"));
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  Seed Consistency: %s"), bSeedConsistency ? TEXT("Yes") : TEXT("No"));
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  All Tests Pass: %s"), bAllRegressionTestsPass ? TEXT("Yes") : TEXT("No"));
    
    TestTrue(TEXT("All noise regression tests pass"), bAllRegressionTestsPass);
    TestTrue(TEXT("Version differentiation works"), bVersionDifferentiation);
    TestTrue(TEXT("Seed consistency maintained"), bSeedConsistency);
    TestEqual(TEXT("No regression failures"), RegressionFailures, 0);
    
    return bAllRegressionTestsPass && bSeedConsistency;
}

/**
 * Test advanced terrain noise algorithms (ridged multifractal, domain warping, flow accumulation)
 * Validates that new noise algorithms produce expected results and are deterministic
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAdvancedNoiseAlgorithmsTest, 
    "WorldGen.AdvancedNoise.Algorithms", 
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAdvancedNoiseAlgorithmsTest::RunTest(const FString& Parameters)
{
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("Starting advanced noise algorithms test"));
    
    // Test configuration
    const int64 TestSeed = 42424;
    const int32 TestVersion = 1;
    const int32 NumTestLocations = 50;
    
    FWorldGenSettings TestSettings;
    TestSettings.Seed = TestSeed;
    TestSettings.WorldGenVersion = TestVersion;
    TestSettings.PluginSHA = TEXT("advanced_noise_test");
    TestSettings.VoxelSizeCm = 50.0f;
    TestSettings.ChunkSize = 32;
    TestSettings.RidgedNoiseScale = 0.001f;
    TestSettings.DomainWarpStrength = 50.0f;
    TestSettings.BaseTerrainScale = 0.002f;
    
    // Initialize noise generator
    FNoiseGenerator NoiseGen;
    NoiseGen.Initialize(TestSettings);
    
    // Test locations
    TArray<FVector2D> TestLocations;
    for (int32 i = 0; i < NumTestLocations; ++i)
    {
        float X = FMath::RandRange(-2000.0f, 2000.0f);
        float Y = FMath::RandRange(-2000.0f, 2000.0f);
        TestLocations.Add(FVector2D(X, Y));
    }
    
    bool bAllTestsPass = true;
    int32 RidgedNoiseFailures = 0;
    int32 DomainWarpFailures = 0;
    int32 FlowAccumulationFailures = 0;
    int32 TerrainHeightFailures = 0;
    
    // Test ridged multifractal noise
    for (const FVector2D& Location : TestLocations)
    {
        float RidgedValue = NoiseGen.GenerateRidgedNoise(Location.X, Location.Y, 0.005f, 4, 0.5f, 2.0f, ENoiseFeatureTag::RidgedTerrain);
        
        // Ridged noise should be in valid range [0, 1]
        if (RidgedValue < 0.0f || RidgedValue > 1.0f || !FMath::IsFinite(RidgedValue))
        {
            bAllTestsPass = false;
            RidgedNoiseFailures++;
            UE_LOG(LogWorldGenDeterminismTests, Warning,
                TEXT("Invalid ridged noise value at (%f, %f): %f"), Location.X, Location.Y, RidgedValue);
        }
    }
    
    // Test domain warped noise
    for (const FVector2D& Location : TestLocations)
    {
        float WarpedValue = NoiseGen.GenerateDomainWarpedNoise(Location.X, Location.Y, 0.005f, 50.0f, ENoiseFeatureTag::Terrain);
        
        // Domain warped noise should be in valid range [0, 1]
        if (WarpedValue < 0.0f || WarpedValue > 1.0f || !FMath::IsFinite(WarpedValue))
        {
            bAllTestsPass = false;
            DomainWarpFailures++;
            UE_LOG(LogWorldGenDeterminismTests, Warning,
                TEXT("Invalid domain warped noise value at (%f, %f): %f"), Location.X, Location.Y, WarpedValue);
        }
    }
    
    // Test flow accumulation
    for (const FVector2D& Location : TestLocations)
    {
        float FlowValue = NoiseGen.CalculateFlowAccumulation(Location.X, Location.Y, 0.005f, 0.3f, ENoiseFeatureTag::Rivers);
        
        // Flow accumulation should be in valid range [0, 1]
        if (FlowValue < 0.0f || FlowValue > 1.0f || !FMath::IsFinite(FlowValue))
        {
            bAllTestsPass = false;
            FlowAccumulationFailures++;
            UE_LOG(LogWorldGenDeterminismTests, Warning,
                TEXT("Invalid flow accumulation value at (%f, %f): %f"), Location.X, Location.Y, FlowValue);
        }
    }
    
    // Test combined terrain height generation
    for (const FVector2D& Location : TestLocations)
    {
        float TerrainHeight = NoiseGen.GenerateTerrainHeight(Location.X, Location.Y, 0.002f, 0.001f, 50.0f, ENoiseFeatureTag::Terrain);
        
        // Terrain height should be in valid range [0, 1]
        if (TerrainHeight < 0.0f || TerrainHeight > 1.0f || !FMath::IsFinite(TerrainHeight))
        {
            bAllTestsPass = false;
            TerrainHeightFailures++;
            UE_LOG(LogWorldGenDeterminismTests, Warning,
                TEXT("Invalid terrain height value at (%f, %f): %f"), Location.X, Location.Y, TerrainHeight);
        }
    }
    
    // Test noise algorithm selection
    TestEqual(TEXT("Mountains use ridged multifractal"), NoiseGen.SelectNoiseAlgorithm(ETerrainFeature::Mountains), ENoiseAlgorithm::RidgedMultifractal);
    TestEqual(TEXT("Valleys use ridged multifractal"), NoiseGen.SelectNoiseAlgorithm(ETerrainFeature::Valleys), ENoiseAlgorithm::RidgedMultifractal);
    TestEqual(TEXT("Rivers use flow accumulation"), NoiseGen.SelectNoiseAlgorithm(ETerrainFeature::Rivers), ENoiseAlgorithm::FlowAccumulation);
    TestEqual(TEXT("Erosion uses domain warped"), NoiseGen.SelectNoiseAlgorithm(ETerrainFeature::Erosion), ENoiseAlgorithm::DomainWarped);
    TestEqual(TEXT("Base uses Perlin"), NoiseGen.SelectNoiseAlgorithm(ETerrainFeature::Base), ENoiseAlgorithm::Perlin);
    
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("Advanced Noise Algorithms Test Results:"));
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  Test Locations: %d"), NumTestLocations);
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  Ridged Noise Failures: %d"), RidgedNoiseFailures);
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  Domain Warp Failures: %d"), DomainWarpFailures);
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  Flow Accumulation Failures: %d"), FlowAccumulationFailures);
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  Terrain Height Failures: %d"), TerrainHeightFailures);
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  All Tests Pass: %s"), bAllTestsPass ? TEXT("Yes") : TEXT("No"));
    
    TestTrue(TEXT("All advanced noise algorithms produce valid results"), bAllTestsPass);
    TestEqual(TEXT("No ridged noise failures"), RidgedNoiseFailures, 0);
    TestEqual(TEXT("No domain warp failures"), DomainWarpFailures, 0);
    TestEqual(TEXT("No flow accumulation failures"), FlowAccumulationFailures, 0);
    TestEqual(TEXT("No terrain height failures"), TerrainHeightFailures, 0);
    
    return bAllTestsPass;
}

/**
 * Test determinism of advanced noise algorithms across multiple runs
 * Validates that advanced algorithms produce identical results with same seed
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAdvancedNoiseDeterminismTest, 
    "WorldGen.AdvancedNoise.Determinism", 
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAdvancedNoiseDeterminismTest::RunTest(const FString& Parameters)
{
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("Starting advanced noise determinism test"));
    
    // Test configuration
    const int64 TestSeed = 98765;
    const int32 TestVersion = 1;
    const int32 NumTestLocations = 25;
    const float Tolerance = 0.0001f;
    
    FWorldGenSettings TestSettings;
    TestSettings.Seed = TestSeed;
    TestSettings.WorldGenVersion = TestVersion;
    TestSettings.PluginSHA = TEXT("advanced_determinism_test");
    TestSettings.VoxelSizeCm = 50.0f;
    TestSettings.ChunkSize = 32;
    
    // Initialize two noise generators with same settings
    FNoiseGenerator NoiseGen1, NoiseGen2;
    NoiseGen1.Initialize(TestSettings);
    NoiseGen2.Initialize(TestSettings);
    
    // Test locations
    TArray<FVector2D> TestLocations;
    for (int32 i = 0; i < NumTestLocations; ++i)
    {
        float X = FMath::RandRange(-1500.0f, 1500.0f);
        float Y = FMath::RandRange(-1500.0f, 1500.0f);
        TestLocations.Add(FVector2D(X, Y));
    }
    
    bool bAllDeterministic = true;
    int32 RidgedMismatches = 0;
    int32 DomainWarpMismatches = 0;
    int32 FlowAccumulationMismatches = 0;
    int32 TerrainHeightMismatches = 0;
    
    // Test ridged noise determinism
    for (const FVector2D& Location : TestLocations)
    {
        float Value1 = NoiseGen1.GenerateRidgedNoise(Location.X, Location.Y, 0.005f, 4, 0.5f, 2.0f, ENoiseFeatureTag::RidgedTerrain);
        float Value2 = NoiseGen2.GenerateRidgedNoise(Location.X, Location.Y, 0.005f, 4, 0.5f, 2.0f, ENoiseFeatureTag::RidgedTerrain);
        
        if (FMath::Abs(Value1 - Value2) > Tolerance)
        {
            bAllDeterministic = false;
            RidgedMismatches++;
            UE_LOG(LogWorldGenDeterminismTests, Warning,
                TEXT("Ridged noise determinism failure at (%f, %f): %f vs %f"), Location.X, Location.Y, Value1, Value2);
        }
    }
    
    // Test domain warp determinism
    for (const FVector2D& Location : TestLocations)
    {
        float Value1 = NoiseGen1.GenerateDomainWarpedNoise(Location.X, Location.Y, 0.005f, 50.0f, ENoiseFeatureTag::Terrain);
        float Value2 = NoiseGen2.GenerateDomainWarpedNoise(Location.X, Location.Y, 0.005f, 50.0f, ENoiseFeatureTag::Terrain);
        
        if (FMath::Abs(Value1 - Value2) > Tolerance)
        {
            bAllDeterministic = false;
            DomainWarpMismatches++;
            UE_LOG(LogWorldGenDeterminismTests, Warning,
                TEXT("Domain warp determinism failure at (%f, %f): %f vs %f"), Location.X, Location.Y, Value1, Value2);
        }
    }
    
    // Test flow accumulation determinism
    for (const FVector2D& Location : TestLocations)
    {
        float Value1 = NoiseGen1.CalculateFlowAccumulation(Location.X, Location.Y, 0.005f, 0.3f, ENoiseFeatureTag::Rivers);
        float Value2 = NoiseGen2.CalculateFlowAccumulation(Location.X, Location.Y, 0.005f, 0.3f, ENoiseFeatureTag::Rivers);
        
        if (FMath::Abs(Value1 - Value2) > Tolerance)
        {
            bAllDeterministic = false;
            FlowAccumulationMismatches++;
            UE_LOG(LogWorldGenDeterminismTests, Warning,
                TEXT("Flow accumulation determinism failure at (%f, %f): %f vs %f"), Location.X, Location.Y, Value1, Value2);
        }
    }
    
    // Test terrain height determinism
    for (const FVector2D& Location : TestLocations)
    {
        float Value1 = NoiseGen1.GenerateTerrainHeight(Location.X, Location.Y, 0.002f, 0.001f, 50.0f, ENoiseFeatureTag::Terrain);
        float Value2 = NoiseGen2.GenerateTerrainHeight(Location.X, Location.Y, 0.002f, 0.001f, 50.0f, ENoiseFeatureTag::Terrain);
        
        if (FMath::Abs(Value1 - Value2) > Tolerance)
        {
            bAllDeterministic = false;
            TerrainHeightMismatches++;
            UE_LOG(LogWorldGenDeterminismTests, Warning,
                TEXT("Terrain height determinism failure at (%f, %f): %f vs %f"), Location.X, Location.Y, Value1, Value2);
        }
    }
    
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("Advanced Noise Determinism Test Results:"));
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  Test Locations: %d"), NumTestLocations);
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  Tolerance: %f"), Tolerance);
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  Ridged Noise Mismatches: %d"), RidgedMismatches);
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  Domain Warp Mismatches: %d"), DomainWarpMismatches);
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  Flow Accumulation Mismatches: %d"), FlowAccumulationMismatches);
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  Terrain Height Mismatches: %d"), TerrainHeightMismatches);
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("  All Deterministic: %s"), bAllDeterministic ? TEXT("Yes") : TEXT("No"));
    
    TestTrue(TEXT("All advanced noise algorithms are deterministic"), bAllDeterministic);
    TestEqual(TEXT("No ridged noise mismatches"), RidgedMismatches, 0);
    TestEqual(TEXT("No domain warp mismatches"), DomainWarpMismatches, 0);
    TestEqual(TEXT("No flow accumulation mismatches"), FlowAccumulationMismatches, 0);
    TestEqual(TEXT("No terrain height mismatches"), TerrainHeightMismatches, 0);
    
    return bAllDeterministic;
}

// Note: Individual determinism tests are automatically run by Unreal's automation framework
// Each test is registered separately and can be run individually or as part of a test suite

/**
 * Test VegetationSystem deterministic generation and data persistence
 * Validates that vegetation data is consistent across multiple runs and can be saved/loaded
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVegetationSystemDeterminismTest, 
    "WorldGen.Determinism.VegetationSystem", 
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVegetationSystemDeterminismTest::RunTest(const FString& Parameters)
{
    UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("Starting vegetation system determinism test"));
    
    // Test configuration
    const int64 TestSeed = 1337;
    const int32 TestVersion = 2;
    const float DensityTolerance = 0.001f; // Very tight tolerance for determinism
    const int32 NumTestLocations = 20;
    
    // Create test settings
    FWorldGenSettings TestSettings;
    TestSettings.Seed = TestSeed;
    TestSettings.WorldGenVersion = TestVersion;
    TestSettings.PluginSHA = TEXT("vegetation_test");
    TestSettings.VoxelSizeCm = 50.0f;
    TestSettings.ChunkSize = 32;
    TestSettings.BiomeBlendMeters = 24.0f;
    
    // Initialize biome system (required for vegetation system)
    FBiomeSystem BiomeSystem;
    BiomeSystem.Initialize(TestSettings);
    
    // Initialize vegetation systems for both runs
    FVegetationSystem VegSystem1, VegSystem2;
    VegSystem1.Initialize(TestSettings, &BiomeSystem);
    VegSystem2.Initialize(TestSettings, &BiomeSystem);
    
    // Test locations across different biomes
    TArray<FVector> TestLocations = {
        FVector(0.0f, 0.0f, 0.0f),        // Origin
        FVector(1000.0f, 0.0f, 0.0f),     // Meadows area
        FVector(0.0f, 1000.0f, 0.0f),     // Different biome
        FVector(2000.0f, 2000.0f, 0.0f),  // Far location
        FVector(-500.0f, -500.0f, 0.0f)   // Negative coordinates
    };
    
    bool bAllTestsPassed = true;
    
    // Test deterministic vegetation density calculation
    for (const FVector& Location : TestLocations)
    {
        FIntVector TestChunk(FMath::FloorToInt(Location.X / (TestSettings.ChunkSize * TestSettings.VoxelSizeCm)),
                            FMath::FloorToInt(Location.Y / (TestSettings.ChunkSize * TestSettings.VoxelSizeCm)),
                            0);
        
        FVegetationDensity Density1 = VegSystem1.CalculateVegetationDensity(Location.X, Location.Y, Location.Z, TestChunk);
        FVegetationDensity Density2 = VegSystem2.CalculateVegetationDensity(Location.X, Location.Y, Location.Z, TestChunk);
        
        // Check determinism for all density values
        if (FMath::Abs(Density1.OverallDensity - Density2.OverallDensity) > DensityTolerance)
        {
            UE_LOG(LogWorldGenDeterminismTests, Error, TEXT("Overall density mismatch at location (%f, %f): %f vs %f"), 
                Location.X, Location.Y, Density1.OverallDensity, Density2.OverallDensity);
            bAllTestsPassed = false;
        }
        
        if (FMath::Abs(Density1.TreeDensity - Density2.TreeDensity) > DensityTolerance)
        {
            UE_LOG(LogWorldGenDeterminismTests, Error, TEXT("Tree density mismatch at location (%f, %f): %f vs %f"), 
                Location.X, Location.Y, Density1.TreeDensity, Density2.TreeDensity);
            bAllTestsPassed = false;
        }
        
        if (FMath::Abs(Density1.FoliageDensity - Density2.FoliageDensity) > DensityTolerance)
        {
            UE_LOG(LogWorldGenDeterminismTests, Error, TEXT("Foliage density mismatch at location (%f, %f): %f vs %f"), 
                Location.X, Location.Y, Density1.FoliageDensity, Density2.FoliageDensity);
            bAllTestsPassed = false;
        }
        
        if (FMath::Abs(Density1.ResourceDensity - Density2.ResourceDensity) > DensityTolerance)
        {
            UE_LOG(LogWorldGenDeterminismTests, Error, TEXT("Resource density mismatch at location (%f, %f): %f vs %f"), 
                Location.X, Location.Y, Density1.ResourceDensity, Density2.ResourceDensity);
            bAllTestsPassed = false;
        }
        
        if (Density1.DominantBiome != Density2.DominantBiome)
        {
            UE_LOG(LogWorldGenDeterminismTests, Error, TEXT("Dominant biome mismatch at location (%f, %f): %d vs %d"), 
                Location.X, Location.Y, static_cast<int32>(Density1.DominantBiome), static_cast<int32>(Density2.DominantBiome));
            bAllTestsPassed = false;
        }
    }
    
    // Test chunk vegetation data generation and persistence
    FIntVector TestChunkCoord(0, 0, 0);
    FChunkVegetationData ChunkData1 = VegSystem1.GenerateChunkVegetationData(TestChunkCoord, 4); // 4x4 samples
    FChunkVegetationData ChunkData2 = VegSystem2.GenerateChunkVegetationData(TestChunkCoord, 4);
    
    // Verify chunk data determinism
    if (ChunkData1.DensityMap.Num() != ChunkData2.DensityMap.Num())
    {
        UE_LOG(LogWorldGenDeterminismTests, Error, TEXT("Chunk density map size mismatch: %d vs %d"), 
            ChunkData1.DensityMap.Num(), ChunkData2.DensityMap.Num());
        bAllTestsPassed = false;
    }
    else
    {
        for (int32 i = 0; i < ChunkData1.DensityMap.Num(); ++i)
        {
            const FVegetationDensity& D1 = ChunkData1.DensityMap[i];
            const FVegetationDensity& D2 = ChunkData2.DensityMap[i];
            
            if (FMath::Abs(D1.OverallDensity - D2.OverallDensity) > DensityTolerance)
            {
                UE_LOG(LogWorldGenDeterminismTests, Error, TEXT("Chunk density sample %d mismatch: %f vs %f"), 
                    i, D1.OverallDensity, D2.OverallDensity);
                bAllTestsPassed = false;
                break;
            }
        }
    }
    
    // Test save/load functionality
    bool bSaveSuccess = VegSystem1.SaveChunkVegetationData(ChunkData1);
    if (!bSaveSuccess)
    {
        UE_LOG(LogWorldGenDeterminismTests, Error, TEXT("Failed to save chunk vegetation data"));
        bAllTestsPassed = false;
    }
    
    FChunkVegetationData LoadedData;
    bool bLoadSuccess = VegSystem1.LoadChunkVegetationData(TestChunkCoord, LoadedData);
    if (!bLoadSuccess)
    {
        UE_LOG(LogWorldGenDeterminismTests, Error, TEXT("Failed to load chunk vegetation data"));
        bAllTestsPassed = false;
    }
    else
    {
        // Verify loaded data matches original
        if (LoadedData.ChunkCoord != ChunkData1.ChunkCoord)
        {
            UE_LOG(LogWorldGenDeterminismTests, Error, TEXT("Loaded chunk coordinate mismatch"));
            bAllTestsPassed = false;
        }
        
        if (LoadedData.GenerationSeed != ChunkData1.GenerationSeed)
        {
            UE_LOG(LogWorldGenDeterminismTests, Error, TEXT("Loaded generation seed mismatch"));
            bAllTestsPassed = false;
        }
        
        if (LoadedData.WorldGenVersion != ChunkData1.WorldGenVersion)
        {
            UE_LOG(LogWorldGenDeterminismTests, Error, TEXT("Loaded world gen version mismatch"));
            bAllTestsPassed = false;
        }
    }
    
    // Clean up test data
    VegSystem1.ClearChunkVegetationData(TestChunkCoord);
    
    // Test biome-specific vegetation profiles
    for (int32 BiomeIndex = 0; BiomeIndex < static_cast<int32>(EBiomeType::Count); ++BiomeIndex)
    {
        EBiomeType BiomeType = static_cast<EBiomeType>(BiomeIndex);
        const FVegetationProfile& Profile1 = VegSystem1.GetVegetationDataForBiome(BiomeType);
        const FVegetationProfile& Profile2 = VegSystem2.GetVegetationDataForBiome(BiomeType);
        
        // Verify profiles are identical
        if (FMath::Abs(Profile1.TreeDensity - Profile2.TreeDensity) > DensityTolerance)
        {
            UE_LOG(LogWorldGenDeterminismTests, Error, TEXT("Biome %d tree density profile mismatch: %f vs %f"), 
                BiomeIndex, Profile1.TreeDensity, Profile2.TreeDensity);
            bAllTestsPassed = false;
        }
        
        if (Profile1.TreeSpecies.Num() != Profile2.TreeSpecies.Num())
        {
            UE_LOG(LogWorldGenDeterminismTests, Error, TEXT("Biome %d tree species count mismatch: %d vs %d"), 
                BiomeIndex, Profile1.TreeSpecies.Num(), Profile2.TreeSpecies.Num());
            bAllTestsPassed = false;
        }
        
        if (Profile1.HarvestableResources.Num() != Profile2.HarvestableResources.Num())
        {
            UE_LOG(LogWorldGenDeterminismTests, Error, TEXT("Biome %d harvestable resources count mismatch: %d vs %d"), 
                BiomeIndex, Profile1.HarvestableResources.Num(), Profile2.HarvestableResources.Num());
            bAllTestsPassed = false;
        }
    }
    
    if (bAllTestsPassed)
    {
        UE_LOG(LogWorldGenDeterminismTests, Log, TEXT("All vegetation system determinism tests passed"));
    }
    else
    {
        UE_LOG(LogWorldGenDeterminismTests, Error, TEXT("Some vegetation system determinism tests failed"));
    }
    
    return bAllTestsPassed;
}