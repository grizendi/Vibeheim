#pragma once

#include "CoreMinimal.h"
#include "VHMNPCTypes.h"
#include "VHMNPCAIController.h"
#include "VHMNeedsComponent.h"
#include "VHMResourceActor.h"
#include "VHMSpeciesDataAsset.h"
#include "VHMNPCTags.h"

#include "Engine/EngineTypes.h"
#include "Engine/OverlapResult.h"

#if __has_include("StateTree/StateTreeTaskBase.h")
#include "StateTree/StateTreeTaskBase.h"
#include "StateTree/StateTreeExecutionContext.h"
#elif __has_include("StateTreeTaskBase.h")
#include "StateTreeTaskBase.h"
#include "StateTreeExecutionContext.h"
#else
#error "StateTree headers not found. Ensure StateTreeModule is enabled for UE 5.6."
#endif

#include "AIController.h"
#include "NavigationSystem.h"

#include "VHMStateTreeTasks.generated.h"

// Helpers
namespace VHMStateTreeUtils
{
    static inline FGameplayTag TagForNeed(EVHMNeed Need)
    {
        switch (Need)
        {
            case EVHMNeed::Hunger: return VHMNPCTags::ResourceFood();
            case EVHMNeed::Thirst: return VHMNPCTags::ResourceWater();
            case EVHMNeed::Energy: return VHMNPCTags::ResourceShelter();
            case EVHMNeed::Health: return VHMNPCTags::ResourceShelter();
            default: return FGameplayTag();
        }
    }
}

USTRUCT(BlueprintType)
struct VIBEHEIM_API FSTT_EvaluateNeedTaskInstance
{
    GENERATED_BODY()
};

USTRUCT(BlueprintType, meta = (DisplayName = "Evaluate Need"))
struct VIBEHEIM_API FSTT_EvaluateNeedTask : public FStateTreeTaskCommonBase
{
    GENERATED_BODY()

    using FInstanceDataType = FSTT_EvaluateNeedTaskInstance;

    virtual const UStruct* GetInstanceDataType() const override { return FSTT_EvaluateNeedTaskInstance::StaticStruct(); }

    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};


USTRUCT(BlueprintType)
struct VIBEHEIM_API FSTT_FindTargetTaskInstance
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "FindTarget")
    int32 RetryCount = 0;

    UPROPERTY(EditAnywhere, Category = "FindTarget")
    float CurrentSearchRadius = 0.0f;

    // Reusable buffers to avoid per-execution allocations
    UPROPERTY(Transient)
    TArray<FOverlapResult> ReusableHits;

    UPROPERTY(Transient)
    TArray<AVHMResourceActor*> Candidates;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Find Target"))
struct VIBEHEIM_API FSTT_FindTargetTask : public FStateTreeTaskCommonBase
{
    GENERATED_BODY()

    using FInstanceDataType = FSTT_FindTargetTaskInstance;
    virtual const UStruct* GetInstanceDataType() const override { return FSTT_FindTargetTaskInstance::StaticStruct(); }

    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};


USTRUCT(BlueprintType)
struct VIBEHEIM_API FSTT_MoveToTaskInstance
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "MoveTo")
    float LastGoalDistance = 0.0f;

    UPROPERTY(EditAnywhere, Category = "MoveTo")
    float TimeSinceProgress = 0.0f;

    UPROPERTY(EditAnywhere, Category = "MoveTo")
    float NextHeartbeatTime = 0.0f;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Move To Target"))
struct VIBEHEIM_API FSTT_MoveToTask : public FStateTreeTaskCommonBase
{
    GENERATED_BODY()

    using FInstanceDataType = FSTT_MoveToTaskInstance;
    virtual const UStruct* GetInstanceDataType() const override { return FSTT_MoveToTaskInstance::StaticStruct(); }

    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
    virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};


USTRUCT(BlueprintType)
struct VIBEHEIM_API FSTT_UseResourceTaskInstance
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "UseResource")
    FVHMUseHandle UseHandle;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Use Resource"))
struct VIBEHEIM_API FSTT_UseResourceTask : public FStateTreeTaskCommonBase
{
    GENERATED_BODY()

    using FInstanceDataType = FSTT_UseResourceTaskInstance;
    virtual const UStruct* GetInstanceDataType() const override { return FSTT_UseResourceTaskInstance::StaticStruct(); }

    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
    virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
    virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};


USTRUCT(BlueprintType)
struct VIBEHEIM_API FSTT_ExploreTaskInstance
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Explore")
    float EndTime = 0.0f;

    UPROPERTY(EditAnywhere, Category = "Explore")
    bool bMoveRequested = false;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Explore"))
struct VIBEHEIM_API FSTT_ExploreTask : public FStateTreeTaskCommonBase
{
    GENERATED_BODY()

    using FInstanceDataType = FSTT_ExploreTaskInstance;
    virtual const UStruct* GetInstanceDataType() const override { return FSTT_ExploreTaskInstance::StaticStruct(); }

    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
    virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};
