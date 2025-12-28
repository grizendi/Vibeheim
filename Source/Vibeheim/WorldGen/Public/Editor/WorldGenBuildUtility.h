#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "WorldGenBuildUtility.generated.h"

class UBiomeService;
class UClimateSystem;
class UHeightfieldService;
class UTexture2D;
class UWorld;
class UWorldGenExternalDataProvider;
class UWorldGenTerrainResource;
class UPCGWorldService;
struct FWorldGenConfig;
struct FTileCoord;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnBuildProgress, int32, CurrentStep,
                                               int32, TotalSteps, const FString&, Status);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBuildComplete, bool, bSuccess,
                                             const FString&, Message);

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
   * @param MapPath Optional map path override (package path or asset path)
   * @param bBuildTerrain When false, skips terrain bake and expects prebaked data
   * @param bBuildPCG When false, skips offline PCG builder
   * @return True if successful
   */
  UFUNCTION(BlueprintCallable, Category = "WorldGen|Build")
  static bool BuildWorldFromSeed(int32 Seed,
                                 const FString& MapPath = TEXT(""),
                                 bool bBuildTerrain = true,
                                 bool bBuildPCG = true);

  /**
   * Instance version that supports progress delegates for UI widgets.
   */
  UFUNCTION(BlueprintCallable, Category = "WorldGen|Build")
  bool BuildWorldFromSeedInstance(int32 Seed,
                                  const FString& MapPath = TEXT(""),
                                  bool bBuildTerrain = true,
                                  bool bBuildPCG = true);

  /**
   * Aligns the PCG World Actor's partition grid size with the WorldGen
   * TileSize. Spawns APCGWorldActor if missing.
   */
  UFUNCTION(BlueprintCallable, Category = "WorldGen|Build")
  static bool AlignPCGGridWithSettings();

  /** Progress multicast for editor UI consumption. */
  UPROPERTY(BlueprintAssignable, Category = "WorldGen|Build")
  FOnBuildProgress OnBuildProgress;

  /** Completion multicast for editor UI consumption. */
  UPROPERTY(BlueprintAssignable, Category = "WorldGen|Build")
  FOnBuildComplete OnBuildComplete;

private:
  /**
   * Helper to initialize services for the build process.
   */
  static bool InitializeBuildServices(UObject *ContextObject,
                                      UHeightfieldService *&OutHeightfield,
                                      UBiomeService *&OutBiome,
                                      UClimateSystem *&OutClimate,
                                      UPCGWorldService *&OutPCG);

  /** Validate that we are running in an editor, non-PIE context. */
  bool ValidateEditorContext(UWorld *&OutWorld, FString &OutError) const;

  /** Orchestrate the full build once context/services are ready. */
  bool RunBuild(UWorld *World, int32 Seed, const FString &MapPath,
                bool bBuildTerrain, bool bBuildPCG, bool bUpdateBuildState);

  /** Generate prebaked terrain and biome cache for a single tile. */
  bool BuildTerrainForTile(UWorldGenTerrainResource *TerrainResource,
                           UHeightfieldService *HeightfieldService,
                           UBiomeService *BiomeService,
                           const FWorldGenConfig &Config,
                           const FTileCoord &TileCoord, FString &OutError,
                           float &OutTileMin, float &OutTileMax);

  /** Invoke the PCG World Partition builder / offline build path. */
  bool TriggerPCGOfflineBuild(UWorld *World, const FWorldGenConfig &Config,
                              UWorldGenTerrainResource *TerrainResource,
                              UPCGWorldService *PCGService,
                              UWorldGenExternalDataProvider *DataProvider,
                              FString &OutPCGHash, TArray<FString> &OutErrors);

  /** Persist build metadata into a companion build state asset. */
  bool SaveBuildState(UWorld *World, int32 Seed,
                      const FWorldGenConfig &Config, const FString &PCGHash,
                      FString &OutError) const;

  /** Broadcast progress to delegates and optionally Slate. */
  void ReportProgress(int32 Current, int32 Total, const FString &Status);

  /** Helper to broadcast completion events. */
  void BroadcastCompletion(bool bSuccess, const FString &Message);

  /** Build deterministic package/object names from the map identifier. */
  static FString MakePackagePath(const FString &MapIdentifier,
                                 const FString &Suffix);
  static FString MakeObjectName(const FString &MapIdentifier,
                                const FString &BaseName);

#if WITH_AUTOMATION_TESTS
public:
  /** Test-only entry to evaluate context flags deterministically. */
  static bool EvaluateContextForTest(bool bIsEditor, bool bIsRunningGame,
                                     EWorldType::Type WorldType);
#endif
};
