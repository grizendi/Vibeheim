#include "PerformanceConsoleCommands.h"
#include "PerformanceProfiler.h"
#include "ChunkStreamingManager.h"
#include "WorldGenManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogPerformanceConsoleCommands, Log, All);

namespace PerformanceConsoleCommands
{
    static FAutoConsoleCommand RunPerformanceRegressionTestsCmd(
        TEXT("wg.performance.RunRegressionTests"),
        TEXT("Run performance regression tests on world generation system. Usage: wg.performance.RunRegressionTests [NumChunks]"),
        FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
        {
            int32 NumChunks = 50; // Default
            if (Args.Num() > 0)
            {
                NumChunks = FCString::Atoi(*Args[0]);
                NumChunks = FMath::Clamp(NumChunks, 10, 200);
            }
            
            UWorld* World = GEngine->GetWorldFromContextObject(GEngine, EGetWorldErrorMode::LogAndReturnNull);
            if (!World)
            {
                UE_LOG(LogPerformanceConsoleCommands, Error, TEXT("No valid world context for performance tests"));
                return;
            }
            
            // Find WorldGenManager in the world
            AWorldGenManager* WorldGenManager = nullptr;
            for (TActorIterator<AWorldGenManager> ActorItr(World); ActorItr; ++ActorItr)
            {
                WorldGenManager = *ActorItr;
                break;
            }
            
            if (!WorldGenManager)
            {
                UE_LOG(LogPerformanceConsoleCommands, Error, TEXT("No WorldGenManager found in current world"));
                return;
            }
            
            UChunkStreamingManager* StreamingManager = WorldGenManager->GetChunkStreamingManager();
            if (!StreamingManager)
            {
                UE_LOG(LogPerformanceConsoleCommands, Error, TEXT("No ChunkStreamingManager available"));
                return;
            }
            
            UE_LOG(LogPerformanceConsoleCommands, Log, TEXT("Running performance regression tests with %d chunks..."), NumChunks);
            
            FPerformanceRegressionResults Results = 
                StreamingManager->RunPerformanceRegressionTests(NumChunks);
            
            UE_LOG(LogPerformanceConsoleCommands, Log, TEXT("Performance Regression Test Results:"));
            UE_LOG(LogPerformanceConsoleCommands, Log, TEXT("  Generation Time Test: %s"), 
                   Results.bPassedGenerationTimeTest ? TEXT("PASS") : TEXT("FAIL"));
            UE_LOG(LogPerformanceConsoleCommands, Log, TEXT("  Memory Usage Test: %s"), 
                   Results.bPassedMemoryUsageTest ? TEXT("PASS") : TEXT("FAIL"));
            UE_LOG(LogPerformanceConsoleCommands, Log, TEXT("  Triangle Count Test: %s"), 
                   Results.bPassedTriangleCountTest ? TEXT("PASS") : TEXT("FAIL"));
            UE_LOG(LogPerformanceConsoleCommands, Log, TEXT("  Average Generation Time: %.2fms"), 
                   Results.AverageGenerationTimeMs);
            UE_LOG(LogPerformanceConsoleCommands, Log, TEXT("  P95 Generation Time: %.2fms"), 
                   Results.P95GenerationTimeMs);
            
            if (Results.FailureReasons.Num() > 0)
            {
                UE_LOG(LogPerformanceConsoleCommands, Warning, TEXT("Test failures:"));
                for (const FString& Reason : Results.FailureReasons)
                {
                    UE_LOG(LogPerformanceConsoleCommands, Warning, TEXT("  - %s"), *Reason);
                }
            }
        })
    );
    
    static FAutoConsoleCommand ValidateLOD0MemoryCmd(
        TEXT("wg.performance.ValidateLOD0Memory"),
        TEXT("Validate that LOD0 chunks are within memory usage limits"),
        FConsoleCommandDelegate::CreateLambda([]()
        {
            UWorld* World = GEngine->GetWorldFromContextObject(GEngine, EGetWorldErrorMode::LogAndReturnNull);
            if (!World)
            {
                UE_LOG(LogPerformanceConsoleCommands, Error, TEXT("No valid world context"));
                return;
            }
            
            AWorldGenManager* WorldGenManager = nullptr;
            for (TActorIterator<AWorldGenManager> ActorItr(World); ActorItr; ++ActorItr)
            {
                WorldGenManager = *ActorItr;
                break;
            }
            
            if (!WorldGenManager)
            {
                UE_LOG(LogPerformanceConsoleCommands, Error, TEXT("No WorldGenManager found"));
                return;
            }
            
            UChunkStreamingManager* StreamingManager = WorldGenManager->GetChunkStreamingManager();
            if (!StreamingManager)
            {
                UE_LOG(LogPerformanceConsoleCommands, Error, TEXT("No ChunkStreamingManager available"));
                return;
            }
            
            bool bValid = StreamingManager->ValidateLOD0MemoryUsage();
            UE_LOG(LogPerformanceConsoleCommands, Log, TEXT("LOD0 Memory Validation: %s"), 
                   bValid ? TEXT("PASS") : TEXT("FAIL"));
        })
    );
    
    static FAutoConsoleCommand ExportPerformanceDataCmd(
        TEXT("wg.performance.ExportData"),
        TEXT("Export performance data to JSON file. Usage: wg.performance.ExportData [FilePath]"),
        FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
        {
            FString FilePath = FPaths::ProjectSavedDir() / TEXT("PerformanceData") / TEXT("performance_export.json");
            if (Args.Num() > 0)
            {
                FilePath = Args[0];
            }
            
            UWorld* World = GEngine->GetWorldFromContextObject(GEngine, EGetWorldErrorMode::LogAndReturnNull);
            if (!World)
            {
                UE_LOG(LogPerformanceConsoleCommands, Error, TEXT("No valid world context"));
                return;
            }
            
            AWorldGenManager* WorldGenManager = nullptr;
            for (TActorIterator<AWorldGenManager> ActorItr(World); ActorItr; ++ActorItr)
            {
                WorldGenManager = *ActorItr;
                break;
            }
            
            if (!WorldGenManager)
            {
                UE_LOG(LogPerformanceConsoleCommands, Error, TEXT("No WorldGenManager found"));
                return;
            }
            
            UChunkStreamingManager* StreamingManager = WorldGenManager->GetChunkStreamingManager();
            if (!StreamingManager)
            {
                UE_LOG(LogPerformanceConsoleCommands, Error, TEXT("No ChunkStreamingManager available"));
                return;
            }
            
            FWorldGenPerformanceProfiler* Profiler = StreamingManager->GetPerformanceProfiler();
            if (!Profiler)
            {
                UE_LOG(LogPerformanceConsoleCommands, Error, TEXT("No performance profiler available"));
                return;
            }
            
            bool bSuccess = Profiler->ExportPerformanceData(FilePath);
            if (bSuccess)
            {
                UE_LOG(LogPerformanceConsoleCommands, Log, TEXT("Performance data exported to: %s"), *FilePath);
            }
            else
            {
                UE_LOG(LogPerformanceConsoleCommands, Error, TEXT("Failed to export performance data to: %s"), *FilePath);
            }
        })
    );
    
    static FAutoConsoleCommand TestStreamingPerformanceCmd(
        TEXT("wg.performance.TestStreaming"),
        TEXT("Test streaming performance during rapid movement. Usage: wg.performance.TestStreaming [Distance] [Speed]"),
        FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
        {
            float Distance = 5000.0f; // 5km default
            float Speed = 2000.0f;    // 20 m/s default
            
            if (Args.Num() > 0)
            {
                Distance = FCString::Atof(*Args[0]);
                Distance = FMath::Clamp(Distance, 1000.0f, 20000.0f);
            }
            
            if (Args.Num() > 1)
            {
                Speed = FCString::Atof(*Args[1]);
                Speed = FMath::Clamp(Speed, 500.0f, 10000.0f);
            }
            
            UWorld* World = GEngine->GetWorldFromContextObject(GEngine, EGetWorldErrorMode::LogAndReturnNull);
            if (!World)
            {
                UE_LOG(LogPerformanceConsoleCommands, Error, TEXT("No valid world context"));
                return;
            }
            
            AWorldGenManager* WorldGenManager = nullptr;
            for (TActorIterator<AWorldGenManager> ActorItr(World); ActorItr; ++ActorItr)
            {
                WorldGenManager = *ActorItr;
                break;
            }
            
            if (!WorldGenManager)
            {
                UE_LOG(LogPerformanceConsoleCommands, Error, TEXT("No WorldGenManager found"));
                return;
            }
            
            UChunkStreamingManager* StreamingManager = WorldGenManager->GetChunkStreamingManager();
            if (!StreamingManager)
            {
                UE_LOG(LogPerformanceConsoleCommands, Error, TEXT("No ChunkStreamingManager available"));
                return;
            }
            
            FWorldGenPerformanceProfiler* Profiler = StreamingManager->GetPerformanceProfiler();
            if (!Profiler)
            {
                UE_LOG(LogPerformanceConsoleCommands, Error, TEXT("No performance profiler available"));
                return;
            }
            
            FVector StartLocation(0, 0, 0);
            FVector EndLocation(Distance, Distance, 0);
            
            UE_LOG(LogPerformanceConsoleCommands, Log, TEXT("Testing streaming performance - Distance: %.1fm, Speed: %.1fm/s"), 
                   Distance, Speed);
            
            FPerformanceRegressionResults Results = 
                Profiler->TestStreamingPerformance(StartLocation, EndLocation, Speed, StreamingManager);
            
            UE_LOG(LogPerformanceConsoleCommands, Log, TEXT("Streaming Performance Test: %s"), 
                   Results.bPassedStreamingTest ? TEXT("PASS") : TEXT("FAIL"));
            
            for (const FString& Reason : Results.FailureReasons)
            {
                UE_LOG(LogPerformanceConsoleCommands, Warning, TEXT("  - %s"), *Reason);
            }
        })
    );
    
    static FAutoConsoleCommand ShowPerformanceStatsCmd(
        TEXT("wg.performance.ShowStats"),
        TEXT("Display current performance statistics"),
        FConsoleCommandDelegate::CreateLambda([]()
        {
            UWorld* World = GEngine->GetWorldFromContextObject(GEngine, EGetWorldErrorMode::LogAndReturnNull);
            if (!World)
            {
                UE_LOG(LogPerformanceConsoleCommands, Error, TEXT("No valid world context"));
                return;
            }
            
            AWorldGenManager* WorldGenManager = nullptr;
            for (TActorIterator<AWorldGenManager> ActorItr(World); ActorItr; ++ActorItr)
            {
                WorldGenManager = *ActorItr;
                break;
            }
            
            if (!WorldGenManager)
            {
                UE_LOG(LogPerformanceConsoleCommands, Error, TEXT("No WorldGenManager found"));
                return;
            }
            
            UChunkStreamingManager* StreamingManager = WorldGenManager->GetChunkStreamingManager();
            if (!StreamingManager)
            {
                UE_LOG(LogPerformanceConsoleCommands, Error, TEXT("No ChunkStreamingManager available"));
                return;
            }
            
            FWorldGenPerformanceProfiler* Profiler = StreamingManager->GetPerformanceProfiler();
            if (!Profiler)
            {
                UE_LOG(LogPerformanceConsoleCommands, Error, TEXT("No performance profiler available"));
                return;
            }
            
            double AvgGenTime, P95GenTime;
            SIZE_T TotalMemoryMB, LOD0MemoryMB;
            int32 AvgTriangles, MaxTriangles;
            
            Profiler->GetCurrentStats(AvgGenTime, P95GenTime, TotalMemoryMB, LOD0MemoryMB, AvgTriangles, MaxTriangles);
            
            UE_LOG(LogPerformanceConsoleCommands, Log, TEXT("=== Current Performance Statistics ==="));
            UE_LOG(LogPerformanceConsoleCommands, Log, TEXT("Generation Times:"));
            UE_LOG(LogPerformanceConsoleCommands, Log, TEXT("  Average: %.2fms (target: ≤5.0ms)"), AvgGenTime);
            UE_LOG(LogPerformanceConsoleCommands, Log, TEXT("  P95: %.2fms (target: ≤9.0ms)"), P95GenTime);
            UE_LOG(LogPerformanceConsoleCommands, Log, TEXT("Memory Usage:"));
            UE_LOG(LogPerformanceConsoleCommands, Log, TEXT("  Total: %llu MB"), TotalMemoryMB);
            UE_LOG(LogPerformanceConsoleCommands, Log, TEXT("  LOD0: %llu MB (target: ≤64MB)"), LOD0MemoryMB);
            UE_LOG(LogPerformanceConsoleCommands, Log, TEXT("Triangle Counts:"));
            UE_LOG(LogPerformanceConsoleCommands, Log, TEXT("  Average: %d"), AvgTriangles);
            UE_LOG(LogPerformanceConsoleCommands, Log, TEXT("  Maximum: %d (target: ≤8000)"), MaxTriangles);
            UE_LOG(LogPerformanceConsoleCommands, Log, TEXT("====================================="));
        })
    );
}