#include "ChunkStreamingExample.h"
#include "VoxelPluginAdapter.h"
#include "ChunkStreamingManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

DEFINE_LOG_CATEGORY(LogChunkStreamingExample);

AChunkStreamingExample::AChunkStreamingExample()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.1f; // Update 10 times per second
    
    // Initialize settings with default values
    StreamingSettings.LOD0Radius = 2;
    StreamingSettings.LOD1Radius = 4;
    StreamingSettings.LOD2Radius = 6;
    StreamingSettings.ChunkSize = 32;
    StreamingSettings.VoxelSizeCm = 50.0f;
    StreamingSettings.Seed = 1337;
    StreamingSettings.WorldGenVersion = 1;
    StreamingSettings.bCollisionUpToLOD1 = true;
}

void AChunkStreamingExample::BeginPlay()
{
    Super::BeginPlay();
    
    // Initialize the voxel adapter
    VoxelAdapter = NewObject<UVoxelPluginAdapter>(this);
    if (!VoxelAdapter->Initialize(StreamingSettings))
    {
        UE_LOG(LogChunkStreamingExample, Error, TEXT("Failed to initialize VoxelPluginAdapter"));
        return;
    }
    
    // Find the player controller and set up streaming
    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
    {
        if (APawn* PlayerPawn = PC->GetPawn())
        {
            if (!VoxelAdapter->BuildWorldAsync(PlayerPawn))
            {
                UE_LOG(LogChunkStreamingExample, Error, TEXT("Failed to start world building"));
                return;
            }
            
            UE_LOG(LogChunkStreamingExample, Log, TEXT("Chunk streaming initialized for player: %s"), 
                   *PlayerPawn->GetName());
        }
    }
}

void AChunkStreamingExample::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    if (!VoxelAdapter || !VoxelAdapter->IsInitialized())
    {
        return;
    }
    
    // Update streaming
    VoxelAdapter->TickStreaming(DeltaTime);
    
    // Log streaming stats periodically
    StatsLogTimer += DeltaTime;
    if (StatsLogTimer >= StatsLogInterval)
    {
        LogStreamingStats();
        StatsLogTimer = 0.0f;
    }
}

void AChunkStreamingExample::LogStreamingStats()
{
    if (!VoxelAdapter)
    {
        return;
    }
    
    int32 LoadedChunks, GeneratingChunks;
    float AvgTime, P95Time;
    VoxelAdapter->GetStreamingStats(LoadedChunks, GeneratingChunks, AvgTime, P95Time);
    
    UE_LOG(LogChunkStreamingExample, Log, 
           TEXT("Streaming Stats - Loaded: %d, Generating: %d, Avg: %.2fms, P95: %.2fms"), 
           LoadedChunks, GeneratingChunks, AvgTime, P95Time);
    
    // Check performance targets
    if (AvgTime > 5.0f)
    {
        UE_LOG(LogChunkStreamingExample, Warning, 
               TEXT("Average generation time (%.2fms) exceeds target (5ms)"), AvgTime);
    }
    
    if (P95Time > 9.0f)
    {
        UE_LOG(LogChunkStreamingExample, Warning, 
               TEXT("P95 generation time (%.2fms) exceeds target (9ms)"), P95Time);
    }
}

void AChunkStreamingExample::SetCollisionUpToLOD1(bool bEnabled)
{
    // Update the console variable
    CVarCollisionUpToLOD1.AsVariable()->Set(bEnabled, ECVF_SetByCode);
    
    UE_LOG(LogChunkStreamingExample, Log, TEXT("Collision up to LOD1 set to: %s"), 
           bEnabled ? TEXT("true") : TEXT("false"));
}

void AChunkStreamingExample::ForceLoadChunkAtLocation(const FVector& WorldLocation, EChunkLOD TargetLOD)
{
    if (!VoxelAdapter)
    {
        UE_LOG(LogChunkStreamingExample, Warning, TEXT("Cannot force load chunk - VoxelAdapter not initialized"));
        return;
    }
    
    UChunkStreamingManager* StreamingManager = VoxelAdapter->GetStreamingManager();
    if (!StreamingManager)
    {
        UE_LOG(LogChunkStreamingExample, Warning, TEXT("Cannot force load chunk - StreamingManager not available"));
        return;
    }
    
    // Convert world location to chunk coordinate
    const float ChunkSizeWorld = StreamingSettings.ChunkSize * StreamingSettings.VoxelSizeCm;
    const FIntVector ChunkCoord(
        FMath::FloorToInt(WorldLocation.X / ChunkSizeWorld),
        FMath::FloorToInt(WorldLocation.Y / ChunkSizeWorld),
        FMath::FloorToInt(WorldLocation.Z / ChunkSizeWorld)
    );
    
    if (StreamingManager->ForceLoadChunk(ChunkCoord, TargetLOD))
    {
        UE_LOG(LogChunkStreamingExample, Log, TEXT("Force loaded chunk (%d, %d, %d) at LOD %d"), 
               ChunkCoord.X, ChunkCoord.Y, ChunkCoord.Z, (int32)TargetLOD);
    }
    else
    {
        UE_LOG(LogChunkStreamingExample, Warning, TEXT("Failed to force load chunk (%d, %d, %d)"), 
               ChunkCoord.X, ChunkCoord.Y, ChunkCoord.Z);
    }
}

TArray<FStreamingChunk> AChunkStreamingExample::GetCurrentlyLoadedChunks() const
{
    if (!VoxelAdapter)
    {
        return TArray<FStreamingChunk>();
    }
    
    UChunkStreamingManager* StreamingManager = VoxelAdapter->GetStreamingManager();
    if (!StreamingManager)
    {
        return TArray<FStreamingChunk>();
    }
    
    return StreamingManager->GetLoadedChunks();
}