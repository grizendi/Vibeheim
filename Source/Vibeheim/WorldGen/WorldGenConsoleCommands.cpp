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

FAutoConsoleCommand FWorldGenConsoleCommands::RecreateVoxelWorldCommand(
    TEXT("wg.RecreateVoxelWorld"),
    TEXT("Recreate the voxel world with current settings (useful after changing material/generator)"),
    FConsoleCommandWithArgsDelegate::CreateStatic(&FWorldGenConsoleCommands::RecreateVoxelWorld)
);

FAutoConsoleCommand FWorldGenConsoleCommands::TestCSGSphereCommand(
    TEXT("wg.TestCSGSphere"),
    TEXT("Test CSG sphere operations (usage: wg.TestCSGSphere X Y Z Radius Add/Remove)"),
    FConsoleCommandWithArgsDelegate::CreateStatic(&FWorldGenConsoleCommands::TestCSGSphere)
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

void FWorldGenConsoleCommands::RecreateVoxelWorld(const TArray<FString>& Args)
{
    AWorldGenManager* WorldGenManager = FindWorldGenManager();
    if (!WorldGenManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("No WorldGenManager found in current world"));
        return;
    }
    
    UVoxelPluginAdapter* Adapter = WorldGenManager->GetVoxelPluginAdapter();
    if (!Adapter)
    {
        UE_LOG(LogTemp, Warning, TEXT("VoxelPluginAdapter not available"));
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("Recreating voxel world with current settings..."));
    
    // Shutdown the current adapter
    Adapter->Shutdown();
    
    // Reinitialize with current settings
    const FWorldGenSettings& Settings = WorldGenManager->GetWorldGenSettings();
    if (Adapter->Initialize(Settings))
    {
        UE_LOG(LogTemp, Log, TEXT("Successfully recreated voxel world"));
        
        // Restart world building if we have a player anchor
        if (WorldGenManager->GetPlayerAnchor())
        {
            Adapter->BuildWorldAsync(WorldGenManager->GetPlayerAnchor());
            UE_LOG(LogTemp, Log, TEXT("Restarted world building around player anchor"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to recreate voxel world"));
    }
}

void FWorldGenConsoleCommands::TestCSGSphere(const TArray<FString>& Args)
{
    if (Args.Num() < 5)
    {
        UE_LOG(LogTemp, Warning, TEXT("Usage: wg.TestCSGSphere X Y Z Radius Add/Remove"));
        UE_LOG(LogTemp, Warning, TEXT("Example: wg.TestCSGSphere 1000 1000 100 200 Add"));
        return;
    }
    
    AWorldGenManager* WorldGenManager = FindWorldGenManager();
    if (!WorldGenManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("No WorldGenManager found in current world"));
        return;
    }
    
    UVoxelPluginAdapter* Adapter = WorldGenManager->GetVoxelPluginAdapter();
    if (!Adapter)
    {
        UE_LOG(LogTemp, Warning, TEXT("VoxelPluginAdapter not available"));
        return;
    }
    
    // Parse arguments
    float X = FCString::Atof(*Args[0]);
    float Y = FCString::Atof(*Args[1]);
    float Z = FCString::Atof(*Args[2]);
    float Radius = FCString::Atof(*Args[3]);
    FString Operation = Args[4];
    
    FVector Center(X, Y, Z);
    EVoxelCSG CSGOperation = Operation.ToLower().Contains(TEXT("add")) ? EVoxelCSG::Add : EVoxelCSG::Subtract;
    
    UE_LOG(LogTemp, Log, TEXT("Applying CSG sphere %s at (%.2f, %.2f, %.2f) with radius %.2f"), 
           CSGOperation == EVoxelCSG::Add ? TEXT("Add") : TEXT("Remove"),
           X, Y, Z, Radius);
    
    if (Adapter->ApplySphere(Center, Radius, CSGOperation))
    {
        UE_LOG(LogTemp, Log, TEXT("CSG sphere operation successful"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("CSG sphere operation failed"));
    }
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