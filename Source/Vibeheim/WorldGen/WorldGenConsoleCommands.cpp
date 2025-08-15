#include "WorldGenConsoleCommands.h"
#include "WorldGenManager.h"
#include "VoxelPluginAdapter.h"
#include "ChunkStreamingManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"

// Define static members
TAutoConsoleVariable<int32> FWorldGenConsoleCommands::CVarSeed(
    TEXT("wg.Seed"),
    1337,
    TEXT("World generation seed value"),
    ECVF_Default
);

TAutoConsoleVariable<int32> FWorldGenConsoleCommands::CVarStreamRadius(
    TEXT("wg.StreamRadius"),
    6,
    TEXT("Streaming radius in chunks"),
    ECVF_Default
);

TAutoConsoleVariable<bool> FWorldGenConsoleCommands::CVarDebugMasks(
    TEXT("wg.DebugMasks"),
    false,
    TEXT("Enable biome mask visualization"),
    ECVF_Default
);

TAutoConsoleVariable<bool> FWorldGenConsoleCommands::CVarDebugChunks(
    TEXT("wg.DebugChunks"),
    false,
    TEXT("Show chunk boundaries and LOD levels"),
    ECVF_Default
);

FAutoConsoleCommand FWorldGenConsoleCommands::TestErrorLoggingCommand(
    TEXT("wg.TestErrorLogging"),
    TEXT("Test structured error logging system"),
    FConsoleCommandWithArgsDelegate::CreateStatic(&FWorldGenConsoleCommands::TestErrorLogging)
);

FAutoConsoleCommand FWorldGenConsoleCommands::TestFallbackGenerationCommand(
    TEXT("wg.TestFallbackGeneration"),
    TEXT("Test fallback terrain generation for a specific chunk (usage: wg.TestFallbackGeneration X Y Z)"),
    FConsoleCommandWithArgsDelegate::CreateStatic(&FWorldGenConsoleCommands::TestFallbackGeneration)
);

FAutoConsoleCommand FWorldGenConsoleCommands::ShowStreamingStatsCommand(
    TEXT("wg.ShowStreamingStats"),
    TEXT("Display current streaming statistics"),
    FConsoleCommandWithArgsDelegate::CreateStatic(&FWorldGenConsoleCommands::ShowStreamingStats)
);

void FWorldGenConsoleCommands::RegisterCommands()
{
    UE_LOG(LogTemp, Log, TEXT("WorldGen console commands registered"));
}

void FWorldGenConsoleCommands::UnregisterCommands()
{
    UE_LOG(LogTemp, Log, TEXT("WorldGen console commands unregistered"));
}

void FWorldGenConsoleCommands::TestErrorLogging(const TArray<FString>& Args)
{
    AWorldGenManager* WorldGenManager = FindWorldGenManager();
    if (!WorldGenManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("No WorldGenManager found in current world"));
        return;
    }
    
    // Test structured error logging
    UVoxelPluginAdapter* Adapter = WorldGenManager->GetVoxelPluginAdapter();
    if (Adapter)
    {
        // Test chunk coordinate logging
        FIntVector TestChunk(10, 20, 30);
        UE_LOG(LogTemp, Warning, TEXT("Testing structured error logging..."));
        
        // This will trigger structured error logging
        Adapter->TestStructuredErrorLogging(TEXT("Test error message for console command"), TestChunk, 
                                           TEXT("TestContext: Console command execution"));
        
        UE_LOG(LogTemp, Log, TEXT("Structured error logging test completed - check logs for [STRUCTURED_ERROR] entries"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("VoxelPluginAdapter not available for error logging test"));
    }
}

void FWorldGenConsoleCommands::TestFallbackGeneration(const TArray<FString>& Args)
{
    if (Args.Num() < 3)
    {
        UE_LOG(LogTemp, Warning, TEXT("Usage: wg.TestFallbackGeneration X Y Z (chunk coordinates)"));
        return;
    }
    
    AWorldGenManager* WorldGenManager = FindWorldGenManager();
    if (!WorldGenManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("No WorldGenManager found in current world"));
        return;
    }
    
    // Parse chunk coordinates
    int32 X = FCString::Atoi(*Args[0]);
    int32 Y = FCString::Atoi(*Args[1]);
    int32 Z = FCString::Atoi(*Args[2]);
    FIntVector ChunkCoord(X, Y, Z);
    
    UE_LOG(LogTemp, Log, TEXT("Testing fallback generation for chunk (%d, %d, %d)"), X, Y, Z);
    
    UVoxelPluginAdapter* Adapter = WorldGenManager->GetVoxelPluginAdapter();
    if (Adapter)
    {
        // Simulate a chunk generation failure to test fallback
        bool bFallbackResult = Adapter->TestFallbackGeneration(ChunkCoord, 
                                                               TEXT("Console command test failure"), true);
        
        UE_LOG(LogTemp, Log, TEXT("Fallback generation test result: %s"), bFallbackResult ? TEXT("Success") : TEXT("Failed"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("VoxelPluginAdapter not available for fallback generation test"));
    }
}

void FWorldGenConsoleCommands::ShowStreamingStats(const TArray<FString>& Args)
{
    AWorldGenManager* WorldGenManager = FindWorldGenManager();
    if (!WorldGenManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("No WorldGenManager found in current world"));
        return;
    }
    
    int32 LoadedChunks, GeneratingChunks;
    float AvgTime, P95Time;
    WorldGenManager->GetStreamingStats(LoadedChunks, GeneratingChunks, AvgTime, P95Time);
    
    const FWorldGenSettings& Settings = WorldGenManager->GetWorldGenSettings();
    
    UE_LOG(LogTemp, Log, TEXT("=== World Generation Streaming Stats ==="));
    UE_LOG(LogTemp, Log, TEXT("Seed: %lld"), Settings.Seed);
    UE_LOG(LogTemp, Log, TEXT("Loaded Chunks: %d"), LoadedChunks);
    UE_LOG(LogTemp, Log, TEXT("Generating Chunks: %d"), GeneratingChunks);
    UE_LOG(LogTemp, Log, TEXT("Average Generation Time: %.2f ms"), AvgTime);
    UE_LOG(LogTemp, Log, TEXT("P95 Generation Time: %.2f ms"), P95Time);
    UE_LOG(LogTemp, Log, TEXT("Ready: %s"), WorldGenManager->IsWorldGenReady() ? TEXT("Yes") : TEXT("No"));
    
    if (WorldGenManager->GetPlayerAnchor())
    {
        FVector PlayerPos = WorldGenManager->GetPlayerAnchor()->GetActorLocation();
        UE_LOG(LogTemp, Log, TEXT("Player Anchor: %s at (%.2f, %.2f, %.2f)"), 
               *WorldGenManager->GetPlayerAnchor()->GetName(), PlayerPos.X, PlayerPos.Y, PlayerPos.Z);
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("Player Anchor: None"));
    }
    
    UE_LOG(LogTemp, Log, TEXT("========================================"));
}

AWorldGenManager* FWorldGenConsoleCommands::FindWorldGenManager()
{
    if (!GEngine || !GEngine->GetCurrentPlayWorld())
    {
        return nullptr;
    }
    
    UWorld* World = GEngine->GetCurrentPlayWorld();
    return Cast<AWorldGenManager>(UGameplayStatics::GetActorOfClass(World, AWorldGenManager::StaticClass()));
}

// Removed automated testing console commands - using manual testing approach instead

// Automated testing functions removed - using manual testing approach instead