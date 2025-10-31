#include "StateTree/VHMStateTreeTasks.h"

#include "AIController.h"
#include "GameFramework/Character.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "Engine/OverlapResult.h"
#include "WorldCollision.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#if __has_include("Navigation/NavigationQueryFilter.h")
#include "Navigation/NavigationQueryFilter.h"
#elif __has_include("NavFilters/NavigationQueryFilter.h")
#include "NavFilters/NavigationQueryFilter.h"
#endif

#include "VHMNPCLogging.h"
#include "VHMNPCVars.h"

namespace
{
    constexpr float HEARTBEAT_INTERVAL = 1.0f; // seconds
    constexpr float PROGRESS_THRESHOLD_CM = 75.0f;
    constexpr float STUCK_TIMEOUT_SEC = 3.0f;
}

static APawn* GetOwnerPawn(FStateTreeExecutionContext& Context)
{
    if (UObject* OwnerObj = Context.GetOwner())
    {
        if (AActor* OwnerActor = Cast<AActor>(OwnerObj))
        {
            if (APawn* AsPawn = Cast<APawn>(OwnerActor))
            {
                return AsPawn;
            }
            if (APawn* PawnOwner = OwnerActor->GetInstigator())
            {
                return PawnOwner;
            }
        }
    }
    return nullptr;
}

static UVHMNeedsComponent* GetNeeds(FStateTreeExecutionContext& Context)
{
    if (APawn* Pawn = GetOwnerPawn(Context))
    {
        return Pawn->FindComponentByClass<UVHMNeedsComponent>();
    }
    return nullptr;
}

static AVHMNPCAIController* GetNPCController(FStateTreeExecutionContext& Context)
{
    if (APawn* Pawn = GetOwnerPawn(Context))
    {
        return Cast<AVHMNPCAIController>(Pawn->GetController());
    }
    return nullptr;
}

static float NowSeconds(UWorld* World)
{
    return World ? World->GetTimeSeconds() : 0.0f;
}

EStateTreeRunStatus FSTT_EvaluateNeedTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    if (UVHMNeedsComponent* Needs = GetNeeds(Context))
    {
        FAgentNeedContext& Ctx = Needs->AgentContext;
        float Score = 0.0f;
        const EVHMNeed Need = Needs->GetMostPressingNeed(Score);
        Ctx.CurrentNeed = Need;
        Ctx.NeedScore = Score;
        Ctx.TargetActor = nullptr;
        Ctx.TargetLocation = FVector::ZeroVector;
        Ctx.ReservationId = FGuid();
        Ctx.UseAcceptanceRadius = 150.0f;
        Ctx.LastEvaluationTime = NowSeconds(Needs->GetWorld());
        UE_LOG(LogVHMStateTree, Verbose, TEXT("EvaluateNeed: Selected %d (Score=%.2f) for %s"), (int32)Need, Score, *GetNameSafe(GetOwnerPawn(Context)));
        return EStateTreeRunStatus::Succeeded;
    }

    return EStateTreeRunStatus::Failed;
}


EStateTreeRunStatus FSTT_FindTargetTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    FSTT_FindTargetTaskInstance& Task = Context.GetInstanceData(*this);
    APawn* Pawn = GetOwnerPawn(Context);
    UVHMNeedsComponent* Needs = GetNeeds(Context);
    if (!Pawn || !Needs)
    {
        return EStateTreeRunStatus::Failed;
    }

    FAgentNeedContext& Ctx = Needs->AgentContext;
    const UVHMSpeciesDataAsset* Species = Needs->SpeciesData;
    const FVHMSpeciesConfig* Config = Species ? &Species->Config : nullptr;

    // Throttle repeated searches based on species/cvar
    const float Now = NowSeconds(Pawn->GetWorld());
    float Throttle = CVarVHM_SearchThrottle.GetValueOnGameThread();
    if (Throttle <= 0.0f)
    {
        Throttle = Config ? Config->SearchCooldown : 1.0f;
    }
    if (Throttle > 0.0f && (Now - Ctx.LastSearchTime) < Throttle)
    {
        return EStateTreeRunStatus::Failed;
    }

    const FVector Origin = Pawn->GetActorLocation();
    const float BaseRadius = Config ? Config->SearchRadius : 2500.0f;
    const float MaxRadius = Config ? Config->SearchRadiusMax : BaseRadius * 2.0f;

    Task.CurrentSearchRadius = (Task.RetryCount == 0) ? BaseRadius : FMath::Min(MaxRadius, BaseRadius * (1.0f + 0.5f * Task.RetryCount));

    TArray<FOverlapResult> Hits;
    FCollisionObjectQueryParams ObjParams;
    ObjParams.AddObjectTypesToQuery(ECC_GameTraceChannel2); // Resource channel
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VHM_FindTarget), /*bTraceComplex*/false);

    const bool bOverlapped = Pawn->GetWorld()->OverlapMultiByObjectType(
        Hits,
        Origin,
        FQuat::Identity,
        ObjParams,
        FCollisionShape::MakeSphere(Task.CurrentSearchRadius),
        QueryParams
    );

    // Mark the time we attempted a search
    Ctx.LastSearchTime = Now;

    if (!bOverlapped || Hits.Num() == 0)
    {
        Task.RetryCount++;
        // Respect retry cap
        int32 MaxRetries = CVarVHM_RetryMax.GetValueOnGameThread();
        if (MaxRetries <= 0)
        {
            MaxRetries = Config ? Config->MaxRetries : 3;
        }
        if (Task.RetryCount >= MaxRetries)
        {
            UE_LOG(LogVHMStateTree, Verbose, TEXT("FindTarget: Retry cap reached for %s (radius=%.0f)."), *GetNameSafe(Pawn), Task.CurrentSearchRadius);
        }
        return EStateTreeRunStatus::Failed;
    }

    const FGameplayTag NeedTag = VHMStateTreeUtils::TagForNeed(Ctx.CurrentNeed);

    TArray<AVHMResourceActor*> Candidates;
    Candidates.Reserve(Hits.Num());

    for (const FOverlapResult& Hit : Hits)
    {
        AVHMResourceActor* Resource = Cast<AVHMResourceActor>(Hit.GetActor());
        if (!Resource)
        {
            continue;
        }

        // Tag filtering and blacklist
        if (!NeedTag.IsValid() || Resource->GetResourceTag().MatchesTag(NeedTag))
        {
            if (!Ctx.BlacklistedTargets.Contains(Resource))
            {
                Candidates.Add(Resource);
            }
        }
    }

    if (Candidates.Num() == 0)
    {
        Task.RetryCount++;
        return EStateTreeRunStatus::Failed;
    }

    Candidates.Sort([&](const AVHMResourceActor& A, const AVHMResourceActor& B)
    {
        const float DA = FVector::DistSquared(Origin, A.GetActorLocation());
        const float DB = FVector::DistSquared(Origin, B.GetActorLocation());
        return DA < DB;
    });

    // Try to reserve a candidate
    for (AVHMResourceActor* Resource : Candidates)
    {
        if (!IsValid(Resource))
        {
            continue;
        }

        if (!IVHMUsableResource::Execute_CanUse(Resource, Ctx.CurrentNeed, Pawn))
        {
            continue;
        }

        const FGuid Reservation = Resource->TryReserve(Pawn);
        if (!Reservation.IsValid())
        {
            continue;
        }

        // Found and reserved
        Ctx.TargetActor = Resource;
        Ctx.TargetLocation = Resource->GetActorLocation();
        Ctx.ReservationId = Reservation;
        Ctx.UseAcceptanceRadius = Resource ? Resource->GetUseRadius() : 150.0f;

        Task.RetryCount = 0;
        UE_LOG(LogVHMStateTree, Verbose, TEXT("FindTarget: Reserved %s for %s (need=%d, dist=%.0f)"), *GetNameSafe(Resource), *GetNameSafe(Pawn), (int32)Ctx.CurrentNeed, FVector::Dist(Origin, Ctx.TargetLocation));
        return EStateTreeRunStatus::Succeeded;
    }

    // None available, escalate next time
    Task.RetryCount++;
    {
        int32 MaxRetries = CVarVHM_RetryMax.GetValueOnGameThread();
        if (MaxRetries <= 0)
        {
            MaxRetries = Config ? Config->MaxRetries : 3;
        }
        if (Task.RetryCount >= MaxRetries)
        {
            UE_LOG(LogVHMStateTree, Verbose, TEXT("FindTarget: Retry cap reached for %s (candidates tried=%d)."), *GetNameSafe(Pawn), Candidates.Num());
        }
    }
    return EStateTreeRunStatus::Failed;
}


EStateTreeRunStatus FSTT_MoveToTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    FSTT_MoveToTaskInstance& Task = Context.GetInstanceData(*this);
    APawn* Pawn = GetOwnerPawn(Context);
    AVHMNPCAIController* AI = GetNPCController(Context);
    UVHMNeedsComponent* Needs = GetNeeds(Context);
    if (!Pawn || !AI || !Needs)
    {
        return EStateTreeRunStatus::Failed;
    }

    const FAgentNeedContext& Ctx = Needs->AgentContext;
    if (!Ctx.TargetActor)
    {
        return EStateTreeRunStatus::Failed;
    }

    const FVector Goal = Ctx.TargetActor ? Ctx.TargetActor->GetActorLocation() : Ctx.TargetLocation;

    FAIMoveRequest MoveReq(Goal);
    MoveReq.SetAcceptanceRadius(Ctx.UseAcceptanceRadius);

    // Navigation filter from species
    if (Needs)
    {
        if (const UVHMSpeciesDataAsset* Species = Needs->SpeciesData)
        {
            if (Species->Config.NavQueryFilter.Get() != nullptr)
            {
                MoveReq.SetNavigationFilter(Species->Config.NavQueryFilter);
            }
        }
    }

    const EPathFollowingRequestResult::Type Result = AI->MoveTo(MoveReq);
    if (Result == EPathFollowingRequestResult::Failed)
    {
        return EStateTreeRunStatus::Failed;
    }

    Task.LastGoalDistance = FVector::Dist(Pawn->GetActorLocation(), Goal);
    Task.TimeSinceProgress = 0.0f;
    Task.NextHeartbeatTime = NowSeconds(Pawn->GetWorld()) + HEARTBEAT_INTERVAL;
    UE_LOG(LogVHMStateTree, Verbose, TEXT("MoveTo: %s -> %s (accept=%.0f)"), *GetNameSafe(Pawn), *GetNameSafe(Ctx.TargetActor), Ctx.UseAcceptanceRadius);
    return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FSTT_MoveToTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
    FSTT_MoveToTaskInstance& Task = Context.GetInstanceData(*this);
    APawn* Pawn = GetOwnerPawn(Context);
    AVHMNPCAIController* AI = GetNPCController(Context);
    UVHMNeedsComponent* Needs = GetNeeds(Context);
    if (!Needs)
    {
        return EStateTreeRunStatus::Failed;
    }
    FAgentNeedContext& Ctx = Needs->AgentContext;
    AVHMResourceActor* Resource = Cast<AVHMResourceActor>(Ctx.TargetActor);
    if (!Pawn || !AI || !Resource)
    {
        return EStateTreeRunStatus::Failed;
    }

    const FVector Goal = Resource->GetActorLocation();
    const float Dist = FVector::Dist(Pawn->GetActorLocation(), Goal);

    // Arrival check
    if (Dist <= FMath::Max(10.0f, Ctx.UseAcceptanceRadius))
    {
        return EStateTreeRunStatus::Succeeded;
    }

    // Progress tracking
    if (Task.LastGoalDistance - Dist > PROGRESS_THRESHOLD_CM)
    {
        Task.LastGoalDistance = Dist;
        Task.TimeSinceProgress = 0.0f;
    }
    else
    {
        Task.TimeSinceProgress += DeltaTime;
        if (Task.TimeSinceProgress >= STUCK_TIMEOUT_SEC)
        {
            // Blacklist and fail
            Ctx.BlacklistedTargets.Add(Resource);
            UE_LOG(LogVHMStateTree, Verbose, TEXT("MoveTo: Stuck - blacklisting %s for %s"), *GetNameSafe(Resource), *GetNameSafe(Pawn));
            return EStateTreeRunStatus::Failed;
        }
    }

    // Heartbeat reservation periodically
    const float Now = NowSeconds(Pawn->GetWorld());
    if (Ctx.ReservationId.IsValid() && Now >= Task.NextHeartbeatTime)
    {
        if (!Resource->HeartbeatReservation(Ctx.ReservationId))
        {
            Ctx.BlacklistedTargets.Add(Resource);
            UE_LOG(LogVHMStateTree, Verbose, TEXT("MoveTo: Lost reservation on %s for %s"), *GetNameSafe(Resource), *GetNameSafe(Pawn));
            return EStateTreeRunStatus::Failed;
        }
        Task.NextHeartbeatTime = Now + HEARTBEAT_INTERVAL;
    }

    return EStateTreeRunStatus::Running;
}


EStateTreeRunStatus FSTT_UseResourceTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    FSTT_UseResourceTaskInstance& Task = Context.GetInstanceData(*this);
    APawn* Pawn = GetOwnerPawn(Context);
    UVHMNeedsComponent* Needs = GetNeeds(Context);
    if (!Needs)
    {
        return EStateTreeRunStatus::Failed;
    }
    FAgentNeedContext& Ctx = Needs->AgentContext;
    AVHMResourceActor* Resource = Cast<AVHMResourceActor>(Ctx.TargetActor);
    if (!Pawn || !Resource || !Ctx.ReservationId.IsValid())
    {
        return EStateTreeRunStatus::Failed;
    }

    Task.UseHandle = IVHMUsableResource::Execute_BeginUse(Resource, Pawn, Ctx.CurrentNeed, Ctx.ReservationId);
    if (!Task.UseHandle.IsValid())
    {
        return EStateTreeRunStatus::Failed;
    }

    UE_LOG(LogVHMStateTree, Verbose, TEXT("UseResource: Begin %s using %s"), *GetNameSafe(Pawn), *GetNameSafe(Resource));
    return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FSTT_UseResourceTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
    FSTT_UseResourceTaskInstance& Task = Context.GetInstanceData(*this);
    UVHMNeedsComponent* Needs = GetNeeds(Context);
    if (!Needs)
    {
        return EStateTreeRunStatus::Failed;
    }
    FAgentNeedContext& Ctx = Needs->AgentContext;
    AVHMResourceActor* Resource = Cast<AVHMResourceActor>(Ctx.TargetActor);
    if (!Resource || !Needs)
    {
        return EStateTreeRunStatus::Failed;
    }

    // Drive consumption and recovery
    const bool bAlive = IVHMUsableResource::Execute_TickUse(Resource, Task.UseHandle, DeltaTime);
    Needs->Recover(Ctx.CurrentNeed, DeltaTime);

    const FNeedStat Stat = Needs->GetNeedStat(Ctx.CurrentNeed);
    if (!bAlive || Stat.ShouldExitLow())
    {
        UE_LOG(LogVHMStateTree, Verbose, TEXT("UseResource: Complete for %s (need=%d)"), *GetNameSafe(GetOwnerPawn(Context)), (int32)Ctx.CurrentNeed);
        return EStateTreeRunStatus::Succeeded;
    }

    return EStateTreeRunStatus::Running;
}

void FSTT_UseResourceTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    FSTT_UseResourceTaskInstance& Task = Context.GetInstanceData(*this);
    UVHMNeedsComponent* Needs = GetNeeds(Context);
    FAgentNeedContext& Ctx = Needs ? Needs->AgentContext : *new FAgentNeedContext();
    if (AVHMResourceActor* Resource = Cast<AVHMResourceActor>(Ctx.TargetActor))
    {
        IVHMUsableResource::Execute_EndUse(Resource, Task.UseHandle);
    }

    Ctx.ReservationId = FGuid();
}


EStateTreeRunStatus FSTT_ExploreTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    FSTT_ExploreTaskInstance& Task = Context.GetInstanceData(*this);
    APawn* Pawn = GetOwnerPawn(Context);
    AVHMNPCAIController* AI = GetNPCController(Context);
    if (!Pawn || !AI)
    {
        return EStateTreeRunStatus::Failed;
    }

    const float MoveRadius = 600.0f;
    const float Duration = 2.5f;
    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(Pawn->GetWorld());
    if (!NavSys)
    {
        return EStateTreeRunStatus::Failed;
    }

    FNavLocation Dest;
    const bool bFound = NavSys->GetRandomPointInNavigableRadius(Pawn->GetActorLocation(), MoveRadius, Dest);
    if (!bFound)
    {
        return EStateTreeRunStatus::Failed;
    }

    AI->MoveToLocation(Dest.Location, /*Acceptance*/75.0f);
    Task.EndTime = NowSeconds(Pawn->GetWorld()) + Duration;
    Task.bMoveRequested = true;
    return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FSTT_ExploreTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
    FSTT_ExploreTaskInstance& Task = Context.GetInstanceData(*this);
    APawn* Pawn = GetOwnerPawn(Context);
    AVHMNPCAIController* AI = GetNPCController(Context);
    if (!Pawn || !AI)
    {
        return EStateTreeRunStatus::Failed;
    }

    const float Now = NowSeconds(Pawn->GetWorld());
    const EPathFollowingStatus::Type MoveStatus = AI->GetMoveStatus();
    if (Now >= Task.EndTime || MoveStatus == EPathFollowingStatus::Idle)
    {
        return EStateTreeRunStatus::Succeeded;
    }

    return EStateTreeRunStatus::Running;
}
