#pragma once
#include "CoreMinimal.h"
#include "Data/WorldGenTypes.h"
#include "Engine/DataAsset.h"
#include "WorldGenTerrainResource.generated.h"


class UTexture2D;

/**
 * Stores baked terrain data (heightmaps, metadata) for use by VHM Renderer
 * in EditorBuildOnce mode, bypassing runtime generation.
 */
UCLASS(BlueprintType)
class VIBEHEIM_API UWorldGenTerrainResource : public UDataAsset {
  GENERATED_BODY()
public:
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
  int32 Seed = 0;

  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
  FTileCoord GridCenter;

  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
  int32 Radius = 0;

  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
  float MinHeight = 0.0f;

  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
  float MaxHeight = 0.0f;

  /** Map of tile coordinates to baked heightmap textures */
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
  TMap<FTileCoord, TObjectPtr<UTexture2D>> HeightTextures;

  // Future expansion: Biome maps, etc.
};
