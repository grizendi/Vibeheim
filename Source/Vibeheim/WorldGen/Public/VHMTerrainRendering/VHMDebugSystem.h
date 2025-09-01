#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Engine/World.h"
#include "VHMTerrainRendering/VHMTypes.h"
#include "Data/WorldGenTypes.h"
#include "VHMDebugSystem.generated.h"

// Forward declarations
class UVHMTerrainRenderer;
class UVirtualHeightfieldMeshComponent;
class UTexture2D;
class UMaterialInstanceDynamic;

/**
 * Debug visualization modes for VHM system
 */
UENUM(BlueprintType)
enum class EVHMDebugMode : uint8
{
    None            UMETA(DisplayName = "None"),
    ComponentBounds UMETA(DisplayName = "Component Bounds"),
    LODLevels       UMETA(DisplayName = "LOD Levels"),
    TileBoundaries  UMETA(DisplayName = "Tile Boundaries"),
    Wireframe       UMETA(DisplayName = "Wireframe"),
    HeightTexture   UMETA(DisplayName = "Height Texture"),
    MaterialDebug   UMETA(DisplayName = "Material Debug"),
    Performance     UMETA(DisplayName = "Performance")
};

/**
 * Debug draw settings for VHM visualization
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FVHMDebugDrawSettings
{
    GENERATED_BODY()

    // Enable debug drawing
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bEnableDebugDraw = false;

    // Debug mode to display
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    EVHMDebugMode DebugMode = EVHMDebugMode::None;

    // Color for component bounds
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    FColor BoundsColor = FColor::Green;

    // Color for LOD level indicators
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    FColor LODColor = FColor::Blue;

    // Color for tile boundaries
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    FColor BoundaryColor = FColor::Red;

    // Line thickness for debug drawing
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    float LineThickness = 2.0f;

    // Duration for debug lines (0 = one frame)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    float LineDuration = 0.0f;

    // Show debug text labels
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bShowDebugText = true;

    // Text scale for debug labels
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    float TextScale = 1.0f;
};

/**
 * Texture export settings for debugging
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FVHMTextureExportSettings
{
    GENERATED_BODY()

    // Export directory path
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Export")
    FString ExportDirectory = TEXT("Debug/VHM/");

    // Export format (PNG, EXR, etc.)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Export")
    FString ExportFormat = TEXT("PNG");

    // Include tile coordinates in filename
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Export")
    bool bIncludeTileCoords = true;

    // Include timestamp in filename
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Export")
    bool bIncludeTimestamp = false;

    // Export height textures
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Export")
    bool bExportHeightTextures = true;

    // Export material textures
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Export")
    bool bExportMaterialTextures = true;
};

/**
 * Performance profiling data for VHM operations
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FVHMProfilingData
{
    GENERATED_BODY()

    // Operation name
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profiling")
    FString OperationName;

    // Start time
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profiling")
    double StartTime = 0.0;

    // End time
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profiling")
    double EndTime = 0.0;

    // Duration in milliseconds
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profiling")
    float DurationMs = 0.0f;

    // Tile coordinate (if applicable)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profiling")
    FTileCoord TileCoord = FTileCoord();

    // Additional data
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profiling")
    TMap<FString, FString> AdditionalData;
};

/**
 * VHM debugging and visualization system
 * Provides tools for debugging VHM terrain rendering, performance profiling, and texture export
 */
UCLASS(BlueprintType)
class VIBEHEIM_API UVHMDebugSystem : public UObject
{
    GENERATED_BODY()

public:
    UVHMDebugSystem();

    /**
     * Initialize the debug system with VHM terrain renderer
     */
    UFUNCTION(BlueprintCallable, Category = "VHM Debug")
    bool Initialize(UVHMTerrainRenderer* InTerrainRenderer, UWorld* InWorld);

    /**
     * Set debug draw settings
     */
    UFUNCTION(BlueprintCallable, Category = "VHM Debug")
    void SetDebugDrawSettings(const FVHMDebugDrawSettings& Settings);

    /**
     * Get current debug draw settings
     */
    UFUNCTION(BlueprintCallable, Category = "VHM Debug")
    const FVHMDebugDrawSettings& GetDebugDrawSettings() const { return DebugDrawSettings; }

    /**
     * Enable/disable debug visualization
     */
    UFUNCTION(BlueprintCallable, Category = "VHM Debug")
    void SetDebugVisualizationEnabled(bool bEnabled);

    /**
     * Set debug visualization mode
     */
    UFUNCTION(BlueprintCallable, Category = "VHM Debug")
    void SetDebugMode(EVHMDebugMode Mode);

    /**
     * Draw debug visualization for all active VHM components
     */
    UFUNCTION(BlueprintCallable, Category = "VHM Debug")
    void DrawDebugVisualization();

    /**
     * Draw debug visualization for a specific tile
     */
    UFUNCTION(BlueprintCallable, Category = "VHM Debug")
    void DrawDebugVisualizationForTile(const FTileCoord& TileCoord);

    /**
     * Export height texture for a specific tile
     */
    UFUNCTION(BlueprintCallable, Category = "VHM Debug")
    bool ExportHeightTexture(const FTileCoord& TileCoord, const FString& OutputPath = TEXT(""));

    /**
     * Export material texture for a specific tile
     */
    UFUNCTION(BlueprintCallable, Category = "VHM Debug")
    bool ExportMaterialTexture(const FTileCoord& TileCoord, const FString& OutputPath = TEXT(""));

    /**
     * Export all textures for a specific tile
     */
    UFUNCTION(BlueprintCallable, Category = "VHM Debug")
    bool ExportAllTexturesForTile(const FTileCoord& TileCoord, const FString& OutputDirectory = TEXT(""));

    /**
     * Start performance profiling for an operation
     */
    UFUNCTION(BlueprintCallable, Category = "VHM Debug")
    void StartProfiling(const FString& OperationName, const FTileCoord& TileCoord = FTileCoord());

    /**
     * End performance profiling for an operation
     */
    UFUNCTION(BlueprintCallable, Category = "VHM Debug")
    void EndProfiling(const FString& OperationName);

    /**
     * Get profiling results for an operation
     */
    UFUNCTION(BlueprintCallable, Category = "VHM Debug")
    bool GetProfilingResults(const FString& OperationName, FVHMProfilingData& OutProfilingData) const;

    /**
     * Get all profiling results
     */
    UFUNCTION(BlueprintCallable, Category = "VHM Debug")
    TArray<FVHMProfilingData> GetAllProfilingResults() const;

    /**
     * Clear profiling data
     */
    UFUNCTION(BlueprintCallable, Category = "VHM Debug")
    void ClearProfilingData();

    /**
     * Set wireframe mode for VHM components
     */
    UFUNCTION(BlueprintCallable, Category = "VHM Debug")
    void SetWireframeMode(bool bEnabled);

    /**
     * Set debug material mode for VHM components
     */
    UFUNCTION(BlueprintCallable, Category = "VHM Debug")
    void SetDebugMaterialMode(bool bEnabled);

    /**
     * Get performance statistics summary
     */
    UFUNCTION(BlueprintCallable, Category = "VHM Debug")
    FString GetPerformanceStatsSummary() const;

    /**
     * Log VHM component information for debugging
     */
    UFUNCTION(BlueprintCallable, Category = "VHM Debug")
    void LogVHMComponentInfo(const FTileCoord& TileCoord) const;

    /**
     * Log texture memory usage information
     */
    UFUNCTION(BlueprintCallable, Category = "VHM Debug")
    void LogTextureMemoryUsage() const;

protected:
    // VHM terrain renderer reference
    UPROPERTY()
    UVHMTerrainRenderer* TerrainRenderer;

    // World reference for debug drawing
    UPROPERTY()
    UWorld* World;

    // Debug draw settings
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    FVHMDebugDrawSettings DebugDrawSettings;

    // Texture export settings
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    FVHMTextureExportSettings TextureExportSettings;

    // Performance profiling data
    UPROPERTY()
    TMap<FString, FVHMProfilingData> ProfilingData;

    // Debug materials for visualization
    UPROPERTY()
    UMaterialInstanceDynamic* WireframeMaterial;

    UPROPERTY()
    UMaterialInstanceDynamic* DebugMaterial;

private:
    /**
     * Draw component bounds for a VHM component
     */
    void DrawComponentBounds(UVirtualHeightfieldMeshComponent* VHMComponent, const FTileCoord& TileCoord);

    /**
     * Draw LOD level indicators for a VHM component
     */
    void DrawLODLevelIndicators(UVirtualHeightfieldMeshComponent* VHMComponent, const FTileCoord& TileCoord);

    /**
     * Draw tile boundary lines
     */
    void DrawTileBoundaries(const FTileCoord& TileCoord);

    /**
     * Draw performance information overlay
     */
    void DrawPerformanceOverlay();

    /**
     * Generate export filename for texture
     */
    FString GenerateExportFilename(const FTileCoord& TileCoord, const FString& TextureType, const FString& Extension) const;

    /**
     * Create debug materials if needed
     */
    void CreateDebugMaterials();

    /**
     * Get tile world bounds for debug drawing
     */
    FBox GetTileWorldBounds(const FTileCoord& TileCoord) const;

    /**
     * Get tile center position for debug drawing
     */
    FVector GetTileCenterPosition(const FTileCoord& TileCoord) const;
};