#include "VHMTerrainRendering/TerrainLODManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogTerrainLOD, Log, All);

// FTileLODData implementation
FTileLODData::FTileLODData(const FTileCoord& TileCoord, float TileSize)
{
    FVector TileCenter = TileCoord.ToWorldPosition(TileSize);
    FVector HalfExtent(TileSize * 0.5f, TileSize * 0.5f, 1000.0f); // Assume max height for bounds
    WorldBounds = FBox(TileCenter - HalfExtent, TileCenter + HalfExtent);
}

// UVHMTerrainLODManager implementation
UVHMTerrainLODManager::UVHMTerrainLODManager()
{
    MaxLODLevels = 4;
    MaxViewDistance = 2000.0f;
    TileSize = 64.0f;
    QualityScale = 1.0f;
    MinQualityScale = 0.5f;
    MaxQualityScale = 1.5f;
    TargetFrameTime = 16.67f; // 60 FPS
    PerformanceToleranceMs = 2.0f;
    
    InitializeLODDistances();
}

void UVHMTerrainLODManager::Initialize(const FVHMSettings& Settings)
{
    MaxLODLevels = Settings.LODLevels;
    MaxViewDistance = Settings.MaxViewDistance;
    TileSize = 64.0f; // Fixed tile size from world gen config
    
    InitializeLODDistances();
    
    UE_LOG(LogTerrainLOD, Log, TEXT("TerrainLODManager initialized: MaxLOD=%d, MaxDistance=%.1f, TileSize=%.1f"), 
           MaxLODLevels, MaxViewDistance, TileSize);
}

void UVHMTerrainLODManager::InitializeLODDistances()
{
    LODDistances.Empty();
    LODDistances.Reserve(MaxLODLevels);
    
    // Create exponential distance falloff for LOD levels
    // LOD 0: 0-200m (highest quality)
    // LOD 1: 200-500m
    // LOD 2: 500-1000m  
    // LOD 3: 1000-2000m (lowest quality)
    for (int32 LODLevel = 0; LODLevel < MaxLODLevels; ++LODLevel)
    {
        float NormalizedLevel = static_cast<float>(LODLevel) / static_cast<float>(MaxLODLevels - 1);
        float Distance = FMath::Pow(NormalizedLevel, 1.5f) * MaxViewDistance;
        LODDistances.Add(Distance);
    }
    
    ValidateLODDistances();
}

void UVHMTerrainLODManager::ValidateLODDistances()
{
    // Ensure distances are monotonically increasing
    for (int32 i = 1; i < LODDistances.Num(); ++i)
    {
        if (LODDistances[i] <= LODDistances[i-1])
        {
            LODDistances[i] = LODDistances[i-1] + 50.0f; // Minimum 50m between LOD levels
        }
    }
}

int32 UVHMTerrainLODManager::CalculateLODLevel(const FTileCoord& TileCoord, const FVector& ViewerPosition)
{
    float Distance = CalculateDistanceToTile(TileCoord, ViewerPosition);
    return DistanceToLODLevel(Distance);
}

void UVHMTerrainLODManager::UpdateLODLevels(const FVector& ViewerPosition)
{
    CurrentFrame++;
    LODTransitionsThisFrame = 0;
    
    // Update LOD data for all registered tiles
    for (auto& TilePair : TileLODMap)
    {
        const FTileCoord& TileCoord = TilePair.Key;
        FTileLODData& LODData = TilePair.Value;
        
        UpdateTileLODData(TileCoord, ViewerPosition);
    }
    
    // Cull distant tiles
    CullDistantTiles(ViewerPosition);
    
    UE_LOG(LogTerrainLOD, VeryVerbose, TEXT("Updated LOD levels for %d tiles, %d transitions this frame"), 
           TileLODMap.Num(), LODTransitionsThisFrame);
}

bool UVHMTerrainLODManager::IsTileVisible(const FTileCoord& TileCoord, const FVector& ViewerPosition)
{
    float Distance = CalculateDistanceToTile(TileCoord, ViewerPosition);
    bool bVisible = IsWithinViewDistance(Distance);
    
    // Update visibility in tile data if it exists
    if (FTileLODData* LODData = TileLODMap.Find(TileCoord))
    {
        LODData->bIsVisible = bVisible;
        LODData->DistanceToViewer = Distance;
    }
    
    return bVisible;
}

void UVHMTerrainLODManager::OptimizeMeshDetail(float InTargetFrameTime)
{
    this->TargetFrameTime = InTargetFrameTime;
    
    // Get current frame time from engine
    float CurrentFrameTime = FApp::GetDeltaTime() * 1000.0f; // Convert to milliseconds
    
    AdaptLODDistances(CurrentFrameTime, InTargetFrameTime);
    
    UE_LOG(LogTerrainLOD, VeryVerbose, TEXT("Optimizing mesh detail: Current=%.2fms, Target=%.2fms, QualityScale=%.2f"), 
           CurrentFrameTime, InTargetFrameTime, QualityScale);
}

int32 UVHMTerrainLODManager::GetCurrentLODLevel(const FTileCoord& TileCoord)
{
    if (const FTileLODData* LODData = TileLODMap.Find(TileCoord))
    {
        return LODData->CurrentLODLevel;
    }
    return -1; // Tile not managed
}

void UVHMTerrainLODManager::SetMaxLODLevels(int32 InMaxLODLevels)
{
    this->MaxLODLevels = FMath::Clamp(InMaxLODLevels, 1, 8);
    InitializeLODDistances();
    
    UE_LOG(LogTerrainLOD, Log, TEXT("Set MaxLODLevels to %d"), this->MaxLODLevels);
}

int32 UVHMTerrainLODManager::GetLODTransitionsThisFrame() const
{
    return LODTransitionsThisFrame;
}

void UVHMTerrainLODManager::ResetFrameStats()
{
    LODTransitionsThisFrame = 0;
}

void UVHMTerrainLODManager::RegisterTile(const FTileCoord& TileCoord)
{
    if (!TileLODMap.Contains(TileCoord))
    {
        FTileLODData LODData(TileCoord, TileSize);
        TileLODMap.Add(TileCoord, LODData);
        
        UE_LOG(LogTerrainLOD, VeryVerbose, TEXT("Registered tile (%d, %d)"), TileCoord.X, TileCoord.Y);
    }
}

void UVHMTerrainLODManager::UnregisterTile(const FTileCoord& TileCoord)
{
    if (TileLODMap.Remove(TileCoord) > 0)
    {
        UE_LOG(LogTerrainLOD, VeryVerbose, TEXT("Unregistered tile (%d, %d)"), TileCoord.X, TileCoord.Y);
    }
}

void UVHMTerrainLODManager::UpdatePerformanceStats(float CurrentFrameTime)
{
    FrameTimeAccumulator += CurrentFrameTime;
    FrameCount++;
    
    // Update average every 60 frames
    if (FrameCount >= 60)
    {
        AverageFrameTime = FrameTimeAccumulator / FrameCount;
        FrameTimeAccumulator = 0.0f;
        FrameCount = 0;
    }
}

int32 UVHMTerrainLODManager::GetVisibleTileCount() const
{
    int32 VisibleCount = 0;
    for (const auto& TilePair : TileLODMap)
    {
        if (TilePair.Value.bIsVisible)
        {
            VisibleCount++;
        }
    }
    return VisibleCount;
}

TArray<FTileCoord> UVHMTerrainLODManager::GetVisibleTiles() const
{
    TArray<FTileCoord> VisibleTiles;
    for (const auto& TilePair : TileLODMap)
    {
        if (TilePair.Value.bIsVisible)
        {
            VisibleTiles.Add(TilePair.Key);
        }
    }
    return VisibleTiles;
}

void UVHMTerrainLODManager::GetLODDistances(TArray<float>& OutLODDistances) const
{
    OutLODDistances = LODDistances;
}

float UVHMTerrainLODManager::CalculateDistanceToTile(const FTileCoord& TileCoord, const FVector& ViewerPosition) const
{
    FVector TileCenter = TileCoord.ToWorldPosition(TileSize);
    TileCenter.Z = ViewerPosition.Z; // Use viewer height for distance calculation
    
    return FVector::Dist(ViewerPosition, TileCenter);
}

int32 UVHMTerrainLODManager::DistanceToLODLevel(float Distance) const
{
    // Apply quality scaling to distance
    float ScaledDistance = Distance / QualityScale;
    
    // Find appropriate LOD level
    for (int32 LODLevel = 0; LODLevel < LODDistances.Num(); ++LODLevel)
    {
        if (ScaledDistance <= LODDistances[LODLevel])
        {
            return LODLevel;
        }
    }
    
    // Return highest LOD level (lowest quality) if beyond all distances
    return MaxLODLevels - 1;
}

bool UVHMTerrainLODManager::IsWithinViewDistance(float Distance) const
{
    return Distance <= (MaxViewDistance * QualityScale);
}

void UVHMTerrainLODManager::UpdateTileLODData(const FTileCoord& TileCoord, const FVector& ViewerPosition)
{
    FTileLODData* LODData = TileLODMap.Find(TileCoord);
    if (!LODData)
    {
        return;
    }
    
    float Distance = CalculateDistanceToTile(TileCoord, ViewerPosition);
    int32 NewLODLevel = DistanceToLODLevel(Distance);
    bool bNewVisibility = IsWithinViewDistance(Distance);
    
    // Track LOD transitions
    if (NewLODLevel != LODData->CurrentLODLevel)
    {
        LODTransitionsThisFrame++;
        UE_LOG(LogTerrainLOD, VeryVerbose, TEXT("Tile (%d, %d) LOD transition: %d -> %d (Distance: %.1f)"), 
               TileCoord.X, TileCoord.Y, LODData->CurrentLODLevel, NewLODLevel, Distance);
    }
    
    // Update LOD data
    LODData->CurrentLODLevel = NewLODLevel;
    LODData->DistanceToViewer = Distance;
    LODData->bIsVisible = bNewVisibility;
    LODData->LastUpdateFrame = CurrentFrame;
}

void UVHMTerrainLODManager::AdaptLODDistances(float CurrentFrameTime, float InTargetFrameTime)
{
    // Only adapt if we have significant performance deviation
    float FrameTimeDelta = CurrentFrameTime - InTargetFrameTime;
    
    if (FMath::Abs(FrameTimeDelta) < PerformanceToleranceMs)
    {
        return; // Performance is within acceptable range
    }
    
    // Adjust quality scale based on performance
    float QualityAdjustment = 0.0f;
    
    if (FrameTimeDelta > 0.0f) // Running slow, reduce quality
    {
        QualityAdjustment = -0.05f * (FrameTimeDelta / InTargetFrameTime);
    }
    else // Running fast, can increase quality
    {
        QualityAdjustment = 0.02f * FMath::Abs(FrameTimeDelta / InTargetFrameTime);
    }
    
    // Apply adjustment with clamping
    QualityScale = FMath::Clamp(QualityScale + QualityAdjustment, MinQualityScale, MaxQualityScale);
    
    UE_LOG(LogTerrainLOD, VeryVerbose, TEXT("Adapted quality scale to %.3f (FrameDelta: %.2fms)"), 
           QualityScale, FrameTimeDelta);
}

void UVHMTerrainLODManager::CullDistantTiles(const FVector& ViewerPosition)
{
    // Remove tiles that are too far away
    TArray<FTileCoord> TilesToRemove;
    
    for (const auto& TilePair : TileLODMap)
    {
        const FTileCoord& TileCoord = TilePair.Key;
        const FTileLODData& LODData = TilePair.Value;
        
        // Cull tiles that haven't been updated recently and are far away
        bool bShouldCull = false;
        
        // Cull if beyond maximum view distance with some hysteresis
        float CullDistance = MaxViewDistance * QualityScale * 1.2f; // 20% hysteresis
        if (LODData.DistanceToViewer > CullDistance)
        {
            bShouldCull = true;
        }
        
        // Cull if not updated for many frames (stale data)
        uint64 FramesSinceUpdate = CurrentFrame - LODData.LastUpdateFrame;
        if (FramesSinceUpdate > 300) // ~5 seconds at 60 FPS
        {
            bShouldCull = true;
        }
        
        if (bShouldCull)
        {
            TilesToRemove.Add(TileCoord);
        }
    }
    
    // Remove culled tiles
    for (const FTileCoord& TileCoord : TilesToRemove)
    {
        TileLODMap.Remove(TileCoord);
        UE_LOG(LogTerrainLOD, VeryVerbose, TEXT("Culled distant tile (%d, %d)"), TileCoord.X, TileCoord.Y);
    }
}
// Static test function for validation
void UVHMTerrainLODManager::RunBasicValidationTest()
{
    UE_LOG(LogTerrainLOD, Log, TEXT("=== TerrainLODManager Basic Validation Test ==="));
    
    // Test LOD distance calculation
    TArray<float> TestLODDistances;
    TestLODDistances.Reserve(4);
    
    for (int32 LODLevel = 0; LODLevel < 4; ++LODLevel)
    {
        float NormalizedLevel = static_cast<float>(LODLevel) / 3.0f;
        float Distance = FMath::Pow(NormalizedLevel, 1.5f) * 2000.0f;
        TestLODDistances.Add(Distance);
        UE_LOG(LogTerrainLOD, Log, TEXT("LOD %d: Distance threshold %.1fm"), LODLevel, Distance);
    }
    
    // Test tile registration and LOD calculation
    FTileCoord TestTile(0, 0);
    FVector TestViewerPos(100.0f, 100.0f, 50.0f);
    
    float TestDistance = FVector::Dist(TestViewerPos, TestTile.ToWorldPosition(64.0f));
    UE_LOG(LogTerrainLOD, Log, TEXT("Test tile (0,0) distance from viewer (100,100,50): %.1fm"), TestDistance);
    
    // Determine expected LOD level
    int32 ExpectedLOD = 3; // Default to lowest quality
    for (int32 Level = 0; Level < TestLODDistances.Num(); ++Level)
    {
        if (TestDistance <= TestLODDistances[Level])
        {
            ExpectedLOD = Level;
            break;
        }
    }
    
    UE_LOG(LogTerrainLOD, Log, TEXT("Expected LOD level: %d"), ExpectedLOD);
    UE_LOG(LogTerrainLOD, Log, TEXT("=== Validation Test Complete ==="));
}