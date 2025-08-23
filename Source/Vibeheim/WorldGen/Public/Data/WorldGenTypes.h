#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Materials/MaterialInterface.h"
// Include PCG headers if available
#if WITH_PCG
#include "PCGGraph.h"
#endif
#include "Curves/CurveFloat.h"
#include "WorldGenTypes.generated.h"

// Forward declarations
class UTexture2D;
class UMaterialInterface;
// Forward declare PCG classes (optional dependency)
class UPCGGraph;
class UCurveFloat;

/**
 * Tile coordinate structure for world partitioning
 * Uses 64-meter tiles with world-space sampling
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FTileCoord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile")
	int32 X = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile")
	int32 Y = 0;

	FTileCoord() = default;
	FTileCoord(int32 InX, int32 InY) : X(InX), Y(InY) {}
	FTileCoord(FIntVector2 InCoord) : X(InCoord.X), Y(InCoord.Y) {}

	// Convert world position to tile coordinate
	static FTileCoord FromWorldPosition(FVector WorldPos, float TileSize = 64.0f)
	{
		return FTileCoord(
			FMath::FloorToInt(WorldPos.X / TileSize),
			FMath::FloorToInt(WorldPos.Y / TileSize)
		);
	}

	// Convert tile coordinate to world position (center of tile)
	FVector ToWorldPosition(float TileSize = 64.0f) const
	{
		return FVector(
			(X + 0.5f) * TileSize,
			(Y + 0.5f) * TileSize,
			0.0f
		);
	}

	// Hash function for use in TMap
	friend uint32 GetTypeHash(const FTileCoord& Coord)
	{
		return HashCombine(GetTypeHash(Coord.X), GetTypeHash(Coord.Y));
	}

	bool operator==(const FTileCoord& Other) const
	{
		return X == Other.X && Y == Other.Y;
	}

	bool operator!=(const FTileCoord& Other) const
	{
		return !(*this == Other);
	}

	FIntVector2 ToIntVector2() const
	{
		return FIntVector2(X, Y);
	}

	// Custom serialization for archive compatibility
	bool Serialize(FArchive& Ar)
	{
		Ar << X;
		Ar << Y;
		return true;
	}

	friend FArchive& operator<<(FArchive& Ar, FTileCoord& TileCoord)
	{
		TileCoord.Serialize(Ar);
		return Ar;
	}
};

/**
 * Biome types supported by the world generation system
 */
UENUM(BlueprintType)
enum class EBiomeType : uint8
{
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
enum class EHeightfieldOperation : uint8
{
	Add = 0,
	Subtract = 1,
	Flatten = 2,
	Smooth = 3,
	MAX UMETA(Hidden)
};

/**
 * Core world generation configuration structure
 * Defines all parameters for deterministic world generation
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FWorldGenConfig
{
	GENERATED_BODY()

	// Core generation parameters
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	int32 Seed = 1337;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	int32 WorldGenVersion = 1;

	// Tile and coordinate system (locked values)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coordinates", meta = (ClampMin = "64", ClampMax = "64"))
	float TileSizeMeters = 64.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coordinates", meta = (ClampMin = "1", ClampMax = "1"))
	float SampleSpacingMeters = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coordinates", meta = (ClampMin = "120", ClampMax = "120"))
	float MaxTerrainHeight = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coordinates")
	float SeaLevel = 0.0f;

	// Streaming radii (in tiles)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming", meta = (ClampMin = "1", ClampMax = "20"))
	int32 GenerateRadius = 9;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming", meta = (ClampMin = "1", ClampMax = "15"))
	int32 LoadRadius = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming", meta = (ClampMin = "1", ClampMax = "10"))
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

	FWorldGenConfig()
	{
		// Ensure locked values are set correctly
		TileSizeMeters = 64.0f;
		SampleSpacingMeters = 1.0f;
		MaxTerrainHeight = 120.0f;
	}
};/**

 * PCG vegetation rule for biome-specific content generation
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FPCGVegetationRule
{
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
 * POI spawn rule configuration
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FPOISpawnRule
{
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
};

/**
 * Biome definition structure containing all biome-specific parameters
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FBiomeDefinition : public FTableRowBase
{
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
struct VIBEHEIM_API FHeightfieldModification
{
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
	FGuid ModificationId = FGuid::NewGuid();

	FHeightfieldModification()
	{
		Timestamp = FDateTime::Now();
		// Ensure we have a valid GUID (in-class initializer should handle this)
		if (!ModificationId.IsValid())
		{
			ModificationId = FGuid::NewGuid();
		}
	}
};

// TStructOpsTypeTraits for FHeightfieldModification
// WithZeroConstructor = false: Uses FGuid::NewGuid() in member initializer, not zero-constructible
template<>
struct TStructOpsTypeTraits<FHeightfieldModification> : public TStructOpsTypeTraitsBase2<FHeightfieldModification>
{
	enum
	{
		WithZeroConstructor = false,      // Uses NewGuid() for unique IDs, not zero-initialized
	};
};

/**
 * POI data structure for point of interest placement
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FPOIData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI")
	FGuid POIId = FGuid::NewGuid();

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

	FPOIData()
	{
		// Validate POI ID is properly initialized (in-class initializer should handle this)
		if (!POIId.IsValid())
		{
			POIId = FGuid::NewGuid();
		}
		ensureMsgf(POIId.IsValid(), TEXT("FPOIData: POIId must be valid after construction"));
	}

	// Custom serialization for archive compatibility
	bool Serialize(FArchive& Ar)
	{
		Ar << POIId;
		Ar << POIName;
		Ar << Location;
		Ar << Rotation;
		Ar << Scale;
		
		// Serialize soft object pointer as string
		if (Ar.IsLoading())
		{
			FString POIBlueprintPath;
			Ar << POIBlueprintPath;
			POIBlueprint = TSoftObjectPtr<UBlueprint>(FSoftObjectPath(POIBlueprintPath));
		}
		else
		{
			FString POIBlueprintPath = POIBlueprint.GetLongPackageName();
			Ar << POIBlueprintPath;
		}
		
		uint8 BiomeTypeValue = static_cast<uint8>(OriginBiome);
		Ar << BiomeTypeValue;
		if (Ar.IsLoading())
		{
			OriginBiome = static_cast<EBiomeType>(BiomeTypeValue);
		}
		
		Ar << bIsSpawned;
		
		return true;
	}

	friend FArchive& operator<<(FArchive& Ar, FPOIData& POIData)
	{
		POIData.Serialize(Ar);
		return Ar;
	}
};

// TStructOpsTypeTraits for FPOIData - Persistent ID pattern uses NewGuid() in member initializer
template<>
struct TStructOpsTypeTraits<FPOIData> : public TStructOpsTypeTraitsBase2<FPOIData>
{
	enum
	{
		WithZeroConstructor = false,      // Uses NewGuid() for unique IDs, not zero-initialized
		WithSerializer = true             // Has custom Serialize() method
	};
};

/**
 * PCG instance data for tracking spawned content
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FPCGInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance")
	FGuid InstanceId = FGuid::NewGuid();

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

	FPCGInstanceData()
	{
		// Validate Instance ID is properly initialized (in-class initializer should handle this)
		if (!InstanceId.IsValid())
		{
			InstanceId = FGuid::NewGuid();
		}
		ensureMsgf(InstanceId.IsValid(), TEXT("FPCGInstanceData: InstanceId must be valid after construction"));
	}

	// Custom serialization for archive compatibility
	bool Serialize(FArchive& Ar)
	{
		Ar << InstanceId;
		Ar << Location;
		Ar << Rotation;
		Ar << Scale;
		
		// Serialize soft object pointer as string
		if (Ar.IsLoading())
		{
			FString MeshPath;
			Ar << MeshPath;
			Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(MeshPath));
		}
		else
		{
			FString MeshPath = Mesh.GetLongPackageName();
			Ar << MeshPath;
		}
		
		Ar << bIsActive;
		Ar << OwningTile;
		
		return true;
	}

	friend FArchive& operator<<(FArchive& Ar, FPCGInstanceData& InstanceData)
	{
		InstanceData.Serialize(Ar);
		return Ar;
	}
};

// TStructOpsTypeTraits for FPCGInstanceData
// WithZeroConstructor = false: Uses FGuid::NewGuid() in member initializer, not zero-constructible
template<>
struct TStructOpsTypeTraits<FPCGInstanceData> : public TStructOpsTypeTraitsBase2<FPCGInstanceData>
{
	enum
	{
		WithZeroConstructor = false,      // Uses NewGuid() for unique IDs, not zero-initialized
		WithSerializer = true             // Has custom Serialize() method
	};
};

/**
 * Performance statistics for PCG generation
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FPCGPerformanceStats
{
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
struct VIBEHEIM_API FStreamingStats
{
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
