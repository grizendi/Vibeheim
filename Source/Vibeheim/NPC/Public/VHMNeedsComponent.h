#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VHMNPCTypes.h"
#include "VHMSpeciesDataAsset.h" // For FVHMSpeciesConfig
#include "VHMNeedsComponent.generated.h"

class UVHMSpeciesDataAsset;

UCLASS(ClassGroup=(Vibeheim), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class VIBEHEIM_API UVHMNeedsComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UVHMNeedsComponent();

    // Current need stats for this NPC (runtime state)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Needs")
    TMap<EVHMNeed, FNeedStat> Needs;

    // Species tuning data (optional). If provided, defaults are applied on BeginPlay.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Needs|Config")
    TObjectPtr<const UVHMSpeciesDataAsset> SpeciesData = nullptr;

    // Initialize from species data explicitly (can be called after spawning)
    UFUNCTION(BlueprintCallable, Category = "Needs")
    void InitializeFromSpecies(const UVHMSpeciesDataAsset* InSpecies);

    // Query the most pressing need and its urgency score [0..inf). Returns the selected need.
    UFUNCTION(BlueprintCallable, Category = "Needs")
    EVHMNeed GetMostPressingNeed(float& OutScore) const;

    // Recover a specific need by its per-species RecoverPerSec (called during resource use)
    UFUNCTION(BlueprintCallable, Category = "Needs")
    void Recover(EVHMNeed Need, float DeltaSeconds);

    // Read-only access to a specific need (returns copy)
    UFUNCTION(BlueprintPure, Category = "Needs")
    FNeedStat GetNeedStat(EVHMNeed Need) const;

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    // Cached species config (copied from SpeciesData if available)
    FVHMSpeciesConfig SpeciesConfig;

    // Cooldown timestamp (WorldTimeSeconds) until which a need should not be selected after satiation
    TMap<EVHMNeed, float> CooldownUntil;

    // Track previous low-state to detect Low->ExitLow transitions for cooldown application
    TMap<EVHMNeed, bool> WasLowLastStep;

    // Track starvation/dehydration timers for health penalty
    float ZeroHungerAccum = 0.0f;
    float ZeroThirstAccum = 0.0f;

    // Accumulator for fixed/quantized update steps
    float TimeAccumulator = 0.0f;

    // One simulation step (decay + penalty + hysteresis bookkeeping)
    void StepNeeds(float StepSeconds);

    // Resolve effective evaluation interval considering LOD and CVars
    float GetEffectiveEvalInterval() const;

    // Resolve effective LOD distance (cm) considering CVars
    float GetEffectiveLODDistance() const;

    // Utility to get current world time in seconds
    float Now() const;

    // Mutable access to a need stat (creates if missing)
    FNeedStat& GetOrAddNeed(EVHMNeed Need);
};
