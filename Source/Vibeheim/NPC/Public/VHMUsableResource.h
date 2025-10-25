#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "VHMNPCTypes.h"
#include "VHMUsableResource.generated.h"

class APawn;

UINTERFACE(BlueprintType)
class VIBEHEIM_API UVHMUsableResource : public UInterface
{
    GENERATED_BODY()
};

class VIBEHEIM_API IVHMUsableResource
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Resource")
    bool CanUse(EVHMNeed NeedType, APawn* User) const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Resource")
    FVHMUseHandle BeginUse(APawn* User, EVHMNeed NeedType, const FGuid& ReservationId);

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Resource")
    bool TickUse(const FVHMUseHandle& Handle, float DeltaSeconds);

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Resource")
    void EndUse(const FVHMUseHandle& Handle);
};
