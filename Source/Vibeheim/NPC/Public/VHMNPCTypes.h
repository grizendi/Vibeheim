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

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<AActor*> BlacklistedTargets;

    // Active reservation token granted by resource during FindTarget.
    // Valid until heartbeat fails or EndUse() releases it.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid ReservationId = FGuid();

    // Cached acceptance radius when moving/using a resource.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float UseAcceptanceRadius = 150.0f;
};
