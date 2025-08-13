#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"

/**
 * Console commands for world generation debugging and testing
 */
class VIBEHEIM_API FWorldGenConsoleCommands
{
public:
    /**
     * Register all world generation console commands
     */
    static void RegisterCommands();

    /**
     * Unregister all world generation console commands
     */
    static void UnregisterCommands();

private:
    /** Console command objects */
    static TAutoConsoleVariable<int32> CVarSeed;
    static TAutoConsoleVariable<int32> CVarStreamRadius;
    static TAutoConsoleVariable<bool> CVarDebugMasks;
    static TAutoConsoleVariable<bool> CVarDebugChunks;
    
    /** Console command functions */
    static FAutoConsoleCommand TestErrorLoggingCommand;
    static FAutoConsoleCommand TestFallbackGenerationCommand;
    static FAutoConsoleCommand ShowStreamingStatsCommand;
    static FAutoConsoleCommand RunIntegrationTestsCommand;
    static FAutoConsoleCommand TestBiomeTransitionsCommand;
    static FAutoConsoleCommand TestPOIPortalSystemCommand;
    static FAutoConsoleCommand ValidateVisualQualityCommand;
    
    /** Command implementations */
    static void TestErrorLogging(const TArray<FString>& Args);
    static void TestFallbackGeneration(const TArray<FString>& Args);
    static void ShowStreamingStats(const TArray<FString>& Args);
    static void RunIntegrationTests(const TArray<FString>& Args);
    static void TestBiomeTransitions(const TArray<FString>& Args);
    static void TestPOIPortalSystem(const TArray<FString>& Args);
    static void ValidateVisualQuality(const TArray<FString>& Args);
    
    /** Helper to find WorldGenManager in the world */
    static class AWorldGenManager* FindWorldGenManager();
};