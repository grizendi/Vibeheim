#include "Services/WaterSystemService.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY_STATIC(LogWaterSystemService, Log, All);

UWaterSystemService::UWaterSystemService()
{
    WorldGenSettings = FWorldGenConfig();
    Config = FWaterSystemConfig();
}

bool UWaterSystemService::Initialize(const FWorldGenConfig& InWorldGen, const FWaterSystemConfig& InConfig)
{
    WorldGenSettings = InWorldGen;
    Config = InConfig;
    UE_LOG(LogWaterSystemService, Log, TEXT("WaterSystemService initialized (Enabled=%s, SeaLevel=%.2f)"),
        (WorldGenSettings.bEnableWater && Config.bEnableWaterSystem) ? TEXT("true") : TEXT("false"),
        WorldGenSettings.SeaLevel);
    return true;
}

void UWaterSystemService::SetConfig(const FWaterSystemConfig& InConfig)
{
    Config = InConfig;
}

void UWaterSystemService::OnTileActivated(const FTileCoord& TileCoord, const FHeightfieldData& Heightfield, FTileWaterData& OutWaterData)
{
    if (!WorldGenSettings.bEnableWater || !Config.bEnableWaterSystem)
    {
        OutWaterData = FTileWaterData();
        return;
    }

    if (!ComputeTileWaterData(Heightfield, OutWaterData))
    {
        UE_LOG(LogWaterSystemService, Warning, TEXT("Failed to compute water data for tile (%d,%d)"), TileCoord.X, TileCoord.Y);
        return;
    }

    SpawnOrUpdateWaterActor(TileCoord, OutWaterData);
}

void UWaterSystemService::OnTileDeactivated(const FTileCoord& TileCoord)
{
    if (TWeakObjectPtr<AActor>* Found = ActiveWaterActors.Find(TileCoord))
    {
        if (Found->IsValid())
        {
            Found->Get()->Destroy();
        }
        ActiveWaterActors.Remove(TileCoord);
    }
}

bool UWaterSystemService::ComputeTileWaterData(const FHeightfieldData& HF, FTileWaterData& OutData) const
{
    const int32 Res = HF.Resolution;
    if (Res <= 0 || HF.HeightData.Num() != Res * Res)
    {
        return false;
    }

    OutData.Resolution = Res;
    OutData.WaterMask.SetNum(Res * Res);
    OutData.ShorelineMask.SetNum(Res * Res);
    OutData.DistanceToWater.SetNum(Res * Res);

    const float SeaLevel = WorldGenSettings.SeaLevel;
    const float SampleSpacing = (Res > 1) ? (WorldGenSettings.TileSizeMeters / static_cast<float>(Res - 1)) : 1.0f;

    // Classify water/land
    for (int32 y = 0; y < Res; ++y)
    {
        for (int32 x = 0; x < Res; ++x)
        {
            const int32 idx = y * Res + x;
            const float h = HF.HeightData[idx];
            OutData.WaterMask[idx] = (h < SeaLevel) ? 1 : 0;
            OutData.ShorelineMask[idx] = 0;
            OutData.DistanceToWater[idx] = TNumericLimits<float>::Max();
        }
    }

    // Detect shoreline edges (4-neighborhood)
    auto IsWater = [&OutData, Res](int32 X, int32 Y) -> bool
    {
        if (X < 0 || Y < 0 || X >= Res || Y >= Res) return false;
        return OutData.WaterMask[Y * Res + X] != 0;
    };

    for (int32 y = 0; y < Res; ++y)
    {
        for (int32 x = 0; x < Res; ++x)
        {
            const int32 idx = y * Res + x;
            const bool w = OutData.WaterMask[idx] != 0;
            bool edge = false;
            // Check 4-neighbors
            edge |= (IsWater(x - 1, y) != w);
            edge |= (IsWater(x + 1, y) != w);
            edge |= (IsWater(x, y - 1) != w);
            edge |= (IsWater(x, y + 1) != w);
            OutData.ShorelineMask[idx] = edge ? 1 : 0;
        }
    }

    // Multi-source BFS for distance to nearest water (in cells)
    TArray<int32> Queue;
    Queue.Reserve(Res * Res);

    TArray<int32> DistCells;
    DistCells.Init(INT32_MAX, Res * Res);

    for (int32 i = 0; i < Res * Res; ++i)
    {
        if (OutData.WaterMask[i] != 0)
        {
            DistCells[i] = 0;
            Queue.Add(i);
        }
    }

    int32 head = 0;
    auto Enqueue = [&Queue, &DistCells](int32 idx, int32 newDist)
    {
        if (DistCells[idx] > newDist)
        {
            DistCells[idx] = newDist;
            Queue.Add(idx);
        }
    };

    while (head < Queue.Num())
    {
        const int32 idx = Queue[head++];
        const int32 x = idx % Res;
        const int32 y = idx / Res;
        const int32 d = DistCells[idx];

        if (x > 0) Enqueue(idx - 1, d + 1);
        if (x + 1 < Res) Enqueue(idx + 1, d + 1);
        if (y > 0) Enqueue(idx - Res, d + 1);
        if (y + 1 < Res) Enqueue(idx + Res, d + 1);
    }

    // Convert to meters and store
    for (int32 i = 0; i < Res * Res; ++i)
    {
        const int32 dc = DistCells[i];
        OutData.DistanceToWater[i] = (dc == INT32_MAX) ? (WorldGenSettings.TileSizeMeters) : (static_cast<float>(dc) * SampleSpacing);
    }

    return true;
}

void UWaterSystemService::SpawnOrUpdateWaterActor(const FTileCoord& TileCoord, const FTileWaterData& WaterData)
{
    // Compute simple coverage to decide whether to spawn a placeholder actor
    int32 WaterCount = 0;
    for (uint8 v : WaterData.WaterMask) { WaterCount += (v != 0); }
    const int32 Total = WaterData.WaterMask.Num();
    const float Coverage = (Total > 0) ? (static_cast<float>(WaterCount) / static_cast<float>(Total)) : 0.0f;
    if (Coverage <= 0.0f)
    {
        // No water, ensure cleanup
        OnTileDeactivated(TileCoord);
        return;
    }

    // Spawn minimal placeholder actor at tile center if not present
    if (!ActiveWaterActors.Contains(TileCoord) || !ActiveWaterActors[TileCoord].IsValid())
    {
        UWorld* World = GetWorld();
        if (!World)
        {
            return;
        }
        const FVector WorldPosMeters = TileCoord.ToWorldPosition(WorldGenSettings);
        // Convert meters to centimeters for UE world
        const FVector Location = WorldPosMeters * 100.0f;
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AActor* WaterActor = World->SpawnActor<AActor>(AActor::StaticClass(), Location, FRotator::ZeroRotator, Params);
        if (WaterActor)
        {
            WaterActor->SetActorLabel(FString::Printf(TEXT("VHM_WaterTile_%d_%d"), TileCoord.X, TileCoord.Y));
            WaterActor->Tags.Add(FName(TEXT("VHM_Water")));
            ActiveWaterActors.Add(TileCoord, WaterActor);
        }
    }
}

