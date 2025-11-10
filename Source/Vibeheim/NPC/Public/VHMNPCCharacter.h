#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "VHMNPCCharacter.generated.h"

class UVHMNeedsComponent;
class UActorComponent;
class UVHMSpeciesDataAsset;
class UNavigationQueryFilter;
class UStateTree; // UE 5.6 StateTree asset forward declaration
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

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

    // Optional default StateTree asset to run for this NPC.
    // If assigned (or found at the default path), it will be loaded and started at BeginPlay.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI", meta = (AllowedClasses = "StateTree", AllowPrivateAccess = "true"))
    TSoftObjectPtr<UStateTree> DefaultStateTreeAsset;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Needs|Config", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<const UVHMSpeciesDataAsset> DefaultSpeciesData;

    void ApplySpeciesNavigationFilter(AController* Controller);

    const UVHMSpeciesDataAsset* ResolveSpeciesData() const;

private:
    bool bLoggedMissingSpeciesWarning = false;

    // Simple visible fallback so spawned NPCs aren't invisible when no skeletal mesh is assigned.
    // Uses Engine BasicShapes by default and has no collision (visual-only).
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> VisualMesh;

    // Optional override for the debug/static visual mesh (e.g., assign your own mesh in BP/Class Defaults)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual", meta = (AllowPrivateAccess = "true"))
    TSoftObjectPtr<UStaticMesh> DefaultVisualMesh;

    // Optional material override for the debug mesh
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual", meta = (AllowPrivateAccess = "true"))
    TSoftObjectPtr<UMaterialInterface> DefaultVisualMaterial;
};

