#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Misc/Guid.h"
#include "VHMNPCTypes.generated.h"

class AActor;
class APawn;

UENUM(BlueprintType)
enum class EVHMNeed : uint8
{
    Hunger,
    Thirst,
    Energy,
    Health
};

USTRUCT(BlueprintType)
struct VIBEHEIM_API FNeedStat
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EVHMNeed Type = EVHMNeed::Hunger;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Current = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Max = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DecayPerSec = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RecoverPerSec = 30.0f;

    // Hysteresis thresholds to prevent oscillation
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EnterLowThreshold = 60.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ExitLowThreshold = 70.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CriticalThreshold = 25.0f;

    // Utility helpers (no side effects)
    float Deficit() const { return Max - Current; }
    bool IsLow() const { return Current <= EnterLowThreshold; }
    bool IsCritical() const { return Current <= CriticalThreshold; }
    bool ShouldExitLow() const { return Current >= ExitLowThreshold; }
};

// Lightweight reservation token for resource usage
USTRUCT(BlueprintType)
struct VIBEHEIM_API FVHMUseHandle
{
    GENERATED_BODY()

    UPROPERTY()
    TWeakObjectPtr<AActor> Resource;

    UPROPERTY()
    TWeakObjectPtr<APawn> User;

    // Default constructor leaves ReservationId invalid (zero-init per UE5.6 standards)
    UPROPERTY()
    FGuid ReservationId = FGuid();

    UPROPERTY()
    float LastHeartbeatTime = 0.0f;

    bool IsValid() const { return Resource.IsValid() && User.IsValid() && ReservationId.IsValid(); }
};

// Per-agent context shared across StateTree tasks
USTRUCT(BlueprintType)
struct VIBEHEIM_API FAgentNeedContext
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EVHMNeed CurrentNeed = EVHMNeed::Hunger;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    AActor* TargetActor = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector TargetLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float NeedScore = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LastEvaluationTime = 0.0f;

    // Last time a resource search was attempted (WorldTimeSeconds)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LastSearchTime = 0.0f;

    // Legacy simple blacklist (kept for BP/debug compatibility). Not time-based.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<AActor*> BlacklistedTargets;

    // LRU/time-based blacklist entries for perf and recovery
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 BlacklistMaxSize = 16;

    UPROPERTY()
    TArray<TWeakObjectPtr<AActor>> BlacklistActors;

    UPROPERTY()
    TArray<float> BlacklistExpiryTimes;

    // Active reservation token granted by resource during FindTarget.
    // Valid until heartbeat fails or EndUse() releases it.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid ReservationId = FGuid();

    // Cached acceptance radius when moving/using a resource.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float UseAcceptanceRadius = 150.0f;

    // Helpers for blacklist management (non-UFUNCTION by design)
    FORCEINLINE void PruneBlacklist(float Now)
    {
        // Remove expired or invalid entries while keeping arrays compact
        for (int32 i = BlacklistActors.Num() - 1; i >= 0; --i)
        {
            const bool bExpired = (i < BlacklistExpiryTimes.Num()) ? (Now > BlacklistExpiryTimes[i]) : true;
            if (!BlacklistActors[i].IsValid() || bExpired)
            {
                BlacklistActors.RemoveAtSwap(i, 1, EAllowShrinking::No);
                if (i < BlacklistExpiryTimes.Num())
                {
                    BlacklistExpiryTimes.RemoveAtSwap(i, 1, EAllowShrinking::No);
                }
            }
        }
    }

    FORCEINLINE bool IsBlacklisted(const AActor* Actor, float Now) const
    {
        if (!Actor)
        {
            return false;
        }
        // Check legacy array first (non-expiring)
        if (BlacklistedTargets.Contains(const_cast<AActor*>(Actor)))
        {
            return true;
        }
        // Check time-based entries
        for (int32 i = 0; i < BlacklistActors.Num(); ++i)
        {
            if (BlacklistActors[i].Get() == Actor)
            {
                const float Until = (i < BlacklistExpiryTimes.Num()) ? BlacklistExpiryTimes[i] : 0.0f;
                return Now <= Until;
            }
        }
        return false;
    }

    FORCEINLINE void AddToBlacklist(AActor* Actor, float Duration, float Now)
    {
        if (!Actor)
        {
            return;
        }

        // Also mirror to legacy array for debug visibility (bounded)
        if (!BlacklistedTargets.Contains(Actor))
        {
            // Keep legacy list reasonably small
            if (BlacklistedTargets.Num() >= FMath::Max(BlacklistMaxSize, 1))
            {
                BlacklistedTargets.RemoveAt(0);
            }
            BlacklistedTargets.Add(Actor);
        }

        PruneBlacklist(Now);

        // If already present, refresh expiry
        for (int32 i = 0; i < BlacklistActors.Num(); ++i)
        {
            if (BlacklistActors[i].Get() == Actor)
            {
                if (i < BlacklistExpiryTimes.Num())
                {
                    BlacklistExpiryTimes[i] = Now + FMath::Max(0.0f, Duration);
                }
                return;
            }
        }

        // Enforce capacity (LRU eviction: drop the oldest/front)
        const int32 Cap = FMath::Max(BlacklistMaxSize, 1);
        if (BlacklistActors.Num() >= Cap)
        {
            BlacklistActors.RemoveAt(0, 1, EAllowShrinking::No);
            if (BlacklistExpiryTimes.Num() > 0)
            {
                BlacklistExpiryTimes.RemoveAt(0, 1, EAllowShrinking::No);
            }
        }

        BlacklistActors.Add(Actor);
        BlacklistExpiryTimes.Add(Now + FMath::Max(0.0f, Duration));
    }
};
