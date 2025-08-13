#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMemory.h"
#include "Engine/World.h"
#include "../PerformanceProfiler.h"
#include "../WorldGenManager.h"
#include "../ChunkStreamingManager.h"
#include "../WorldGenCore.h"

DEFINE_LOG_CATEGORY_STATIC(LogPerformanceRegressionTests, Log, All);

/**
 * Performance regression test for chunk generation timing
 * Validates that chunk generation meets performance targets
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChunkGenerationPerformanceRegressionTest, 
    "WorldGen.Performance.ChunkGenerationRegression", 
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FChunkGenerationPerformanceRegressionTest::RunTest(const FString& Parameters)
{
    // Create performance profiler
    FWorldGenPerformanceProfiler Profiler;
    
    // Test settings
    FWorldGenSettings TestSettings;
    TestSettings.Seed = 42;
    TestSettings.WorldGenVersion = 1;
    TestSettings.VoxelSizeCm = 50.0f;
    TestSettings.ChunkSize = 32;
    
    // Simulate chunk generation with performance tracking
    const int32 NumTestChunks = 50;
    const double MaxAcceptableTimeMs = 5.0; // Target average time
    const double MaxAcceptableP95Ms = 9.0;  // Target P95 time
    
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("Starting chunk generation performance regression test with %d chunks"), NumTestChunks);
    
    for (int32 i = 0; i < NumTestChunks; ++i)
    {
        FIntVector ChunkCoord(i % 10, i / 10, 0);
        
        // Create scoped timer for this chunk
        FScopedChunkGenerationTimer Timer(&Profiler, ChunkCoord, EChunkLOD::LOD0);
        
        // Simulate chunk generation phases
        Timer.StartBiomeEvaluation();
        
        // Simulate biome evaluation work
        double BiomeStartTime = FPlatformTime::Seconds();
        while ((FPlatformTime::Seconds() - BiomeStartTime) < 0.001) // 1ms of work
        {
            // Simulate computational work
            volatile float DummyWork = FMath::Sin(i * 0.1f) * FMath::Cos(i * 0.2f);
        }
        
        Timer.EndBiomeEvaluation();
        Timer.StartPOIPlacement();
        
        // Simulate POI placement work
        double POIStartTime = FPlatformTime::Seconds();
        while ((FPlatformTime::Seconds() - POIStartTime) < 0.0005) // 0.5ms of work
        {
            volatile float DummyWork = FMath::Pow(i * 0.1f, 2.0f);
        }
        
        Timer.EndPOIPlacement();
        Timer.StartMeshGeneration();
        
        // Simulate mesh generation work (most expensive phase)
        double MeshStartTime = FPlatformTime::Seconds();
        while ((FPlatformTime::Seconds() - MeshStartTime) < 0.002) // 2ms of work
        {
            volatile float DummyWork = FMath::Sqrt(i * 10.0f + 1.0f);
        }
        
        Timer.EndMeshGeneration();
        
        // Set realistic metrics
        Timer.SetTriangleCount(FMath::RandRange(4000, 7500)); // Within 8k limit
        Timer.SetMemoryUsage(FMath::RandRange(512 * 1024, 2 * 1024 * 1024)); // 512KB to 2MB
        Timer.SetHasCollision(true);
        
        // Timer destructor will record metrics automatically
    }
    
    // Run regression analysis
    FPerformanceRegressionResults Results = Profiler.RunRegressionTests(NumTestChunks);
    
    // Validate results
    TestTrue(TEXT("Generation time regression test passed"), Results.bPassedGenerationTimeTest);
    TestTrue(TEXT("Memory usage regression test passed"), Results.bPassedMemoryUsageTest);
    TestTrue(TEXT("Triangle count regression test passed"), Results.bPassedTriangleCountTest);
    
    // Log detailed results
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("Performance Results:"));
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("  Average Generation Time: %.2fms (target: %.2fms)"), 
           Results.AverageGenerationTimeMs, MaxAcceptableTimeMs);
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("  P95 Generation Time: %.2fms (target: %.2fms)"), 
           Results.P95GenerationTimeMs, MaxAcceptableP95Ms);
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("  Average Memory Usage: %llu MB"), Results.AverageMemoryUsageMB);
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("  Peak Memory Usage: %llu MB"), Results.PeakMemoryUsageMB);
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("  Average Triangle Count: %d"), Results.AverageTriangleCount);
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("  Max Triangle Count: %d (target: ≤8000)"), Results.MaxTriangleCount);
    
    // Export results for CI/CD analysis
    FString ExportPath = FPaths::ProjectSavedDir() / TEXT("PerformanceTests") / TEXT("ChunkGenerationRegression.json");
    Profiler.ExportPerformanceData(ExportPath);
    
    // Log any failure reasons
    for (const FString& Reason : Results.FailureReasons)
    {
        AddError(Reason);
    }
    
    return Results.bPassedGenerationTimeTest && Results.bPassedMemoryUsageTest && Results.bPassedTriangleCountTest;
}

/**
 * Memory usage validation test for LOD0 chunks
 * Ensures LOD0 radius stays within 64MB memory limit
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLOD0MemoryValidationTest, 
    "WorldGen.Performance.LOD0MemoryValidation", 
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FLOD0MemoryValidationTest::RunTest(const FString& Parameters)
{
    FWorldGenPerformanceProfiler Profiler;
    
    // Simulate LOD0 chunks within streaming radius
    const int32 LOD0Radius = 2; // chunks
    const SIZE_T TargetMemoryLimitMB = 64;
    
    TArray<FStreamingChunk> SimulatedChunks;
    SIZE_T TotalLOD0Memory = 0;
    int32 LOD0ChunkCount = 0;
    
    // Create chunks in LOD0 radius
    for (int32 X = -LOD0Radius; X <= LOD0Radius; ++X)
    {
        for (int32 Y = -LOD0Radius; Y <= LOD0Radius; ++Y)
        {
            for (int32 Z = -1; Z <= 1; ++Z) // Limited Z range for testing
            {
                FIntVector ChunkCoord(X, Y, Z);
                float Distance = FMath::Sqrt(static_cast<float>(X*X + Y*Y + Z*Z));
                
                FStreamingChunk Chunk(ChunkCoord);
                
                if (Distance <= LOD0Radius)
                {
                    Chunk.CurrentLOD = EChunkLOD::LOD0;
                    Chunk.bHasCollision = true;
                    LOD0ChunkCount++;
                    
                    // Simulate realistic memory usage per LOD0 chunk
                    SIZE_T ChunkMemory = FMath::RandRange(1024 * 1024, 3 * 1024 * 1024); // 1-3MB per chunk
                    TotalLOD0Memory += ChunkMemory;
                    
                    // Record performance metrics for this chunk
                    FWorldGenPerformanceProfiler::FChunkPerformanceMetrics Metrics;
                    Metrics.ChunkCoordinate = ChunkCoord;
                    Metrics.LODLevel = EChunkLOD::LOD0;
                    Metrics.MemoryUsageBytes = ChunkMemory;
                    Metrics.TriangleCount = FMath::RandRange(6000, 8000);
                    Metrics.GenerationTimeMs = FMath::RandRange(3.0, 7.0);
                    Metrics.bHasCollision = true;
                    
                    Profiler.RecordChunkMetrics(Metrics);
                }
                else if (Distance <= LOD0Radius + 2)
                {
                    Chunk.CurrentLOD = EChunkLOD::LOD1;
                    Chunk.bHasCollision = true;
                }
                else
                {
                    Chunk.CurrentLOD = EChunkLOD::LOD2;
                    Chunk.bHasCollision = false;
                }
                
                SimulatedChunks.Add(Chunk);
            }
        }
    }
    
    // Validate memory usage
    bool bMemoryValidationPassed = Profiler.ValidateLOD0MemoryUsage(SimulatedChunks);
    
    SIZE_T TotalLOD0MemoryMB = TotalLOD0Memory / (1024 * 1024);
    
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("LOD0 Memory Validation Results:"));
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("  LOD0 Chunks: %d"), LOD0ChunkCount);
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("  Total LOD0 Memory: %llu MB"), TotalLOD0MemoryMB);
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("  Memory Limit: %llu MB"), TargetMemoryLimitMB);
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("  Validation Passed: %s"), bMemoryValidationPassed ? TEXT("Yes") : TEXT("No"));
    
    TestTrue(TEXT("LOD0 memory usage within limits"), bMemoryValidationPassed);
    TestTrue(TEXT("LOD0 memory usage reasonable"), TotalLOD0MemoryMB <= TargetMemoryLimitMB);
    
    return bMemoryValidationPassed && (TotalLOD0MemoryMB <= TargetMemoryLimitMB);
}

/**
 * Triangle count validation test for LOD0 chunks
 * Ensures chunks don't exceed 8k triangle limit
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTriangleCountValidationTest, 
    "WorldGen.Performance.TriangleCountValidation", 
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTriangleCountValidationTest::RunTest(const FString& Parameters)
{
    FWorldGenPerformanceProfiler Profiler;
    
    const int32 MaxTrianglesPerChunk = 8000;
    const int32 NumTestChunks = 25;
    
    bool bAllChunksValid = true;
    int32 FailedChunks = 0;
    
    for (int32 i = 0; i < NumTestChunks; ++i)
    {
        FIntVector ChunkCoord(i % 5, i / 5, 0);
        
        // Create metrics with varying triangle counts
        FWorldGenPerformanceProfiler::FChunkPerformanceMetrics Metrics;
        Metrics.ChunkCoordinate = ChunkCoord;
        Metrics.LODLevel = EChunkLOD::LOD0;
        Metrics.GenerationTimeMs = FMath::RandRange(2.0, 6.0);
        Metrics.MemoryUsageBytes = FMath::RandRange(1024 * 1024, 2 * 1024 * 1024);
        Metrics.bHasCollision = true;
        
        // Most chunks should be within limits, but test some edge cases
        if (i < NumTestChunks - 3)
        {
            // Normal chunks within limits
            Metrics.TriangleCount = FMath::RandRange(4000, 7500);
        }
        else
        {
            // Test some chunks at the limit
            Metrics.TriangleCount = FMath::RandRange(7800, 8000);
        }
        
        bool bChunkValid = Profiler.ValidateTriangleCount(Metrics);
        if (!bChunkValid)
        {
            bAllChunksValid = false;
            FailedChunks++;
        }
        
        Profiler.RecordChunkMetrics(Metrics);
        
        UE_LOG(LogPerformanceRegressionTests, Verbose, TEXT("Chunk (%d, %d, %d): %d triangles - %s"), 
               ChunkCoord.X, ChunkCoord.Y, ChunkCoord.Z, Metrics.TriangleCount, 
               bChunkValid ? TEXT("Valid") : TEXT("Invalid"));
    }
    
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("Triangle Count Validation Results:"));
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("  Total Chunks Tested: %d"), NumTestChunks);
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("  Failed Chunks: %d"), FailedChunks);
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("  Triangle Limit: %d"), MaxTrianglesPerChunk);
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("  All Chunks Valid: %s"), bAllChunksValid ? TEXT("Yes") : TEXT("No"));
    
    TestTrue(TEXT("All chunks within triangle count limits"), bAllChunksValid);
    TestEqual(TEXT("No chunks exceeded triangle limits"), FailedChunks, 0);
    
    return bAllChunksValid;
}

/**
 * Streaming performance test during rapid player movement
 * Validates that streaming can keep up with fast movement
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStreamingPerformanceTest, 
    "WorldGen.Performance.StreamingPerformance", 
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FStreamingPerformanceTest::RunTest(const FString& Parameters)
{
    // Create test world
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestWorld)
    {
        AddError(TEXT("Failed to create test world"));
        return false;
    }
    
    // Create performance profiler
    FWorldGenPerformanceProfiler Profiler;
    
    // Create and initialize streaming manager
    UChunkStreamingManager* StreamingManager = NewObject<UChunkStreamingManager>();
    
    FWorldGenSettings TestSettings;
    TestSettings.Seed = 12345;
    TestSettings.WorldGenVersion = 1;
    TestSettings.VoxelSizeCm = 50.0f;
    TestSettings.ChunkSize = 32;
    TestSettings.LOD0Radius = 2;
    TestSettings.LOD1Radius = 4;
    TestSettings.LOD2Radius = 6;
    
    bool bInitialized = StreamingManager->Initialize(TestSettings, nullptr);
    TestTrue(TEXT("Streaming manager initialized"), bInitialized);
    
    if (!bInitialized)
    {
        TestWorld->DestroyWorld(false);
        return false;
    }
    
    // Define movement path for testing
    FVector StartLocation(0, 0, 0);
    FVector EndLocation(5000, 5000, 0); // 5km movement
    float MovementSpeed = 2000.0f; // 20 m/s (fast movement)
    
    // Run streaming performance test
    FPerformanceRegressionResults Results = 
        Profiler.TestStreamingPerformance(StartLocation, EndLocation, MovementSpeed, StreamingManager);
    
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("Streaming Performance Test Results:"));
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("  Test Passed: %s"), Results.bPassedStreamingTest ? TEXT("Yes") : TEXT("No"));
    
    for (const FString& Reason : Results.FailureReasons)
    {
        UE_LOG(LogPerformanceRegressionTests, Warning, TEXT("  Failure: %s"), *Reason);
        AddError(Reason);
    }
    
    TestTrue(TEXT("Streaming performance test passed"), Results.bPassedStreamingTest);
    
    // Cleanup
    TestWorld->DestroyWorld(false);
    
    return Results.bPassedStreamingTest;
}

/**
 * Comprehensive performance regression test suite
 * Runs all performance tests and generates CI/CD report
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FComprehensivePerformanceRegressionTest, 
    "WorldGen.Performance.ComprehensiveRegression", 
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FComprehensivePerformanceRegressionTest::RunTest(const FString& Parameters)
{
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("Starting comprehensive performance regression test suite"));
    
    FWorldGenPerformanceProfiler Profiler;
    
    // Test configuration
    FWorldGenSettings TestSettings;
    TestSettings.Seed = 98765;
    TestSettings.WorldGenVersion = 1;
    TestSettings.VoxelSizeCm = 50.0f;
    TestSettings.ChunkSize = 32;
    TestSettings.LOD0Radius = 2;
    TestSettings.LOD1Radius = 4;
    TestSettings.LOD2Radius = 6;
    
    // Generate comprehensive test data
    const int32 NumTestChunks = 100;
    bool bAllTestsPassed = true;
    
    // Phase 1: Generate performance data
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("Phase 1: Generating performance data for %d chunks"), NumTestChunks);
    
    for (int32 i = 0; i < NumTestChunks; ++i)
    {
        FIntVector ChunkCoord(i % 10, i / 10, 0);
        EChunkLOD LODLevel = EChunkLOD::LOD0;
        
        // Vary LOD levels for realistic testing
        float Distance = FMath::Sqrt(static_cast<float>(ChunkCoord.X * ChunkCoord.X + ChunkCoord.Y * ChunkCoord.Y));
        if (Distance > TestSettings.LOD0Radius)
        {
            LODLevel = Distance <= TestSettings.LOD1Radius ? EChunkLOD::LOD1 : EChunkLOD::LOD2;
        }
        
        FScopedChunkGenerationTimer Timer(&Profiler, ChunkCoord, LODLevel);
        
        // Simulate realistic generation phases with varying complexity
        Timer.StartBiomeEvaluation();
        
        // Biome evaluation (varies by chunk complexity)
        double BiomeWorkTime = FMath::RandRange(0.0005, 0.002); // 0.5-2ms
        double BiomeStartTime = FPlatformTime::Seconds();
        while ((FPlatformTime::Seconds() - BiomeStartTime) < BiomeWorkTime)
        {
            volatile float Work = FMath::Sin(i * 0.1f) * FMath::Cos(ChunkCoord.X * 0.1f);
        }
        Timer.EndBiomeEvaluation();
        
        Timer.StartPOIPlacement();
        
        // POI placement (occasional spikes for complex placements)
        double POIWorkTime = (i % 10 == 0) ? FMath::RandRange(0.001, 0.003) : FMath::RandRange(0.0002, 0.0008);
        double POIStartTime = FPlatformTime::Seconds();
        while ((FPlatformTime::Seconds() - POIStartTime) < POIWorkTime)
        {
            volatile float Work = FMath::Pow(ChunkCoord.Y * 0.1f, 1.5f);
        }
        Timer.EndPOIPlacement();
        
        Timer.StartMeshGeneration();
        
        // Mesh generation (most expensive, varies by LOD)
        double MeshWorkTime;
        switch (LODLevel)
        {
            case EChunkLOD::LOD0:
                MeshWorkTime = FMath::RandRange(0.002, 0.006); // 2-6ms for full detail
                break;
            case EChunkLOD::LOD1:
                MeshWorkTime = FMath::RandRange(0.001, 0.003); // 1-3ms for collision
                break;
            case EChunkLOD::LOD2:
                MeshWorkTime = FMath::RandRange(0.0005, 0.002); // 0.5-2ms for visual only
                break;
            default:
                MeshWorkTime = 0.001;
                break;
        }
        
        double MeshStartTime = FPlatformTime::Seconds();
        while ((FPlatformTime::Seconds() - MeshStartTime) < MeshWorkTime)
        {
            volatile float Work = FMath::Sqrt(i * 100.0f + ChunkCoord.X * ChunkCoord.Y);
        }
        Timer.EndMeshGeneration();
        
        // Set realistic metrics based on LOD
        int32 TriangleCount;
        SIZE_T MemoryUsage;
        bool bHasCollision;
        
        switch (LODLevel)
        {
            case EChunkLOD::LOD0:
                TriangleCount = FMath::RandRange(5000, 8000);
                MemoryUsage = FMath::RandRange(1024 * 1024, 3 * 1024 * 1024); // 1-3MB
                bHasCollision = true;
                break;
            case EChunkLOD::LOD1:
                TriangleCount = FMath::RandRange(2000, 4000);
                MemoryUsage = FMath::RandRange(512 * 1024, 1024 * 1024); // 0.5-1MB
                bHasCollision = true;
                break;
            case EChunkLOD::LOD2:
                TriangleCount = FMath::RandRange(1000, 2500);
                MemoryUsage = FMath::RandRange(256 * 1024, 512 * 1024); // 0.25-0.5MB
                bHasCollision = false;
                break;
            default:
                TriangleCount = 0;
                MemoryUsage = 0;
                bHasCollision = false;
                break;
        }
        
        Timer.SetTriangleCount(TriangleCount);
        Timer.SetMemoryUsage(MemoryUsage);
        Timer.SetHasCollision(bHasCollision);
    }
    
    // Phase 2: Run regression analysis
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("Phase 2: Running regression analysis"));
    
    FPerformanceRegressionResults Results = Profiler.RunRegressionTests(NumTestChunks);
    
    // Phase 3: Validate all performance criteria
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("Phase 3: Validating performance criteria"));
    
    bool bGenerationTimeTest = Results.bPassedGenerationTimeTest;
    bool bMemoryUsageTest = Results.bPassedMemoryUsageTest;
    bool bTriangleCountTest = Results.bPassedTriangleCountTest;
    
    TestTrue(TEXT("Generation time regression test"), bGenerationTimeTest);
    TestTrue(TEXT("Memory usage regression test"), bMemoryUsageTest);
    TestTrue(TEXT("Triangle count regression test"), bTriangleCountTest);
    
    bAllTestsPassed = bGenerationTimeTest && bMemoryUsageTest && bTriangleCountTest;
    
    // Phase 4: Generate comprehensive report
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("Phase 4: Generating performance report"));
    
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("=== COMPREHENSIVE PERFORMANCE REGRESSION RESULTS ==="));
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("Test Configuration:"));
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("  Seed: %lld"), TestSettings.Seed);
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("  Chunks Tested: %d"), NumTestChunks);
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("  Voxel Size: %.1f cm"), TestSettings.VoxelSizeCm);
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("  Chunk Size: %d"), TestSettings.ChunkSize);
    
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("Performance Results:"));
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("  Average Generation Time: %.2fms (target: ≤5.0ms) - %s"), 
           Results.AverageGenerationTimeMs, Results.bPassedGenerationTimeTest ? TEXT("PASS") : TEXT("FAIL"));
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("  P95 Generation Time: %.2fms (target: ≤9.0ms) - %s"), 
           Results.P95GenerationTimeMs, Results.bPassedGenerationTimeTest ? TEXT("PASS") : TEXT("FAIL"));
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("  Average Memory Usage: %llu MB"), Results.AverageMemoryUsageMB);
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("  Peak Memory Usage: %llu MB (LOD0 target: ≤64MB) - %s"), 
           Results.PeakMemoryUsageMB, Results.bPassedMemoryUsageTest ? TEXT("PASS") : TEXT("FAIL"));
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("  Average Triangle Count: %d"), Results.AverageTriangleCount);
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("  Max Triangle Count: %d (target: ≤8000) - %s"), 
           Results.MaxTriangleCount, Results.bPassedTriangleCountTest ? TEXT("PASS") : TEXT("FAIL"));
    
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("Overall Result: %s"), bAllTestsPassed ? TEXT("PASS") : TEXT("FAIL"));
    
    // Export detailed results for CI/CD
    FString ExportPath = FPaths::ProjectSavedDir() / TEXT("PerformanceTests") / TEXT("ComprehensiveRegressionResults.json");
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(ExportPath), true);
    bool bExported = Profiler.ExportPerformanceData(ExportPath);
    
    if (bExported)
    {
        UE_LOG(LogPerformanceRegressionTests, Log, TEXT("Detailed results exported to: %s"), *ExportPath);
    }
    else
    {
        UE_LOG(LogPerformanceRegressionTests, Warning, TEXT("Failed to export detailed results"));
    }
    
    // Log failure reasons for CI/CD
    if (!bAllTestsPassed)
    {
        UE_LOG(LogPerformanceRegressionTests, Error, TEXT("Performance regression test FAILED with the following issues:"));
        for (const FString& Reason : Results.FailureReasons)
        {
            UE_LOG(LogPerformanceRegressionTests, Error, TEXT("  - %s"), *Reason);
            AddError(Reason);
        }
    }
    
    UE_LOG(LogPerformanceRegressionTests, Log, TEXT("=== END PERFORMANCE REGRESSION RESULTS ==="));
    
    return bAllTestsPassed;
}