#include "Services/RiverFlowService.h"

#include "Algo/Sort.h"
#include "Services/IHeightfieldService.h"

DEFINE_LOG_CATEGORY_STATIC(LogRiverFlowService, Log, All);

namespace
{
    constexpr float KINDA_SMALL_METERS = 1.e-4f;
}

URiverFlowService::URiverFlowService()
{
    WorldGenSettings = FWorldGenConfig();
    Config = FRiverSystemConfig();
}

bool URiverFlowService::Initialize(const FWorldGenConfig& InWorldGen, const FRiverSystemConfig& InConfig)
{
    WorldGenSettings = InWorldGen;
    Config = InConfig;
    CachedFlowData.Empty();

    UE_LOG(LogRiverFlowService, Log, TEXT("RiverFlowService initialized (Enabled=%s, FlowResolution=%d)"),
        WorldGenSettings.bEnableRivers ? TEXT("true") : TEXT("false"),
        Config.FlowMapResolution);

    return true;
}

void URiverFlowService::SetConfig(const FRiverSystemConfig& InConfig)
{
    Config = InConfig;
    CachedFlowData.Empty();
}

bool URiverFlowService::ComputeFlowMap(const FTileCoord& TileCoord, const FHeightfieldData& Heightfield, FRiverFlowTileData& OutFlowData)
{
    if (!ShouldComputeFlow())
    {
        CachedFlowData.Remove(TileCoord);
        OutFlowData = FRiverFlowTileData();
        return true;
    }

    if (const FRiverFlowTileData* Existing = CachedFlowData.Find(TileCoord))
    {
        OutFlowData = *Existing;
        return true;
    }

    return BuildFlowData(TileCoord, Heightfield, OutFlowData);
}

void URiverFlowService::OnTileDeactivated(const FTileCoord& TileCoord)
{
    CachedFlowData.Remove(TileCoord);
}

const FRiverFlowTileData* URiverFlowService::GetCachedFlowData(const FTileCoord& TileCoord) const
{
    return CachedFlowData.Find(TileCoord);
}

bool URiverFlowService::ShouldComputeFlow() const
{
    return WorldGenSettings.bEnableRivers;
}

bool URiverFlowService::BuildFlowData(const FTileCoord& TileCoord, const FHeightfieldData& Heightfield, FRiverFlowTileData& OutFlowData)
{
    const int32 FlowResolution = FMath::Max(2, Config.FlowMapResolution);
    if (Config.FlowMapResolution <= 1)
    {
        UE_LOG(LogRiverFlowService, Verbose, TEXT("FlowMapResolution %d is below minimum; falling back to %d samples"), Config.FlowMapResolution, FlowResolution);
    }
    TArray<float> DownsampledHeights;
    if (!DownsampleHeightfield(Heightfield, FlowResolution, DownsampledHeights))
    {
        UE_LOG(LogRiverFlowService, Warning, TEXT("Failed to downsample heightfield for tile (%d,%d)"), TileCoord.X, TileCoord.Y);
        return false;
    }

    FRiverFlowTileData FlowData;
    FlowData.Resolution = FlowResolution;
    FlowData.CellSizeMeters = WorldGenSettings.TileSizeMeters / static_cast<float>(FlowResolution);
    FlowData.AccumulationThreshold = Config.FlowAccumulationThreshold;

    const int32 SampleCount = FlowResolution * FlowResolution;
    FlowData.FlowDirections.SetNum(SampleCount);
    for (FVector2D& Dir : FlowData.FlowDirections)
    {
        Dir = FVector2D::ZeroVector;
    }

    FlowData.FlowAccumulation.Init(1.0f, SampleCount);
    FlowData.DownstreamIndices.SetNum(SampleCount);
    for (int32& Downstream : FlowData.DownstreamIndices)
    {
        Downstream = INDEX_NONE;
    }

    ComputeFlowDirections(DownsampledHeights, FlowData);
    ComputeFlowAccumulation(DownsampledHeights, FlowData);

    FRiverFlowTileData& Cached = CachedFlowData.FindOrAdd(TileCoord);
    Cached = MoveTemp(FlowData);
    OutFlowData = Cached;
    return true;
}

bool URiverFlowService::DownsampleHeightfield(const FHeightfieldData& Heightfield, int32 FlowResolution, TArray<float>& OutHeights) const
{
    const int32 HeightResolution = Heightfield.Resolution;
    if (HeightResolution <= 0 || Heightfield.HeightData.Num() != HeightResolution * HeightResolution || FlowResolution <= 1)
    {
        return false;
    }

    const int32 SampleCount = FlowResolution * FlowResolution;
    OutHeights.SetNum(SampleCount);

    for (int32 y = 0; y < FlowResolution; ++y)
    {
        const float NormalizedY = (static_cast<float>(y) + 0.5f) / static_cast<float>(FlowResolution);
        for (int32 x = 0; x < FlowResolution; ++x)
        {
            const float NormalizedX = (static_cast<float>(x) + 0.5f) / static_cast<float>(FlowResolution);
            const int32 Index = y * FlowResolution + x;
            OutHeights[Index] = SampleHeightBilinear(Heightfield, NormalizedX, NormalizedY);
        }
    }

    return true;
}

float URiverFlowService::SampleHeightBilinear(const FHeightfieldData& Heightfield, float NormalizedX, float NormalizedY) const
{
    const int32 Res = Heightfield.Resolution;
    if (Res <= 0 || Heightfield.HeightData.Num() != Res * Res)
    {
        return 0.0f;
    }

    const float ScaledX = FMath::Clamp(NormalizedX, 0.0f, 0.9999f) * static_cast<float>(Res - 1);
    const float ScaledY = FMath::Clamp(NormalizedY, 0.0f, 0.9999f) * static_cast<float>(Res - 1);

    const int32 X0 = FMath::Clamp(FMath::FloorToInt(ScaledX), 0, Res - 1);
    const int32 Y0 = FMath::Clamp(FMath::FloorToInt(ScaledY), 0, Res - 1);
    const int32 X1 = FMath::Clamp(X0 + 1, 0, Res - 1);
    const int32 Y1 = FMath::Clamp(Y0 + 1, 0, Res - 1);

    const float Fx = ScaledX - static_cast<float>(X0);
    const float Fy = ScaledY - static_cast<float>(Y0);

    const int32 Index00 = Y0 * Res + X0;
    const int32 Index10 = Y0 * Res + X1;
    const int32 Index01 = Y1 * Res + X0;
    const int32 Index11 = Y1 * Res + X1;

    const float H00 = Heightfield.HeightData.IsValidIndex(Index00) ? Heightfield.HeightData[Index00] : 0.0f;
    const float H10 = Heightfield.HeightData.IsValidIndex(Index10) ? Heightfield.HeightData[Index10] : H00;
    const float H01 = Heightfield.HeightData.IsValidIndex(Index01) ? Heightfield.HeightData[Index01] : H00;
    const float H11 = Heightfield.HeightData.IsValidIndex(Index11) ? Heightfield.HeightData[Index11] : H01;

    const float HX0 = FMath::Lerp(H00, H10, Fx);
    const float HX1 = FMath::Lerp(H01, H11, Fx);
    return FMath::Lerp(HX0, HX1, Fy);
}

void URiverFlowService::ComputeFlowDirections(const TArray<float>& Heights, FRiverFlowTileData& InOutFlowData) const
{
    const int32 Res = InOutFlowData.Resolution;
    if (Res <= 1)
    {
        return;
    }

    const float CellSize = FMath::Max(InOutFlowData.CellSizeMeters, KINDA_SMALL_METERS);

    auto SampleHeight = [&Heights, Res](int32 X, int32 Y)
    {
        X = FMath::Clamp(X, 0, Res - 1);
        Y = FMath::Clamp(Y, 0, Res - 1);
        return Heights[Y * Res + X];
    };

    for (int32 Y = 0; Y < Res; ++Y)
    {
        for (int32 X = 0; X < Res; ++X)
        {
            const float HLeft = SampleHeight(X - 1, Y);
            const float HRight = SampleHeight(X + 1, Y);
            const float HDown = SampleHeight(X, Y - 1);
            const float HUp = SampleHeight(X, Y + 1);

            const float Ddx = (HRight - HLeft) / (2.0f * CellSize);
            const float Ddy = (HUp - HDown) / (2.0f * CellSize);

            const FVector2D Gradient(Ddx, Ddy);
            FVector2D FlowDir = Gradient.IsNearlyZero() ? FVector2D::ZeroVector : (-Gradient).GetSafeNormal();

            const int32 Index = Y * Res + X;
            InOutFlowData.FlowDirections[Index] = FlowDir;
            InOutFlowData.DownstreamIndices[Index] = ResolveDownstreamIndex(X, Y, Res, Heights);
        }
    }
}

int32 URiverFlowService::ResolveDownstreamIndex(int32 X, int32 Y, int32 Resolution, const TArray<float>& Heights) const
{
    const int32 CurrentIndex = Y * Resolution + X;
    if (!Heights.IsValidIndex(CurrentIndex))
    {
        return INDEX_NONE;
    }

    const float CurrentHeight = Heights[CurrentIndex];
    float BestSlope = 0.0f;
    int32 BestIndex = INDEX_NONE;

    for (int32 OffsetY = -1; OffsetY <= 1; ++OffsetY)
    {
        for (int32 OffsetX = -1; OffsetX <= 1; ++OffsetX)
        {
            if (OffsetX == 0 && OffsetY == 0)
            {
                continue;
            }

            int32 NeighX = FMath::Clamp(X + OffsetX, 0, Resolution - 1);
            int32 NeighY = FMath::Clamp(Y + OffsetY, 0, Resolution - 1);

            if (NeighX == X && NeighY == Y)
            {
                continue;
            }

            const int32 NeighborIndex = NeighY * Resolution + NeighX;
            if (!Heights.IsValidIndex(NeighborIndex))
            {
                continue;
            }

            const float NeighborHeight = Heights[NeighborIndex];
            const float Drop = CurrentHeight - NeighborHeight;
            if (Drop <= 0.0f)
            {
                continue;
            }

            const float Distance = (OffsetX == 0 || OffsetY == 0) ? 1.0f : FMath::Sqrt(2.0f);
            const float Slope = Drop / Distance;

            if (Slope > BestSlope)
            {
                BestSlope = Slope;
                BestIndex = NeighborIndex;
            }
        }
    }

    return BestIndex;
}

void URiverFlowService::ComputeFlowAccumulation(const TArray<float>& Heights, FRiverFlowTileData& InOutFlowData) const
{
    const int32 SampleCount = Heights.Num();
    if (SampleCount == 0)
    {
        InOutFlowData.MinAccumulation = 0.0f;
        InOutFlowData.MaxAccumulation = 0.0f;
        return;
    }

    TArray<int32> SortedIndices;
    SortedIndices.SetNum(SampleCount);
    for (int32 Index = 0; Index < SampleCount; ++Index)
    {
        SortedIndices[Index] = Index;
    }

    SortedIndices.Sort([&Heights](int32 A, int32 B)
    {
        return Heights[A] > Heights[B];
    });

    for (int32 Index : SortedIndices)
    {
        const int32 Downstream = InOutFlowData.DownstreamIndices.IsValidIndex(Index) ? InOutFlowData.DownstreamIndices[Index] : INDEX_NONE;
        if (Downstream != INDEX_NONE && InOutFlowData.FlowAccumulation.IsValidIndex(Downstream))
        {
            InOutFlowData.FlowAccumulation[Downstream] += InOutFlowData.FlowAccumulation[Index];
        }
    }

    float MinAccum = TNumericLimits<float>::Max();
    float MaxAccum = 0.0f;
    for (float AccumValue : InOutFlowData.FlowAccumulation)
    {
        MinAccum = FMath::Min(MinAccum, AccumValue);
        MaxAccum = FMath::Max(MaxAccum, AccumValue);
    }

    if (MinAccum == TNumericLimits<float>::Max())
    {
        MinAccum = 0.0f;
    }

    InOutFlowData.MinAccumulation = MinAccum;
    InOutFlowData.MaxAccumulation = MaxAccum;
}
