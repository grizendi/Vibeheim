#include "ChunkStreamingManager.h"
#include "Interfaces/IVoxelWorldService.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformFilemanager.h"
#include "Async/TaskGraphInterfaces.h"
#include "Stats/Stats.h"

DEFINE_LOG_CATEGORY(LogChunkStreaming);

// Console variable for collision toggle
TAutoConsoleVariable<bool> CVarCollisionUpToLOD1(
    TEXT("worldgen.CollisionUpToLOD1"),
    true,
    TEXT("Enable collision only up to LOD1 (performance testing toggle)"),
    ECVF_Default
);

void FChunkGenerationTask::DoWork()
{
    if (!Manager)
    {
        return;
    }

    // Simulate chunk generation work
    // In a real implementation, this would call into the voxel generation system
    double WorkStartTime = FPlatformTime::Seconds();
    
    // Simulate generation time based on LOD
    double TargetGenerationTime = 0.003; // 3ms base time
    switch (TargetLOD)
    {
        case EChunkLOD::LOD0:
            TargetGenerationTime = 0.004; // 4ms for full detail
            break;
        case EChunkLOD::LOD1:
            TargetGenerationTime = 0.003; // 3ms for collision
            break;
        case EChunkLOD::LOD2:
            TargetGenerationTime = 0.002; // 2ms for visual only
            break;
    }

    // Simulate work by spinning for the target time
    while ((FPlatformTime::Seconds() - WorkStartTime) < TargetGenerationTime)
    {
        // Simulate work
        FPlatformProcess::Sleep(0.0001f);
    }

    double TotalTime = FPlatformTime::Seconds() - StartTime;
    
    // Notify manager of completion
    Manager->OnChunkGenerationComplete(ChunkCoordinate, TargetLOD, TotalTime);
}

UChunkStreamingManager::UChunkStreamingManager()
    : VoxelWorldService(nullptr)
    , bIsInitialized(false)
    , TotalGenerationTime(0.0)
    , TotalGeneratedChunks(0)
    , LastStatsLogTime(0.0f)
    , LastRollingStatsLogTime(0.0f)
{
    // Initialize performance profiler
    PerformanceProfiler = MakeUnique<FWorldGenPerformanceProfiler>();
}

UChunkStreamingManager::~UChunkStreamingManager()
{
    // Ensure proper cleanup
    Shutdown();
}

bool UChunkStreamingManager::Initialize(const FWorldGenSettings& Settings, IVoxelWorldService* InVoxelWorldService)
{
    if (!InVoxelWorldService)
    {
        UE_LOG(LogChunkStreaming, Error, TEXT("[STRUCTURED_ERROR] Cannot initialize ChunkStreamingManager - VoxelWorldService is null - Seed: %lld"), Settings.Seed);
        return false;
    }

    CurrentSettings = Settings;
    VoxelWorldService = InVoxelWorldService;
    bIsInitialized = true;

    // Initialize performance tracking
    RecentGenerationTimes.Reserve(100);
    
    UE_LOG(LogChunkStreaming, Log, TEXT("ChunkStreamingManager initialized - Seed: %lld, LOD0:%d, LOD1:%d, LOD2:%d chunks"), 
           Settings.Seed, Settings.LOD0Radius, Settings.LOD1Radius, Settings.LOD2Radius);

    return true;
}

void UChunkStreamingManager::SetPlayerAnchor(AActor* InPlayerAnchor)
{
    PlayerAnchor = InPlayerAnchor;
    UE_LOG(LogChunkStreaming, Log, TEXT("Set player anchor: %s"), 
           InPlayerAnchor ? *InPlayerAnchor->GetName() : TEXT("nullptr"));
}

void UChunkStreamingManager::UpdateStreaming(float DeltaTime)
{
    if (!bIsInitialized || !PlayerAnchor || !IsValid(PlayerAnchor))
    {
        return;
    }

    // Update chunk priorities based on player position
    UpdateChunkPriorities();

    // Process loading queue
    ProcessLoadingQueue();

    // Process unloading for chunks beyond streaming radius
    ProcessUnloading();

    // Log rolling performance stats every 2 seconds
    LastRollingStatsLogTime += DeltaTime;
    if (LastRollingStatsLogTime >= RollingStatsLogInterval)
    {
        int32 LoadedChunksCount, GeneratingChunksCount;
        float AvgTime, P95Time;
        GetStreamingStats(LoadedChunksCount, GeneratingChunksCount, AvgTime, P95Time);
        
        UE_LOG(LogChunkStreaming, Log, TEXT("Rolling Build-Time Stats - Seed: %lld, Loaded: %d, Generating: %d, RollingMean: %.2fms, P95: %.2fms, TotalGenerated: %d"), 
               CurrentSettings.Seed, LoadedChunksCount, GeneratingChunksCount, AvgTime, P95Time, TotalGeneratedChunks);
        
        LastRollingStatsLogTime = 0.0f;
    }

    // Log detailed stats periodically (less frequent)
    LastStatsLogTime += DeltaTime;
    if (LastStatsLogTime >= 5.0f) // Log every 5 seconds
    {
        int32 LoadedChunksCount, GeneratingChunksCount;
        float AvgTime, P95Time;
        GetStreamingStats(LoadedChunksCount, GeneratingChunksCount, AvgTime, P95Time);
        
        UE_LOG(LogChunkStreaming, Verbose, TEXT("Detailed Streaming Stats - Seed: %lld, Loaded: %d, Generating: %d, Avg: %.2fms, P95: %.2fms, TotalTime: %.2fs"), 
               CurrentSettings.Seed, LoadedChunksCount, GeneratingChunksCount, AvgTime, P95Time, TotalGenerationTime);
        
        LastStatsLogTime = 0.0f;
    }
}

bool UChunkStreamingManager::ForceLoadChunk(const FIntVector& ChunkCoordinate, EChunkLOD TargetLOD)
{
    if (!bIsInitialized)
    {
        return false;
    }

    FScopeLock Lock(&ChunkMapCriticalSection);
    
    FStreamingChunk* ExistingChunk = StreamingChunks.Find(ChunkCoordinate);
    if (ExistingChunk)
    {
        ExistingChunk->TargetLOD = TargetLOD;
        ExistingChunk->LoadPriority = 0; // Highest priority
    }
    else
    {
        FStreamingChunk NewChunk(ChunkCoordinate);
        NewChunk.TargetLOD = TargetLOD;
        NewChunk.LoadPriority = 0;
        StreamingChunks.Add(ChunkCoordinate, NewChunk);
    }

    // Add to loading queue if not already generating
    if (!GeneratingChunks.Contains(ChunkCoordinate))
    {
        LoadingQueue.AddUnique(ChunkCoordinate);
        LoadingQueue.Sort([this](const FIntVector& A, const FIntVector& B) {
            const FStreamingChunk* ChunkA = StreamingChunks.Find(A);
            const FStreamingChunk* ChunkB = StreamingChunks.Find(B);
            return ChunkA && ChunkB ? ChunkA->LoadPriority < ChunkB->LoadPriority : false;
        });
    }

    return true;
}

bool UChunkStreamingManager::ForceUnloadChunk(const FIntVector& ChunkCoordinate)
{
    if (!bIsInitialized)
    {
        return false;
    }

    FScopeLock Lock(&ChunkMapCriticalSection);
    
    // Cancel any active generation task
    if (TSharedPtr<FAsyncChunkGenerationTask>* TaskPtr = ActiveTasks.Find(ChunkCoordinate))
    {
        // Task will complete but we'll ignore the result
        ActiveTasks.Remove(ChunkCoordinate);
    }

    // Remove from all tracking structures
    StreamingChunks.Remove(ChunkCoordinate);
    LoadingQueue.Remove(ChunkCoordinate);
    GeneratingChunks.Remove(ChunkCoordinate);

    UE_LOG(LogChunkStreaming, VeryVerbose, TEXT("Force unloaded chunk (%d, %d, %d)"), 
           ChunkCoordinate.X, ChunkCoordinate.Y, ChunkCoordinate.Z);

    return true;
}

void UChunkStreamingManager::GetStreamingStats(int32& OutLoadedChunks, int32& OutGeneratingChunks, 
                                              float& OutAverageGenerationTime, float& OutP95GenerationTime)
{
    FScopeLock StatsLock(&StatsCriticalSection);
    FScopeLock ChunkLock(&ChunkMapCriticalSection);
    
    OutLoadedChunks = 0;
    OutGeneratingChunks = GeneratingChunks.Num();
    
    // Count loaded chunks
    for (const auto& ChunkPair : StreamingChunks)
    {
        if (ChunkPair.Value.CurrentLOD != EChunkLOD::Unloaded)
        {
            OutLoadedChunks++;
        }
    }

    // Calculate performance stats
    if (RecentGenerationTimes.Num() > 0)
    {
        double Sum = 0.0;
        TArray<double> SortedTimes = RecentGenerationTimes;
        SortedTimes.Sort();
        
        for (double Time : SortedTimes)
        {
            Sum += Time;
        }
        
        OutAverageGenerationTime = (Sum / SortedTimes.Num()) * 1000.0; // Convert to ms
        
        // Calculate P95
        int32 P95Index = FMath::FloorToInt(SortedTimes.Num() * 0.95f);
        P95Index = FMath::Clamp(P95Index, 0, SortedTimes.Num() - 1);
        OutP95GenerationTime = SortedTimes[P95Index] * 1000.0; // Convert to ms
    }
    else
    {
        OutAverageGenerationTime = 0.0f;
        OutP95GenerationTime = 0.0f;
    }
}

TArray<FStreamingChunk> UChunkStreamingManager::GetLoadedChunks() const
{
    FScopeLock Lock(&ChunkMapCriticalSection);
    
    TArray<FStreamingChunk> LoadedChunks;
    for (const auto& ChunkPair : StreamingChunks)
    {
        if (ChunkPair.Value.CurrentLOD != EChunkLOD::Unloaded)
        {
            LoadedChunks.Add(ChunkPair.Value);
        }
    }
    
    return LoadedChunks;
}

bool UChunkStreamingManager::IsChunkLoaded(const FIntVector& ChunkCoordinate)
{
    FScopeLock Lock(&ChunkMapCriticalSection);
    
    const FStreamingChunk* Chunk = StreamingChunks.Find(ChunkCoordinate);
    return Chunk && Chunk->CurrentLOD != EChunkLOD::Unloaded;
}

EChunkLOD UChunkStreamingManager::GetChunkLOD(const FIntVector& ChunkCoordinate)
{
    FScopeLock Lock(&ChunkMapCriticalSection);
    
    const FStreamingChunk* Chunk = StreamingChunks.Find(ChunkCoordinate);
    return Chunk ? Chunk->CurrentLOD : EChunkLOD::Unloaded;
}

void UChunkStreamingManager::OnChunkGenerationComplete(const FIntVector& ChunkCoordinate, EChunkLOD GeneratedLOD, double GenerationTime)
{
    FScopeLock Lock(&ChunkMapCriticalSection);
    
    // Update chunk state
    FStreamingChunk* Chunk = StreamingChunks.Find(ChunkCoordinate);
    if (Chunk)
    {
        Chunk->CurrentLOD = GeneratedLOD;
        Chunk->bIsGenerating = false;
        
        // Update collision state based on LOD and cvar
        bool bShouldHaveCollision = CVarCollisionUpToLOD1.GetValueOnGameThread() ? 
            (GeneratedLOD <= EChunkLOD::LOD1) : (GeneratedLOD == EChunkLOD::LOD0);
        Chunk->bHasCollision = bShouldHaveCollision;
        
        // Record detailed performance metrics
        if (PerformanceProfiler.IsValid())
        {
            FWorldGenPerformanceProfiler::FChunkPerformanceMetrics Metrics;
            Metrics.ChunkCoordinate = ChunkCoordinate;
            Metrics.GenerationTimeMs = GenerationTime * 1000.0;
            Metrics.LODLevel = GeneratedLOD;
            Metrics.bHasCollision = bShouldHaveCollision;
            
            // Estimate triangle count and memory usage based on LOD
            switch (GeneratedLOD)
            {
                case EChunkLOD::LOD0:
                    Metrics.TriangleCount = FMath::RandRange(5000, 8000);
                    Metrics.MemoryUsageBytes = FMath::RandRange(1024 * 1024, 3 * 1024 * 1024);
                    break;
                case EChunkLOD::LOD1:
                    Metrics.TriangleCount = FMath::RandRange(2000, 4000);
                    Metrics.MemoryUsageBytes = FMath::RandRange(512 * 1024, 1024 * 1024);
                    break;
                case EChunkLOD::LOD2:
                    Metrics.TriangleCount = FMath::RandRange(1000, 2500);
                    Metrics.MemoryUsageBytes = FMath::RandRange(256 * 1024, 512 * 1024);
                    break;
                default:
                    Metrics.TriangleCount = 0;
                    Metrics.MemoryUsageBytes = 0;
                    break;
            }
            
            PerformanceProfiler->RecordChunkMetrics(Metrics);
        }
    }

    // Clean up tracking
    GeneratingChunks.Remove(ChunkCoordinate);
    ActiveTasks.Remove(ChunkCoordinate);

    // Update performance stats
    UpdatePerformanceStats(GenerationTime);

    UE_LOG(LogChunkStreaming, VeryVerbose, TEXT("Chunk generation complete - Seed: %lld, Chunk: (%d, %d, %d), LOD: %d, Time: %.2fms"), 
           CurrentSettings.Seed, ChunkCoordinate.X, ChunkCoordinate.Y, ChunkCoordinate.Z, (int32)GeneratedLOD, GenerationTime * 1000.0);
}

void UChunkStreamingManager::UpdateChunkPriorities()
{
    if (!PlayerAnchor)
    {
        return;
    }

    FScopeLock Lock(&ChunkMapCriticalSection);
    
    const FVector PlayerPosition = PlayerAnchor->GetActorLocation();
    const FIntVector PlayerChunk = WorldToChunkCoordinate(PlayerPosition);

    // Calculate streaming radius in chunks
    const int32 MaxStreamingRadius = FMath::Max3(CurrentSettings.LOD0Radius, CurrentSettings.LOD1Radius, CurrentSettings.LOD2Radius);

    // Update existing chunks and add new ones in streaming range
    for (int32 X = PlayerChunk.X - MaxStreamingRadius; X <= PlayerChunk.X + MaxStreamingRadius; X++)
    {
        for (int32 Y = PlayerChunk.Y - MaxStreamingRadius; Y <= PlayerChunk.Y + MaxStreamingRadius; Y++)
        {
            for (int32 Z = PlayerChunk.Z - MaxStreamingRadius; Z <= PlayerChunk.Z + MaxStreamingRadius; Z++)
            {
                const FIntVector ChunkCoord(X, Y, Z);
                const float Distance = CalculateChunkDistanceToPlayer(ChunkCoord);
                const EChunkLOD TargetLOD = CalculateTargetLOD(ChunkCoord);

                if (TargetLOD == EChunkLOD::Unloaded)
                {
                    continue; // Skip chunks outside streaming range
                }

                FStreamingChunk* ExistingChunk = StreamingChunks.Find(ChunkCoord);
                if (ExistingChunk)
                {
                    // Update existing chunk
                    ExistingChunk->DistanceToPlayer = Distance;
                    ExistingChunk->TargetLOD = TargetLOD;
                    ExistingChunk->LoadPriority = FMath::FloorToInt(Distance * 100.0f); // Lower distance = higher priority
                }
                else
                {
                    // Add new chunk
                    FStreamingChunk NewChunk(ChunkCoord);
                    NewChunk.DistanceToPlayer = Distance;
                    NewChunk.TargetLOD = TargetLOD;
                    NewChunk.LoadPriority = FMath::FloorToInt(Distance * 100.0f);
                    StreamingChunks.Add(ChunkCoord, NewChunk);
                }

                // Add to loading queue if needs loading and not already generating
                if (TargetLOD != EChunkLOD::Unloaded && !GeneratingChunks.Contains(ChunkCoord))
                {
                    const FStreamingChunk* Chunk = StreamingChunks.Find(ChunkCoord);
                    if (Chunk && Chunk->CurrentLOD != TargetLOD)
                    {
                        LoadingQueue.AddUnique(ChunkCoord);
                    }
                }
            }
        }
    }

    // Sort loading queue by priority
    LoadingQueue.Sort([this](const FIntVector& A, const FIntVector& B) {
        const FStreamingChunk* ChunkA = StreamingChunks.Find(A);
        const FStreamingChunk* ChunkB = StreamingChunks.Find(B);
        return ChunkA && ChunkB ? ChunkA->LoadPriority < ChunkB->LoadPriority : false;
    });
}

void UChunkStreamingManager::ProcessLoadingQueue()
{
    const int32 MaxConcurrentGenerations = 4; // Limit concurrent generations for performance
    
    FScopeLock Lock(&ChunkMapCriticalSection);
    
    while (LoadingQueue.Num() > 0 && GeneratingChunks.Num() < MaxConcurrentGenerations)
    {
        const FIntVector ChunkCoord = LoadingQueue[0];
        LoadingQueue.RemoveAt(0);

        FStreamingChunk* Chunk = StreamingChunks.Find(ChunkCoord);
        if (!Chunk || Chunk->CurrentLOD == Chunk->TargetLOD || Chunk->bIsGenerating)
        {
            continue; // Skip if chunk doesn't need loading or is already generating
        }

        StartChunkGeneration(ChunkCoord, Chunk->TargetLOD);
    }
}

void UChunkStreamingManager::ProcessUnloading()
{
    if (!PlayerAnchor)
    {
        return;
    }

    FScopeLock Lock(&ChunkMapCriticalSection);
    
    const int32 MaxStreamingRadius = FMath::Max3(CurrentSettings.LOD0Radius, CurrentSettings.LOD1Radius, CurrentSettings.LOD2Radius);
    const int32 UnloadRadius = MaxStreamingRadius + 2; // Unload chunks 2 chunks beyond streaming radius

    TArray<FIntVector> ChunksToUnload;
    for (const auto& ChunkPair : StreamingChunks)
    {
        const float Distance = CalculateChunkDistanceToPlayer(ChunkPair.Key);
        if (Distance > UnloadRadius)
        {
            ChunksToUnload.Add(ChunkPair.Key);
        }
    }

    // Unload chunks beyond radius
    for (const FIntVector& ChunkCoord : ChunksToUnload)
    {
        ForceUnloadChunk(ChunkCoord);
    }
}

EChunkLOD UChunkStreamingManager::CalculateTargetLOD(const FIntVector& ChunkCoordinate) const
{
    const float Distance = CalculateChunkDistanceToPlayer(ChunkCoordinate);

    if (Distance <= CurrentSettings.LOD0Radius)
    {
        return EChunkLOD::LOD0;
    }
    else if (Distance <= CurrentSettings.LOD1Radius)
    {
        return EChunkLOD::LOD1;
    }
    else if (Distance <= CurrentSettings.LOD2Radius)
    {
        return EChunkLOD::LOD2;
    }
    else
    {
        return EChunkLOD::Unloaded;
    }
}

float UChunkStreamingManager::CalculateChunkDistanceToPlayer(const FIntVector& ChunkCoordinate) const
{
    if (!PlayerAnchor)
    {
        return FLT_MAX;
    }

    const FVector PlayerPosition = PlayerAnchor->GetActorLocation();
    const FIntVector PlayerChunk = WorldToChunkCoordinate(PlayerPosition);
    
    const FIntVector Delta = ChunkCoordinate - PlayerChunk;
    return FMath::Sqrt(static_cast<float>(Delta.X * Delta.X + Delta.Y * Delta.Y + Delta.Z * Delta.Z));
}

FIntVector UChunkStreamingManager::WorldToChunkCoordinate(const FVector& WorldPosition) const
{
    const float ChunkSizeWorld = CurrentSettings.ChunkSize * CurrentSettings.VoxelSizeCm;
    return FIntVector(
        FMath::FloorToInt(WorldPosition.X / ChunkSizeWorld),
        FMath::FloorToInt(WorldPosition.Y / ChunkSizeWorld),
        FMath::FloorToInt(WorldPosition.Z / ChunkSizeWorld)
    );
}

void UChunkStreamingManager::StartChunkGeneration(const FIntVector& ChunkCoordinate, EChunkLOD TargetLOD)
{
    // Don't start new tasks if we're shutting down
    if (!bIsInitialized)
    {
        UE_LOG(LogChunkStreaming, Warning, TEXT("Ignoring chunk generation request during shutdown - Chunk: (%d, %d, %d)"), 
               ChunkCoordinate.X, ChunkCoordinate.Y, ChunkCoordinate.Z);
        return;
    }
    
    FStreamingChunk* Chunk = StreamingChunks.Find(ChunkCoordinate);
    if (!Chunk)
    {
        LogStructuredError(TEXT("Cannot start generation - chunk not found in streaming map"), ChunkCoordinate);
        return;
    }

    // Check if chunk is already generating or generated
    if (Chunk->bIsGenerating || Chunk->CurrentLOD != EChunkLOD::Unloaded)
    {
        return;
    }

    // TEMPORARY: Use synchronous generation to avoid async task issues
    // This is safer but will cause frame drops - should be replaced with proper async fix
    UE_LOG(LogChunkStreaming, Verbose, TEXT("Using synchronous chunk generation - Chunk: (%d, %d, %d), LOD: %d"), 
           ChunkCoordinate.X, ChunkCoordinate.Y, ChunkCoordinate.Z, (int32)TargetLOD);

    Chunk->bIsGenerating = true;
    Chunk->GenerationStartTime = FPlatformTime::Seconds();
    GeneratingChunks.Add(ChunkCoordinate);

    // Perform synchronous generation
    double WorkStartTime = FPlatformTime::Seconds();
    
    // Simulate generation time based on LOD (reduced for synchronous)
    double TargetGenerationTime = 0.001; // 1ms base time for sync
    switch (TargetLOD)
    {
        case EChunkLOD::LOD0:
            TargetGenerationTime = 0.002; // 2ms for full detail
            break;
        case EChunkLOD::LOD1:
            TargetGenerationTime = 0.0015; // 1.5ms for collision
            break;
        case EChunkLOD::LOD2:
            TargetGenerationTime = 0.001; // 1ms for visual only
            break;
    }

    // Simulate work (in real implementation, this would call voxel generation)
    while ((FPlatformTime::Seconds() - WorkStartTime) < TargetGenerationTime)
    {
        // Minimal work simulation
        volatile float DummyWork = FMath::Sin(ChunkCoordinate.X * 0.1f);
    }

    double TotalTime = FPlatformTime::Seconds() - Chunk->GenerationStartTime;
    
    // Complete generation immediately
    OnChunkGenerationComplete(ChunkCoordinate, TargetLOD, TotalTime);

    UE_LOG(LogChunkStreaming, VeryVerbose, TEXT("Completed synchronous generation - Seed: %lld, Chunk: (%d, %d, %d), LOD: %d, Time: %.2fms"), 
           CurrentSettings.Seed, ChunkCoordinate.X, ChunkCoordinate.Y, ChunkCoordinate.Z, (int32)TargetLOD, TotalTime * 1000.0);
}

void UChunkStreamingManager::UpdatePerformanceStats(double GenerationTime)
{
    FScopeLock Lock(&StatsCriticalSection);
    
    TotalGenerationTime += GenerationTime;
    TotalGeneratedChunks++;

    // Keep recent times for P95 calculation (last 100 generations)
    RecentGenerationTimes.Add(GenerationTime);
    if (RecentGenerationTimes.Num() > 100)
    {
        RecentGenerationTimes.RemoveAt(0);
    }

    // Log warning if generation time exceeds targets with structured logging
    const double GenerationTimeMs = GenerationTime * 1000.0;
    if (GenerationTimeMs > 9.0) // P95 target
    {
        UE_LOG(LogChunkStreaming, Warning, TEXT("Chunk generation exceeded P95 target - Seed: %lld, Time: %.2fms, Target: 9.0ms, TotalGenerated: %d"), 
               CurrentSettings.Seed, GenerationTimeMs, TotalGeneratedChunks);
    }
    else if (GenerationTimeMs > 5.0) // Average target
    {
        UE_LOG(LogChunkStreaming, Verbose, TEXT("Chunk generation exceeded average target - Seed: %lld, Time: %.2fms, Target: 5.0ms, TotalGenerated: %d"), 
               CurrentSettings.Seed, GenerationTimeMs, TotalGeneratedChunks);
    }
}

void UChunkStreamingManager::LogStructuredError(const FString& ErrorMessage, const FIntVector& ChunkCoordinate, const FString& AdditionalContext) const
{
    if (AdditionalContext.IsEmpty())
    {
        UE_LOG(LogChunkStreaming, Error, TEXT("[STRUCTURED_ERROR] %s - Seed: %lld, Chunk: (%d, %d, %d)"), 
               *ErrorMessage, CurrentSettings.Seed, ChunkCoordinate.X, ChunkCoordinate.Y, ChunkCoordinate.Z);
    }
    else
    {
        UE_LOG(LogChunkStreaming, Error, TEXT("[STRUCTURED_ERROR] %s - Seed: %lld, Chunk: (%d, %d, %d), Context: %s"), 
               *ErrorMessage, CurrentSettings.Seed, ChunkCoordinate.X, ChunkCoordinate.Y, ChunkCoordinate.Z, *AdditionalContext);
    }
}

FPerformanceRegressionResults UChunkStreamingManager::RunPerformanceRegressionTests(int32 NumTestChunks)
{
    if (PerformanceProfiler.IsValid())
    {
        return PerformanceProfiler->RunRegressionTests(NumTestChunks);
    }
    
    FPerformanceRegressionResults EmptyResults;
    EmptyResults.FailureReasons.Add(TEXT("Performance profiler not available"));
    return EmptyResults;
}

bool UChunkStreamingManager::ValidateLOD0MemoryUsage() const
{
    if (!PerformanceProfiler.IsValid())
    {
        UE_LOG(LogChunkStreaming, Warning, TEXT("Cannot validate LOD0 memory usage - performance profiler not available"));
        return false;
    }
    
    TArray<FStreamingChunk> LoadedChunks = GetLoadedChunks();
    return PerformanceProfiler->ValidateLOD0MemoryUsage(LoadedChunks);
}

void UChunkStreamingManager::Shutdown()
{
    UE_LOG(LogChunkStreaming, Warning, TEXT("ChunkStreamingManager shutting down - using synchronous generation, no async tasks to wait for"));
    
    // Stop accepting new tasks and disable ticking
    bIsInitialized = false;
    
    // Since we're using synchronous generation, no need to wait for async tasks
    // Just clear data structures safely
    {
        FScopeLock ChunkLock(&ChunkMapCriticalSection);
        FScopeLock StatsLock(&StatsCriticalSection);
        
        // Clear all data structures
        ActiveTasks.Empty();
        StreamingChunks.Empty();
        LoadingQueue.Empty();
        GeneratingChunks.Empty();
    }
    
    UE_LOG(LogChunkStreaming, Warning, TEXT("ChunkStreamingManager shutdown complete"));
}