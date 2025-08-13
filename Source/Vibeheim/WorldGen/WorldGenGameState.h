#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "Data/WorldGenSettings.h"
#include "Data/VoxelEditOp.h"
#include "WorldGenGameState.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogWorldGenNetworking, Log, All);

/**
 * Replicated chunk synchronization data for late-joining clients
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FChunkSyncData
{
    GENERATED_BODY()

    /** Chunk coordinate */
    UPROPERTY(BlueprintReadOnly)
    FIntVector ChunkCoordinate;

    /** Edit operations applied to this chunk */
    UPROPERTY(BlueprintReadOnly)
    TArray<FVoxelEditOp> EditOperations;

    /** Chunk generation seed (for validation) */
    UPROPERTY(BlueprintReadOnly)
    int64 ChunkSeed;

    FChunkSyncData()
        : ChunkCoordinate(FIntVector::ZeroValue)
        , ChunkSeed(0)
    {
    }

    FChunkSyncData(const FIntVector& InChunkCoordinate, const TArray<FVoxelEditOp>& InEditOperations, int64 InChunkSeed)
        : ChunkCoordinate(InChunkCoordinate)
        , EditOperations(InEditOperations)
        , ChunkSeed(InChunkSeed)
    {
    }
};

/**
 * Game State class that manages replicated world generation properties for multiplayer consistency
 * Provides foundation for future multiplayer support without blocking single-player development
 */
UCLASS(BlueprintType, Blueprintable)
class VIBEHEIM_API AWorldGenGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    AWorldGenGameState();

    // Begin AGameStateBase interface
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void BeginPlay() override;
    // End AGameStateBase interface

    /**
     * Initialize world generation properties (Server only)
     * @param InSeed The world generation seed
     * @param InWorldGenVersion The world generation version
     * @return True if initialization was successful
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    bool InitializeWorldGeneration(int64 InSeed, int32 InWorldGenVersion);

    /**
     * Get the replicated world generation seed
     * @return The current world generation seed
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    int64 GetWorldGenSeed() const { return Seed; }

    /**
     * Get the replicated world generation version
     * @return The current world generation version
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    int32 GetWorldGenVersion() const { return WorldGenVersion; }

    /**
     * Check if world generation properties have been initialized
     * @return True if seed and version are set
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    bool IsWorldGenInitialized() const { return bWorldGenInitialized; }

    /**
     * Apply a voxel edit operation (Server only)
     * @param EditOp The edit operation to apply
     * @param bBroadcastToClients Whether to broadcast this edit to all clients
     * @return True if the operation was successfully applied
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    bool ApplyVoxelEdit(const FVoxelEditOp& EditOp, bool bBroadcastToClients = true);

    /**
     * Request chunk synchronization from server (Client only)
     * @param ChunkCoordinate The chunk coordinate to synchronize
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    void RequestChunkSync(const FIntVector& ChunkCoordinate);

    /**
     * Get all recorded edit operations for a specific chunk
     * @param ChunkCoordinate The chunk coordinate to query
     * @return Array of edit operations for the chunk
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    TArray<FVoxelEditOp> GetChunkEditOperations(const FIntVector& ChunkCoordinate) const;

    /**
     * Check if the local player has authority for world generation operations
     * @return True if this is the server or has authority
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    bool HasWorldGenAuthority() const;

protected:
    /**
     * Server RPC to apply voxel edit operation
     * @param EditOp The edit operation to apply
     */
    UFUNCTION(Server, Reliable, WithValidation, Category = "World Generation")
    void Server_ApplyEdit(const FVoxelEditOp& EditOp);
    void Server_ApplyEdit_Implementation(const FVoxelEditOp& EditOp);
    bool Server_ApplyEdit_Validate(const FVoxelEditOp& EditOp);

    /**
     * Multicast RPC to broadcast voxel edit operation to all clients
     * @param EditOp The edit operation to broadcast
     */
    UFUNCTION(NetMulticast, Reliable, Category = "World Generation")
    void Multicast_ApplyEdit(const FVoxelEditOp& EditOp);
    void Multicast_ApplyEdit_Implementation(const FVoxelEditOp& EditOp);

    /**
     * Server RPC for clients to request chunk synchronization
     * @param ChunkCoordinate The chunk coordinate to synchronize
     */
    UFUNCTION(Server, Reliable, WithValidation, Category = "World Generation")
    void Server_RequestChunkSync(const FIntVector& ChunkCoordinate);
    void Server_RequestChunkSync_Implementation(const FIntVector& ChunkCoordinate);
    bool Server_RequestChunkSync_Validate(const FIntVector& ChunkCoordinate);

    /**
     * Client RPC to send chunk synchronization data
     * @param SyncData The chunk synchronization data
     */
    UFUNCTION(Client, Reliable, Category = "World Generation")
    void Client_ApplyChunkSync(const FChunkSyncData& SyncData);
    void Client_ApplyChunkSync_Implementation(const FChunkSyncData& SyncData);

    /**
     * Called when world generation seed is replicated
     */
    UFUNCTION()
    void OnRep_Seed();

    /**
     * Called when world generation version is replicated
     */
    UFUNCTION()
    void OnRep_WorldGenVersion();

    /**
     * Validate that an edit operation is authorized and valid
     * @param EditOp The edit operation to validate
     * @param PlayerController The player controller requesting the operation
     * @return True if the operation is valid and authorized
     */
    bool ValidateEditOperation(const FVoxelEditOp& EditOp, APlayerController* PlayerController) const;

    /**
     * Apply edit operation locally (called on both server and clients)
     * @param EditOp The edit operation to apply
     */
    void ApplyEditOperationLocal(const FVoxelEditOp& EditOp);

    /**
     * Get the world generation manager from the world
     * @return Pointer to the world generation manager, nullptr if not found
     */
    class AWorldGenManager* GetWorldGenManager() const;

private:
    /** Replicated world generation seed for multiplayer consistency */
    UPROPERTY(ReplicatedUsing = OnRep_Seed, BlueprintReadOnly, Category = "World Generation", meta = (AllowPrivateAccess = "true"))
    int64 Seed;

    /** Replicated world generation version for compatibility tracking */
    UPROPERTY(ReplicatedUsing = OnRep_WorldGenVersion, BlueprintReadOnly, Category = "World Generation", meta = (AllowPrivateAccess = "true"))
    int32 WorldGenVersion;

    /** Whether world generation has been initialized */
    UPROPERTY(BlueprintReadOnly, Category = "World Generation", meta = (AllowPrivateAccess = "true"))
    bool bWorldGenInitialized;

    /** Map of chunk coordinates to their edit operations for persistence and sync */
    TMap<FIntVector, TArray<FVoxelEditOp>> ChunkEditOperations;

    /** Maximum number of edit operations to track per chunk (for memory management) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Generation", meta = (AllowPrivateAccess = "true", ClampMin = "100", ClampMax = "10000"))
    int32 MaxEditOperationsPerChunk;

    /** Maximum distance for chunk sync requests (to prevent abuse) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Generation", meta = (AllowPrivateAccess = "true", ClampMin = "1000.0", ClampMax = "50000.0"))
    float MaxChunkSyncDistance;

    /** Whether to enable debug logging for networking operations */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (AllowPrivateAccess = "true"))
    bool bEnableNetworkingDebugLog;
};