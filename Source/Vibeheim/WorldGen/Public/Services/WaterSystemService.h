#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/WorldGenTypes.h"
#include "Services/IHeightfieldService.h"
#include "WaterSystemService.generated.h"

/**
 * Service responsible for tile-based water body management and shoreline data
 * - Computes per-tile water/shoreline masks and distance-to-water field
 * - Spawns/cleans up simple placeholder actors for water tiles (minimal integration)
 */
UCLASS(BlueprintType)
class VIBEHEIM_API UWaterSystemService : public UObject
{
    GENERATED_BODY()

public:
    UWaterSystemService();

    /** Initialize with worldgen + water settings */
    UFUNCTION(BlueprintCallable, Category = "Water")
    bool Initialize(const FWorldGenConfig& InWorldGen, const FWaterSystemConfig& InConfig);

    /** Update runtime config (optional) */
    UFUNCTION(BlueprintCallable, Category = "Water")
    void SetConfig(const FWaterSystemConfig& InConfig);

    /** Compute shoreline/water data for a tile and optionally spawn placeholder actor */
    void OnTileActivated(const FTileCoord& TileCoord, const FHeightfieldData& Heightfield, FTileWaterData& OutWaterData);

    /** Cleanup any water data/actors for a tile */
    void OnTileDeactivated(const FTileCoord& TileCoord);

private:
    UPROPERTY()
    FWorldGenConfig WorldGenSettings;

    UPROPERTY()
    FWaterSystemConfig Config;

    // Minimal placeholder actors spawned per active water tile
    UPROPERTY(Transient)
    TMap<FTileCoord, TWeakObjectPtr<AActor>> ActiveWaterActors;

private:
    bool ComputeTileWaterData(const FHeightfieldData& HF, FTileWaterData& OutData) const;
    void SpawnOrUpdateWaterActor(const FTileCoord& TileCoord, const FTileWaterData& WaterData);
};

