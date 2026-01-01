#pragma once

#include "CoreMinimal.h"
#include "Data/WorldGenTypes.h"
#include "GameFramework/Actor.h"
#include "PCGComponent.h"
#include "VibeheimWorldGenPCGBakeActor.generated.h"

/**
 * Persistent actor placed in the world to hold PCG components for baking.
 * This actor ensures that PCG content is generated via the World Partition
 * Builder rather than transient runtime generation.
 */
UCLASS(Blueprintable, BlueprintType)
class VIBEHEIM_API AVibeheimWorldGenPCGBakeActor : public AActor {
  GENERATED_BODY()

public:
  AVibeheimWorldGenPCGBakeActor();

  /** Configure this actor with components based on biome definitions. */
  UFUNCTION(BlueprintCallable, CallInEditor, Category = "World Gen")
  void ConfigureFromSettings(const UWorldGenSettings *Settings);

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
  class UBoxComponent *Root;

  /** Map of biome type to its PCG component (for reference/debugging) */
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Gen")
  TMap<EBiomeType, UPCGComponent *> BiomeComponents;

protected:
  virtual void BeginPlay() override;
};
