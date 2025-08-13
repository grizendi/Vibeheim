#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMemory.h"
#include "Stats/Stats.h"
#include "Engine/Engine.h"
#include "WorldGenCore.h"
#include "WorldGenTypes.h"

DECLARE_LOG_CATEGORY_EXTERN(LogWorldGenPerformance, Log, All);

// Performance stats group for Unreal Insights
DECLARE_STATS_GROUP(TEXT("WorldGen"), STATGROUP_WorldGen, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("Chunk Generation"), STAT_ChunkGeneration, STATGROUP_WorldGen);
DECLARE_CYCLE_STAT(TEXT("Biome Evaluation"), STAT_BiomeEvaluation, STATGROUP_WorldGen);
DECLARE_CYCLE_STAT(TEXT("POI Placement"), STAT_POIPlacement, STATGROUP_WorldGen);
DECLARE_CYCLE_STAT(TEXT("Mesh Generation"), STAT_MeshGeneration, STATGROUP_WorldGen);
DECLARE_MEMORY_STAT(TEXT("Chunk Memory"), STAT_ChunkMemory, STATGROUP_WorldGen);
DECLARE_DWORD_COUNTER_STAT(TEXT("Active Chunks"), STAT_ActiveChunks, STATGROUP_WorldGen);

/**
 * Performance profiler for world generation systems
 * Tracks timing, memory usage, and provides optimization recommendations
 */
class VIBEHEIM_API FWorldGenPerformanceProfiler
{
public:
    /** Performance metrics for a single chunk */
    struct FChunkPerformanceMetrics
    {
        FIntVector ChunkCoordinate;
        double GenerationTimeMs;
        double BiomeEvaluationTimeMs;
        double POIPlacementTimeMs;
        double MeshGenerationTimeMs;
        SIZE_T MemoryUsageBytes;
        int32 TriangleCount;
        EChunkLOD LODLevel;
        bool bHasCollision;
        FDateTime Timestamp;

        FChunkPerformanceMetrics()
            : ChunkCoordinate(FIntVector::ZeroValue)
            , GenerationTimeMs(0.0)
            , BiomeEvaluationTimeMs(0.0)
            , POIPlacementTimeMs(0.0)
            , MeshGenerationTimeMs(0.0)
            , MemoryUsageBytes(0)
            , TriangleCount(0)
            , LODLevel(EChunkLOD::Unloaded)
            , bHasCollision(false)
            , Timestamp(FDateTime::Now())
        {}
    };

    /** Streaming performance metrics */
    struct FStreamingPerformanceMetrics
    {
        double PlayerMovementSpeed;
        int32 ChunksLoadedPerSecond;
        int32 ChunksUnloadedPerSecond;
        double AverageLoadTimeMs;
        double P95LoadTimeMs;
        SIZE_T TotalMemoryUsageMB;
        SIZE_T LOD0MemoryUsageMB;
        int32 TotalActiveChunks;
        int32 LOD0ActiveChunks;
        FDateTime Timestamp;

        FStreamingPerformanceMetrics()
            : PlayerMovementSpeed(0.0)
            , ChunksLoadedPerSecond(0)
            , ChunksUnloadedPerSecond(0)
            , AverageLoadTimeMs(0.0)
            , P95LoadTimeMs(0.0)
            , TotalMemoryUsageMB(0)
            , LOD0MemoryUsageMB(0)
            , TotalActiveChunks(0)
            , LOD0ActiveChunks(0)
            , Timestamp(FDateTime::Now())
        {}
    };

public:

private:
    /** Recent chunk performance metrics for analysis */
    TArray<FChunkPerformanceMetrics> RecentChunkMetrics;
    
    /** Recent streaming performance metrics */
    TArray<FStreamingPerformanceMetrics> RecentStreamingMetrics;
    
    /** Maximum number of metrics to keep in memory */
    static constexpr int32 MaxMetricsHistory = 1000;
    
    /** Performance targets */
    static constexpr double TargetAverageGenerationTimeMs = 5.0;
    static constexpr double TargetP95GenerationTimeMs = 9.0;
    static constexpr SIZE_T TargetLOD0MemoryLimitMB = 64;
    static constexpr int32 TargetMaxTrianglesPerChunk = 8000;
    
    /** Critical section for thread-safe access */
    mutable FCriticalSection MetricsCriticalSection;

public:
    FWorldGenPerformanceProfiler();
    ~FWorldGenPerformanceProfiler();

    /** Record chunk generation performance metrics */
    void RecordChunkMetrics(const FChunkPerformanceMetrics& Metrics);
    
    /** Record streaming performance metrics */
    void RecordStreamingMetrics(const FStreamingPerformanceMetrics& Metrics);
    
    /** Get current performance statistics */
    void GetCurrentStats(double& OutAverageGenerationTimeMs, double& OutP95GenerationTimeMs,
                        SIZE_T& OutTotalMemoryUsageMB, SIZE_T& OutLOD0MemoryUsageMB,
                        int32& OutAverageTriangleCount, int32& OutMaxTriangleCount) const;
    
    /** Run performance regression tests */
    FPerformanceRegressionResults RunRegressionTests(int32 NumTestChunks = 50);
    
    /** Test streaming performance during rapid movement */
    FPerformanceRegressionResults TestStreamingPerformance(const FVector& StartLocation, 
                                                          const FVector& EndLocation, 
                                                          float MovementSpeed,
                                                          class UChunkStreamingManager* StreamingManager);
    
    /** Validate LOD0 memory usage constraints */
    bool ValidateLOD0MemoryUsage(const TArray<FStreamingChunk>& LoadedChunks) const;
    
    /** Validate triangle count constraints */
    bool ValidateTriangleCount(const FChunkPerformanceMetrics& Metrics) const;
    
    /** Generate performance optimization recommendations */
    TArray<FString> GenerateOptimizationRecommendations() const;
    
    /** Export performance data for CI/CD analysis */
    bool ExportPerformanceData(const FString& FilePath) const;
    
    /** Clear all stored metrics */
    void ClearMetrics();
    
    /** Get metrics for specific time range */
    TArray<FChunkPerformanceMetrics> GetMetricsInTimeRange(const FDateTime& StartTime, const FDateTime& EndTime) const;
    
    /** Calculate performance trends */
    void CalculatePerformanceTrends(double& OutGenerationTimeTrend, double& OutMemoryUsageTrend) const;

private:
    /** Helper to calculate percentile from sorted array */
    template<typename T>
    T CalculatePercentile(const TArray<T>& SortedValues, float Percentile) const;
    
    /** Helper to update Unreal Insights stats */
    void UpdateUnrealInsightsStats(const FChunkPerformanceMetrics& Metrics);
    
    /** Helper to log performance warnings */
    void LogPerformanceWarnings(const FChunkPerformanceMetrics& Metrics) const;
};

/**
 * RAII helper for timing chunk generation operations
 */
class VIBEHEIM_API FScopedChunkGenerationTimer
{
private:
    FWorldGenPerformanceProfiler* Profiler;
    FWorldGenPerformanceProfiler::FChunkPerformanceMetrics Metrics;
    double StartTime;
    double BiomeStartTime;
    double POIStartTime;
    double MeshStartTime;

public:
    FScopedChunkGenerationTimer(FWorldGenPerformanceProfiler* InProfiler, const FIntVector& ChunkCoordinate, EChunkLOD LODLevel);
    ~FScopedChunkGenerationTimer();
    
    /** Mark the start of biome evaluation phase */
    void StartBiomeEvaluation();
    
    /** Mark the end of biome evaluation phase */
    void EndBiomeEvaluation();
    
    /** Mark the start of POI placement phase */
    void StartPOIPlacement();
    
    /** Mark the end of POI placement phase */
    void EndPOIPlacement();
    
    /** Mark the start of mesh generation phase */
    void StartMeshGeneration();
    
    /** Mark the end of mesh generation phase */
    void EndMeshGeneration();
    
    /** Set triangle count for the generated mesh */
    void SetTriangleCount(int32 TriangleCount);
    
    /** Set memory usage for the chunk */
    void SetMemoryUsage(SIZE_T MemoryUsageBytes);
    
    /** Set collision flag */
    void SetHasCollision(bool bHasCollision);
};

/**
 * Performance optimization utilities
 */
class VIBEHEIM_API FWorldGenPerformanceOptimizer
{
public:
    /** Optimize chunk generation settings based on performance data */
    static FWorldGenSettings OptimizeSettings(const FWorldGenSettings& CurrentSettings, 
                                             const FPerformanceRegressionResults& Results);
    
    /** Suggest LOD adjustments based on performance */
    static void SuggestLODAdjustments(const FWorldGenSettings& CurrentSettings,
                                    const TArray<FWorldGenPerformanceProfiler::FChunkPerformanceMetrics>& Metrics,
                                    int32& OutSuggestedLOD0Radius,
                                    int32& OutSuggestedLOD1Radius,
                                    int32& OutSuggestedLOD2Radius);
    
    /** Calculate optimal chunk size based on performance targets */
    static int32 CalculateOptimalChunkSize(const FWorldGenSettings& CurrentSettings,
                                          const TArray<FWorldGenPerformanceProfiler::FChunkPerformanceMetrics>& Metrics);
    
    /** Optimize memory usage by adjusting quality settings */
    static void OptimizeMemoryUsage(FWorldGenSettings& Settings, SIZE_T TargetMemoryLimitMB);
};