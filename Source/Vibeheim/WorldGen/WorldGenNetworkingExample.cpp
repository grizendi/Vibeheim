#include "WorldGenNetworkingExample.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Data/VoxelEditOp.h"

AWorldGenNetworkingExample::AWorldGenNetworkingExample()
{
    PrimaryActorTick.bCanEverTick = false;
    
    // Set default values
    bEnableNetworkingExample = true;
    ExampleSeed = 1337;
    ExampleWorldGenVersion = 1;
}

void AWorldGenNetworkingExample::BeginPlay()
{
    Super::BeginPlay();

    if (bEnableNetworkingExample)
    {
        UE_LOG(LogWorldGenNetworking, Log, TEXT("WorldGenNetworkingExample initialized"));
    }
}

bool AWorldGenNetworkingExample::InitializeNetworkedWorldGeneration()
{
    if (!bEnableNetworkingExample)
    {
        return false;
    }

    // Get the world generation game state
    AWorldGenGameState* GameState = GetWorldGenGameState();
    if (!GameState)
    {
        UE_LOG(LogWorldGenNetworking, Error, TEXT("Failed to get WorldGenGameState"));
        return false;
    }

    // Get the world generation manager
    AWorldGenManager* WorldGenManager = GetWorldGenManager();
    if (!WorldGenManager)
    {
        UE_LOG(LogWorldGenNetworking, Error, TEXT("Failed to get WorldGenManager"));
        return false;
    }

    // Initialize networking (server only)
    if (GameState->HasWorldGenAuthority())
    {
        if (!GameState->InitializeWorldGeneration(ExampleSeed, ExampleWorldGenVersion))
        {
            UE_LOG(LogWorldGenNetworking, Error, TEXT("Failed to initialize networked world generation"));
            return false;
        }

        // Initialize the world generation manager with the same settings
        FWorldGenSettings Settings;
        Settings.Seed = ExampleSeed;
        Settings.WorldGenVersion = ExampleWorldGenVersion;
        
        if (!WorldGenManager->InitializeWorldGeneration(Settings))
        {
            UE_LOG(LogWorldGenNetworking, Error, TEXT("Failed to initialize WorldGenManager"));
            return false;
        }

        UE_LOG(LogWorldGenNetworking, Log, TEXT("Successfully initialized networked world generation - Seed: %lld"), ExampleSeed);
    }
    else
    {
        // Client - wait for replication
        UE_LOG(LogWorldGenNetworking, Log, TEXT("Client waiting for world generation initialization from server"));
    }

    return true;
}

bool AWorldGenNetworkingExample::ApplyNetworkedVoxelEdit(const FVector& Location, float Radius, bool bAdditive)
{
    if (!bEnableNetworkingExample)
    {
        return false;
    }

    AWorldGenGameState* GameState = GetWorldGenGameState();
    if (!GameState)
    {
        UE_LOG(LogWorldGenNetworking, Error, TEXT("Failed to get WorldGenGameState for voxel edit"));
        return false;
    }

    // Calculate affected chunk (simplified calculation)
    const float ChunkSizeWorld = 32.0f * 50.0f; // 32 voxels * 50cm per voxel
    FIntVector ChunkCoordinate = FIntVector(
        FMath::FloorToInt(Location.X / ChunkSizeWorld),
        FMath::FloorToInt(Location.Y / ChunkSizeWorld),
        FMath::FloorToInt(Location.Z / ChunkSizeWorld)
    );

    // Create edit operation
    FVoxelEditOp EditOp(Location, Radius, bAdditive ? EVoxelCSG::Add : EVoxelCSG::Subtract, ChunkCoordinate);

    // Apply the edit through the game state (handles networking automatically)
    if (GameState->HasWorldGenAuthority())
    {
        // Server - apply directly
        return GameState->ApplyVoxelEdit(EditOp, true);
    }
    else
    {
        // Client - send to server
        // In a real implementation, this would be called through a player controller or input system
        UE_LOG(LogWorldGenNetworking, Log, TEXT("Client would send voxel edit to server"));
        return true;
    }
}

void AWorldGenNetworkingExample::HandleLateJoiningPlayer(const FIntVector& PlayerChunkLocation)
{
    if (!bEnableNetworkingExample)
    {
        return;
    }

    AWorldGenGameState* GameState = GetWorldGenGameState();
    if (!GameState)
    {
        UE_LOG(LogWorldGenNetworking, Error, TEXT("Failed to get WorldGenGameState for late join handling"));
        return;
    }

    if (GameState->HasWorldGenAuthority())
    {
        // Server - we would typically sync multiple chunks around the player
        UE_LOG(LogWorldGenNetworking, Log, TEXT("Server handling late-joining player at chunk (%d,%d,%d)"), 
               PlayerChunkLocation.X, PlayerChunkLocation.Y, PlayerChunkLocation.Z);
        
        // Example: sync chunks in a 3x3x3 area around the player
        for (int32 X = -1; X <= 1; X++)
        {
            for (int32 Y = -1; Y <= 1; Y++)
            {
                for (int32 Z = -1; Z <= 1; Z++)
                {
                    FIntVector ChunkToSync = PlayerChunkLocation + FIntVector(X, Y, Z);
                    // In a real implementation, we would check if the chunk has edits and sync if needed
                    UE_LOG(LogWorldGenNetworking, Log, TEXT("Would sync chunk (%d,%d,%d) for late-joining player"), 
                           ChunkToSync.X, ChunkToSync.Y, ChunkToSync.Z);
                }
            }
        }
    }
    else
    {
        // Client - request sync for chunks around player location
        UE_LOG(LogWorldGenNetworking, Log, TEXT("Client requesting chunk sync for late join"));
        GameState->RequestChunkSync(PlayerChunkLocation);
    }
}

bool AWorldGenNetworkingExample::ValidateWorldConsistency()
{
    if (!bEnableNetworkingExample)
    {
        return true;
    }

    AWorldGenGameState* GameState = GetWorldGenGameState();
    if (!GameState)
    {
        UE_LOG(LogWorldGenNetworking, Error, TEXT("Failed to get WorldGenGameState for consistency validation"));
        return false;
    }

    AWorldGenManager* WorldGenManager = GetWorldGenManager();
    if (!WorldGenManager)
    {
        UE_LOG(LogWorldGenNetworking, Error, TEXT("Failed to get WorldGenManager for consistency validation"));
        return false;
    }

    // Check if world generation is initialized
    if (!GameState->IsWorldGenInitialized())
    {
        UE_LOG(LogWorldGenNetworking, Warning, TEXT("World generation not yet initialized"));
        return false;
    }

    // Validate seed consistency
    int64 NetworkSeed = GameState->GetWorldGenSeed();
    int64 LocalSeed = WorldGenManager->GetWorldGenSettings().Seed;
    
    if (NetworkSeed != LocalSeed)
    {
        UE_LOG(LogWorldGenNetworking, Error, TEXT("Seed mismatch - Network: %lld, Local: %lld"), NetworkSeed, LocalSeed);
        return false;
    }

    // Validate version consistency
    int32 NetworkVersion = GameState->GetWorldGenVersion();
    int32 LocalVersion = WorldGenManager->GetWorldGenSettings().WorldGenVersion;
    
    if (NetworkVersion != LocalVersion)
    {
        UE_LOG(LogWorldGenNetworking, Error, TEXT("Version mismatch - Network: %d, Local: %d"), NetworkVersion, LocalVersion);
        return false;
    }

    UE_LOG(LogWorldGenNetworking, Log, TEXT("World consistency validation passed - Seed: %lld, Version: %d"), 
           NetworkSeed, NetworkVersion);
    return true;
}

AWorldGenGameState* AWorldGenNetworkingExample::GetWorldGenGameState() const
{
    if (UWorld* World = GetWorld())
    {
        return World->GetGameState<AWorldGenGameState>();
    }
    return nullptr;
}

AWorldGenManager* AWorldGenNetworkingExample::GetWorldGenManager() const
{
    if (UWorld* World = GetWorld())
    {
        for (TActorIterator<AWorldGenManager> ActorItr(World); ActorItr; ++ActorItr)
        {
            return *ActorItr;
        }
    }
    return nullptr;
}

void AWorldGenNetworkingExample::OnWorldGenInitialized()
{
    UE_LOG(LogWorldGenNetworking, Log, TEXT("World generation initialization callback"));
    
    // Example: Perform any post-initialization setup
    if (ValidateWorldConsistency())
    {
        UE_LOG(LogWorldGenNetworking, Log, TEXT("World generation networking ready"));
    }
}

void AWorldGenNetworkingExample::OnVoxelEditApplied(const FVoxelEditOp& EditOp)
{
    UE_LOG(LogWorldGenNetworking, Log, TEXT("Voxel edit applied callback - Chunk: (%d,%d,%d)"), 
           EditOp.AffectedChunk.X, EditOp.AffectedChunk.Y, EditOp.AffectedChunk.Z);
    
    // Example: Perform any post-edit processing
    // This could include updating UI, triggering effects, etc.
}