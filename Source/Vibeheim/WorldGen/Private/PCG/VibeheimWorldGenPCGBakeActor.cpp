#include "PCG/VibeheimWorldGenPCGBakeActor.h"
#include "Components/SceneComponent.h"
#include "Data/WorldGenAssets.h"
#include "PCGGraph.h"
#include "WorldGenSettings.h"

#include "Components/BoxComponent.h"

AVibeheimWorldGenPCGBakeActor::AVibeheimWorldGenPCGBakeActor() {
  PrimaryActorTick.bCanEverTick = false;

  Root = CreateDefaultSubobject<UBoxComponent>(TEXT("Root"));
  Root->SetBoxExtent(
      FVector(10000000.0f, 10000000.0f, 10000000.0f)); // Covers huge area
  SetRootComponent(Root);

  SetActorLabel(TEXT("VibeheimWorldGenPCGBakeActor"));
}

void AVibeheimWorldGenPCGBakeActor::BeginPlay() { Super::BeginPlay(); }

void AVibeheimWorldGenPCGBakeActor::ConfigureFromSettings(
    const UWorldGenSettings *Settings) {
  if (!Settings) {
    return;
  }

  // Clear existing dynamic components (optional, but safer to just update or
  // reuse if IDK) For now, we assume this is called during build setup.

  UBiomeDefinitionsAsset *BiomeAsset =
      Settings->SelectedBiomeDefinitionsAsset.LoadSynchronous();
  if (!BiomeAsset) {
    return;
  }

  // Iterate biomes and spawn/configure PCG components
  for (const TPair<EBiomeType, FBiomeDefinition> &Pair : BiomeAsset->Biomes) {
    const EBiomeType BiomeType = Pair.Key;
    const FBiomeDefinition &BiomeDef = Pair.Value;

    if (BiomeDef.BiomePCGGraph.IsNull()) {
      continue;
    }

    UPCGGraph *Graph = BiomeDef.BiomePCGGraph.LoadSynchronous();
    if (!Graph) {
      continue;
    }

    FString CompName = FString::Printf(TEXT("PCG_%s"), *BiomeDef.BiomeName);
    UPCGComponent *ExistingComp = FindObject<UPCGComponent>(this, *CompName);

    UPCGComponent *PCGComp = ExistingComp;
    if (!PCGComp) {
      PCGComp = NewObject<UPCGComponent>(this, *CompName);
      PCGComp->RegisterComponent();
    }

    PCGComp->SetGraph(Graph);
    PCGComp->GenerationTrigger =
        EPCGComponentGenerationTrigger::GenerateOnDemand; // Controlled by
                                                          // builder

    // Fix for "invalid bounds" error:
    // We will ensure the actor has valid bounds via BoxComponent instead.
    // PCGComp->bIsUnbounded = true; // Not a valid member

    // Ensure it's not transient
    PCGComp->SetFlags(RF_Transactional);

    BiomeComponents.Add(BiomeType, PCGComp);
  }
}
