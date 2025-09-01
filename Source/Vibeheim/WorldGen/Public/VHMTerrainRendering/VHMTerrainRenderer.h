#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "VHMTerrainRendering/IVHMTerrainRenderer.h"
#include "VHMTerrainRendering/IHeightfieldTextureManager.h"
#include "VHMTerrainRendering/ITerrainMaterialSystem.h"
#include "VHMTerrainRendering/VHMTypes.h"
#include "Data/WorldGenTypes.h"
#include "VHMTerrainRenderer.generated.h"

// Forward declarations
class UWorldGenSettings;
class UHeightfieldService;
class UTileStreamingService;
class UHeightfieldTextureManager;
class UBiomeService;
class UVHMTerrainMaterialSystem;
class UVirtualHeightfieldMeshComponent;
class UWorld;
class AActor;

/**
 * Main VHM terrain rendering coordinator
 * Manages VirtualHeightfieldMeshComponent lifecycle and integrates with world generation services
 * 
 * Note: Current implementation creates basic VHM components. VHM-specific API calls
 * will be added once the correct UE5 VirtualHeightfieldMesh API is determined.
 */
UCLASS(BlueprintType)
class VIBEHEIM_API UVHMTerrainRenderer : public UObject, public IVHMTerrainRendererInterface
{
    GENERATED_BODY()

public:
    UVHMTerrainRenderer();

    // IVHMTerrainRendererInterface interface
    virtual bool Initialize(UWorldGenSettings* Settings, 
                          UHeightfieldService* HeightfieldService,
                          UTileStreamingService* TileStreamingService) override;

    /**
     * Initialize with biome service for material system
     */
    UFUNCTION(BlueprintCallable, Category = "VHM")
    bool InitializeWithBiomeService(UWorldGenSettings* Settings, 
                                  UHeightfieldService* HeightfieldService,
                                  UTileStreamingService* TileStreamingService,
                                  UBiomeService* BiomeService);
    
    virtual bool CreateTerrainMeshForTile(const FTileCoord& TileCoord) override;
    virtual bool UpdateTerrainMesh(const FTileCoord& TileCoord, const TArray<FHeightfieldModification>& Modifications) override;
    virtual void RemoveTerrainMesh(const FTileCoord& TileCoord) override;
    virtual UVirtualHeightfieldMeshComponent* GetVHMComponent(const FTileCoord& TileCoord) override;
    virtual void UpdateLODLevels(const FVector& ViewerPosition) override;
    virtual FVHMPerformanceStats GetPerformanceStats() const override;
    virtual void OnTileStreamingEvent(const FTileCoord& TileCoord, bool bTileLoaded) override;
    virtual void Cleanup() override;

    /**
     * Get VHM settings for configuration
     */
    UFUNCTION(BlueprintCallable, Category = "VHM")
    const FVHMSettings& GetVHMSettings() const { return VHMSettings; }

    /**
     * Set VHM settings (for runtime configuration changes)
     */
    UFUNCTION(BlueprintCallable, Category = "VHM")
    void SetVHMSettings(const FVHMSettings& NewSettings);

    /**
     * Get terrain mesh data for a specific tile
     */
    UFUNCTION(BlueprintCallable, Category = "VHM")
    bool GetTerrainMeshData(const FTileCoord& TileCoord, FTerrainMeshData& OutMeshData) const;

    /**
     * Get all active terrain meshes
     */
    UFUNCTION(BlueprintCallable, Category = "VHM")
    TArray<FTileCoord> GetActiveMeshTiles() const;

protected:
    // VHM system configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VHM")
    FVHMSettings VHMSettings;

    // Service references
    UPROPERTY()
    UWorldGenSettings* WorldGenSettings;

    UPROPERTY()
    UHeightfieldService* HeightfieldService;

    UPROPERTY()
    UTileStreamingService* TileStreamingService;

    UPROPERTY()
    TScriptInterface<UHeightfieldTextureManager> HeightfieldTextureManager;

    UPROPERTY()
    UBiomeService* BiomeService;

    UPROPERTY()
    TScriptInterface<ITerrainMaterialSystem> TerrainMaterialSystem;

    // Terrain mesh management
    UPROPERTY()
    TMap<FTileCoord, FTerrainMeshData> TerrainMeshes;

    // Performance tracking
    mutable FVHMPerformanceStats PerformanceStats;
    TArray<float> RecentMeshGenerationTimes;
    static const int32 MaxRecentTimes = 50;

    // World reference for component creation
    UPROPERTY()
    UWorld* CachedWorld;

private:
    /**
     * Create VHM component for a tile
     */
    UVirtualHeightfieldMeshComponent* CreateVHMComponent(const FTileCoord& TileCoord);

    /**
     * Configure VHM component properties
     */
    void ConfigureVHMComponent(UVirtualHeightfieldMeshComponent* VHMComponent, const FTileCoord& TileCoord);

    /**
     * Generate mesh from heightfield data for VHM component
     */
    bool GenerateMeshFromHeightfield(UVirtualHeightfieldMeshComponent* VHMComponent, const FTileCoord& TileCoord, UTexture2D* HeightTexture);

    /**
     * Validate VHM component is properly configured
     */
    bool ValidateVHMComponent(UVirtualHeightfieldMeshComponent* VHMComponent, const FTileCoord& TileCoord) const;

    /**
     * Calculate world bounds for a tile
     */
    FBox CalculateTileWorldBounds(const FTileCoord& TileCoord) const;

    /**
     * Calculate LOD level based on distance from viewer
     */
    int32 CalculateLODLevel(const FTileCoord& TileCoord, const FVector& ViewerPosition) const;

    /**
     * Update performance statistics
     */
    void UpdatePerformanceStats(float MeshGenerationTime) const;

    /**
     * Record mesh generation time for performance tracking
     */
    void RecordMeshGenerationTime(float GenerationTime);

    /**
     * Get tile center position in world coordinates
     */
    FVector GetTileCenterWorldPosition(const FTileCoord& TileCoord) const;

    /**
     * Get tile corner position in world coordinates (bottom-left corner)
     */
    FVector GetTileCornerWorldPosition(const FTileCoord& TileCoord) const;

    /**
     * Validate tile coordinate is within reasonable bounds
     */
    bool IsValidTileCoordinate(const FTileCoord& TileCoord) const;

    /**
     * Cleanup mesh data for a tile
     */
    void CleanupMeshData(const FTileCoord& TileCoord);

    /**
     * Initialize heightfield texture manager
     */
    bool InitializeHeightfieldTextureManager();

    /**
     * Initialize terrain material system
     */
    bool InitializeTerrainMaterialSystem();
};