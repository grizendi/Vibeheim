#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "VHMUsableResource.h"
#include "VHMResourceActor.generated.h"

class UPrimitiveComponent;
class USceneComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnResourceDepletedSignature);

USTRUCT()
struct FVHMReservationSlot
{
    GENERATED_BODY()

    UPROPERTY()
    TWeakObjectPtr<APawn> User;

    UPROPERTY()
    float LastHeartbeatTime = 0.0f;
};

UCLASS(Abstract, Blueprintable)
class VIBEHEIM_API AVHMResourceActor : public AActor, public IVHMUsableResource
{
    GENERATED_BODY()

public:
    AVHMResourceActor();

    // IVHMUsableResource
    virtual bool CanUse_Implementation(EVHMNeed NeedType, APawn* User) const override;
    virtual FVHMUseHandle BeginUse_Implementation(APawn* User, EVHMNeed NeedType, const FGuid& ReservationId) override;
    virtual bool TickUse_Implementation(const FVHMUseHandle& Handle, float DeltaSeconds) override;
    virtual void EndUse_Implementation(const FVHMUseHandle& Handle) override;

    UFUNCTION(BlueprintCallable, Category = "Resource")
    FGuid TryReserve(APawn* User);

    UFUNCTION(BlueprintCallable, Category = "Resource")
    bool HeartbeatReservation(const FGuid& ReservationId);

    UFUNCTION(BlueprintCallable, Category = "Resource")
    void ReleaseReservation(const FGuid& ReservationId);

    UFUNCTION(BlueprintCallable, Category = "Resource")
    void ReleaseReservationsFor(APawn* User);

    UFUNCTION(BlueprintCallable, Category = "Resource")
    void OnUserDestroyed(APawn* User);

    UFUNCTION(BlueprintCallable, Category = "Resource")
    bool SpendQuantity(int32 Amount = 1);

    UFUNCTION(BlueprintPure, Category = "Resource")
    bool IsQuantityInfinite() const { return Quantity < 0; }

    UFUNCTION(BlueprintPure, Category = "Resource")
    int32 GetRemainingQuantity() const { return Quantity; }

    UFUNCTION(BlueprintPure, Category = "Resource")
    int32 GetActiveReservationCount() const { return ActiveReservations.Num(); }

    UPROPERTY(BlueprintAssignable, Category = "Resource")
    FOnResourceDepletedSignature OnResourceDepleted;

protected:
    virtual void PostInitializeComponents() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    bool HasOpenReservationSlot() const;
    void ExpireStaleReservations(float CurrentTime);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resource")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource")
    FGameplayTag ResourceTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource", meta = (ClampMin = "0.0"))
    float UseRadius;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource", meta = (ClampMin = "-1"))
    int32 Quantity;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource", meta = (ClampMin = "0"))
    int32 MaxConcurrentUsers;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource", meta = (ClampMin = "0.1"))
    float ReservationHeartbeatTimeout;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource")
    bool bAutoConfigureCollision;

    UPROPERTY(VisibleInstanceOnly, Category = "Resource")
    TMap<FGuid, FVHMReservationSlot> ActiveReservations;
};

UCLASS()
class VIBEHEIM_API AVHMFoodResourceActor : public AVHMResourceActor
{
    GENERATED_BODY()

public:
    AVHMFoodResourceActor();
};

UCLASS()
class VIBEHEIM_API AVHMWaterResourceActor : public AVHMResourceActor
{
    GENERATED_BODY()

public:
    AVHMWaterResourceActor();
};

UCLASS()
class VIBEHEIM_API AVHMShelterResourceActor : public AVHMResourceActor
{
    GENERATED_BODY()

public:
    AVHMShelterResourceActor();
};
