#include "PerformanceProfiler.h"
#include "ChunkStreamingManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "Engine/Engine.h"

DEFINE_LOG_CATEGORY(LogWorldGenPerformance);

FWorldGenPerformanceProfiler::FWorldGenPerformanceProfiler()
{
    RecentChunkMetrics.Reserve(MaxMetricsHistory);
    RecentStreamingMetrics.Reserve(MaxMetricsHistory / 10); // Less frequent streaming metrics
}

FWorldGenPerformanceProfiler::~FWorldGenPerformanceProfiler()
{
    ClearMetrics();
}

void FWorldGenPerformanceProfiler::RecordChunkMetrics(const FChunkPerformanceMetrics& Metrics)
{
    FScopeLock Lock(&MetricsCriticalSection);
    
    // Add new metrics
    RecentChunkMetrics.Add(Metrics);
    
    // Maintain history limit
    if (RecentChunkMetrics.Num() > MaxMetricsHistory)
    {
        RecentChunkMetrics.RemoveAt(0);
    }
    
    // Update Unreal Insights stats
    UpdateUnrealInsightsStats(Metrics);
    
    // Log performance warnings if targets are exceeded
    LogPerformanceWarnings(Metrics);
}

void FWorldGenPerformanceProfiler::RecordStreamingMetrics(const FStreamingPerformanceMetrics& Metrics)
{
    FScopeLock Lock(&MetricsCriticalSection);
    
    RecentStreamingMetrics.Add(Metrics);
    
    if (RecentStreamingMetrics.Num() > MaxMetricsHistory / 10)
    {
        RecentStreamingMetrics.RemoveAt(0);
    }
    
    // Update memory stats
    SET_MEMORY_STAT(STAT_ChunkMemory, Metrics.TotalMemoryUsageMB * 1024 * 1024);
    SET_DWORD_STAT(STAT_ActiveChunks, Metrics.TotalActiveChunks);
}

void FWorldGenPerformanceProfiler::GetCurrentStats(double& OutAverageGenerationTimeMs, double& OutP95GenerationTimeMs,
                                                  SIZE_T& OutTotalMemoryUsageMB, SIZE_T& OutLOD0MemoryUsageMB,
                                                  int32& OutAverageTriangleCount, int32& OutMaxTriangleCount) const
{
    FScopeLock Lock(&MetricsCriticalSection);
    
    if (RecentChunkMetrics.Num() == 0)
    {
        OutAverageGenerationTimeMs = OutP95GenerationTimeMs = 0.0;
        OutTotalMemoryUsageMB = OutLOD0MemoryUsageMB = 0;
        OutAverageTriangleCount = OutMaxTriangleCount = 0;
        return;
    }
    
    // Calculate generation time stats
    TArray<double> GenerationTimes;
    TArray<int32> TriangleCounts;
    SIZE_T TotalMemory = 0;
    SIZE_T LOD0Memory = 0;
    
    for (const FChunkPerformanceMetrics& Metrics : RecentChunkMetrics)
    {
        GenerationTimes.Add(Metrics.GenerationTimeMs);
        TriangleCounts.Add(Metrics.TriangleCount);
        TotalMemory += Metrics.MemoryUsageBytes;
        
        if (Metrics.LODLevel == EChunkLOD::LOD0)
        {
            LOD0Memory += Metrics.MemoryUsageBytes;
        }
    }
    
    // Sort for percentile calculation
    GenerationTimes.Sort();
    TriangleCounts.Sort();
    
    // Calculate averages
    double TotalTime = 0.0;
    int32 TotalTriangles = 0;
    for (int32 i = 0; i < GenerationTimes.Num(); ++i)
    {
        TotalTime += GenerationTimes[i];
        TotalTriangles += TriangleCounts[i];
    }
    
    OutAverageGenerationTimeMs = TotalTime / GenerationTimes.Num();
    OutP95GenerationTimeMs = CalculatePercentile(GenerationTimes, 0.95f);
    OutTotalMemoryUsageMB = TotalMemory / (1024 * 1024);
    OutLOD0MemoryUsageMB = LOD0Memory / (1024 * 1024);
    OutAverageTriangleCount = TotalTriangles / TriangleCounts.Num();
    OutMaxTriangleCount = TriangleCounts.Last();
}

FPerformanceRegressionResults FWorldGenPerformanceProfiler::RunRegressionTests(int32 NumTestChunks)
{
    FPerformanceRegressionResults Results;
    
    UE_LOG(LogWorldGenPerformance, Log, TEXT("Starting performance regression tests with %d chunks"), NumTestChunks);
    
    // Get recent metrics for analysis
    FScopeLock Lock(&MetricsCriticalSection);
    
    if (RecentChunkMetrics.Num() < NumTestChunks)
    {
        Results.FailureReasons.Add(FString::Printf(TEXT("Insufficient metrics data: %d available, %d required"), 
                                                  RecentChunkMetrics.Num(), NumTestChunks));
        return Results;
    }
    
    // Use the most recent metrics
    TArray<FChunkPerformanceMetrics> TestMetrics;
    for (int32 i = FMath::Max(0, RecentChunkMetrics.Num() - NumTestChunks); i < RecentChunkMetrics.Num(); ++i)
    {
        TestMetrics.Add(RecentChunkMetrics[i]);
    }
    
    // Calculate performance statistics
    TArray<double> GenerationTimes;
    TArray<SIZE_T> MemoryUsages;
    TArray<int32> TriangleCounts;
    SIZE_T LOD0Memory = 0;
    int32 LOD0Count = 0;
    
    for (const FChunkPerformanceMetrics& Metrics : TestMetrics)
    {
        GenerationTimes.Add(Metrics.GenerationTimeMs);
        MemoryUsages.Add(Metrics.MemoryUsageBytes);
        TriangleCounts.Add(Metrics.TriangleCount);
        
        if (Metrics.LODLevel == EChunkLOD::LOD0)
        {
            LOD0Memory += Metrics.MemoryUsageBytes;
            LOD0Count++;
        }
    }
    
    GenerationTimes.Sort();
    MemoryUsages.Sort();
    TriangleCounts.Sort();
    
    // Calculate statistics
    Results.AverageGenerationTimeMs = GenerationTimes.Num() > 0 ? 
        Algo::Accumulate(GenerationTimes, 0.0) / GenerationTimes.Num() : 0.0;
    Results.P95GenerationTimeMs = CalculatePercentile(GenerationTimes, 0.95f);
    Results.AverageMemoryUsageMB = MemoryUsages.Num() > 0 ? 
        Algo::Accumulate(MemoryUsages, SIZE_T(0)) / (MemoryUsages.Num() * 1024 * 1024) : 0;
    Results.PeakMemoryUsageMB = MemoryUsages.Num() > 0 ? MemoryUsages.Last() / (1024 * 1024) : 0;
    Results.AverageTriangleCount = TriangleCounts.Num() > 0 ? 
        Algo::Accumulate(TriangleCounts, 0) / TriangleCounts.Num() : 0;
    Results.MaxTriangleCount = TriangleCounts.Num() > 0 ? TriangleCounts.Last() : 0;
    
    // Test against targets
    Results.bPassedGenerationTimeTest = Results.AverageGenerationTimeMs <= TargetAverageGenerationTimeMs &&
                                       Results.P95GenerationTimeMs <= TargetP95GenerationTimeMs;
    
    Results.bPassedMemoryUsageTest = (LOD0Memory / (1024 * 1024)) <= TargetLOD0MemoryLimitMB;
    
    Results.bPassedTriangleCountTest = Results.MaxTriangleCount <= TargetMaxTrianglesPerChunk;
    
    // Log failures
    if (!Results.bPassedGenerationTimeTest)
    {
        FString Reason = FString::Printf(TEXT("Generation time exceeded targets - Avg: %.2fms (target: %.2fms), P95: %.2fms (target: %.2fms)"),
                                        Results.AverageGenerationTimeMs, TargetAverageGenerationTimeMs,
                                        Results.P95GenerationTimeMs, TargetP95GenerationTimeMs);
        Results.FailureReasons.Add(Reason);
        UE_LOG(LogWorldGenPerformance, Warning, TEXT("%s"), *Reason);
    }
    
    if (!Results.bPassedMemoryUsageTest)
    {
        FString Reason = FString::Printf(TEXT("LOD0 memory usage exceeded target - Used: %llu MB (target: %llu MB)"),
                                        LOD0Memory / (1024 * 1024), TargetLOD0MemoryLimitMB);
        Results.FailureReasons.Add(Reason);
        UE_LOG(LogWorldGenPerformance, Warning, TEXT("%s"), *Reason);
    }
    
    if (!Results.bPassedTriangleCountTest)
    {
        FString Reason = FString::Printf(TEXT("Triangle count exceeded target - Max: %d (target: %d)"),
                                        Results.MaxTriangleCount, TargetMaxTrianglesPerChunk);
        Results.FailureReasons.Add(Reason);
        UE_LOG(LogWorldGenPerformance, Warning, TEXT("%s"), *Reason);
    }
    
    Results.bPassedStreamingTest = true; // Will be set by streaming test
    
    UE_LOG(LogWorldGenPerformance, Log, TEXT("Performance regression test completed - Passed: %s"),
           (Results.bPassedGenerationTimeTest && Results.bPassedMemoryUsageTest && Results.bPassedTriangleCountTest) ? 
           TEXT("Yes") : TEXT("No"));
    
    return Results;
}

bool FWorldGenPerformanceProfiler::ValidateLOD0MemoryUsage(const TArray<FStreamingChunk>& LoadedChunks) const
{
    SIZE_T TotalLOD0Memory = 0;
    int32 LOD0ChunkCount = 0;
    
    for (const FStreamingChunk& Chunk : LoadedChunks)
    {
        if (Chunk.CurrentLOD == EChunkLOD::LOD0)
        {
            // Estimate memory usage based on chunk parameters
            // This is a simplified calculation - in practice you'd track actual memory usage
            SIZE_T EstimatedMemory = 32 * 32 * 32 * sizeof(float) * 2; // Rough estimate for voxel data + mesh
            TotalLOD0Memory += EstimatedMemory;
            LOD0ChunkCount++;
        }
    }
    
    SIZE_T TotalLOD0MemoryMB = TotalLOD0Memory / (1024 * 1024);
    bool bWithinLimits = TotalLOD0MemoryMB <= TargetLOD0MemoryLimitMB;
    
    if (!bWithinLimits)
    {
        UE_LOG(LogWorldGenPerformance, Warning, TEXT("LOD0 memory usage validation failed - Used: %llu MB, Target: %llu MB, Chunks: %d"),
               TotalLOD0MemoryMB, TargetLOD0MemoryLimitMB, LOD0ChunkCount);
    }
    
    return bWithinLimits;
}

bool FWorldGenPerformanceProfiler::ValidateTriangleCount(const FChunkPerformanceMetrics& Metrics) const
{
    bool bWithinLimits = Metrics.TriangleCount <= TargetMaxTrianglesPerChunk;
    
    if (!bWithinLimits)
    {
        UE_LOG(LogWorldGenPerformance, Warning, TEXT("Triangle count validation failed for chunk (%d, %d, %d) - Count: %d, Target: %d"),
               Metrics.ChunkCoordinate.X, Metrics.ChunkCoordinate.Y, Metrics.ChunkCoordinate.Z,
               Metrics.TriangleCount, TargetMaxTrianglesPerChunk);
    }
    
    return bWithinLimits;
}

template<typename T>
T FWorldGenPerformanceProfiler::CalculatePercentile(const TArray<T>& SortedValues, float Percentile) const
{
    if (SortedValues.Num() == 0)
    {
        return T{};
    }
    
    if (SortedValues.Num() == 1)
    {
        return SortedValues[0];
    }
    
    float Index = Percentile * (SortedValues.Num() - 1);
    int32 LowerIndex = FMath::FloorToInt(Index);
    int32 UpperIndex = FMath::CeilToInt(Index);
    
    if (LowerIndex == UpperIndex)
    {
        return SortedValues[LowerIndex];
    }
    
    float Weight = Index - LowerIndex;
    return SortedValues[LowerIndex] * (1.0f - Weight) + SortedValues[UpperIndex] * Weight;
}

void FWorldGenPerformanceProfiler::UpdateUnrealInsightsStats(const FChunkPerformanceMetrics& Metrics)
{
    // Update cycle stats for Unreal Insights
    SET_FLOAT_STAT(STAT_ChunkGeneration, Metrics.GenerationTimeMs);
    SET_FLOAT_STAT(STAT_BiomeEvaluation, Metrics.BiomeEvaluationTimeMs);
    SET_FLOAT_STAT(STAT_POIPlacement, Metrics.POIPlacementTimeMs);
    SET_FLOAT_STAT(STAT_MeshGeneration, Metrics.MeshGenerationTimeMs);
    SET_MEMORY_STAT(STAT_ChunkMemory, Metrics.MemoryUsageBytes);
}

void FWorldGenPerformanceProfiler::LogPerformanceWarnings(const FChunkPerformanceMetrics& Metrics) const
{
    if (Metrics.GenerationTimeMs > TargetP95GenerationTimeMs)
    {
        UE_LOG(LogWorldGenPerformance, Warning, TEXT("Chunk generation exceeded P95 target - Chunk: (%d, %d, %d), Time: %.2fms, Target: %.2fms"),
               Metrics.ChunkCoordinate.X, Metrics.ChunkCoordinate.Y, Metrics.ChunkCoordinate.Z,
               Metrics.GenerationTimeMs, TargetP95GenerationTimeMs);
    }
    else if (Metrics.GenerationTimeMs > TargetAverageGenerationTimeMs)
    {
        UE_LOG(LogWorldGenPerformance, Verbose, TEXT("Chunk generation exceeded average target - Chunk: (%d, %d, %d), Time: %.2fms, Target: %.2fms"),
               Metrics.ChunkCoordinate.X, Metrics.ChunkCoordinate.Y, Metrics.ChunkCoordinate.Z,
               Metrics.GenerationTimeMs, TargetAverageGenerationTimeMs);
    }
    
    if (Metrics.TriangleCount > TargetMaxTrianglesPerChunk)
    {
        UE_LOG(LogWorldGenPerformance, Warning, TEXT("Chunk triangle count exceeded target - Chunk: (%d, %d, %d), Count: %d, Target: %d"),
               Metrics.ChunkCoordinate.X, Metrics.ChunkCoordinate.Y, Metrics.ChunkCoordinate.Z,
               Metrics.TriangleCount, TargetMaxTrianglesPerChunk);
    }
}

// FScopedChunkGenerationTimer Implementation
FScopedChunkGenerationTimer::FScopedChunkGenerationTimer(FWorldGenPerformanceProfiler* InProfiler, 
                                                        const FIntVector& ChunkCoordinate, 
                                                        EChunkLOD LODLevel)
    : Profiler(InProfiler)
    , StartTime(FPlatformTime::Seconds())
    , BiomeStartTime(0.0)
    , POIStartTime(0.0)
    , MeshStartTime(0.0)
{
    Metrics.ChunkCoordinate = ChunkCoordinate;
    Metrics.LODLevel = LODLevel;
    Metrics.Timestamp = FDateTime::Now();
    
    SCOPE_CYCLE_COUNTER(STAT_ChunkGeneration);
}

FScopedChunkGenerationTimer::~FScopedChunkGenerationTimer()
{
    double EndTime = FPlatformTime::Seconds();
    Metrics.GenerationTimeMs = (EndTime - StartTime) * 1000.0;
    
    if (Profiler)
    {
        Profiler->RecordChunkMetrics(Metrics);
    }
}

void FScopedChunkGenerationTimer::StartBiomeEvaluation()
{
    BiomeStartTime = FPlatformTime::Seconds();
    SCOPE_CYCLE_COUNTER(STAT_BiomeEvaluation);
}

void FScopedChunkGenerationTimer::EndBiomeEvaluation()
{
    if (BiomeStartTime > 0.0)
    {
        double EndTime = FPlatformTime::Seconds();
        Metrics.BiomeEvaluationTimeMs = (EndTime - BiomeStartTime) * 1000.0;
        BiomeStartTime = 0.0;
    }
}

void FScopedChunkGenerationTimer::StartPOIPlacement()
{
    POIStartTime = FPlatformTime::Seconds();
    SCOPE_CYCLE_COUNTER(STAT_POIPlacement);
}

void FScopedChunkGenerationTimer::EndPOIPlacement()
{
    if (POIStartTime > 0.0)
    {
        double EndTime = FPlatformTime::Seconds();
        Metrics.POIPlacementTimeMs = (EndTime - POIStartTime) * 1000.0;
        POIStartTime = 0.0;
    }
}

void FScopedChunkGenerationTimer::StartMeshGeneration()
{
    MeshStartTime = FPlatformTime::Seconds();
    SCOPE_CYCLE_COUNTER(STAT_MeshGeneration);
}

void FScopedChunkGenerationTimer::EndMeshGeneration()
{
    if (MeshStartTime > 0.0)
    {
        double EndTime = FPlatformTime::Seconds();
        Metrics.MeshGenerationTimeMs = (EndTime - MeshStartTime) * 1000.0;
        MeshStartTime = 0.0;
    }
}

void FScopedChunkGenerationTimer::SetTriangleCount(int32 TriangleCount)
{
    Metrics.TriangleCount = TriangleCount;
}

void FScopedChunkGenerationTimer::SetMemoryUsage(SIZE_T MemoryUsageBytes)
{
    Metrics.MemoryUsageBytes = MemoryUsageBytes;
}

void FScopedChunkGenerationTimer::SetHasCollision(bool bHasCollision)
{
    Metrics.bHasCollision = bHasCollision;
}

FPerformanceRegressionResults FWorldGenPerformanceProfiler::TestStreamingPerformance(
    const FVector& StartLocation, const FVector& EndLocation, float MovementSpeed, UChunkStreamingManager* StreamingManager)
{
    FPerformanceRegressionResults Results;
    
    if (!StreamingManager)
    {
        Results.FailureReasons.Add(TEXT("StreamingManager is null"));
        Results.bPassedStreamingTest = false;
        return Results;
    }
    
    UE_LOG(LogWorldGenPerformance, Log, TEXT("Starting streaming performance test - Movement from (%.1f, %.1f, %.1f) to (%.1f, %.1f, %.1f) at %.1f units/sec"),
           StartLocation.X, StartLocation.Y, StartLocation.Z, EndLocation.X, EndLocation.Y, EndLocation.Z, MovementSpeed);
    
    // Create a mock player anchor for testing
    UWorld* World = StreamingManager->GetWorld();
    if (!World)
    {
        Results.FailureReasons.Add(TEXT("No valid world context for streaming test"));
        Results.bPassedStreamingTest = false;
        return Results;
    }
    
    AActor* TestAnchor = World->SpawnActor<AActor>();
    if (!TestAnchor)
    {
        Results.FailureReasons.Add(TEXT("Failed to spawn test anchor"));
        Results.bPassedStreamingTest = false;
        return Results;
    }
    
    TestAnchor->SetActorLocation(StartLocation);
    StreamingManager->SetPlayerAnchor(TestAnchor);
    
    // Record initial metrics
    FStreamingPerformanceMetrics InitialMetrics;
    int32 InitialLoadedChunks, InitialGeneratingChunks;
    float InitialAvgTime, InitialP95Time;
    StreamingManager->GetStreamingStats(InitialLoadedChunks, InitialGeneratingChunks, InitialAvgTime, InitialP95Time);
    
    InitialMetrics.TotalActiveChunks = InitialLoadedChunks;
    InitialMetrics.AverageLoadTimeMs = InitialAvgTime;
    InitialMetrics.P95LoadTimeMs = InitialP95Time;
    
    // Simulate movement
    FVector Direction = (EndLocation - StartLocation).GetSafeNormal();
    float TotalDistance = FVector::Dist(StartLocation, EndLocation);
    float TestDuration = TotalDistance / MovementSpeed;
    
    double TestStartTime = FPlatformTime::Seconds();
    double LastUpdateTime = TestStartTime;
    int32 ChunksLoadedDuringTest = 0;
    int32 ChunksUnloadedDuringTest = 0;
    TArray<double> LoadTimes;
    
    // Movement simulation loop
    while ((FPlatformTime::Seconds() - TestStartTime) < TestDuration)
    {
        double CurrentTime = FPlatformTime::Seconds();
        float ElapsedTime = CurrentTime - TestStartTime;
        float Progress = FMath::Clamp(ElapsedTime / TestDuration, 0.0f, 1.0f);
        
        // Update anchor position
        FVector CurrentPosition = FMath::Lerp(StartLocation, EndLocation, Progress);
        TestAnchor->SetActorLocation(CurrentPosition);
        
        // Update streaming
        float DeltaTime = CurrentTime - LastUpdateTime;
        StreamingManager->UpdateStreaming(DeltaTime);
        
        // Record metrics every 100ms
        if ((CurrentTime - LastUpdateTime) >= 0.1)
        {
            int32 CurrentLoadedChunks, CurrentGeneratingChunks;
            float CurrentAvgTime, CurrentP95Time;
            StreamingManager->GetStreamingStats(CurrentLoadedChunks, CurrentGeneratingChunks, CurrentAvgTime, CurrentP95Time);
            
            // Track chunk loading/unloading
            if (CurrentLoadedChunks > InitialLoadedChunks)
            {
                ChunksLoadedDuringTest += (CurrentLoadedChunks - InitialLoadedChunks);
            }
            else if (CurrentLoadedChunks < InitialLoadedChunks)
            {
                ChunksUnloadedDuringTest += (InitialLoadedChunks - CurrentLoadedChunks);
            }
            
            LoadTimes.Add(CurrentAvgTime);
            InitialLoadedChunks = CurrentLoadedChunks;
            LastUpdateTime = CurrentTime;
        }
        
        // Small sleep to prevent busy waiting
        FPlatformProcess::Sleep(0.01f);
    }
    
    // Calculate final metrics
    double ActualTestDuration = FPlatformTime::Seconds() - TestStartTime;
    Results.bPassedStreamingTest = true;
    
    // Validate streaming performance
    if (LoadTimes.Num() > 0)
    {
        LoadTimes.Sort();
        double AverageLoadTime = Algo::Accumulate(LoadTimes, 0.0) / LoadTimes.Num();
        double P95LoadTime = CalculatePercentile(LoadTimes, 0.95f);
        
        // Check if load times are within acceptable limits during rapid movement
        if (AverageLoadTime > TargetAverageGenerationTimeMs * 2.0) // Allow 2x during rapid movement
        {
            Results.bPassedStreamingTest = false;
            Results.FailureReasons.Add(FString::Printf(TEXT("Average load time during rapid movement too high: %.2fms"), AverageLoadTime));
        }
        
        if (P95LoadTime > TargetP95GenerationTimeMs * 2.0) // Allow 2x during rapid movement
        {
            Results.bPassedStreamingTest = false;
            Results.FailureReasons.Add(FString::Printf(TEXT("P95 load time during rapid movement too high: %.2fms"), P95LoadTime));
        }
    }
    
    UE_LOG(LogWorldGenPerformance, Log, TEXT("Streaming performance test completed - Duration: %.2fs, Chunks Loaded: %d, Chunks Unloaded: %d, Passed: %s"),
           ActualTestDuration, ChunksLoadedDuringTest, ChunksUnloadedDuringTest, Results.bPassedStreamingTest ? TEXT("Yes") : TEXT("No"));
    
    // Cleanup
    TestAnchor->Destroy();
    
    return Results;
}

bool FWorldGenPerformanceProfiler::ExportPerformanceData(const FString& FilePath) const
{
    FScopeLock Lock(&MetricsCriticalSection);
    
    FString JsonContent = TEXT("{\n");
    JsonContent += FString::Printf(TEXT("  \"exportTimestamp\": \"%s\",\n"), *FDateTime::Now().ToString());
    JsonContent += FString::Printf(TEXT("  \"totalChunkMetrics\": %d,\n"), RecentChunkMetrics.Num());
    JsonContent += FString::Printf(TEXT("  \"totalStreamingMetrics\": %d,\n"), RecentStreamingMetrics.Num());
    
    // Export performance targets for CI validation
    JsonContent += TEXT("  \"performanceTargets\": {\n");
    JsonContent += FString::Printf(TEXT("    \"averageGenerationTimeMs\": %.2f,\n"), TargetAverageGenerationTimeMs);
    JsonContent += FString::Printf(TEXT("    \"p95GenerationTimeMs\": %.2f,\n"), TargetP95GenerationTimeMs);
    JsonContent += FString::Printf(TEXT("    \"lod0MemoryLimitMB\": %llu,\n"), TargetLOD0MemoryLimitMB);
    JsonContent += FString::Printf(TEXT("    \"maxTrianglesPerChunk\": %d\n"), TargetMaxTrianglesPerChunk);
    JsonContent += TEXT("  },\n");
    
    // Export recent chunk metrics
    JsonContent += TEXT("  \"chunkMetrics\": [\n");
    for (int32 i = 0; i < RecentChunkMetrics.Num(); ++i)
    {
        const FChunkPerformanceMetrics& Metrics = RecentChunkMetrics[i];
        JsonContent += TEXT("    {\n");
        JsonContent += FString::Printf(TEXT("      \"chunkCoordinate\": [%d, %d, %d],\n"), 
                                      Metrics.ChunkCoordinate.X, Metrics.ChunkCoordinate.Y, Metrics.ChunkCoordinate.Z);
        JsonContent += FString::Printf(TEXT("      \"generationTimeMs\": %.3f,\n"), Metrics.GenerationTimeMs);
        JsonContent += FString::Printf(TEXT("      \"biomeEvaluationTimeMs\": %.3f,\n"), Metrics.BiomeEvaluationTimeMs);
        JsonContent += FString::Printf(TEXT("      \"poiPlacementTimeMs\": %.3f,\n"), Metrics.POIPlacementTimeMs);
        JsonContent += FString::Printf(TEXT("      \"meshGenerationTimeMs\": %.3f,\n"), Metrics.MeshGenerationTimeMs);
        JsonContent += FString::Printf(TEXT("      \"memoryUsageBytes\": %llu,\n"), Metrics.MemoryUsageBytes);
        JsonContent += FString::Printf(TEXT("      \"triangleCount\": %d,\n"), Metrics.TriangleCount);
        JsonContent += FString::Printf(TEXT("      \"lodLevel\": %d,\n"), static_cast<int32>(Metrics.LODLevel));
        JsonContent += FString::Printf(TEXT("      \"hasCollision\": %s,\n"), Metrics.bHasCollision ? TEXT("true") : TEXT("false"));
        JsonContent += FString::Printf(TEXT("      \"timestamp\": \"%s\"\n"), *Metrics.Timestamp.ToString());
        JsonContent += TEXT("    }");
        if (i < RecentChunkMetrics.Num() - 1)
        {
            JsonContent += TEXT(",");
        }
        JsonContent += TEXT("\n");
    }
    JsonContent += TEXT("  ]\n");
    JsonContent += TEXT("}\n");
    
    bool bSuccess = FFileHelper::SaveStringToFile(JsonContent, *FilePath);
    if (bSuccess)
    {
        UE_LOG(LogWorldGenPerformance, Log, TEXT("Performance data exported to: %s"), *FilePath);
    }
    else
    {
        UE_LOG(LogWorldGenPerformance, Error, TEXT("Failed to export performance data to: %s"), *FilePath);
    }
    
    return bSuccess;
}

void FWorldGenPerformanceProfiler::ClearMetrics()
{
    FScopeLock Lock(&MetricsCriticalSection);
    RecentChunkMetrics.Empty();
    RecentStreamingMetrics.Empty();
}