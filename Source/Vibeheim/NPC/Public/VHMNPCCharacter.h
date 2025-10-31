#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "VHMNPCCharacter.generated.h"

class UVHMNeedsComponent;
class UActorComponent;
class UVHMSpeciesDataAsset;
class UNavigationQueryFilter;

/**
 * Core NPC pawn used by the Vibeheim needs system.
 * Owns the needs/state tree components and applies species data at runtime.
 */
UCLASS()
class VIBEHEIM_API AVHMNPCCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AVHMNPCCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    virtual void BeginPlay() override;
    virtual void PossessedBy(AController* NewController) override;

    UFUNCTION(BlueprintPure, Category = "Needs")
    UVHMNeedsComponent* GetNeedsComponent() const { return NeedsComponent; }

    UFUNCTION(BlueprintPure, Category = "AI")
    UActorComponent* GetStateTreeComponent() const { return StateTreeComponent; }

    UFUNCTION(BlueprintCallable, Category = "Needs")
    void InitializeNeedsFromSpecies(const UVHMSpeciesDataAsset* InSpecies);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Needs", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UVHMNeedsComponent> NeedsComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UActorComponent> StateTreeComponent;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Needs|Config", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<const UVHMSpeciesDataAsset> DefaultSpeciesData;

    void ApplySpeciesNavigationFilter(AController* Controller);

    const UVHMSpeciesDataAsset* ResolveSpeciesData() const;

private:
    bool bLoggedMissingSpeciesWarning = false;
};

