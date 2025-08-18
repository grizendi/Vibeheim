#include "Utils/HashUtils.h"
#include "Services/IHeightfieldService.h"
#include "Misc/DateTime.h"

int32 UHashUtils::CalculateXXHash64(const TArray<uint8>& Data, int32 Seed)
{
	return CalculateXXHash64(Data, 0);
}

int32 UHashUtils::CalculateFloatArrayHash(const TArray<float>& FloatData, int32 Seed)
{
	return CalculateFloatArrayHash(FloatData, 0);
}

int32 UHashUtils::CalculateVectorArrayHash(const TArray<FVector>& VectorData, int32 Seed)
{
	return CalculateVectorArrayHash(VectorData, 0);
}

int32 UHashUtils::GenerateTileSeed(int32 BaseSeed, FTileCoord TileCoord, int32 LayerType)
{
	return GenerateTileSeed(BaseSeed, TileCoord, 0);
}

int32 UHashUtils::HashCoordinates(int32 X, int32 Y, int32 Seed)
{
	return HashCoordinates(X, Y, 0);
}

int32 UHashUtils::CalculateXXHash64(const TArray<uint8>& Data, int32 Seed)
{
	if (Data.Num() == 0)
	{
		return Seed;
	}
	return XXHash64Internal(Data.GetData(), Data.Num(), Seed);
}

int32 UHashUtils::CalculateFloatArrayHash(const TArray<float>& FloatData, int32 Seed)
{
	if (FloatData.Num() == 0)
	{
		return Seed;
	}
	return XXHash64Internal(FloatData.GetData(), FloatData.Num() * sizeof(float), Seed);
}

int32 UHashUtils::CalculateVectorArrayHash(const TArray<FVector>& VectorData, int32 Seed)
{
	if (VectorData.Num() == 0)
	{
		return Seed;
	}
	return XXHash64Internal(VectorData.GetData(), VectorData.Num() * sizeof(FVector), Seed);
}

FTileChecksum UHashUtils::CalculateHeightfieldChecksum(const FHeightfieldData& HeightfieldData, int32 GenerationSeed, int32 WorldGenVersion)
{
	FTileChecksum Checksum;
	Checksum.TileCoord = HeightfieldData.TileCoord;
	Checksum.GenerationSeed = GenerationSeed;
	Checksum.WorldGenVersion = WorldGenVersion;
	Checksum.GenerationTime = FDateTime::Now();

	// Calculate individual checksums
	Checksum.HeightDataChecksum = CalculateFloatArrayHash(HeightfieldData.HeightData, GenerationSeed);
	Checksum.NormalDataChecksum = CalculateVectorArrayHash(HeightfieldData.NormalData, GenerationSeed);
	Checksum.SlopeDataChecksum = CalculateFloatArrayHash(HeightfieldData.SlopeData, GenerationSeed);

	// Calculate combined checksum including metadata
	TArray<uint8> CombinedData;
	CombinedData.Append(reinterpret_cast<const uint8*>(&Checksum.HeightDataChecksum), sizeof(int32));
	CombinedData.Append(reinterpret_cast<const uint8*>(&Checksum.NormalDataChecksum), sizeof(int32));
	CombinedData.Append(reinterpret_cast<const uint8*>(&Checksum.SlopeDataChecksum), sizeof(int32));
	CombinedData.Append(reinterpret_cast<const uint8*>(&HeightfieldData.TileCoord.X), sizeof(int32));
	CombinedData.Append(reinterpret_cast<const uint8*>(&HeightfieldData.TileCoord.Y), sizeof(int32));
	CombinedData.Append(reinterpret_cast<const uint8*>(&HeightfieldData.Resolution), sizeof(int32));

	Checksum.CombinedChecksum = CalculateXXHash64(CombinedData, GenerationSeed);

	return Checksum;
}

bool UHashUtils::ValidateHeightfieldChecksum(const FHeightfieldData& HeightfieldData, const FTileChecksum& ExpectedChecksum)
{
	// Calculate current checksum for comparison
	FTileChecksum CurrentChecksum = CalculateHeightfieldChecksum(
		HeightfieldData, 
		ExpectedChecksum.GenerationSeed, 
		ExpectedChecksum.WorldGenVersion
	);

	// Compare critical checksums
	bool bValid = (CurrentChecksum.HeightDataChecksum == ExpectedChecksum.HeightDataChecksum) &&
				  (CurrentChecksum.NormalDataChecksum == ExpectedChecksum.NormalDataChecksum) &&
				  (CurrentChecksum.SlopeDataChecksum == ExpectedChecksum.SlopeDataChecksum) &&
				  (CurrentChecksum.CombinedChecksum == ExpectedChecksum.CombinedChecksum) &&
				  (CurrentChecksum.TileCoord == ExpectedChecksum.TileCoord);

	return bValid;
}

int32 UHashUtils::GenerateTileSeed(int32 BaseSeed, FTileCoord TileCoord, int32 LayerType)
{
	// Combine base seed with tile coordinates and layer type
	TArray<uint8> SeedData;
	SeedData.Append(reinterpret_cast<const uint8*>(&BaseSeed), sizeof(int32));
	SeedData.Append(reinterpret_cast<const uint8*>(&TileCoord.X), sizeof(int32));
	SeedData.Append(reinterpret_cast<const uint8*>(&TileCoord.Y), sizeof(int32));
	SeedData.Append(reinterpret_cast<const uint8*>(&LayerType), sizeof(uint32));

	return CalculateXXHash64(SeedData, BaseSeed);
}

int32 UHashUtils::GeneratePCGSeed(int32 BaseSeed, FTileCoord TileCoord, int32 PrototypeId, int32 Index)
{
	// Generate deterministic seed for PCG content: (Seed, TileCoord, PrototypeId, Index) → PCG seed
	TArray<uint8> SeedData;
	SeedData.Append(reinterpret_cast<const uint8*>(&BaseSeed), sizeof(int32));
	SeedData.Append(reinterpret_cast<const uint8*>(&TileCoord.X), sizeof(int32));
	SeedData.Append(reinterpret_cast<const uint8*>(&TileCoord.Y), sizeof(int32));
	SeedData.Append(reinterpret_cast<const uint8*>(&PrototypeId), sizeof(int32));
	SeedData.Append(reinterpret_cast<const uint8*>(&Index), sizeof(int32));

	return CalculateXXHash64(SeedData, BaseSeed);
}

int32 UHashUtils::HashCoordinates(int32 X, int32 Y, int32 Seed)
{
	TArray<uint8> CoordData;
	CoordData.Append(reinterpret_cast<const uint8*>(&X), sizeof(int32));
	CoordData.Append(reinterpret_cast<const uint8*>(&Y), sizeof(int32));
	
	return CalculateXXHash64(CoordData, Seed);
}

bool UHashUtils::ValidateTileBorderSeam(const FHeightfieldData& Tile1, const FHeightfieldData& Tile2)
{
	if (Tile1.Resolution != Tile2.Resolution)
	{
		return false;
	}

	// Determine which edge to compare based on tile coordinates
	FTileCoord Diff = FTileCoord(Tile2.TileCoord.X - Tile1.TileCoord.X, Tile2.TileCoord.Y - Tile1.TileCoord.Y);
	
	int32 Resolution = Tile1.Resolution;
	float Tolerance = 0.001f; // Small tolerance for floating point comparison

	if (Diff.X == 1 && Diff.Y == 0) // Tile2 is to the right of Tile1
	{
		// Compare right edge of Tile1 with left edge of Tile2
		for (int32 Y = 0; Y < Resolution; Y++)
		{
			float Height1 = Tile1.GetHeightAtSample(Resolution - 1, Y);
			float Height2 = Tile2.GetHeightAtSample(0, Y);
			
			if (FMath::Abs(Height1 - Height2) > Tolerance)
			{
				return false;
			}
		}
		return true;
	}
	else if (Diff.X == -1 && Diff.Y == 0) // Tile2 is to the left of Tile1
	{
		// Compare left edge of Tile1 with right edge of Tile2
		for (int32 Y = 0; Y < Resolution; Y++)
		{
			float Height1 = Tile1.GetHeightAtSample(0, Y);
			float Height2 = Tile2.GetHeightAtSample(Resolution - 1, Y);
			
			if (FMath::Abs(Height1 - Height2) > Tolerance)
			{
				return false;
			}
		}
		return true;
	}
	else if (Diff.X == 0 && Diff.Y == 1) // Tile2 is above Tile1
	{
		// Compare top edge of Tile1 with bottom edge of Tile2
		for (int32 X = 0; X < Resolution; X++)
		{
			float Height1 = Tile1.GetHeightAtSample(X, Resolution - 1);
			float Height2 = Tile2.GetHeightAtSample(X, 0);
			
			if (FMath::Abs(Height1 - Height2) > Tolerance)
			{
				return false;
			}
		}
		return true;
	}
	else if (Diff.X == 0 && Diff.Y == -1) // Tile2 is below Tile1
	{
		// Compare bottom edge of Tile1 with top edge of Tile2
		for (int32 X = 0; X < Resolution; X++)
		{
			float Height1 = Tile1.GetHeightAtSample(X, 0);
			float Height2 = Tile2.GetHeightAtSample(X, Resolution - 1);
			
			if (FMath::Abs(Height1 - Height2) > Tolerance)
			{
				return false;
			}
		}
		return true;
	}

	// Tiles are not adjacent
	return false;
}

int32 UHashUtils::XXHash64Internal(const void* Data, size_t Length, int32 Seed)
{
	const uint8* ByteData = static_cast<const uint8*>(Data);
	int32 Hash;

	if (Length >= 32)
	{
		const uint8* const End = ByteData + Length;
		const uint8* const Limit = End - 32;

		int32 V1 = Seed + PRIME64_1 + PRIME64_2;
		int32 V2 = Seed + PRIME64_2;
		int32 V3 = Seed + 0;
		int32 V4 = Seed - PRIME64_1;

		do
		{
			V1 = RotateLeft64(V1 + (*reinterpret_cast<const int32*>(ByteData) * PRIME64_2), 31) * PRIME64_1;
			ByteData += 8;
			V2 = RotateLeft64(V2 + (*reinterpret_cast<const int32*>(ByteData) * PRIME64_2), 31) * PRIME64_1;
			ByteData += 8;
			V3 = RotateLeft64(V3 + (*reinterpret_cast<const int32*>(ByteData) * PRIME64_2), 31) * PRIME64_1;
			ByteData += 8;
			V4 = RotateLeft64(V4 + (*reinterpret_cast<const int32*>(ByteData) * PRIME64_2), 31) * PRIME64_1;
			ByteData += 8;
		} while (ByteData <= Limit);

		Hash = RotateLeft64(V1, 1) + RotateLeft64(V2, 7) + RotateLeft64(V3, 12) + RotateLeft64(V4, 18);

		V1 *= PRIME64_2; V1 = RotateLeft64(V1, 31); V1 *= PRIME64_1; Hash ^= V1; Hash = Hash * PRIME64_1 + PRIME64_4;
		V2 *= PRIME64_2; V2 = RotateLeft64(V2, 31); V2 *= PRIME64_1; Hash ^= V2; Hash = Hash * PRIME64_1 + PRIME64_4;
		V3 *= PRIME64_2; V3 = RotateLeft64(V3, 31); V3 *= PRIME64_1; Hash ^= V3; Hash = Hash * PRIME64_1 + PRIME64_4;
		V4 *= PRIME64_2; V4 = RotateLeft64(V4, 31); V4 *= PRIME64_1; Hash ^= V4; Hash = Hash * PRIME64_1 + PRIME64_4;
	}
	else
	{
		Hash = Seed + PRIME64_5;
	}

	Hash += static_cast<int32>(Length);

	const uint8* const End = ByteData + (Length & 31);
	while (ByteData + 8 <= End)
	{
		int32 K1 = *reinterpret_cast<const int32*>(ByteData);
		K1 *= PRIME64_2;
		K1 = RotateLeft64(K1, 31);
		K1 *= PRIME64_1;
		Hash ^= K1;
		Hash = RotateLeft64(Hash, 27) * PRIME64_1 + PRIME64_4;
		ByteData += 8;
	}

	if (ByteData + 4 <= End)
	{
		Hash ^= static_cast<int32>(*reinterpret_cast<const uint32*>(ByteData)) * PRIME64_1;
		Hash = RotateLeft64(Hash, 23) * PRIME64_2 + PRIME64_3;
		ByteData += 4;
	}

	while (ByteData < End)
	{
		Hash ^= (*ByteData) * PRIME64_5;
		Hash = RotateLeft64(Hash, 11) * PRIME64_1;
		ByteData++;
	}

	Hash ^= Hash >> 33;
	Hash *= PRIME64_2;
	Hash ^= Hash >> 29;
	Hash *= PRIME64_3;
	Hash ^= Hash >> 32;

	return Hash;
}
