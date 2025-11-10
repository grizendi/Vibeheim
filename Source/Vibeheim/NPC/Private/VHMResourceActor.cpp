#include "VHMResourceActor.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "VHMNPCTags.h"
#include "Components/SphereComponent.h"

namespace
{
    constexpr float DEFAULT_USE_RADIUS = 150.0f;
    constexpr int32 DEFAULT_MAX_CONCURRENT_USERS = 1;
    constexpr float DEFAULT_HEARTBEAT_TIMEOUT = 2.0f;
}

AVHMResourceActor::AVHMResourceActor()
{
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    PresenceSphere = CreateDefaultSubobject<USphereComponent>(TEXT("Presence"));
    PresenceSphere->SetupAttachment(SceneRoot);
    PresenceSphere->SetSphereRadius(DEFAULT_USE_RADIUS);
    PresenceSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    PresenceSphere->SetCollisionObjectType(ECC_GameTraceChannel2);
    PresenceSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    PresenceSphere->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Overlap);
    PresenceSphere->SetHiddenInGame(true);
    PresenceSphere->SetGenerateOverlapEvents(false);

    ResourceTag = FGameplayTag();
    UseRadius = DEFAULT_USE_RADIUS;
    Quantity = -1; // Infinite by default
    MaxConcurrentUsers = DEFAULT_MAX_CONCURRENT_USERS;
    ReservationHeartbeatTimeout = DEFAULT_HEARTBEAT_TIMEOUT;
    bAutoConfigureCollision = true;
}

bool AVHMResourceActor::CanUse_Implementation(EVHMNeed NeedType, APawn* User) const
{
    if (!IsValid(User))
    {
        return false;
    }

    if (ResourceTag.IsValid())
    {
        // Default CanUse simply ensures we still have quantity when finite.
        if (!IsQuantityInfinite() && Quantity <= 0)
        {
            return false;
        }
    }

    if (!HasOpenReservationSlot())
    {
        return false;
    }

    return true;
}

FVHMUseHandle AVHMResourceActor::BeginUse_Implementation(APawn* User, EVHMNeed NeedType, const FGuid& ReservationId)
{
    FVHMUseHandle Handle;

    if (!IsValid(User))
    {
        return Handle;
    }

    const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    const FVHMReservationSlot* Reservation = ActiveReservations.Find(ReservationId);
    if (!Reservation || Reservation->User.Get() != User)
    {
        return Handle;
    }

    Handle.Resource = this;
    Handle.User = User;
    Handle.ReservationId = ReservationId;
    Handle.LastHeartbeatTime = CurrentTime;

    return Handle;
}

bool AVHMResourceActor::TickUse_Implementation(const FVHMUseHandle& Handle, float DeltaSeconds)
{
    if (!Handle.IsValid())
    {
        return false;
    }

    // Default TickUse keeps reservation alive while resource is in use.
    return HeartbeatReservation(Handle.ReservationId);
}

void AVHMResourceActor::EndUse_Implementation(const FVHMUseHandle& Handle)
{
    if (Handle.ReservationId.IsValid())
    {
        ReleaseReservation(Handle.ReservationId);
    }
}

FGuid AVHMResourceActor::TryReserve(APawn* User)
{
    if (!IsValid(User))
    {
        return FGuid();
    }

    UWorld* World = GetWorld();
    const float CurrentTime = World ? World->GetTimeSeconds() : 0.0f;
    ExpireStaleReservations(CurrentTime);

    if (!IsQuantityInfinite() && Quantity <= 0)
    {
        return FGuid();
    }

    for (const TPair<FGuid, FVHMReservationSlot>& Pair : ActiveReservations)
    {
        if (Pair.Value.User == User)
        {
            return Pair.Key;
        }
    }

    if (!HasOpenReservationSlot())
    {
        return FGuid();
    }

    FGuid NewId = FGuid::NewGuid();
    ensureAlwaysMsgf(NewId.IsValid(), TEXT("ReservationId must be valid"));

    FVHMReservationSlot& Slot = ActiveReservations.Add(NewId);
    Slot.User = User;
    Slot.LastHeartbeatTime = CurrentTime;

    return NewId;
}

bool AVHMResourceActor::HeartbeatReservation(const FGuid& ReservationId)
{
    if (!ReservationId.IsValid())
    {
        return false;
    }

    FVHMReservationSlot* Slot = ActiveReservations.Find(ReservationId);
    if (!Slot)
    {
        return false;
    }

    const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    if (CurrentTime - Slot->LastHeartbeatTime > ReservationHeartbeatTimeout)
    {
        ActiveReservations.Remove(ReservationId);
        return false;
    }

    Slot->LastHeartbeatTime = CurrentTime;
    return true;
}

void AVHMResourceActor::ReleaseReservation(const FGuid& ReservationId)
{
    if (!ReservationId.IsValid())
    {
        return;
    }

    ActiveReservations.Remove(ReservationId);
}

void AVHMResourceActor::ReleaseReservationsFor(APawn* User)
{
    if (!IsValid(User))
    {
        return;
    }

    TArray<FGuid> ReservationsToRemove;
    ReservationsToRemove.Reserve(ActiveReservations.Num());

    for (const TPair<FGuid, FVHMReservationSlot>& Pair : ActiveReservations)
    {
        if (Pair.Value.User == User)
        {
            ReservationsToRemove.Add(Pair.Key);
        }
    }

    for (const FGuid& Id : ReservationsToRemove)
    {
        ActiveReservations.Remove(Id);
    }
}

void AVHMResourceActor::OnUserDestroyed(APawn* User)
{
    ReleaseReservationsFor(User);
}

bool AVHMResourceActor::SpendQuantity(int32 Amount)
{
    if (Amount <= 0 || IsQuantityInfinite())
    {
        return true;
    }

    if (Quantity <= 0)
    {
        return false;
    }

    Quantity = FMath::Max(0, Quantity - Amount);
    if (Quantity == 0)
    {
        OnResourceDepleted.Broadcast();
    }

    return Quantity > 0;
}

void AVHMResourceActor::PostInitializeComponents()
{
    Super::PostInitializeComponents();

    if (!bAutoConfigureCollision)
    {
        return;
    }

    // Ensure presence sphere radius roughly matches use radius for search visibility
    if (PresenceSphere)
    {
        PresenceSphere->SetSphereRadius(FMath::Max(UseRadius, 30.0f));
        PresenceSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        PresenceSphere->SetCollisionObjectType(ECC_GameTraceChannel2);
        PresenceSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
        PresenceSphere->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Overlap);
    }

    // Also configure any other primitives the actor might have
    TArray<UPrimitiveComponent*> PrimitiveComponents;
    GetComponents(PrimitiveComponents);
    for (UPrimitiveComponent* Primitive : PrimitiveComponents)
    {
        if (!Primitive || Primitive == PresenceSphere)
        {
            continue;
        }
        Primitive->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Primitive->SetCollisionObjectType(ECC_GameTraceChannel2);
        Primitive->SetCollisionResponseToAllChannels(ECR_Ignore);
        Primitive->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Overlap);
    }
}

void AVHMResourceActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ActiveReservations.Empty();
    Super::EndPlay(EndPlayReason);
}

bool AVHMResourceActor::HasOpenReservationSlot() const
{
    if (IsQuantityInfinite())
    {
        return MaxConcurrentUsers <= 0 || ActiveReservations.Num() < MaxConcurrentUsers;
    }

    const int32 MaxUsers = FMath::Min(MaxConcurrentUsers, Quantity);
    return ActiveReservations.Num() < MaxUsers;
}

void AVHMResourceActor::ExpireStaleReservations(float CurrentTime)
{
    if (ReservationHeartbeatTimeout <= 0.0f)
    {
        return;
    }

    TArray<FGuid> ExpiredIds;
    ExpiredIds.Reserve(ActiveReservations.Num());

    for (const TPair<FGuid, FVHMReservationSlot>& Pair : ActiveReservations)
    {
        if (CurrentTime - Pair.Value.LastHeartbeatTime > ReservationHeartbeatTimeout)
        {
            ExpiredIds.Add(Pair.Key);
        }
    }

    for (const FGuid& Id : ExpiredIds)
    {
        ActiveReservations.Remove(Id);
    }
}

AVHMFoodResourceActor::AVHMFoodResourceActor()
{
    ResourceTag = VHMNPCTags::ResourceFood();
}

AVHMWaterResourceActor::AVHMWaterResourceActor()
{
    ResourceTag = VHMNPCTags::ResourceWater();
    MaxConcurrentUsers = 0; // Unlimited concurrent users for infinite sources
}

AVHMShelterResourceActor::AVHMShelterResourceActor()
{
    ResourceTag = VHMNPCTags::ResourceShelter();
    MaxConcurrentUsers = 1;
}
