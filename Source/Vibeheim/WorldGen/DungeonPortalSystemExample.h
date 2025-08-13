#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DungeonPortalSystem.h"
#include "WorldGenManager.h"
#include "DungeonPortalSystemExample.generated.h"

/**
 * Example class demonstrating how to use the Dungeon Portal System
 * This shows how to create custom portal types, manage portals, and handle interactions
 */
UCLASS(BlueprintType, Blueprintable)
class VIBEHEIM_API ADungeonPortalSystemExample : public AActor
{
    GENERATED_BODY()

public:
    ADungeonPortalSystemExample();

protected:
    virtual void BeginPlay() override;

public:
    /**
     * Example: Create a custom portal spawn rule
     */
    UFUNCTION(BlueprintCallable, Category = "Portal Example")
    void CreateCustomPortalRule();

    /**
     * Example: Get all active portals in the world
     */
    UFUNCTION(BlueprintCallable, Category = "Portal Example")
    void ListAllActivePortals();

    /**
     * Example: Find portals near a specific location
     */
    UFUNCTION(BlueprintCallable, Category = "Portal Example")
    void FindPortalsNearLocation(const FVector& Location, float SearchRadius);

    /**
     * Example: Manually activate/deactivate a portal
     */
    UFUNCTION(BlueprintCallable, Category = "Portal Example")
    void TogglePortalAtLocation(const FVector& Location, float SearchRadius);

    /**
     * Example: Get portal placement statistics
     */
    UFUNCTION(BlueprintCallable, Category = "Portal Example")
    void ShowPortalStatistics();

protected:
    /** Reference to the world generation manager */
    UPROPERTY(BlueprintReadOnly, Category = "Portal Example")
    TObjectPtr<AWorldGenManager> WorldGenManager;

    /** Example custom portal spawn rule */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal Example")
    FPortalSpawnRule CustomPortalRule;
};