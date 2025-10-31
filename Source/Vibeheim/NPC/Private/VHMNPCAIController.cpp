#include "VHMNPCAIController.h"


AVHMNPCAIController::AVHMNPCAIController(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bAttachToPawn = true;
}

void AVHMNPCAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    // Intentionally do not set a default navigation filter here.
    // Move requests should supply the species filter explicitly per FAIMoveRequest.
}

void AVHMNPCAIController::SetSpeciesNavigationFilter(TSubclassOf<UNavigationQueryFilter> InFilterClass)
{
    SpeciesNavigationFilterClass = InFilterClass;
}
