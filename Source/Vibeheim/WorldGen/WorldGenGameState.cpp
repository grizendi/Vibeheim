#include "WorldGenGameState.h"
#include "WorldGenManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY(LogWorldGenNetworking);

AWorldGenGameState::AWorldGenGameState()
{
    // Set default values
    Seed = 0;
    WorldGenVersion = 0;
    bWorldGenInitialized = false;
    MaxEditOperationsPerChunk = 1000;
    MaxChunkSyncDistance = 10000.0f; // 100 meters
    bEnableNetworkingDebugLog = false;

    // Enable replication
    bReplicates = true;
    bAlwaysRelevant = true;
}

void AWorldGenGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // Replicate world generation properties to all clients
    DOREPLIFETIME(AWorldGenGameState, Seed);
    DOREPLIFETIME(AWorldGenGameState, WorldGenVersion);
}

void AWorldGenGameState::BeginPlay()
{
    Super::BeginPlay();

    if (bEnableNetworkingDebugLog)
    {
        UE_LOG(LogWorldGenNetworking, Log, TEXT("WorldGenGameState BeginPlay - HasAuthority: %s"), 
               HasAuthority() ? TEXT("true") : TEXT("false"));
    }
}

bool AWorldGenGameState::InitializeWorldGeneration(int64 InSeed, int32 InWorldGenVersion)
{
    // Only allow initialization on server
    if (!HasAuthority())
    {
        UE_LOG(LogWorldGenNetworking, Warning, TEXT("InitializeWorldGeneration called on client - ignoring"));
        return false;
    }

    if (bWorldGenInitialized)
    {
        UE_LOG(LogWorldGenNetworking, Warning, TEXT("World generation already initialized - Seed: %lld, Version: %d"), 
               Seed, WorldGenVersion);
        return false;
    }

    // Set replicated properties
    Seed = InSeed;
    WorldGenVersion = InWorldGenVersion;
    bWorldGenInitialized = true;

    UE_LOG(LogWorldGenNetworking, Log, TEXT("World generation initialized - Seed: %lld, Version: %d"), 
           Seed, WorldGenVersion);

    return true;
}

bool AWorldGenGameState::ApplyVoxelEdit(const FVoxelEditOp& EditOp, bool bBroadcastToClients)
{
    // Only allow on server
    if (!HasAuthority())
    {
        UE_LOG(LogWorldGenNetworking, Warning, TEXT("ApplyVoxelEdit called on client - use Server_ApplyEdit instead"));
        return false;
    }

    // Apply the edit operation locally on server
    ApplyEditOperationLocal(EditOp);

    // Broadcast to clients if requested
    if (bBroadcastToClients)
    {
        Multicast_ApplyEdit(EditOp);
    }

    if (bEnableNetworkingDebugLog)
    {
        UE_LOG(LogWorldGenNetworking, Log, TEXT("Applied voxel edit - Chunk: (%d,%d,%d), Center: (%f,%f,%f), Radius: %f"), 
               EditOp.AffectedChunk.X, EditOp.AffectedChunk.Y, EditOp.AffectedChunk.Z,
               EditOp.Center.X, EditOp.Center.Y, EditOp.Center.Z, EditOp.Radius);
    }

    return true;
}

void AWorldGenGameState::RequestChunkSync(const FIntVector& ChunkCoordinate)
{
    // Only allow on clients
    if (HasAuthority())
    {
        UE_LOG(LogWorldGenNetworking, Warning, TEXT("RequestChunkSync called on server - not needed"));
        return;
    }

    // Send request to server
    Server_RequestChunkSync(ChunkCoordinate);

    if (bEnableNetworkingDebugLog)
    {
        UE_LOG(LogWorldGenNetworking, Log, TEXT("Requested chunk sync for chunk: (%d,%d,%d)"), 
               ChunkCoordinate.X, ChunkCoordinate.Y, ChunkCoordinate.Z);
    }
}

TArray<FVoxelEditOp> AWorldGenGameState::GetChunkEditOperations(const FIntVector& ChunkCoordinate) const
{
    if (const TArray<FVoxelEditOp>* FoundOps = ChunkEditOperations.Find(ChunkCoordinate))
    {
        return *FoundOps;
    }
    return TArray<FVoxelEditOp>();
}

bool AWorldGenGameState::HasWorldGenAuthority() const
{
    return HasAuthority();
}

void AWorldGenGameState::Server_ApplyEdit_Implementation(const FVoxelEditOp& EditOp)
{
    // Validate the operation
    APlayerController* PC = Cast<APlayerController>(GetWorld()->GetFirstPlayerController());
    if (!ValidateEditOperation(EditOp, PC))
    {
        UE_LOG(LogWorldGenNetworking, Warning, TEXT("Server rejected invalid edit operation"));
        return;
    }

    // Apply the edit operation
    ApplyVoxelEdit(EditOp, true);
}

bool AWorldGenGameState::Server_ApplyEdit_Validate(const FVoxelEditOp& EditOp)
{
    // Basic validation - more comprehensive validation in ValidateEditOperation
    return EditOp.Radius > 0.0f && EditOp.Radius < 10000.0f; // Reasonable radius limits
}

void AWorldGenGameState::Multicast_ApplyEdit_Implementation(const FVoxelEditOp& EditOp)
{
    // Skip on server since it already applied the edit
    if (HasAuthority())
    {
        return;
    }

    // Apply the edit operation locally on clients
    ApplyEditOperationLocal(EditOp);

    if (bEnableNetworkingDebugLog)
    {
        UE_LOG(LogWorldGenNetworking, Log, TEXT("Client received voxel edit - Chunk: (%d,%d,%d)"), 
               EditOp.AffectedChunk.X, EditOp.AffectedChunk.Y, EditOp.AffectedChunk.Z);
    }
}

void AWorldGenGameState::Server_RequestChunkSync_Implementation(const FIntVector& ChunkCoordinate)
{
    // Validate the request
    APlayerController* RequestingPC = Cast<APlayerController>(GetWorld()->GetFirstPlayerController());
    if (!RequestingPC)
    {
        UE_LOG(LogWorldGenNetworking, Warning, TEXT("Invalid player controller for chunk sync request"));
        return;
    }

    // Basic distance validation to prevent abuse
    if (APawn* PlayerPawn = RequestingPC->GetPawn())
    {
        FVector PlayerLocation = PlayerPawn->GetActorLocation();
        FVector ChunkWorldLocation = FVector(ChunkCoordinate) * 3200.0f; // Approximate chunk size in world units
        float Distance = FVector::Dist(PlayerLocation, ChunkWorldLocation);
        
        if (Distance > MaxChunkSyncDistance)
        {
            UE_LOG(LogWorldGenNetworking, Warning, TEXT("Chunk sync request too far from player - Distance: %f"), Distance);
            return;
        }
    }

    // Get edit operations for the requested chunk
    TArray<FVoxelEditOp> ChunkOps = GetChunkEditOperations(ChunkCoordinate);
    
    // Calculate chunk seed for validation (simplified - in real implementation would use proper seed mixing)
    int64 ChunkSeed = Seed ^ (ChunkCoordinate.X * 73856093) ^ (ChunkCoordinate.Y * 19349663) ^ (ChunkCoordinate.Z * 83492791);

    // Create sync data
    FChunkSyncData SyncData(ChunkCoordinate, ChunkOps, ChunkSeed);

    // Send sync data to requesting client
    if (RequestingPC)
    {
        Client_ApplyChunkSync(SyncData);
    }

    if (bEnableNetworkingDebugLog)
    {
        UE_LOG(LogWorldGenNetworking, Log, TEXT("Sent chunk sync data - Chunk: (%d,%d,%d), Operations: %d"), 
               ChunkCoordinate.X, ChunkCoordinate.Y, ChunkCoordinate.Z, ChunkOps.Num());
    }
}

bool AWorldGenGameState::Server_RequestChunkSync_Validate(const FIntVector& ChunkCoordinate)
{
    // Basic validation - ensure chunk coordinates are reasonable
    const int32 MaxChunkCoord = 10000; // Reasonable world size limit
    return FMath::Abs(ChunkCoordinate.X) < MaxChunkCoord && 
           FMath::Abs(ChunkCoordinate.Y) < MaxChunkCoord && 
           FMath::Abs(ChunkCoordinate.Z) < MaxChunkCoord;
}

void AWorldGenGameState::Client_ApplyChunkSync_Implementation(const FChunkSyncData& SyncData)
{
    // Apply all edit operations for this chunk
    for (const FVoxelEditOp& EditOp : SyncData.EditOperations)
    {
        ApplyEditOperationLocal(EditOp);
    }

    if (bEnableNetworkingDebugLog)
    {
        UE_LOG(LogWorldGenNetworking, Log, TEXT("Applied chunk sync - Chunk: (%d,%d,%d), Operations: %d"), 
               SyncData.ChunkCoordinate.X, SyncData.ChunkCoordinate.Y, SyncData.ChunkCoordinate.Z, 
               SyncData.EditOperations.Num());
    }

    // Notify world generation manager about the sync
    if (AWorldGenManager* WorldGenManager = GetWorldGenManager())
    {
        // In a full implementation, we would notify the manager to rebuild the chunk
        // For now, just log that sync was completed
        UE_LOG(LogWorldGenNetworking, Log, TEXT("Chunk sync completed for chunk: (%d,%d,%d)"), 
               SyncData.ChunkCoordinate.X, SyncData.ChunkCoordinate.Y, SyncData.ChunkCoordinate.Z);
    }
}

void AWorldGenGameState::OnRep_Seed()
{
    UE_LOG(LogWorldGenNetworking, Log, TEXT("World generation seed replicated: %lld"), Seed);
    
    // Notify world generation manager about seed change
    if (AWorldGenManager* WorldGenManager = GetWorldGenManager())
    {
        // In a full implementation, we would update the manager's seed
        // For now, just log the replication
        UE_LOG(LogWorldGenNetworking, Log, TEXT("Notified WorldGenManager of seed change"));
    }
}

void AWorldGenGameState::OnRep_WorldGenVersion()
{
    UE_LOG(LogWorldGenNetworking, Log, TEXT("World generation version replicated: %d"), WorldGenVersion);
    
    // Check for version compatibility
    if (WorldGenVersion != 1) // Current expected version
    {
        UE_LOG(LogWorldGenNetworking, Warning, TEXT("World generation version mismatch - Expected: 1, Received: %d"), 
               WorldGenVersion);
    }
}

bool AWorldGenGameState::ValidateEditOperation(const FVoxelEditOp& EditOp, APlayerController* PlayerController) const
{
    // Basic authority validation
    if (!HasAuthority())
    {
        return false;
    }

    // Validate player controller
    if (!PlayerController)
    {
        UE_LOG(LogWorldGenNetworking, Warning, TEXT("Edit operation validation failed - No player controller"));
        return false;
    }

    // Validate edit operation parameters
    if (EditOp.Radius <= 0.0f || EditOp.Radius > 10000.0f)
    {
        UE_LOG(LogWorldGenNetworking, Warning, TEXT("Edit operation validation failed - Invalid radius: %f"), EditOp.Radius);
        return false;
    }

    // Validate player proximity (prevent editing too far from player)
    if (APawn* PlayerPawn = PlayerController->GetPawn())
    {
        float Distance = FVector::Dist(PlayerPawn->GetActorLocation(), EditOp.Center);
        const float MaxEditDistance = 5000.0f; // 50 meters
        
        if (Distance > MaxEditDistance)
        {
            UE_LOG(LogWorldGenNetworking, Warning, TEXT("Edit operation validation failed - Too far from player: %f"), Distance);
            return false;
        }
    }

    return true;
}

void AWorldGenGameState::ApplyEditOperationLocal(const FVoxelEditOp& EditOp)
{
    // Store the edit operation for persistence and future sync
    TArray<FVoxelEditOp>& ChunkOps = ChunkEditOperations.FindOrAdd(EditOp.AffectedChunk);
    ChunkOps.Add(EditOp);

    // Limit the number of operations per chunk to prevent memory issues
    if (ChunkOps.Num() > MaxEditOperationsPerChunk)
    {
        // Remove oldest operations (simple FIFO)
        ChunkOps.RemoveAt(0, ChunkOps.Num() - MaxEditOperationsPerChunk);
        UE_LOG(LogWorldGenNetworking, Warning, TEXT("Trimmed edit operations for chunk (%d,%d,%d) - too many operations"), 
               EditOp.AffectedChunk.X, EditOp.AffectedChunk.Y, EditOp.AffectedChunk.Z);
    }

    // Apply the edit operation to the world generation system
    if (AWorldGenManager* WorldGenManager = GetWorldGenManager())
    {
        // In a full implementation, we would apply the edit through the manager
        // For now, just log that we would apply it
        if (bEnableNetworkingDebugLog)
        {
            UE_LOG(LogWorldGenNetworking, Log, TEXT("Would apply edit operation to WorldGenManager"));
        }
    }
}

AWorldGenManager* AWorldGenGameState::GetWorldGenManager() const
{
    if (UWorld* World = GetWorld())
    {
        // Find the world generation manager in the world
        for (TActorIterator<AWorldGenManager> ActorItr(World); ActorItr; ++ActorItr)
        {
            return *ActorItr;
        }
    }
    return nullptr;
}