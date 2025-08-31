#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/VirtualHeightfieldMeshComponent.h"
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
};