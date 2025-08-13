#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "WorldGenTypes.generated.h"

/** Enum for chunk LOD levels */
UENUM(BlueprintType)
enum class EChunkLOD : uint8
{
    LOD0 = 0,  // Full detail + collision (2 chunks radius)
    LOD1 = 1,  // Collision enabled (4 chunks radius)
    LOD2 = 2,  // Visual only (6 chunks radius)
    Unloaded = 255
};

/** Performance regression test results */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FPerformanceRegressionResults
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    bool bPassedGenerationTimeTest;
    
    UPROPERTY(BlueprintReadOnly)
    bool bPassedMemoryUsageTest;
    
    UPROPERTY(BlueprintReadOnly)
    bool bPassedTriangleCountTest;
    
    UPROPERTY(BlueprintReadOnly)
    bool bPassedStreamingTest;
    
    UPROPERTY(BlueprintReadOnly)
    double AverageGenerationTimeMs;
    
    UPROPERTY(BlueprintReadOnly)
    double P95GenerationTimeMs;
    
    UPROPERTY(BlueprintReadOnly)
    int64 AverageMemoryUsageMB;
    
    UPROPERTY(BlueprintReadOnly)
    int64 PeakMemoryUsageMB;
    
    UPROPERTY(BlueprintReadOnly)
    int32 AverageTriangleCount;
    
    UPROPERTY(BlueprintReadOnly)
    int32 MaxTriangleCount;
    
    UPROPERTY(BlueprintReadOnly)
    TArray<FString> FailureReasons;
    
    UPROPERTY(BlueprintReadOnly)
    FDateTime TestTimestamp;

    FPerformanceRegressionResults()
        : bPassedGenerationTimeTest(false)
        , bPassedMemoryUsageTest(false)
        , bPassedTriangleCountTest(false)
        , bPassedStreamingTest(false)
        , AverageGenerationTimeMs(0.0)
        , P95GenerationTimeMs(0.0)
        , AverageMemoryUsageMB(0)
        , PeakMemoryUsageMB(0)
        , AverageTriangleCount(0)
        , MaxTriangleCount(0)
        , TestTimestamp(FDateTime::Now())
    {
    }
};

/** Forward declarations */
class UChunkStreamingManager;
struct FStreamingChunk;