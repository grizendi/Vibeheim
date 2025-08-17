#pragma once

#include "CoreMinimal.h"
#include "Data/WorldGenTypes.h"
#include "HashUtils.generated.h"

/**
 * Tile checksum data for integrity validation
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FTileChecksum
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Checksum")
	FTileCoord TileCoord;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Checksum")
	int32 HeightDataChecksum = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Checksum")
	int32 NormalDataChecksum = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Checksum")
	int32 SlopeDataChecksum = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Checksum")
	int32 CombinedChecksum = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Checksum")
	int32 GenerationSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Checksum")
	int32 WorldGenVersion = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Checksum")
	FDateTime GenerationTime;

	FTileChecksum()
	{
		TileCoord = FTileCoord(0, 0);
		HeightDataChecksum = 0;
		NormalDataChecksum = 0;
		SlopeDataChecksum = 0;
		CombinedChecksum = 0;
		GenerationSeed = 0;
		WorldGenVersion = 1;
		GenerationTime = FDateTime::Now();
	}

	bool operator==(const FTileChecksum& Other) const
	{
		return TileCoord == Other.TileCoord && 
			   CombinedChecksum == Other.CombinedChecksum &&
			   GenerationSeed == Other.GenerationSeed &&
			   WorldGenVersion == Other.WorldGenVersion;
	}

	bool operator!=(const FTileChecksum& Other) const
	{
		return !(*this == Other);
	}
};

/**
 * Hash utility class for deterministic world generation
 * Provides xxHash64 implementation and checksum validation
 */
UCLASS(BlueprintType)
class VIBEHEIM_API UHashUtils : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Calculate xxHash64 for a byte array
	 */
	UFUNCTION(BlueprintCallable, Category = "Hash")
	static int32 CalculateXXHash64(const TArray<uint8>& Data, int32 Seed);

	/**
	 * Calculate xxHash64 for float array (heightfield data)
	 */
	UFUNCTION(BlueprintCallable, Category = "Hash")
	static int32 CalculateFloatArrayHash(const TArray<float>& FloatData, int32 Seed);

	/**
	 * Calculate xxHash64 for vector array (normal data)
	 */
	UFUNCTION(BlueprintCallable, Category = "Hash")
	static int32 CalculateVectorArrayHash(const TArray<FVector>& VectorData, int32 Seed);
	
	/**
	 * Calculate checksum for heightfield data
	 */
	UFUNCTION(BlueprintCallable, Category = "Hash")
	static FTileChecksum CalculateHeightfieldChecksum(const struct FHeightfieldData& HeightfieldData, int32 GenerationSeed, int32 WorldGenVersion);

	/**
	 * Validate heightfield checksum integrity
	 */
	UFUNCTION(BlueprintCallable, Category = "Hash")
	static bool ValidateHeightfieldChecksum(const struct FHeightfieldData& HeightfieldData, const FTileChecksum& ExpectedChecksum);

	/**
	 * Generate deterministic seed for tile coordinate
	 */
	UFUNCTION(BlueprintCallable, Category = "Hash")
	static int32 GenerateTileSeed(int32 BaseSeed, FTileCoord TileCoord, int32 LayerType);
	
	/**
	 * Generate deterministic seed for PCG content
	 */
	UFUNCTION(BlueprintCallable, Category = "Hash")
	static int32 GeneratePCGSeed(int32 BaseSeed, FTileCoord TileCoord, int32 PrototypeId, int32 Index);

	/**
	 * Hash two 32-bit integers into single 64-bit hash
	 */
	UFUNCTION(BlueprintCallable, Category = "Hash")
	static int32 HashCoordinates(int32 X, int32 Y, int32 Seed);

	/**
	 * Fast hash for coordinates (for use in tight loops)
	 */
	static FORCEINLINE uint32 FastHashCoordinates(int32 X, int32 Y, uint32 Seed = 0)
	{
		// Simple multiplicative hash for coordinates
		uint32 Hash = static_cast<uint32>(X * 73856093) ^ static_cast<uint32>(Y * 19349663) ^ Seed;
		Hash ^= Hash >> 16;
		Hash *= 0x85ebca6b;
		Hash ^= Hash >> 13;
		Hash *= 0xc2b2ae35;
		Hash ^= Hash >> 16;
		return Hash;
	}

	/**
	 * Check if two tiles should have seamless borders
	 */
	UFUNCTION(BlueprintCallable, Category = "Hash")
	static bool ValidateTileBorderSeam(const struct FHeightfieldData& Tile1, const struct FHeightfieldData& Tile2);

private:
	/**
	 * Internal xxHash64 implementation
	 */
	static int32 XXHash64Internal(const void* Data, size_t Length, int32 Seed);

	/**
	 * xxHash64 constants
	 */
	static constexpr int32 PRIME64_1 = 0x9E3779B185EBCA87ULL;
	static constexpr int32 PRIME64_2 = 0xC2B2AE3D27D4EB4FULL;
	static constexpr int32 PRIME64_3 = 0x165667B19E3779F9ULL;
	static constexpr int32 PRIME64_4 = 0x85EBCA77C2B2AE63ULL;
	static constexpr int32 PRIME64_5 = 0x27D4EB2F165667C5ULL;

	/**
	 * Rotate left operation for xxHash64
	 */
	static FORCEINLINE int32 RotateLeft64(int32 Value, int32 Amount)
	{
		return (Value << Amount) | (Value >> (64 - Amount));
	}
};
