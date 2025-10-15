#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/WorldGenTypes.h"
#include "RiverFlowService.generated.h"

struct FHeightfieldData;

/**
 * Service responsible for computing river flow networks from tile heightfields.
 * Produces a downsampled flow map with directions and accumulation values that
 * downstream systems (carving, placement, exports) can consume.
 */
UCLASS(BlueprintType)
class VIBEHEIM_API URiverFlowService : public UObject
{
    GENERATED_BODY()

public:
    URiverFlowService();

    /** Initialize with core world generation + river configuration */
    UFUNCTION(BlueprintCallable, Category = "Rivers")
    bool Initialize(const FWorldGenConfig& InWorldGen, const FRiverSystemConfig& InConfig);

    /** Update runtime configuration (clears cached flow data) */
    UFUNCTION(BlueprintCallable, Category = "Rivers")
    void SetConfig(const FRiverSystemConfig& InConfig);

    /** Compute or fetch cached flow map for the requested tile */
    bool ComputeFlowMap(const FTileCoord& TileCoord, const FHeightfieldData& Heightfield, FRiverFlowTileData& OutFlowData);

    /** Drop cached data for a tile when it leaves memory */
    void OnTileDeactivated(const FTileCoord& TileCoord);

    /** Access cached flow data (nullptr when not computed) */
    const FRiverFlowTileData* GetCachedFlowData(const FTileCoord& TileCoord) const;

private:
    bool ShouldComputeFlow() const;
    bool BuildFlowData(const FTileCoord& TileCoord, const FHeightfieldData& Heightfield, FRiverFlowTileData& OutFlowData);
    bool DownsampleHeightfield(const FHeightfieldData& Heightfield, int32 FlowResolution, TArray<float>& OutHeights) const;
    float SampleHeightBilinear(const FHeightfieldData& Heightfield, float NormalizedX, float NormalizedY) const;
    void ComputeFlowDirections(const TArray<float>& Heights, FRiverFlowTileData& InOutFlowData) const;
    int32 ResolveDownstreamIndex(int32 X, int32 Y, int32 Resolution, const TArray<float>& Heights) const;
    void ComputeFlowAccumulation(const TArray<float>& Heights, FRiverFlowTileData& InOutFlowData) const;

private:
    UPROPERTY()
    FWorldGenConfig WorldGenSettings;

    UPROPERTY()
    FRiverSystemConfig Config;

    UPROPERTY()
    TMap<FTileCoord, FRiverFlowTileData> CachedFlowData;
};
