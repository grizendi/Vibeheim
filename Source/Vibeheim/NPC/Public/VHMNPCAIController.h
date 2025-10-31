#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "VHMNPCAIController.generated.h"

class UNavigationQueryFilter;

/**
 * Thin AI controller for NPC needs agents.
 * Provides a hook to configure navigation filters per species.
 */
UCLASS()
class VIBEHEIM_API AVHMNPCAIController : public AAIController
{
    GENERATED_BODY()

public:
    AVHMNPCAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    virtual void OnPossess(APawn* InPawn) override;

    void SetSpeciesNavigationFilter(TSubclassOf<UNavigationQueryFilter> InFilterClass);
    TSubclassOf<UNavigationQueryFilter> GetSpeciesNavigationFilter() const { return SpeciesNavigationFilterClass; }

protected:
    UPROPERTY(Transient)
    TSubclassOf<UNavigationQueryFilter> SpeciesNavigationFilterClass;
};
