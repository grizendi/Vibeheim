#pragma once

#include "CoreMinimal.h"
#include "Services/PCGWorldServiceTypes.h"
#include "UObject/Object.h"
#include "WorldGenExternalDataProvider.generated.h"

class UBiomeService;
class UClimateSystem;
class UHeightfieldService;
class UWorldGenTerrainResource;
class UPCGParamData;

/**
 * Provides worldgen data to PCG graphs via parameter data and lightweight queries.
 * Handles prebaked terrain resources and runtime services for deterministic outputs.
 */
UCLASS(BlueprintType)
class VIBEHEIM_API UWorldGenExternalDataProvider : public UObject
{
        GENERATED_BODY()

public:
        UWorldGenExternalDataProvider();

        /** Initialize the provider with worldgen services and config state. */
        void Initialize(const FWorldGenConfig& InConfig,
                        UHeightfieldService* InHeightfieldService,
                        UBiomeService* InBiomeService,
                        UClimateSystem* InClimateSystem,
                        UWorldGenTerrainResource* InTerrainResource = nullptr);

        /** Build PCG parameter data for a partition cell using deterministic aggregates. */
        UFUNCTION(BlueprintCallable, Category = "WorldGen|PCG")
        UPCGParamData* CreateParamDataForCell(const FIntPoint& Cell) const;

        /** Query terrain height using prebaked data when available, falling back to runtime services. */
        UFUNCTION(BlueprintCallable, Category = "WorldGen|PCG")
        float GetTerrainHeight(const FVector& WorldPosition) const;

        /** Query biome using runtime services or prebaked cache. */
        UFUNCTION(BlueprintCallable, Category = "WorldGen|PCG")
        FBiomeResult GetBiomeData(const FVector& WorldPosition) const;

        /** Query climate data using configured climate system (temperature/moisture/ring bias). */
        UFUNCTION(BlueprintCallable, Category = "WorldGen|PCG")
        FClimateData GetClimateData(const FVector& WorldPosition) const;

        /** Optional prebaked terrain resource binding for baked EditorBuildOnce worlds. */
        void SetTerrainResource(UWorldGenTerrainResource* InTerrainResource);

private:
        bool GatherHeightSamples(const FTileCoord& TileCoord,
                                 TArray<float>& OutHeights) const;
        FPCGTileMetrics AnalyzeHeightSamples(const TArray<float>& Heights) const;
        FBiomeResult ResolveBiome(const FVector2D& TileCenter, float Altitude,
                                  const FTileCoord& TileCoord) const;
        FClimateData ResolveClimate(const FVector2D& TileCenter,
                                    float Altitude) const;
        bool BuildCellData(const FIntPoint& Cell, FTileCoord& OutTileCoord,
                           FPCGTileMetrics& OutMetrics, FBiomeResult& OutBiome,
                           FClimateData& OutClimate, int32& OutTileSeed) const;
        uint32 BuildTileSeed(const FTileCoord& TileCoord) const;
        FVector2D ComputeTileCenter(const FTileCoord& TileCoord) const;
        int32 ResolveGridSize() const;

private:
        FWorldGenConfig CachedConfig;

        UPROPERTY()
        UHeightfieldService* HeightfieldService;

        UPROPERTY()
        UBiomeService* BiomeService;

        UPROPERTY()
        UClimateSystem* ClimateSystem;

        UPROPERTY()
        UWorldGenTerrainResource* TerrainResource;

#if WITH_AUTOMATION_TESTS
        friend struct FWorldGenExternalDataProviderTestAccessor;
#endif
};

#if WITH_AUTOMATION_TESTS
struct FWorldGenExternalDataProviderTestAccessor
{
        static bool BuildCellData(const UWorldGenExternalDataProvider* Provider, const FIntPoint& Cell,
                                  FTileCoord& OutTile, FPCGTileMetrics& OutMetrics, FBiomeResult& OutBiome,
                                  FClimateData& OutClimate, int32& OutTileSeed)
        {
                return Provider ? Provider->BuildCellData(Cell, OutTile, OutMetrics, OutBiome, OutClimate,
                                                          OutTileSeed)
                                : false;
        }
};
#endif // WITH_AUTOMATION_TESTS
