#include "VHMNeedsComponent.h"

#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "VHMNPCLogging.h"
#include "VHMNPCVars.h"
#include "VHMSpeciesDataAsset.h"

UVHMNeedsComponent::UVHMNeedsComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UVHMNeedsComponent::BeginPlay()
{
    Super::BeginPlay();

    if (SpeciesData)
    {
        SpeciesConfig = SpeciesData->Config; // copy for runtime use
    }

    // Ensure all baseline needs exist with sane defaults if not authored
    GetOrAddNeed(EVHMNeed::Hunger).Type = EVHMNeed::Hunger;
    GetOrAddNeed(EVHMNeed::Thirst).Type = EVHMNeed::Thirst;
    GetOrAddNeed(EVHMNeed::Energy).Type = EVHMNeed::Energy;
    GetOrAddNeed(EVHMNeed::Health).Type = EVHMNeed::Health;
}

void UVHMNeedsComponent::InitializeFromSpecies(const UVHMSpeciesDataAsset* InSpecies)
{
    SpeciesData = InSpecies;
    if (SpeciesData)
    {
        SpeciesConfig = SpeciesData->Config;

        // Apply defaults from species into the runtime map
        for (const TPair<EVHMNeed, FNeedStat>& Pair : SpeciesConfig.DefaultNeeds)
        {
            Needs.Add(Pair.Key, Pair.Value);
        }
    }

    // Ensure all baseline needs exist even if not in data
    GetOrAddNeed(EVHMNeed::Hunger).Type = EVHMNeed::Hunger;
    GetOrAddNeed(EVHMNeed::Thirst).Type = EVHMNeed::Thirst;
    GetOrAddNeed(EVHMNeed::Energy).Type = EVHMNeed::Energy;
    GetOrAddNeed(EVHMNeed::Health).Type = EVHMNeed::Health;
}

void UVHMNeedsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Quantized/Fixed step integration
    const float Step = FMath::Max(0.001f, GetEffectiveEvalInterval());
    const bool bDeterministic = CVarVHM_Deterministic.GetValueOnGameThread() != 0;

    TimeAccumulator += DeltaTime;

    // In deterministic mode, integrate using fixed steps to remove DeltaTime variance
    // In non-deterministic mode, also quantize to Step to reduce update frequency for perf
    while (TimeAccumulator >= Step)
    {
        StepNeeds(Step);
        TimeAccumulator -= Step;
    }

    // If accumulator never reached Step (very small Delta), do nothing this frame.
    // StateTree querying for GetMostPressingNeed() is lock-free and uses current members.
}

void UVHMNeedsComponent::StepNeeds(float StepSeconds)
{
    const int32 bFast = CVarVHM_FastDecay.GetValueOnGameThread();
    const float RateScale = bFast ? 5.0f : 1.0f;

    // Apply decay per need
    for (TPair<EVHMNeed, FNeedStat>& Pair : Needs)
    {
        FNeedStat& S = Pair.Value;
        const float Decay = FMath::Max(0.0f, S.DecayPerSec) * StepSeconds * RateScale;
        S.Current = FMath::Clamp(S.Current - Decay, 0.0f, S.Max);
    }

    // Starvation/dehydration health penalty
    FNeedStat& Hunger = GetOrAddNeed(EVHMNeed::Hunger);
    FNeedStat& Thirst = GetOrAddNeed(EVHMNeed::Thirst);
    FNeedStat& Health = GetOrAddNeed(EVHMNeed::Health);

    ZeroHungerAccum = (Hunger.Current <= 0.0f) ? (ZeroHungerAccum + StepSeconds) : 0.0f;
    ZeroThirstAccum = (Thirst.Current <= 0.0f) ? (ZeroThirstAccum + StepSeconds) : 0.0f;

    const bool bPenalty = (ZeroHungerAccum >= SpeciesConfig.ZeroNeedPenaltyDelay) || (ZeroThirstAccum >= SpeciesConfig.ZeroNeedPenaltyDelay);
    if (bPenalty && Health.Current > 0.0f)
    {
        const float Penalty = FMath::Max(0.0f, SpeciesConfig.HealthPenaltyPerSec) * StepSeconds * RateScale;
        Health.Current = FMath::Max(0.0f, Health.Current - Penalty);
    }

    // Hysteresis & cooldown bookkeeping
    const float NowTime = Now();
    for (TPair<EVHMNeed, FNeedStat>& Pair : Needs)
    {
        const EVHMNeed N = Pair.Key;
        FNeedStat& S = Pair.Value;

        const bool bWasLow = WasLowLastStep.FindRef(N);
        const bool bIsLow = S.IsLow();

        if (bWasLow && S.ShouldExitLow())
        {
            // Enter post-satiation cooldown
            CooldownUntil.Add(N, NowTime + SpeciesConfig.PostSatiationCooldown);
        }

        WasLowLastStep.Add(N, bIsLow);
    }
}

EVHMNeed UVHMNeedsComponent::GetMostPressingNeed(float& OutScore) const
{
    OutScore = 0.0f;
    EVHMNeed BestNeed = EVHMNeed::Hunger;

    const float NowTime = Now();

    for (const TPair<EVHMNeed, FNeedStat>& Pair : Needs)
    {
        const EVHMNeed NeedType = Pair.Key;
        const FNeedStat& S = Pair.Value;

        float Score = 0.0f;

        // Basic deficit-based score
        if (S.Max > 0.0f)
        {
            const float Deficit = S.Max - S.Current;
            Score = Deficit / S.Max; // 0..1
        }

        // Criticality multiplier
        if (S.IsCritical())
        {
            Score *= 2.0f;
        }

        // Post-satiation cooldown (skip unless critical)
        const float* CooldownPtr = CooldownUntil.Find(NeedType);
        if (CooldownPtr && NowTime < *CooldownPtr && !S.IsCritical())
        {
            Score = 0.0f;
        }

        if (Score > OutScore)
        {
            OutScore = Score;
            BestNeed = NeedType;
        }
    }

    return BestNeed;
}

void UVHMNeedsComponent::Recover(EVHMNeed Need, float DeltaSeconds)
{
    FNeedStat& S = GetOrAddNeed(Need);
    const int32 bFast = CVarVHM_FastDecay.GetValueOnGameThread();
    const float RateScale = bFast ? 5.0f : 1.0f;
    const float Rec = FMath::Max(0.0f, S.RecoverPerSec) * DeltaSeconds * RateScale;
    S.Current = FMath::Clamp(S.Current + Rec, 0.0f, S.Max);
}

FNeedStat UVHMNeedsComponent::GetNeedStat(EVHMNeed Need) const
{
    if (const FNeedStat* Found = Needs.Find(Need))
    {
        return *Found;
    }
    // Return a default-initialized stat with the requested type if missing
    FNeedStat Default;
    Default.Type = Need;
    return Default;
}

float UVHMNeedsComponent::GetEffectiveEvalInterval() const
{
    // Global override via CVars (support two names for compatibility)
    float EvalOverride = CVarVHM_EvalInterval.GetValueOnGameThread();
    if (EvalOverride <= 0.0f)
    {
        EvalOverride = CVarVHM_EvaluationInterval.GetValueOnGameThread();
    }

    if (EvalOverride > 0.0f)
    {
        return EvalOverride;
    }

    // Species-driven intervals with distance LOD
    const float LODDist = GetEffectiveLODDistance();
    float Interval = SpeciesConfig.EvaluationInterval;

    const AActor* OwnerActor = GetOwner();
    const APawn* Player0 = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    if (OwnerActor && Player0 && LODDist > 0.0f)
    {
        const float DistSq = FVector::DistSquared(OwnerActor->GetActorLocation(), Player0->GetActorLocation());
        if (DistSq > FMath::Square(LODDist))
        {
            Interval = SpeciesConfig.LODEvaluationInterval;
        }
    }

    return FMath::Max(0.01f, Interval);
}

float UVHMNeedsComponent::GetEffectiveLODDistance() const
{
    const float CVarDist = CVarVHM_LODDistance.GetValueOnGameThread();
    return (CVarDist > 0.0f) ? CVarDist : SpeciesConfig.LODDistance;
}

float UVHMNeedsComponent::Now() const
{
    const UWorld* World = GetWorld();
    return World ? World->GetTimeSeconds() : 0.0f;
}

FNeedStat& UVHMNeedsComponent::GetOrAddNeed(EVHMNeed Need)
{
    FNeedStat* Found = Needs.Find(Need);
    if (Found)
    {
        return *Found;
    }
    FNeedStat NewStat;
    NewStat.Type = Need;
    return Needs.Add(Need, NewStat);
}
