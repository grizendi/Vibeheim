#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Data/WorldGenTypes.h"
#include "VHMTypes.generated.h"

// Forward declarations
class UVirtualHeightfieldMeshComponent;
class UMaterialInstanceDynamic;
class UTexture2D;

/**
 * VHM system configuration settings
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FVHMSettings
{
    GENERATED_BODY()

    // Texture resolution for height data (64x64 for 64m tiles with 1m spacing)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VHM")
    int32 HeightTextureResolution = 64;
    
    // Number of LOD levels for distance-based quality
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VHM")
    int32 LODLevels = 4;
    
    // Maximum viewing distance for terrain rendering
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VHM")
    float MaxViewDistance = 2000.0f;
    
    // Enable real-time terrain editing support
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VHM")
    bool bEnableRealTimeEditing = true;
    
    // RVT settings for material blending
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VHM")
    bool bUseRuntimeVirtualTexturing = true;

    // Performance budget for mesh generation (milliseconds per tile)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VHM")
    float MeshGenerationBudgetMs = 2.0f;

    // Texture format for height data (R16F or R32F)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VHM")
    bool bUseHighPrecisionHeightTextures = false;

    // Enable seamless boundary stitching between tiles
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boundary")
    bool bEnableBoundaryStitching = true;

    // Blend factor for boundary stitching (0.0 = primary tile, 1.0 = adjacent tile)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boundary")
    float BoundaryBlendFactor = 0.5f;

    // Radius of tiles to update when boundary changes occur
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boundary")
    int32 BoundaryUpdateRadius = 1;
};

/**
 * Terrain mesh data for a single tile
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FTerrainMeshData
{
    GENERATED_BODY()

    // Tile coordinate this mesh represents
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
    FTileCoord TileCoord = FTileCoord();
    
    // VHM component instance
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
    TObjectPtr<UVirtualHeightfieldMeshComponent> VHMComponent = nullptr;
    
    // Height texture for this tile
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
    TObjectPtr<UTexture2D> HeightTexture = nullptr;
    
    // Material instance for biome-specific rendering
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
    TObjectPtr<UMaterialInstanceDynamic> MaterialInstance = nullptr;
    
    // Current LOD level
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
    int32 CurrentLODLevel = 0;
    
    // Last update timestamp for cache management
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
    double LastUpdateTime = 0.0;

    // Mesh bounds in world space
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
    FBox WorldBounds = FBox(ForceInit);

    // Whether this mesh is currently visible
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
    bool bIsVisible = false;
};

/**
 * Tile boundary edge information for seamless stitching
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FTileBoundaryEdge
{
    GENERATED_BODY()

    // Edge direction (North, South, East, West)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boundary")
    uint8 EdgeDirection = 0; // 0=North, 1=East, 2=South, 3=West

    // Height values along the edge (from start to end)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boundary")
    TArray<float> EdgeHeights;

    // Normal vectors along the edge
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boundary")
    TArray<FVector> EdgeNormals;

    // Texture coordinates along the edge
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boundary")
    TArray<FVector2D> EdgeUVs;

    // Adjacent tile coordinate
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boundary")
    FTileCoord AdjacentTile = FTileCoord();

    FTileBoundaryEdge()
    {
        EdgeHeights.Reserve(64); // Default resolution
        EdgeNormals.Reserve(64);
        EdgeUVs.Reserve(64);
    }
};

/**
 * Tile boundary data for seamless mesh transitions
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FTileBoundaryData
{
    GENERATED_BODY()

    // Tile coordinate this boundary data represents
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boundary")
    FTileCoord TileCoord = FTileCoord();

    // Boundary edges (North, East, South, West)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boundary")
    TArray<FTileBoundaryEdge> BoundaryEdges;

    // Last update timestamp
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boundary")
    double LastUpdateTime = 0.0;

    // Whether boundary data is valid
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boundary")
    bool bIsValid = false;

    FTileBoundaryData()
    {
        BoundaryEdges.SetNum(4); // North, East, South, West
    }
};

/**
 * VHM performance statistics
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FVHMPerformanceStats
{
    GENERATED_BODY()

    // Number of active VHM components
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    int32 ActiveVHMComponents = 0;

    // Total texture memory usage in MB
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    float TextureMemoryUsageMB = 0.0f;

    // Average mesh generation time in milliseconds
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    float AverageMeshGenerationMs = 0.0f;

    // Last mesh generation time in milliseconds
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    float LastMeshGenerationMs = 0.0f;

    // Number of LOD transitions this frame
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    int32 LODTransitionsThisFrame = 0;

    // Current frame rate
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    float CurrentFPS = 0.0f;

    // Total number of meshes created
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    int32 TotalMeshesCreated = 0;

    // Average generation time in milliseconds
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    float AverageGenerationTimeMs = 0.0f;

    // Peak generation time in milliseconds
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    float PeakGenerationTimeMs = 0.0f;

    // Total generation time in milliseconds
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    float TotalGenerationTimeMs = 0.0f;

    // Number of boundary stitching operations this frame
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    int32 BoundaryStitchingOperations = 0;
};
