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
class UTexture2D;
class UTextureRenderTarget2D;

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
 * RVT texture layer configuration
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FRVTTextureLayer
{
    GENERATED_BODY()

    // Layer name for identification
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer")
    FString LayerName = TEXT("BaseColor");

    // Texture format for this layer
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer")
    TEnumAsByte<EPixelFormat> PixelFormat = PF_B8G8R8A8;

    // Enable compression for this layer
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer")
    bool bEnableCompression = true;

    // Layer priority for streaming
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer")
    int32 StreamingPriority = 1;
};

/**
 * Wrapper struct for biome blend configurations to work with TMap
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FTileBiomeBlendConfig
{
    GENERATED_BODY()

    // Biome definitions for blending
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
    TArray<FBiomeDefinition> BiomeBlends;

    // Last update time
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
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

    // Texture detail layers (base, normal, roughness)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVT")
    TArray<FRVTTextureLayer> TextureLayers;

    // Enable biome-based texture blending
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVT")
    bool bEnableBiomeBlending = true;

    // Streaming distance for texture loading
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVT")
    float StreamingDistance = 2000.0f;

    // Performance optimization level (0-3)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVT")
    int32 OptimizationLevel = 2;
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

    /**
     * Create RVT texture streaming for large terrain areas
     */
    UFUNCTION(BlueprintCallable, Category = "RVT")
    bool SetupRVTTextureStreaming(const TArray<FTileCoord>& InStreamingTiles);

    /**
     * Add biome-based texture blending through RVT system
     */
    UFUNCTION(BlueprintCallable, Category = "RVT")
    bool ConfigureBiomeTextureBlending(const FTileCoord& TileCoord, const TArray<FBiomeDefinition>& BiomeBlends);

    /**
     * Implement texture detail layers (base, normal, roughness) for terrain materials
     */
    UFUNCTION(BlueprintCallable, Category = "RVT")
    bool SetupTextureDetailLayers(const TArray<FRVTTextureLayer>& DetailLayers);

    /**
     * Create RVT performance optimization and memory management
     */
    UFUNCTION(BlueprintCallable, Category = "RVT")
    void OptimizeRVTPerformance();

    /**
     * Get RVT streaming statistics
     */
    UFUNCTION(BlueprintCallable, Category = "RVT")
    void GetRVTStreamingStats(int32& OutActiveTextures, float& OutMemoryUsageMB, int32& OutStreamingRequests) const;

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

    // Runtime Virtual Texture for terrain (using UObject for UE5.6 compatibility)
    UPROPERTY()
    TObjectPtr<UObject> TerrainRVT;

    // Material instances per tile
    UPROPERTY()
    TMap<FTileCoord, TObjectPtr<UMaterialInstanceDynamic>> TileMaterials;

    // Material blend data per tile
    UPROPERTY()
    TMap<FTileCoord, FTileMaterialBlendData> TileBlendData;

    // RVT initialization state
    bool bRVTInitialized = false;

    // RVT streaming tiles
    UPROPERTY()
    TArray<FTileCoord> StreamingTiles;

    // RVT texture layers
    UPROPERTY()
    TMap<FString, TObjectPtr<UTexture2D>> RVTTextureLayers;

    // RVT render targets for dynamic content
    UPROPERTY()
    TMap<FString, TObjectPtr<UTextureRenderTarget2D>> RVTRenderTargets;

    // Biome blend configurations per tile
    UPROPERTY()
    TMap<FTileCoord, FTileBiomeBlendConfig> TileBiomeBlends;

    // RVT streaming statistics
    mutable int32 ActiveRVTTextures = 0;
    mutable float RVTMemoryUsage = 0.0f;
    mutable int32 StreamingRequests = 0;

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
    UObject* CreateTerrainRVT();

    /**
     * Configure RVT parameters
     */
    void ConfigureRVTParameters(UObject* RVT);

    /**
     * Create RVT component for advanced RVT setup (UE5.6 compatible)
     */
    UObject* CreateRVTComponent();

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

    /**
     * Create RVT texture layer
     */
    UTexture2D* CreateRVTTextureLayer(const FRVTTextureLayer& LayerConfig);

    /**
     * Create RVT render target for dynamic content
     */
    UTextureRenderTarget2D* CreateRVTRenderTarget(const FString& LayerName, int32 Resolution);

    /**
     * Update RVT texture streaming based on viewer position
     */
    void UpdateRVTTextureStreaming(const FVector& ViewerPosition);

    /**
     * Apply biome blending to RVT textures
     */
    void ApplyBiomeBlendingToRVT(const FTileCoord& TileCoord, const TArray<FBiomeDefinition>& BiomeBlends);

    /**
     * Generate texture detail layers for terrain
     */
    void GenerateTextureDetailLayers(const FTileCoord& TileCoord, const FBiomeDefinition& BiomeData);

    /**
     * Optimize RVT memory usage
     */
    void OptimizeRVTMemoryUsage();

    /**
     * Update RVT streaming statistics
     */
    void UpdateRVTStreamingStats() const;

    /**
     * Validate RVT configuration
     */
    bool ValidateRVTConfiguration() const;

    /**
     * Initialize default texture layers
     */
    void InitializeDefaultTextureLayers();
};