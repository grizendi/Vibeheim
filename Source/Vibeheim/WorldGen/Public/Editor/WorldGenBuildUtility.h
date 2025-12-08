#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "WorldGenBuildUtility.generated.h"

class UHeightfieldService;
class UBiomeService;
class UClimateSystem;
class UPCGWorldService;

/**
 * Editor-only utility to bake the Vibeheim world from a seed.
 * Orchestrates heightfield generation, biome classification, and PCG partition
 * population without relying on runtime tile streaming.
 */
UCLASS(BlueprintType)
class VIBEHEIM_API UWorldGenBuildUtility : public UObject {
  GENERATED_BODY()

public:
  /**
   * Main entry point to build the world for the current editor world.
   * @param Seed World seed from config (if 0, uses config default)
   * @return True if successful
   */
  UFUNCTION(BlueprintCallable, Category = "WorldGen|Build")
  static bool BuildWorldFromSeed(int32 Seed);

  /**
   * Aligns the PCG World Actor's partition grid size with the WorldGen
   * TileSize. Spawns APCGWorldActor if missing.
   */
  UFUNCTION(BlueprintCallable, Category = "WorldGen|Build")
  static bool AlignPCGGridWithSettings();

private:
  /**
   * Helper to initialize services for the build process.
   */
  static bool InitializeBuildServices(UObject *ContextObject,
                                      UHeightfieldService *&OutHeightfield,
                                      UBiomeService *&OutBiome,
                                      UClimateSystem *&OutClimate,
                                      UPCGWorldService *&OutPCG);
};
