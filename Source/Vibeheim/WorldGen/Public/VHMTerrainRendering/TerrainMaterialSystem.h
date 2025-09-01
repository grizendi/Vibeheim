#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "VHMTerrainRendering/ITerrainMaterialSystem.h"
#include "VHMTerrainRendering/VHMTypes.h"
#include "Data/WorldGenTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
#include "TerrainMaterialSystem.generated.h"

// Forward declarations
class UBiomeService;
class UMaterialInterface;
class UVirtualTexture2D;
class URuntimeVirtualTexture;
class URuntimeVirtualTextureComponent;

/**
 * Material blend data for tile boundaries
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FTileMaterialBlendData
{
    GENERATED_BODY()

    // Adjacent tile coordinates for blending
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend")
    TArray<FTileCoord> AdjacentTiles;

    // Blend weights for each adjacent tile
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend")
    TMap<FTileCoord, float> BlendWeights;

    // Last update time for cache management
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend")
    double LastUpdateTime = 0.0;
};

/**
 * RVT configuration for terrain materials
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FTerrainRVTConfig
{
    GENERATED_BODY()

    // RVT resolution for terrain texturing
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVT")
    int32 RVTResolution = 4096;

    // RVT tile size
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVT")
    int32 RVTTileSize = 256;

    // Enable RVT streaming
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVT")
    bool bEnableRVTStreaming = true;

    // RVT memory budget in MB
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVT")
    float RVTMemoryBudgetMB = 512.0f;
};

/**
 * Concrete implementation of terrain material system for biome-based rendering
 * Handles material creation, blending, and RVT integration for VHM terrain
 */
UCLASS(BlueprintType)
class VIBEHEIM_API UVHMTerrainMaterialSystem : public UObject, public ITerrainMaterialSystem
{
    GENERATED_BODY()

public:
    UVHMTerrainMaterialSystem();

    /**
     * Initialize the terrain material system
     * @param InBiomeService Reference to biome service for biome data
     * @param InVHMSettings VHM configuration settings
     * @return True if initialization succeeded
     */
    UFUNCTION(BlueprintCallable, Category = "TerrainMaterial")
    bool Initialize(UBiomeService* InBiomeService, const FVHMSettings& InVHMSettings);

    // ITerrainMaterialSystem interface
    virtual UMaterialInstanceDynamic* CreateTileMaterial(const FTileCoord& TileCoord, const FBiomeDefinition& BiomeData) override;
    virtual bool UpdateMaterialParameters(const FTileCoord& TileCoord, const FBiomeDefinition& BiomeData) override;
    virtual void BlendMaterialsAcrossTiles(const TArray<FTileCoord>& AdjacentTiles) override;
    virtual bool InitializeRuntimeVirtualTexturing() override;
    virtual UMaterialInstanceDynamic* GetTileMaterial(const FTileCoord& TileCoord) override;
    virtual void RemoveTileMaterial(const FTileCoord& TileCoord) override;
    virtual bool IsRVTInitialized() const override;

    /**
     * Set base terrain material template
     * @param InBaseMaterial Base material to use for terrain rendering
     */
    UFUNCTION(BlueprintCallable, Category = "TerrainMaterial")
    void SetBaseMaterial(UMaterialInterface* InBaseMaterial);

    /**
     * Get RVT configuration
     */
    UFUNCTION(BlueprintCallable, Category = "TerrainMaterial")
    const FTerrainRVTConfig& GetRVTConfig() const { return RVTConfig; }

    /**
     * Set RVT configuration
     */
    UFUNCTION(BlueprintCallable, Category = "TerrainMaterial")
    void SetRVTConfig(const FTerrainRVTConfig& InRVTConfig);

    /**
     * Get material blend data for a tile
     */
    UFUNCTION(BlueprintCallable, Category = "TerrainMaterial")
    bool GetTileMaterialBlendData(const FTileCoord& TileCoord, FTileMaterialBlendData& OutBlendData) const;

    /**
     * Force update all materials (for debugging)
     */
    UFUNCTION(BlueprintCallable, Category = "TerrainMaterial")
    void ForceUpdateAllMaterials();

    /**
     * Get material memory usage statistics
     */
    UFUNCTION(BlueprintCallable, Category = "TerrainMaterial")
    float GetMaterialMemoryUsageMB() const;

protected:
    // Service references
    UPROPERTY()
    UBiomeService* BiomeService;

    // VHM settings
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
    FVHMSettings VHMSettings;

    // RVT configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVT")
    FTerrainRVTConfig RVTConfig;

    // Base material template
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials")
    TObjectPtr<UMaterialInterface> BaseMaterial;

    // Runtime Virtual Texture for terrain
    UPROPERTY()
    TObjectPtr<URuntimeVirtualTexture> TerrainRVT;

    // Material instances per tile
    UPROPERTY()
    TMap<FTileCoord, TObjectPtr<UMaterialInstanceDynamic>> TileMaterials;

    // Material blend data per tile
    UPROPERTY()
    TMap<FTileCoord, FTileMaterialBlendData> TileBlendData;

    // RVT initialization state
    bool bRVTInitialized = false;

private:
    /**
     * Create material instance from biome definition
     */
    UMaterialInstanceDynamic* CreateMaterialFromBiome(const FBiomeDefinition& BiomeData);

    /**
     * Apply biome-specific parameters to material
     */
    void ApplyBiomeParameters(UMaterialInstanceDynamic* Material, const FBiomeDefinition& BiomeData);

    /**
     * Calculate blend weights for adjacent tiles
     */
    TMap<FTileCoord, float> CalculateBlendWeights(const FTileCoord& CenterTile, const TArray<FTileCoord>& AdjacentTiles);

    /**
     * Apply material blending parameters
     */
    void ApplyMaterialBlending(UMaterialInstanceDynamic* Material, const FTileMaterialBlendData& BlendData);

    /**
     * Create RVT texture for terrain
     */
    URuntimeVirtualTexture* CreateTerrainRVT();

    /**
     * Configure RVT parameters
     */
    void ConfigureRVTParameters(URuntimeVirtualTexture* RVT);

    /**
     * Create RVT component for advanced RVT setup (UE5.6 compatible)
     */
    URuntimeVirtualTextureComponent* CreateRVTComponent();

    /**
     * Get default material if base material is not set
     */
    UMaterialInterface* GetDefaultMaterial();

    /**
     * Validate biome definition has required material data
     */
    bool ValidateBiomeDefinition(const FBiomeDefinition& BiomeData) const;

    /**
     * Get tile center position for material calculations
     */
    FVector2D GetTileCenterPosition(const FTileCoord& TileCoord) const;

    /**
     * Calculate material parameter hash for caching
     */
    uint32 CalculateMaterialParameterHash(const FBiomeDefinition& BiomeData) const;

    /**
     * Cleanup material resources
     */
    void CleanupMaterialResources();
};