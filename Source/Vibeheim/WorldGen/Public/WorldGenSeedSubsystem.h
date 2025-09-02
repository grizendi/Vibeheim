#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WorldGenSeedSubsystem.generated.h"

/**
 * Authoritative seed source for all world generation systems
 * Ensures all systems use the same seed for deterministic generation
 */
UCLASS()
class VIBEHEIM_API UWorldGenSeedSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// Seed management
	UFUNCTION(BlueprintCallable, Category = "WorldGen")
	int32 GetAuthoritativeSeed() const { return AuthoritativeSeed; }

	UFUNCTION(BlueprintCallable, Category = "WorldGen")
	void SetAuthoritativeSeed(int32 NewSeed);

	// Ensure all systems read from this authoritative source
	UFUNCTION(BlueprintCallable, Category = "WorldGen")
	void PropagateToAllSystems();

private:
	UPROPERTY()
	int32 AuthoritativeSeed = 1337;
};
