#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Optional.h"
// Include PCG headers if available
#if defined(WITH_PCG) && WITH_PCG
#include "PCGGraph.h"
#endif
#include "Curves/CurveFloat.h"
#include "Data/WorldGenBuildState.h"
#include "WorldGenTypes.generated.h"

// Forward declarations
class UTexture2D;
class UMaterialInterface;
class UStaticMesh;
class UBlueprint;
// Forward declare PCG classes (optional dependency)
class UPCGGraph;
class UCurveFloat;
struct FWorldGenConfig;

/**
 * Tile coordinate structure for world partitioning
 * Uses 64-meter tiles with world-space sampling
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FTileCoord {
  GENERATED_BODY()

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile")
  int32 X = 0;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile")
  int32 Y = 0;

  FTileCoord() = default;
  FTileCoord(int32 InX, int32 InY) : X(InX), Y(InY) {}
  FTileCoord(FIntVector2 InCoord) : X(InCoord.X), Y(InCoord.Y) {}

  // Convert world position to tile coordinate
  static FTileCoord FromWorldPosition(FVector WorldPos, float TileSize) {
    return FTileCoord(FMath::FloorToInt(WorldPos.X / TileSize),
                      FMath::FloorToInt(WorldPos.Y / TileSize));
  }

  static FTileCoord FromWorldPosition(const FVector &WorldPos,
                                      const struct FWorldGenConfig &Config);

  // Convert tile coordinate to world position (center of tile)
  FVector ToWorldPosition(float TileSize) const {
    return FVector((X + 0.5f) * TileSize, (Y + 0.5f) * TileSize, 0.0f);
  }

  /**
   * Convert this tile coordinate to a PCG partition grid cell.
   * Expects TileSize and PCGGridSize to have an integer ratio for deterministic alignment.
   */
  FIntPoint ToPCGGridCell(float PCGGridSize, float TileSize) const;

  /**
   * Construct a tile coordinate from a PCG partition grid cell.
   * Expects TileSize and PCGGridSize to have an integer ratio for deterministic alignment.
   */
  static FTileCoord FromPCGGridCell(const FIntPoint &Cell, float PCGGridSize,
                                    float TileSize);

  /**
   * Verify that TileSize and PCGGridSize have an integer ratio within tolerance.
   * Accepts either direction (TileSize as multiple of PCG grid or vice versa) to avoid silent drift.
   */
  static bool IsAlignedWithPCGGrid(float TileSize, float PCGGridSize,
                                   float Tolerance = KINDA_SMALL_NUMBER);

  FVector ToWorldPosition(const struct FWorldGenConfig &Config) const;

  // Hash function for use in TMap
  friend uint32 GetTypeHash(const FTileCoord &Coord);

  bool operator==(const FTileCoord &Other) const {
    return X == Other.X && Y == Other.Y;
  }

  bool operator!=(const FTileCoord &Other) const { return !(*this == Other); }

  FIntVector2 ToIntVector2() const { return FIntVector2(X, Y); }

  // Custom serialization for archive compatibility
  bool Serialize(FArchive &Ar) {
    Ar << X;
    Ar << Y;
    return true;
  }
};

/**
 * Biome types supported by the world generation system
 */
UENUM(BlueprintType)
enum class EBiomeType : uint8 {
  None = 0,
  Meadows = 1,
  Forest = 2,
  Mountains = 3,
  Ocean = 4,
  MAX UMETA(Hidden)
};

/**
 * Heightfield modification operations
 */
UENUM(BlueprintType)
enum class EHeightfieldOperation : uint8 {
  Add = 0,
  Subtract = 1,
  Flatten = 2,
  Smooth = 3,
  Noise = 4,
  MAX UMETA(Hidden)
};

/**
 * World generation build modes
 */
UENUM(BlueprintType)
enum class EWorldGenBuildMode : uint8 {
  RuntimeStreaming UMETA(DisplayName = "Runtime Streaming (Legacy)"),
  EditorBuildOnce UMETA(DisplayName = "Editor Build Once (Valheim-Style)"),
  Hybrid UMETA(DisplayName = "Hybrid (Baked Terrain + Dynamic Features)")
};

/**
 * Policy for handling stale or mismatched build state.
 */
UENUM(BlueprintType)
enum class EWorldBuildStatePolicy : uint8 {
  FallbackToRuntime UMETA(DisplayName = "Fallback to Runtime Generation"),
  RequireRebuild UMETA(DisplayName = "Require Rebuild")
};

/**
 * Core world generation configuration structure
 * Defines all parameters for deterministic world generation
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FWorldGenConfig {
  GENERATED_BODY()

  // Core generation parameters
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
  int32 Seed = 1337;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
  int32 WorldGenVersion = 1;

  // Build configuration
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
  EWorldGenBuildMode BuildMode = EWorldGenBuildMode::RuntimeStreaming;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
  bool bUseWorldPartitionStreaming = true;

  /** Policy for handling stale/mismatched build states when using baked worlds. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
  EWorldBuildStatePolicy StaleBuildPolicy =
      EWorldBuildStatePolicy::FallbackToRuntime;

  // Tile and coordinate system (locked values)
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coordinates",
            meta = (ClampMin = "64", ClampMax = "64"))
  float TileSizeMeters = 64.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coordinates",
            meta = (ClampMin = "1", ClampMax = "1"))
  float SampleSpacingMeters = 1.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coordinates",
            meta = (ClampMin = "120", ClampMax = "120"))
  float MaxTerrainHeight = 120.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coordinates")
  float SeaLevel = 0.0f;

  // Streaming radii (in tiles)
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming",
            meta = (ClampMin = "1", ClampMax = "20"))
  int32 GenerateRadius = 9;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming",
            meta = (ClampMin = "1", ClampMax = "15"))
  int32 LoadRadius = 5;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming",
            meta = (ClampMin = "1", ClampMax = "10"))
  int32 ActiveRadius = 3;

  // Heightfield settings
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heightfield")
  float HeightfieldScale = 100.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heightfield")
  int32 HeightfieldResolution = 1024;

  // World Partition settings
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldPartition")
  int32 CellSize = 12800; // 128m cells

  // PCG settings
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCG")
  float VegetationDensity = 1.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCG")
  float POIDensity = 0.1f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCG")
  int32 MaxHISMInstances = 10000;

  /** When true, headless environments still spawn logical instances for
   * testing. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCG")
  bool bAllowHeadlessLogicalInstances = true;

  /** Enables runtime frustum culling for the PCG scheduler. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCG|Scheduler",
            meta = (DisplayName = "Enable Frustum Culling"))
  bool bEnableFrustumCulling = true;

  /** Additional margin (in Unreal units) added to the camera frustum when
   * scheduling tiles. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCG|Scheduler",
            meta = (ClampMin = "0.0"))
  float FrustumCullingMargin = 500.0f;

  /**
   * Optional overrides for frustum margins. Keys support specific component
   * names, biome names, or NAME_None for a global default. The runtime
   * scheduler resolves values in that order.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCG|Scheduler")
  TMap<FName, float> FrustumCullingMarginByLOD;

  /** Maximum number of concurrent PCG tasks allowed before new requests fall
   * back to HISM generation. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCG|Scheduler",
            meta = (ClampMin = "1"))
  int32 MaxConcurrentPCGTasks = 4;

  // Biome noise parameters
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biomes")
  float BiomeScale = 0.001f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biomes")
  float BiomeBlendDistance = 500.0f;

  // RVT settings
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVT")
  int32 RVTResolution = 4096;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVT")
  int32 RVTTileSize = 256;

  // Performance targets
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
  float TileGenTargetMs = 2.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
  float PCGTargetMsPerTile = 1.0f;

  // Feature toggles (Phase 0)
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Features")
  bool bEnableWater = false;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Features")
  bool bEnableRivers = false;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Features")
  bool bEnableRings = false;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Features")
  bool bEnablePCGGraphs = false;

  // VHM terrain rendering settings
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VHM")
  int32 VHMHeightTextureResolution = 64;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VHM")
  int32 VHMLODLevels = 4;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VHM")
  float VHMMaxViewDistance = 2000.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VHM")
  bool bVHMEnableRealTimeEditing = true;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VHM")
  bool bVHMUseRuntimeVirtualTexturing = true;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VHM")
  float VHMMeshGenerationBudgetMs = 2.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VHM")
  bool bVHMUseHighPrecisionHeightTextures = false;

  FWorldGenConfig() {
    // Ensure locked values are set correctly
    TileSizeMeters = 64.0f;
    SampleSpacingMeters = 1.0f;
    MaxTerrainHeight = 120.0f;
  }
};

inline FTileCoord FTileCoord::FromWorldPosition(const FVector &WorldPos,
                                                const FWorldGenConfig &Config) {
  return FromWorldPosition(WorldPos, Config.TileSizeMeters);
}

inline FVector
FTileCoord::ToWorldPosition(const FWorldGenConfig &Config) const {
  return ToWorldPosition(Config.TileSizeMeters);
}

inline FIntPoint FTileCoord::ToPCGGridCell(float PCGGridSize,
                                           float TileSize) const {
  const float SafeTileSize = FMath::Max(TileSize, KINDA_SMALL_NUMBER);
  const float SafeGridSize = FMath::Max(PCGGridSize, KINDA_SMALL_NUMBER);
  const float TilesPerPCGCell = SafeTileSize / SafeGridSize;
  const bool bAligned =
      IsAlignedWithPCGGrid(SafeTileSize, SafeGridSize, KINDA_SMALL_NUMBER);
  ensureMsgf(
      bAligned,
      TEXT("FTileCoord::ToPCGGridCell expects TileSize to be an integer "
           "multiple of PCGGridSize for reversible mapping (TileSize=%.3f, "
           "PCGGridSize=%.3f)"),
      SafeTileSize, SafeGridSize);

  if (bAligned) {
    const int32 Scale = FMath::Max(1, FMath::RoundToInt(TilesPerPCGCell));
    return FIntPoint(X * Scale, Y * Scale);
  }

  // Fallback: derive cell from world-space tile origin to keep determinism
  const FVector2D TileOrigin(static_cast<float>(X) * SafeTileSize,
                             static_cast<float>(Y) * SafeTileSize);
  return FIntPoint(FMath::FloorToInt(TileOrigin.X / SafeGridSize),
                   FMath::FloorToInt(TileOrigin.Y / SafeGridSize));
}

inline FTileCoord FTileCoord::FromPCGGridCell(const FIntPoint &Cell,
                                              float PCGGridSize,
                                              float TileSize) {
  const float SafeTileSize = FMath::Max(TileSize, KINDA_SMALL_NUMBER);
  const float SafeGridSize = FMath::Max(PCGGridSize, KINDA_SMALL_NUMBER);
  const float TilesPerPCGCell = SafeTileSize / SafeGridSize;
  const bool bAligned =
      IsAlignedWithPCGGrid(SafeTileSize, SafeGridSize, KINDA_SMALL_NUMBER);
  ensureMsgf(
      bAligned,
      TEXT("FTileCoord::FromPCGGridCell expects TileSize to be an integer "
           "multiple of PCGGridSize for reversible mapping (TileSize=%.3f, "
           "PCGGridSize=%.3f)"),
      SafeTileSize, SafeGridSize);

  if (bAligned) {
    const int32 Scale = FMath::Max(1, FMath::RoundToInt(TilesPerPCGCell));
    return FTileCoord(FMath::FloorToInt(static_cast<float>(Cell.X) /
                                        static_cast<float>(Scale)),
                      FMath::FloorToInt(static_cast<float>(Cell.Y) /
                                        static_cast<float>(Scale)));
  }

  // Fallback: map PCG cell origin back into tile grid deterministically
  const FVector2D CellOrigin(static_cast<float>(Cell.X) * SafeGridSize,
                             static_cast<float>(Cell.Y) * SafeGridSize);
  return FTileCoord(FMath::FloorToInt(CellOrigin.X / SafeTileSize),
                    FMath::FloorToInt(CellOrigin.Y / SafeTileSize));
}

inline bool FTileCoord::IsAlignedWithPCGGrid(float TileSize, float PCGGridSize,
                                             float Tolerance) {
  if (TileSize <= Tolerance || PCGGridSize <= Tolerance) {
    return false;
  }

  // We only treat grids as aligned when a tile spans an integer number of PCG
  // cells to keep conversions reversible.
  const float TileToGridRatio = TileSize / PCGGridSize;
  if (TileToGridRatio < 1.0f - Tolerance) {
    return false;
  }

  const float RoundedTileToGrid = FMath::RoundToFloat(TileToGridRatio);
  return FMath::IsNearlyEqual(TileToGridRatio, RoundedTileToGrid, Tolerance);
}

/**
 * Macro world configuration (continental scale)
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FMacroWorldConfig {
  GENERATED_BODY()

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MacroWorld")
  float WorldRadiusMeters = 10000.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MacroWorld")
  float ContinentScale = 0.001f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MacroWorld")
  float IslandFalloff = 2.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MacroWorld")
  float OceanDepth = -50.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MacroWorld")
  float CoastSharpness = 1.5f;

  // Optional falloff curve for island shaping
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MacroWorld")
  TObjectPtr<UCurveFloat> IslandFalloffCurve = nullptr;
};

/**
 * Biome ring definition for radial progression
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FBiomeRingDefinition {
  GENERATED_BODY()

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biomes")
  EBiomeType BiomeType = EBiomeType::Meadows;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biomes")
  float InnerRadius = 0.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biomes")
  float OuterRadius = 1000.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biomes")
  float BlendWidth = 200.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biomes")
  float Weight = 1.0f;

  // Ring-specific modifiers
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biomes")
  float HeightInfluence = 1.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biomes")
  float ClimateInfluence = 1.0f;

  // Transition rules
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biomes")
  TArray<EBiomeType> AllowedNeighbors;
};

/**
 * Shoreline foam visual settings
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FShorelineFoamSettings {
  GENERATED_BODY()

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water")
  bool bEnable = true;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water")
  float FoamWidth = 10.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water")
  float FoamIntensity = 1.0f;
};

/**
 * Water system configuration
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FWaterSystemConfig {
  GENERATED_BODY()

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water")
  bool bEnableWaterSystem = true;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water")
  float SeaLevel = 0.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water")
  float ShorelineDetectionThreshold = 2.0f;

  // Shoreline effects
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water")
  FShorelineFoamSettings ShorelineFoam;

  // Water body spawning
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water")
  float MinWaterBodySize = 100.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water")
  int32 MaxWaterBodiesPerTile = 5;
};

/**
 * Per-tile water analysis data (mask, shoreline, distance field)
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FTileWaterData {
  GENERATED_BODY()

  // Resolution of the water data grid (matches heightfield resolution)
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water")
  int32 Resolution = 0;

  // Binary mask: 1 = water, 0 = land
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water")
  TArray<uint8> WaterMask;

  // Binary mask of shoreline samples (edge between water and land)
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water")
  TArray<uint8> ShorelineMask;

  // Distance from each sample to nearest water (in meters)
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water")
  TArray<float> DistanceToWater;
};

/**
 * Per-tile river flow data derived from heightfield gradients.
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FRiverFlowTileData {
  GENERATED_BODY()

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rivers")
  int32 Resolution = 0;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rivers")
  float CellSizeMeters = 0.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rivers")
  TArray<FVector2D> FlowDirections;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rivers")
  TArray<float> FlowAccumulation;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rivers")
  TArray<int32> DownstreamIndices;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rivers")
  float MinAccumulation = 0.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rivers")
  float MaxAccumulation = 0.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rivers")
  float AccumulationThreshold = 0.0f;

  bool IsValid() const {
    const int32 SampleCount = Resolution * Resolution;
    return Resolution > 0 && FlowDirections.Num() == SampleCount &&
           FlowAccumulation.Num() == SampleCount &&
           DownstreamIndices.Num() == SampleCount;
  }
};

/**
 * River and lake generation configuration
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FRiverSystemConfig {
  GENERATED_BODY()

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rivers")
  float MinRiverWidth = 5.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rivers")
  float MaxRiverWidth = 20.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rivers")
  float RiverBedDepth = 2.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rivers")
  float SplineSmoothing = 0.5f;

  // Lake parameters
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rivers")
  float LakeMinRadius = 50.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rivers")
  float LakeMaxRadius = 200.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rivers")
  int32 MaxLakesPerTile = 2;

  // Flow computation
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rivers")
  int32 FlowMapResolution = 32;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rivers")
  float FlowAccumulationThreshold = 10.0f;
};

/**
 * Streaming budgets configuration
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FStreamingBudgetsConfig {
  GENERATED_BODY()

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming")
  float StreamingBudgetMsPerTick = 2.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming")
  int32 PrefetchRings = 2;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming")
  int32 WorkQueueThreads = 2;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming")
  int32 MaxActiveTiles = 25;

  // Stage budgets (in milliseconds)
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming")
  float HeightGenerationBudget = 0.5f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming")
  float BiomeCalculationBudget = 0.3f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming")
  float PCGGenerationBudget = 0.8f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming")
  float VHMMeshBudget = 0.4f;
};

/**
 * PCG vegetation rule for biome-specific content generation
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FPCGVegetationRule {
  GENERATED_BODY()

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vegetation")
  TSoftObjectPtr<UStaticMesh> VegetationMesh;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vegetation")
  float Density = 1.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vegetation")
  float MinScale = 0.8f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vegetation")
  float MaxScale = 1.2f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vegetation")
  float SlopeLimit = 45.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vegetation")
  float MinHeight = -1000.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vegetation")
  float MaxHeight = 1000.0f;
};

/**
 * PCG spawn parameters for controlling biome content generation
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FPCGSpawnParams {
  GENERATED_BODY()

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCG")
  TOptional<EBiomeType> BiomeOverride;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCG")
  bool bForceBiome = false;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCG")
  float BiomeWeightScale = 1.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCG")
  float SlopeResponse = 1.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCG")
  float WaterResponse = 1.0f;

  FPCGSpawnParams() {
    BiomeOverride.Reset();
    bForceBiome = false;
    BiomeWeightScale = 1.0f;
    SlopeResponse = 1.0f;
    WaterResponse = 1.0f;
  }
};

/**
 * Operation mode to use when stamping terrain for POI placement.
 */
UENUM(BlueprintType)
enum class EPOITerrainStampMode : uint8 {
  None = 0,
  Flatten,
  Raise,
  Lower,
  Smooth
};

/**
 * Configures how terrain should be modified for a POI spawn.
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FPOITerrainStampSettings {
  GENERATED_BODY()

  /** Operation that will be applied when stamping the terrain. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI")
  EPOITerrainStampMode Operation = EPOITerrainStampMode::Flatten;

  /** Radius of the stamp in meters. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI")
  float RadiusMeters = 5.0f;

  /** Blend strength for flatten/raise/smooth operations (0..1). */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI")
  float Strength = 1.0f;

  /** Additional height to raise the center (meters) when Operation == Raise. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI")
  float RaiseHeightMeters = 1.0f;

  /** Depth to lower the terrain (meters) when Operation == Lower. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI")
  float LowerDepthMeters = 1.0f;

  /** Number of smoothing iterations when Operation == Smooth. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI")
  int32 SmoothIterations = 2;

  /** If true, stamp checks include neighboring tiles to avoid seams. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI")
  bool bAffectNeighborTiles = false;
};

/**
 * POI spawn rule configuration
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FPOISpawnRule {
  GENERATED_BODY()

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI")
  FString POIName;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI")
  TSoftObjectPtr<UBlueprint> POIBlueprint;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI")
  float SpawnChance = 0.1f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI")
  float MinDistanceFromOthers = 500.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI")
  float SlopeLimit = 30.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI")
  bool bRequiresFlatGround = true;

  /** Enforce blue-noise spacing across tiles using global reservations. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI")
  bool bEnforceGlobalSpacing = true;

  /** Optional override for world scale spacing (meters). Uses
   * MinDistanceFromOthers when <= 0. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI")
  float GlobalSpacingOverride = 0.0f;

  /** When true the system will spawn at most one POI with this rule across the
   * world. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI")
  bool bUniquePerWorld = false;

  /** Terrain stamping behaviour to apply after spawning this POI. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI")
  FPOITerrainStampSettings TerrainStampSettings;
};

/**
 * Biome definition structure containing all biome-specific parameters
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FBiomeDefinition : public FTableRowBase {
  GENERATED_BODY()

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
  EBiomeType BiomeType = EBiomeType::None;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
  FString BiomeName;

  // Terrain properties
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
  float BaseHeight = 0.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
  float HeightVariation = 100.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
  TSoftObjectPtr<UCurveFloat> HeightCurve;

  // PCG rules
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCG")
  TSoftObjectPtr<UPCGGraph> BiomePCGGraph;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCG")
  TArray<FPCGVegetationRule> VegetationRules;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCG")
  TArray<FPOISpawnRule> POIRules;

  // Visual properties
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
  TSoftObjectPtr<UMaterialInterface> TerrainMaterial;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
  FLinearColor RVTBlendColor = FLinearColor::White;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
  TSoftObjectPtr<UTexture2D> BiomeMask;

  // Climate requirements
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climate")
  float MinTemperature = -50.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climate")
  float MaxTemperature = 50.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climate")
  float MinMoisture = 0.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climate")
  float MaxMoisture = 1.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climate")
  float BiomeWeight = 1.0f;
};

/**
 * Heightfield modification data for persistence
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FHeightfieldModification {
  GENERATED_BODY()

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modification")
  FVector2D Center = FVector2D::ZeroVector;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modification")
  float Radius = 100.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modification")
  float Strength = 1.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modification")
  EHeightfieldOperation Operation = EHeightfieldOperation::Add;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modification")
  FDateTime Timestamp;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modification")
  FTileCoord AffectedTile;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modification")
  FGuid ModificationId = FGuid();

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modification")
  int32 Order = 0;

  // Derived parameters persisted for bit-for-bit determinism
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modification")
  int32 KernelRadius = 0;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modification")
  float FlattenTargetZ = 0.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modification")
  bool bFlattenUsesTarget = false;

  FHeightfieldModification() {
    ModificationId = FGuid::NewGuid();
    Timestamp = FDateTime::Now();
    ensureMsgf(ModificationId.IsValid(),
               TEXT("FHeightfieldModification: ModificationId must be valid "
                    "after construction"));
  }
};

// TStructOpsTypeTraits for FHeightfieldModification
// WithZeroConstructor = true: Uses in-class FGuid() initializer for UE5.6
// reflection compatibility
template <>
struct TStructOpsTypeTraits<FHeightfieldModification>
    : public TStructOpsTypeTraitsBase2<FHeightfieldModification> {
  enum {
    WithZeroConstructor =
        true, // Uses in-class FGuid() initializer for reflection compatibility
  };
};

/**
 * POI data structure for point of interest placement
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FPOIData {
  GENERATED_BODY()

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI")
  FGuid POIId = FGuid();

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI")
  FString POIName;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI")
  FVector Location = FVector::ZeroVector;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI")
  FRotator Rotation = FRotator::ZeroRotator;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI")
  FVector Scale = FVector::OneVector;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI")
  TSoftObjectPtr<UBlueprint> POIBlueprint;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI")
  EBiomeType OriginBiome = EBiomeType::None;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI")
  bool bIsSpawned = false;

  FPOIData() {
    POIId = FGuid::NewGuid();
    ensureMsgf(POIId.IsValid(),
               TEXT("FPOIData: POIId must be valid after construction"));
  }

  // Custom serialization for archive compatibility
  bool Serialize(FArchive &Ar) {
    Ar << POIId;
    Ar << POIName;
    Ar << Location;
    Ar << Rotation;
    Ar << Scale;

    // Serialize soft object pointer as string
    if (Ar.IsLoading()) {
      FString POIBlueprintPath;
      Ar << POIBlueprintPath;
      POIBlueprint =
          TSoftObjectPtr<UBlueprint>(FSoftObjectPath(POIBlueprintPath));
    } else {
      FString POIBlueprintPath = POIBlueprint.GetLongPackageName();
      Ar << POIBlueprintPath;
    }

    uint8 BiomeTypeValue = static_cast<uint8>(OriginBiome);
    Ar << BiomeTypeValue;
    if (Ar.IsLoading()) {
      OriginBiome = static_cast<EBiomeType>(BiomeTypeValue);
    }

    Ar << bIsSpawned;

    return true;
  }

  friend FArchive &operator<<(FArchive &Ar, FPOIData &POIData) {
    POIData.Serialize(Ar);
    return Ar;
  }
};

// TStructOpsTypeTraits for FPOIData - Uses in-class FGuid() initializer for
// UE5.6 reflection compatibility
template <>
struct TStructOpsTypeTraits<FPOIData>
    : public TStructOpsTypeTraitsBase2<FPOIData> {
  enum {
    WithZeroConstructor =
        true, // Uses in-class FGuid() initializer for reflection compatibility
    WithSerializer = true // Has custom Serialize() method
  };
};

/**
 * PCG instance data for tracking spawned content
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FPCGInstanceData {
  GENERATED_BODY()

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance")
  FGuid InstanceId = FGuid();

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance")
  FVector Location = FVector::ZeroVector;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance")
  FRotator Rotation = FRotator::ZeroRotator;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance")
  FVector Scale = FVector::OneVector;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance")
  TSoftObjectPtr<UStaticMesh> Mesh;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance")
  bool bIsActive = true;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance")
  FTileCoord OwningTile;

  FPCGInstanceData() {
    InstanceId = FGuid::NewGuid();
    ensureMsgf(
        InstanceId.IsValid(),
        TEXT("FPCGInstanceData: InstanceId must be valid after construction"));
  }

  // Custom serialization for archive compatibility
  bool Serialize(FArchive &Ar) {
    Ar << InstanceId;
    Ar << Location;
    Ar << Rotation;
    Ar << Scale;

    // Serialize soft object pointer as string
    if (Ar.IsLoading()) {
      FString MeshPath;
      Ar << MeshPath;
      Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(MeshPath));
    } else {
      FString MeshPath = Mesh.GetLongPackageName();
      Ar << MeshPath;
    }

    Ar << bIsActive;
    OwningTile.Serialize(Ar);

    return true;
  }
};

// TStructOpsTypeTraits for FPCGInstanceData
// WithZeroConstructor = true: Uses in-class FGuid() initializer for UE5.6
// reflection compatibility
template <>
struct TStructOpsTypeTraits<FPCGInstanceData>
    : public TStructOpsTypeTraitsBase2<FPCGInstanceData> {
  enum {
    WithZeroConstructor =
        true, // Uses in-class FGuid() initializer for reflection compatibility
    WithSerializer = true // Has custom Serialize() method
  };
};

/**
 * Performance statistics for PCG generation
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FPCGPerformanceStats {
  GENERATED_BODY()

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
  float AverageGenerationTimeMs = 0.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
  float LastGenerationTimeMs = 0.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
  int32 TotalInstancesGenerated = 0;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
  int32 ActiveHISMInstances = 0;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
  float MemoryUsageMB = 0.0f;
};

/**
 * Streaming performance statistics
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FStreamingStats {
  GENERATED_BODY()

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming")
  int32 LoadedCells = 0;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming")
  int32 PendingLoads = 0;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming")
  int32 PendingUnloads = 0;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming")
  float AverageLoadTimeMs = 0.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming")
  float StreamingMemoryUsageMB = 0.0f;
};
