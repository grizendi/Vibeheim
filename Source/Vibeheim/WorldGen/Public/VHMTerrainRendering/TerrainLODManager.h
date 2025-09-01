#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "VHMTerrainRendering/ITerrainLODManager.h"
#include "VHMTerrainRendering/VHMTypes.h"
#include "Data/WorldGenTypes.h"
#include "TerrainLODManager.generated.h"

/**
 * LOD data for a specific tile
 */
USTRUCT()
struct VIBEHEIM_API FTileLODData
{
    GENERATED_BODY()

    // Current LOD level for this tile
    UPROPERTY()
    int32 CurrentLODLevel = 0;

    // Distance from viewer to tile center
    UPROPERTY()
    float DistanceToViewer = 0.0f;

    // Whether tile is currently visible
    UPROPERTY()
    bool bIsVisible = false;

    // Last frame this tile was updated
    UPROPERTY()
    uint64 LastUpdateFrame = 0;

    // World bounds of the tile
    UPROPERTY()
    FBox WorldBounds = FBox(ForceInit);

    FTileLODData() = default;
    FTileLODData(const FTileCoord& TileCoord, float TileSize = 64.0f);
};

/**
 * Concrete implementation of terrain Level-of-Detail management
 * Handles distance-based quality, culling, and adaptive performance optimization
 */
UCLASS(BlueprintType)
class VIBEHEIM_API UVHMTerrainLODManager : public UObject, public ITerrainLODManager
{
    GENERATED_BODY()

public:
    UVHMTerrainLODManager();

    // ITerrainLODManager interface
    virtual int32 CalculateLODLevel(const FTileCoord& TileCoord, const FVector& ViewerPosition) override;
    virtual void UpdateLODLevels(const FVector& ViewerPosition) override;
    virtual bool IsTileVisible(const FTileCoord& TileCoord, const FVector& ViewerPosition) override;
    virtual void OptimizeMeshDetail(float InTargetFrameTime) override;
    virtual int32 GetCurrentLODLevel(const FTileCoord& TileCoord) override;
    virtual void SetMaxLODLevels(int32 InMaxLODLevels) override;
    virtual int32 GetLODTransitionsThisFrame() const override;
    virtual void ResetFrameStats() override;

    // Configuration and management
    UFUNCTION(BlueprintCallable, Category = "LOD")
    void Initialize(const FVHMSettings& Settings);

    UFUNCTION(BlueprintCallable, Category = "LOD")
    void RegisterTile(const FTileCoord& TileCoord);

    UFUNCTION(BlueprintCallable, Category = "LOD")
    void UnregisterTile(const FTileCoord& TileCoord);

    UFUNCTION(BlueprintCallable, Category = "LOD")
    void SetTileSize(float NewTileSize) { TileSize = NewTileSize; }

    UFUNCTION(BlueprintCallable, Category = "LOD")
    float GetTileSize() const { return TileSize; }

    // Performance monitoring
    UFUNCTION(BlueprintCallable, Category = "LOD")
    void UpdatePerformanceStats(float CurrentFrameTime);

    UFUNCTION(BlueprintCallable, Category = "LOD")
    int32 GetVisibleTileCount() const;

    UFUNCTION(BlueprintCallable, Category = "LOD")
    float GetAverageFrameTime() const { return AverageFrameTime; }

    // Debug and visualization
    UFUNCTION(BlueprintCallable, Category = "LOD")
    TArray<FTileCoord> GetVisibleTiles() const;

    UFUNCTION(BlueprintCallable, Category = "LOD")
    void GetLODDistances(TArray<float>& OutLODDistances) const;

    // Testing and validation
    UFUNCTION(BlueprintCallable, Category = "LOD")
    static void RunBasicValidationTest();

protected:
    // LOD calculation helpers
    float CalculateDistanceToTile(const FTileCoord& TileCoord, const FVector& ViewerPosition) const;
    int32 DistanceToLODLevel(float Distance) const;
    bool IsWithinViewDistance(float Distance) const;
    void UpdateTileLODData(const FTileCoord& TileCoord, const FVector& ViewerPosition);

    // Performance optimization
    void AdaptLODDistances(float CurrentFrameTime, float InTargetFrameTime);
    void CullDistantTiles(const FVector& ViewerPosition);

private:
    // Configuration
    UPROPERTY()
    int32 MaxLODLevels = 4;

    UPROPERTY()
    float MaxViewDistance = 2000.0f;

    UPROPERTY()
    float TileSize = 64.0f;

    UPROPERTY()
    TArray<float> LODDistances;

    // Tile tracking
    UPROPERTY()
    TMap<FTileCoord, FTileLODData> TileLODMap;

    // Performance tracking
    UPROPERTY()
    int32 LODTransitionsThisFrame = 0;

    UPROPERTY()
    float AverageFrameTime = 16.67f; // Target 60 FPS

    UPROPERTY()
    float FrameTimeAccumulator = 0.0f;

    UPROPERTY()
    int32 FrameCount = 0;

    UPROPERTY()
    uint64 CurrentFrame = 0;

    // Adaptive quality settings
    UPROPERTY()
    float QualityScale = 1.0f;

    UPROPERTY()
    float MinQualityScale = 0.5f;

    UPROPERTY()
    float MaxQualityScale = 1.5f;

    // Performance thresholds
    UPROPERTY()
    float TargetFrameTime = 16.67f; // 60 FPS

    UPROPERTY()
    float PerformanceToleranceMs = 2.0f;

    // Initialize default LOD distances
    void InitializeLODDistances();
    void ValidateLODDistances();
};