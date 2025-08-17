#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Data/WorldGenTypes.h"
#include "IHeightfieldService.generated.h"

/**
 * Heightfield data structure containing height, normal, and slope information
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FHeightfieldData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heightfield")
	TArray<float> HeightData; // Height values in world units

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heightfield")
	TArray<FVector> NormalData; // Surface normals

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heightfield")
	TArray<float> SlopeData; // Slope angles in degrees

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heightfield")
	int32 Resolution = 64; // Samples per side (64x64 for a tile)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heightfield")
	FTileCoord TileCoord; // Which tile this data represents

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heightfield")
	float MinHeight = 0.0f; // Minimum height in this data

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heightfield")
	float MaxHeight = 0.0f; // Maximum height in this data

	FHeightfieldData()
	{
		Resolution = 64;
		MinHeight = 0.0f;
		MaxHeight = 0.0f;
	}

	/**
	 * Get height at specific sample coordinates (0-63 for a 64x64 tile)
	 */
	float GetHeightAtSample(int32 X, int32 Y) const
	{
		if (X >= 0 && X < Resolution && Y >= 0 && Y < Resolution)
		{
			int32 Index = Y * Resolution + X;
			if (HeightData.IsValidIndex(Index))
			{
				return HeightData[Index];
			}
		}
		return 0.0f;
	}

	/**
	 * Get normal at specific sample coordinates
	 */
	FVector GetNormalAtSample(int32 X, int32 Y) const
	{
		if (X >= 0 && X < Resolution && Y >= 0 && Y < Resolution)
		{
			int32 Index = Y * Resolution + X;
			if (NormalData.IsValidIndex(Index))
			{
				return NormalData[Index];
			}
		}
		return FVector::UpVector;
	}

	/**
	 * Get slope at specific sample coordinates
	 */
	float GetSlopeAtSample(int32 X, int32 Y) const
	{
		if (X >= 0 && X < Resolution && Y >= 0 && Y < Resolution)
		{
			int32 Index = Y * Resolution + X;
			if (SlopeData.IsValidIndex(Index))
			{
				return SlopeData[Index];
			}
		}
		return 0.0f;
	}
};

UINTERFACE(MinimalAPI, Blueprintable)
class UHeightfieldServiceInterface : public UInterface
{
        GENERATED_BODY()
};

/**
 * Interface for heightfield generation and management services
 * Handles terrain height generation, modification, and rendering integration
 */
class VIBEHEIM_API IHeightfieldServiceInterface
{
        GENERATED_BODY()

public:
	/**
	 * Initialize the heightfield service with world generation settings
	 */
	virtual bool Initialize(const FWorldGenConfig& Settings) = 0;

	/**
	 * Generate heightfield data for a specific tile
	 */
	virtual FHeightfieldData GenerateHeightfield(uint64 Seed, FTileCoord TileCoord) = 0;

	/**
	 * Modify heightfield at a specific world location
	 */
	virtual bool ModifyHeightfield(FVector Location, float Radius, float Strength, EHeightfieldOperation Operation) = 0;

	/**
	 * Get height at a specific world position (interpolated)
	 */
	virtual float GetHeightAtLocation(FVector2D WorldPos) = 0;

	/**
	 * Get normal at a specific world position (interpolated)
	 */
	virtual FVector GetNormalAtLocation(FVector2D WorldPos) = 0;

	/**
	 * Get slope at a specific world position (interpolated)
	 */
	virtual float GetSlopeAtLocation(FVector2D WorldPos) = 0;

	/**
	 * Save heightfield modifications to persistent storage
	 */
	virtual bool SaveHeightfieldModifications() = 0;

	/**
	 * Load heightfield modifications from persistent storage
	 */
	virtual bool LoadHeightfieldModifications() = 0;

	/**
	 * Upload heightfield data to GPU for rendering (VHM integration)
	 */
	virtual bool UploadHeightfieldToGPU(const FHeightfieldData& HeightfieldData) = 0;

	/**
	 * Apply thermal smoothing to heightfield data
	 */
	virtual void ApplyThermalSmoothing(FHeightfieldData& HeightfieldData, int32 Iterations = 1) = 0;

	/**
	 * Calculate normals and slopes from height data
	 */
	virtual void CalculateNormalsAndSlopes(FHeightfieldData& HeightfieldData) = 0;

	/**
	 * Export heightfield data as PNG for debugging
	 */
	virtual bool ExportHeightfieldPNG(const FHeightfieldData& HeightfieldData, const FString& OutputPath) = 0;

	/**
	 * Get cached heightfield data for a tile (if available)
	 */
	virtual bool GetCachedHeightfield(FTileCoord TileCoord, FHeightfieldData& OutHeightfieldData) = 0;

	/**
	 * Cache heightfield data for a tile
	 */
	virtual void CacheHeightfield(const FHeightfieldData& HeightfieldData) = 0;

	/**
	 * Clear heightfield cache for memory management
	 */
	virtual void ClearHeightfieldCache() = 0;

	/**
	 * Get performance statistics for heightfield generation
	 */
        virtual void GetPerformanceStats(float& OutAverageGenerationTimeMs, int32& OutCachedTiles) = 0;
};
