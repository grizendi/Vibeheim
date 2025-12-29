#include "WorldGenExternalDataProvider.h"

#include "Data/WorldGenTerrainResource.h"
#include "Data/WorldGenTypes.h"
#include "Services/BiomeService.h"
#include "Services/ClimateSystem.h"
#include "Services/HeightfieldService.h"

#if VHM_PCG_ENABLED
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "PCGData.h"
#include "PCGParamData.h"
#endif // VHM_PCG_ENABLED

DEFINE_LOG_CATEGORY_STATIC(LogWorldGenExternalDataProvider, Log, All);

namespace
{
constexpr int32 kMaxGridSize = 4096;

float ResolveWeight(const FBiomeResult& BiomeResult)
{
        if (BiomeResult.PrimaryBiome == EBiomeType::None)
        {
                return 0.0f;
        }

        if (const float* Weight = BiomeResult.BiomeWeights.Find(BiomeResult.PrimaryBiome))
        {
                return *Weight;
        }

        return 1.0f;
}
} // namespace

UWorldGenExternalDataProvider::UWorldGenExternalDataProvider()
{
        HeightfieldService = nullptr;
        BiomeService = nullptr;
        ClimateSystem = nullptr;
        TerrainResource = nullptr;
        CachedConfig = FWorldGenConfig();
}

void UWorldGenExternalDataProvider::Initialize(
    const FWorldGenConfig& InConfig,
    UHeightfieldService* InHeightfieldService,
    UBiomeService* InBiomeService,
    UClimateSystem* InClimateSystem,
    UWorldGenTerrainResource* InTerrainResource)
{
        CachedConfig = InConfig;
        HeightfieldService = InHeightfieldService;
        BiomeService = InBiomeService;
        ClimateSystem = InClimateSystem;
        TerrainResource = InTerrainResource;
}

void UWorldGenExternalDataProvider::SetTerrainResource(UWorldGenTerrainResource* InTerrainResource)
{
        TerrainResource = InTerrainResource;
}

int32 UWorldGenExternalDataProvider::ResolveGridSize() const
{
        const float SamplesPerSide =
            CachedConfig.TileSizeMeters / FMath::Max(KINDA_SMALL_NUMBER, CachedConfig.SampleSpacingMeters);
        return FMath::Clamp(FMath::RoundToInt(SamplesPerSide), 1, kMaxGridSize);
}

FVector2D UWorldGenExternalDataProvider::ComputeTileCenter(const FTileCoord& TileCoord) const
{
        const float TileSize = CachedConfig.TileSizeMeters;
        const FVector2D LocalCenter((static_cast<float>(TileCoord.X) + 0.5f) * TileSize,
                                    (static_cast<float>(TileCoord.Y) + 0.5f) * TileSize);
        if (TerrainResource)
        {
                return TerrainResource->WorldOrigin + LocalCenter;
        }

        return LocalCenter;
}

uint32 UWorldGenExternalDataProvider::BuildTileSeed(const FTileCoord& TileCoord) const
{
        return HashCombine(HashCombine(GetTypeHash(TileCoord.X), GetTypeHash(TileCoord.Y)),
                           GetTypeHash(CachedConfig.Seed));
}

float UWorldGenExternalDataProvider::GetTerrainHeight(const FVector& WorldPosition) const
{
        const FVector2D WorldXY(WorldPosition.X, WorldPosition.Y);

        if (TerrainResource)
        {
                const float Sampled = TerrainResource->GetHeightAtWorldPosition(WorldPosition);
                if (FMath::IsFinite(Sampled))
                {
                        return Sampled;
                }
        }

        if (HeightfieldService)
        {
                const float Sampled = HeightfieldService->SampleHeightWorldXY(WorldXY);
                if (FMath::IsFinite(Sampled))
                {
                        return Sampled;
                }
        }

        UE_LOG(LogWorldGenExternalDataProvider, Warning,
               TEXT("GetTerrainHeight: Missing data for world position (%s); using SeaLevel %.2f"),
               *WorldPosition.ToString(), CachedConfig.SeaLevel);
        return CachedConfig.SeaLevel;
}

FBiomeResult UWorldGenExternalDataProvider::GetBiomeData(const FVector& WorldPosition) const
{
        const FVector2D WorldXY(WorldPosition.X, WorldPosition.Y);
        const float Altitude = FMath::IsFinite(WorldPosition.Z)
                                   ? WorldPosition.Z
                                   : GetTerrainHeight(WorldPosition);

        if (BiomeService)
        {
                return BiomeService->DetermineBiome(WorldXY, Altitude);
        }

        if (TerrainResource)
        {
                return TerrainResource->GetBiomeAtWorldPosition(WorldPosition);
        }

        UE_LOG(LogWorldGenExternalDataProvider, Warning,
               TEXT("GetBiomeData: No biome sources available for (%s)"),
               *WorldPosition.ToString());
        return FBiomeResult();
}

FClimateData UWorldGenExternalDataProvider::GetClimateData(const FVector& WorldPosition) const
{
        const FVector2D WorldXY(WorldPosition.X, WorldPosition.Y);
        const float Altitude = FMath::IsFinite(WorldPosition.Z)
                                   ? WorldPosition.Z
                                   : GetTerrainHeight(WorldPosition);

        if (ClimateSystem)
        {
                return ClimateSystem->CalculateClimate(WorldXY, Altitude);
        }

        UE_LOG(LogWorldGenExternalDataProvider, Warning,
               TEXT("GetClimateData: ClimateSystem unavailable; returning defaults for (%s)"),
               *WorldPosition.ToString());
        return FClimateData();
}

bool UWorldGenExternalDataProvider::GatherHeightSamples(const FTileCoord& TileCoord,
                                                        TArray<float>& OutHeights) const
{
        OutHeights.Reset();

        const int32 GridSize = ResolveGridSize();
        const int32 ExpectedSamples = GridSize * GridSize;

        if (HeightfieldService)
        {
                FHeightfieldData Heightfield;
                if (!HeightfieldService->GetCachedHeightfield(TileCoord, Heightfield))
                {
                        Heightfield = HeightfieldService->GenerateHeightfield(CachedConfig.Seed, TileCoord);
                }

                if (Heightfield.HeightData.Num() == ExpectedSamples)
                {
                        OutHeights = Heightfield.HeightData;
                        return true;
                }
        }

        if (TerrainResource && TerrainResource->HasTileData(TileCoord))
        {
                const FVector2D TileOrigin =
                    TerrainResource->WorldOrigin +
                    FVector2D(TileCoord.X * CachedConfig.TileSizeMeters,
                              TileCoord.Y * CachedConfig.TileSizeMeters);
                const float SampleSpacing =
                    FMath::Max(CachedConfig.SampleSpacingMeters, KINDA_SMALL_NUMBER);

                OutHeights.Reserve(ExpectedSamples);
                for (int32 Y = 0; Y < GridSize; ++Y)
                {
                        for (int32 X = 0; X < GridSize; ++X)
                        {
                                const FVector SamplePosition(
                                    TileOrigin.X + (static_cast<float>(X) + 0.5f) * SampleSpacing,
                                    TileOrigin.Y + (static_cast<float>(Y) + 0.5f) * SampleSpacing, 0.0f);
                                OutHeights.Add(TerrainResource->GetHeightAtWorldPosition(SamplePosition));
                        }
                }

                if (OutHeights.Num() == ExpectedSamples)
                {
                        return true;
                }

                OutHeights.Reset();
        }

        return false;
}

FPCGTileMetrics UWorldGenExternalDataProvider::AnalyzeHeightSamples(
    const TArray<float>& Heights) const
{
        FPCGTileMetrics Metrics;
        const int32 GridSize = ResolveGridSize();
        const int32 ExpectedSize = GridSize * GridSize;
        if (Heights.Num() != ExpectedSize || GridSize <= 0)
        {
                return Metrics;
        }

        const float SeaLevel = CachedConfig.SeaLevel;
        float SumHeight = 0.0f;
        float SumSlope = 0.0f;
        float MaxSlope = 0.0f;
        float MinHeight = Heights[0];
        float MaxHeight = Heights[0];
        float WaterCoverageSamples = 0.0f;
        float AboveWaterSum = 0.0f;
        float BelowWaterSum = 0.0f;
        float MinAbsWaterDistance = TNumericLimits<float>::Max();

        auto SampleHeight = [&Heights, GridSize](int32 X, int32 Y) -> float {
                const int32 Index = Y * GridSize + X;
                return Heights.IsValidIndex(Index) ? Heights[Index] : 0.0f;
        };

        auto CalculateSlope = [&SampleHeight, GridSize](int32 X, int32 Y) -> float {
                const float CenterHeight = SampleHeight(X, Y);
                float LocalMaxSlope = 0.0f;

                for (int32 DX = -1; DX <= 1; ++DX)
                {
                        for (int32 DY = -1; DY <= 1; ++DY)
                        {
                                if (DX == 0 && DY == 0)
                                {
                                        continue;
                                }

                                const int32 NX = X + DX;
                                const int32 NY = Y + DY;
                                if (NX < 0 || NX >= GridSize || NY < 0 || NY >= GridSize)
                                {
                                        continue;
                                }

                                const float NeighborHeight = SampleHeight(NX, NY);
                                const float HeightDelta = FMath::Abs(NeighborHeight - CenterHeight);
                                const float Distance = FMath::Sqrt(static_cast<float>(DX * DX + DY * DY));
                                const float Slope = FMath::RadiansToDegrees(FMath::Atan2(HeightDelta, Distance));
                                LocalMaxSlope = FMath::Max(LocalMaxSlope, Slope);
                        }
                }

                return LocalMaxSlope;
        };

        for (int32 Y = 0; Y < GridSize; ++Y)
        {
                for (int32 X = 0; X < GridSize; ++X)
                {
                        const float Height = SampleHeight(X, Y);
                        SumHeight += Height;
                        MinHeight = FMath::Min(MinHeight, Height);
                        MaxHeight = FMath::Max(MaxHeight, Height);

                        const float Slope = CalculateSlope(X, Y);
                        SumSlope += Slope;
                        MaxSlope = FMath::Max(MaxSlope, Slope);

                        const float WaterDelta = Height - SeaLevel;
                        MinAbsWaterDistance = FMath::Min(MinAbsWaterDistance, FMath::Abs(WaterDelta));

                        if (WaterDelta >= 0.0f)
                        {
                                AboveWaterSum += WaterDelta;
                        }
                        else
                        {
                                BelowWaterSum += -WaterDelta;
                                WaterCoverageSamples += 1.0f;
                        }
                }
        }

        const float SampleCount = static_cast<float>(ExpectedSize);
        Metrics.AverageHeight = SumHeight / SampleCount;
        Metrics.MinHeight = MinHeight;
        Metrics.MaxHeight = MaxHeight;
        Metrics.AverageSlope = SumSlope / SampleCount;
        Metrics.MaxSlope = MaxSlope;
        Metrics.WaterCoverageRatio =
            SampleCount > 0.0f ? WaterCoverageSamples / SampleCount : 0.0f;

        const float AboveSamples = SampleCount - WaterCoverageSamples;
        Metrics.AverageAboveWater =
            AboveSamples > KINDA_SMALL_NUMBER ? AboveWaterSum / AboveSamples : 0.0f;
        Metrics.AverageBelowWater = WaterCoverageSamples > KINDA_SMALL_NUMBER
                                        ? BelowWaterSum / WaterCoverageSamples
                                        : 0.0f;
        Metrics.MinAbsWaterDistance =
            (MinAbsWaterDistance == TNumericLimits<float>::Max())
                ? 0.0f
                : MinAbsWaterDistance;

        return Metrics;
}

FBiomeResult UWorldGenExternalDataProvider::ResolveBiome(const FVector2D& TileCenter,
                                                         float Altitude,
                                                         const FTileCoord& TileCoord) const
{
        if (BiomeService)
        {
                return BiomeService->DetermineBiome(TileCenter, Altitude);
        }

        if (TerrainResource)
        {
                const FVector WorldLocation(TileCenter.X, TileCenter.Y, Altitude);
                return TerrainResource->GetBiomeAtWorldPosition(WorldLocation);
        }

        UE_LOG(LogWorldGenExternalDataProvider, Warning,
               TEXT("ResolveBiome: No biome source available for tile (%d,%d)"),
               TileCoord.X, TileCoord.Y);
        return FBiomeResult();
}

FClimateData UWorldGenExternalDataProvider::ResolveClimate(const FVector2D& TileCenter,
                                                           float Altitude) const
{
        if (ClimateSystem)
        {
                return ClimateSystem->CalculateClimate(TileCenter, Altitude);
        }

        return FClimateData();
}

bool UWorldGenExternalDataProvider::BuildCellData(
    const FIntPoint& Cell, FTileCoord& OutTileCoord, FPCGTileMetrics& OutMetrics,
    FBiomeResult& OutBiome, FClimateData& OutClimate, int32& OutTileSeed) const
{
        OutMetrics = FPCGTileMetrics();
        OutBiome = FBiomeResult();
        OutClimate = FClimateData();
        OutTileSeed = 0;

        OutTileCoord =
            FTileCoord::FromPCGGridCell(Cell, CachedConfig.TileSizeMeters, CachedConfig.TileSizeMeters);

        TArray<float> HeightSamples;
        if (!GatherHeightSamples(OutTileCoord, HeightSamples))
        {
                UE_LOG(LogWorldGenExternalDataProvider, Error,
                       TEXT("BuildCellData: Missing terrain samples for PCG cell (%d,%d) [Graph=ExternalDataProvider]."),
                       Cell.X, Cell.Y);
                return false;
        }

        OutMetrics = AnalyzeHeightSamples(HeightSamples);
        const FVector2D TileCenter = ComputeTileCenter(OutTileCoord);
        const float Altitude = OutMetrics.AverageHeight;
        OutBiome = ResolveBiome(TileCenter, Altitude, OutTileCoord);
        OutClimate = ResolveClimate(TileCenter, Altitude);
        OutTileSeed = static_cast<int32>(BuildTileSeed(OutTileCoord) & 0x7FFFFFFF);
        return true;
}

UPCGParamData* UWorldGenExternalDataProvider::CreateParamDataForCell(
    const FIntPoint& Cell) const
{
#if !VHM_PCG_ENABLED
        UE_LOG(LogWorldGenExternalDataProvider, Warning,
               TEXT("CreateParamDataForCell called but PCG is disabled; returning null."));
        return nullptr;
#else
        FTileCoord TileCoord;
        FPCGTileMetrics Metrics;
        FBiomeResult Biome;
        FClimateData Climate;
        int32 TileSeed = 0;
        if (!BuildCellData(Cell, TileCoord, Metrics, Biome, Climate, TileSeed))
        {
                return nullptr;
        }

        const float BiomeWeight = ResolveWeight(Biome);

        UObject* Outer = const_cast<UWorldGenExternalDataProvider*>(this);
        UPCGParamData* ParamData =
            NewObject<UPCGParamData>(Outer ? Outer : GetTransientPackage(), NAME_None, RF_Transient);
        if (!ParamData)
        {
                UE_LOG(LogWorldGenExternalDataProvider, Error,
                       TEXT("Failed to allocate PCG param data for cell (%d,%d)."),
                       Cell.X, Cell.Y);
                return nullptr;
        }

        UPCGMetadata* Metadata = ParamData->MutableMetadata();
        if (!Metadata)
        {
                UE_LOG(LogWorldGenExternalDataProvider, Error,
                       TEXT("Missing metadata when creating param data for cell (%d,%d)."),
                       Cell.X, Cell.Y);
                return ParamData;
        }

        const auto EntryKey = Metadata->AddEntry();
        auto EnsureAttribute = [Metadata, EntryKey](const FName& Name, auto&& Value,
                                                    const bool bAllowInterpolation) {
                using ValueType = typename TDecay<decltype(Value)>::Type;

                FPCGMetadataAttribute<ValueType>* Attribute =
                    Metadata->GetMutableTypedAttribute<ValueType>(Name);
                if (!Attribute)
                {
                        Attribute = Metadata->CreateAttribute<ValueType>(
                            Name, Value, bAllowInterpolation, true);
                        if (!Attribute)
                        {
                                UE_LOG(LogWorldGenExternalDataProvider, Error,
                                       TEXT("Failed to create metadata attribute '%s'"),
                                       *Name.ToString());
                                return;
                        }
                }

                Attribute->SetValue(EntryKey, Value);
        };

        EnsureAttribute(VHMPCGAttr::AverageHeight, Metrics.AverageHeight, true);
        EnsureAttribute(VHMPCGAttr::MinHeight, Metrics.MinHeight, true);
        EnsureAttribute(VHMPCGAttr::MaxHeight, Metrics.MaxHeight, true);
        EnsureAttribute(VHMPCGAttr::AverageSlope, Metrics.AverageSlope, true);
        EnsureAttribute(VHMPCGAttr::MaxSlope, Metrics.MaxSlope, true);
        EnsureAttribute(VHMPCGAttr::WaterCoverage, Metrics.WaterCoverageRatio, true);
        EnsureAttribute(VHMPCGAttr::AverageAboveWater, Metrics.AverageAboveWater, true);
        EnsureAttribute(VHMPCGAttr::AverageBelowWater, Metrics.AverageBelowWater, true);
        EnsureAttribute(VHMPCGAttr::MinWaterDistance, Metrics.MinAbsWaterDistance, true);
        EnsureAttribute(VHMPCGAttr::SeaLevel, CachedConfig.SeaLevel, true);
        EnsureAttribute(VHMPCGAttr::TileSize, CachedConfig.TileSizeMeters, true);
        EnsureAttribute(VHMPCGAttr::BiomeId, static_cast<int32>(Biome.PrimaryBiome), false);
        EnsureAttribute(VHMPCGAttr::BiomeWeight, BiomeWeight, true);
        EnsureAttribute(VHMPCGAttr::TileX, TileCoord.X, false);
        EnsureAttribute(VHMPCGAttr::TileY, TileCoord.Y, false);
        EnsureAttribute(VHMPCGAttr::TileSeed, TileSeed, false);
        EnsureAttribute(VHMPCGAttr::DensityScale, 1.0f, true);
        EnsureAttribute(VHMPCGAttr::ClimateTemperature, Climate.Temperature, true);
        EnsureAttribute(VHMPCGAttr::ClimateMoisture, Climate.Moisture, true);
        EnsureAttribute(VHMPCGAttr::ClimateRingBias, Climate.RingBias, true);
        const FWorldPartitionPCGDataLayers& DataLayers = CachedConfig.PCGDataLayers;
        EnsureAttribute(VHMPCGAttr::DataLayerTerrainClutter,
                        DataLayers.TerrainClutter, false);
        EnsureAttribute(VHMPCGAttr::DataLayerTrees, DataLayers.Trees, false);
        EnsureAttribute(VHMPCGAttr::DataLayerRocks, DataLayers.Rocks, false);
        EnsureAttribute(VHMPCGAttr::DataLayerPOIs, DataLayers.POIs, false);
        EnsureAttribute(VHMPCGAttr::DataLayerDynamic, DataLayers.Dynamic, false);

        return ParamData;
#endif // VHM_PCG_ENABLED
}
