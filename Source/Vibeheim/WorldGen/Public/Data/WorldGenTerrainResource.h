#pragma once
#include "CoreMinimal.h"
#include "Data/WorldGenTypes.h"
#include "Services/BiomeService.h"
#include "Engine/DataAsset.h"
#include "WorldGenTerrainResource.generated.h"


class UTexture2D;

/**
 * Stores prebaked terrain data (heightmaps, metadata) for use by VHM Renderer
 * in EditorBuildOnce mode, bypassing runtime generation.
 */
UCLASS(BlueprintType)
class VIBEHEIM_API UWorldGenTerrainResource : public UDataAsset {
  GENERATED_BODY()
public:
  // Coordinate system metadata (world origin in Unreal units)
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Coordinates")
  FVector2D WorldOrigin = FVector2D::ZeroVector;

  // Tile dimensions (meters, matches FWorldGenConfig.TileSizeMeters)
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Coordinates")
  float TileSizeMeters = 64.0f;

  // Distance between samples inside a tile (meters)
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Coordinates")
  float SampleSpacingMeters = 1.0f;

  // Global terrain metadata
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
  float MinHeight = 0.0f;

  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
  float MaxHeight = 0.0f;

  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
  float SeaLevel = 0.0f;

  /** Map of tile coordinates to prebaked heightmap textures */
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
  TMap<FIntPoint, TSoftObjectPtr<UTexture2D>> HeightTextures;

  /** Map of tile coordinates to dominant biome cache (per tile center) */
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
  TMap<FIntPoint, FBiomeResult> BiomeCache;

  /** Convert world position to tile coord using origin and tile size. */
  FTileCoord WorldPosToTile(const FVector &WorldPos) const;

  /** True when the given tile has prebaked data. */
  UFUNCTION(BlueprintCallable, Category = "Terrain")
  bool HasTileData(const FTileCoord &Tile) const;

  /** Sample prebaked height at world position (returns SeaLevel on failure). */
  UFUNCTION(BlueprintCallable, Category = "Terrain")
  float GetHeightAtWorldPosition(const FVector &WorldPos) const;

  /** Return dominant biome for the tile containing WorldPos. */
  UFUNCTION(BlueprintCallable, Category = "Terrain")
  FBiomeResult GetBiomeAtWorldPosition(const FVector &WorldPos) const;
};
