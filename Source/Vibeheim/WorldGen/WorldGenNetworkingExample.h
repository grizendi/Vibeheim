#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldGenGameState.h"
#include "WorldGenManager.h"
#include "WorldGenNetworkingExample.generated.h"

/**
 * Example class demonstrating how to integrate WorldGenGameState with WorldGenManager
 * This shows the foundation for future multiplayer support
 */
UCLASS(BlueprintType, Blueprintable)
class VIBEHEIM_API AWorldGenNetworkingExample : public AActor
{
    GENERATED_BODY()

public:
    AWorldGenNetworkingExample();

protected:
    virtual void BeginPlay() override;

public:
    /**
     * Example: Initialize world generation with networking support
     * This shows how a game mode would set up both single-player and multiplayer world generation
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation Example")
    bool InitializeNetworkedWorldGeneration();

    /**
     * Example: Apply a voxel edit with networking
     * This shows how player actions would be networked in multiplayer
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation Example")
    bool ApplyNetworkedVoxelEdit(const FVector& Location, float Radius, bool bAdditive = false);

    /**
     * Example: Handle late-joining player
     * This shows how a new player would sync with the current world state
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation Example")
    void HandleLateJoiningPlayer(const FIntVector& PlayerChunkLocation);

    /**
     * Example: Validate multiplayer world consistency
     * This shows how to ensure all players have the same world state
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation Example")
    bool ValidateWorldConsistency();

protected:
    /**
     * Get the world generation game state
     */
    AWorldGenGameState* GetWorldGenGameState() const;

    /**
     * Get the world generation manager
     */
    AWorldGenManager* GetWorldGenManager() const;

    /**
     * Example callback for when world generation is initialized
     */
    UFUNCTION()
    void OnWorldGenInitialized();

    /**
     * Example callback for when a voxel edit is applied
     */
    UFUNCTION()
    void OnVoxelEditApplied(const FVoxelEditOp& EditOp);

private:
    /** Whether networking example is enabled */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Generation Example", meta = (AllowPrivateAccess = "true"))
    bool bEnableNetworkingExample;

    /** Example seed for demonstration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Generation Example", meta = (AllowPrivateAccess = "true"))
    int64 ExampleSeed;

    /** Example world generation version */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Generation Example", meta = (AllowPrivateAccess = "true"))
    int32 ExampleWorldGenVersion;
};