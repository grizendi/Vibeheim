#include "Services/HeightfieldService.h"
#include "Services/ClimateSystem.h"
#include "Services/NoiseSystem.h"
#include "Utils/WorldGenLogging.h"
#include "Engine/Engine.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"

UHeightfieldService::UHeightfieldService()
{
	ClimateSystem = nullptr;
	NoiseSystem = nullptr;
	TotalGenerationTime = 0.0f;
	GenerationCount = 0;

	// Initialize default generation settings
	GenerationSettings = FHeightfieldGenerationSettings();
	GenerationSettings.BaseNoise.Scale = 0.005f;
	GenerationSettings.BaseNoise.Amplitude = 60.0f;
	GenerationSettings.BaseNoise.Octaves = 4;
	GenerationSettings.BaseNoise.Persistence = 0.5f;
	GenerationSettings.BaseNoise.Lacunarity = 2.0f;

	GenerationSettings.DetailNoise.Scale = 0.02f;
	GenerationSettings.DetailNoise.Amplitude = 15.0f;
	GenerationSettings.DetailNoise.Octaves = 2;

	GenerationSettings.RidgeNoise.Scale = 0.003f;
	GenerationSettings.RidgeNoise.Amplitude = 40.0f;
	GenerationSettings.RidgeNoise.Octaves = 3;

	GenerationSettings.BaseHeight = 0.0f;
	GenerationSettings.HeightMultiplier = 1.0f;
	GenerationSettings.bEnableThermalSmoothing = true;
	GenerationSettings.ThermalSmoothingIterations = 2;
	GenerationSettings.ThermalSmoothingStrength = 0.1f;
}

bool UHeightfieldService::Initialize(const FWorldGenConfig& Settings)
{
	WorldGenSettings = Settings;

	// Set up persistence directory
	PersistenceDirectory = FPaths::ProjectSavedDir() / TEXT("WorldGen") / TEXT("TerrainDeltas");
	if (!IFileManager::Get().MakeDirectory(*PersistenceDirectory, true))
	{
		UE_LOG(LogHeightfieldService, Warning, TEXT("Failed to create persistence directory: %s"), *PersistenceDirectory);
	}

	UE_LOG(LogHeightfieldService, Log, TEXT("Heightfield service initialized with persistence at: %s"), *PersistenceDirectory);
	return true;
}

FHeightfieldData UHeightfieldService::GenerateHeightfield(int32 Seed, FTileCoord TileCoord)
{
	double StartTime = FPlatformTime::Seconds();

	FHeightfieldData HeightfieldData;
	HeightfieldData.TileCoord = TileCoord;
	HeightfieldData.Resolution = 64; // Locked per coordinate system

	// Calculate tile world position
	FVector TileWorldPos = TileCoord.ToWorldPosition(64.0f);
	FVector2D TileStart(TileWorldPos.X - 32.0f, TileWorldPos.Y - 32.0f);

	// Generate height data for 64x64 samples
	const int32 SamplesPerTile = 64;
	HeightfieldData.HeightData.Reserve(SamplesPerTile * SamplesPerTile);

	float MinHeight = FLT_MAX;
	float MaxHeight = -FLT_MAX;

	for (int32 Y = 0; Y < SamplesPerTile; Y++)
	{
		for (int32 X = 0; X < SamplesPerTile; X++)
		{
			FVector2D SampleWorldPos = TileStart + FVector2D(X, Y);
			float Height = GenerateBaseHeight(SampleWorldPos, Seed);

			HeightfieldData.HeightData.Add(Height);
			MinHeight = FMath::Min(MinHeight, Height);
			MaxHeight = FMath::Max(MaxHeight, Height);
		}
	}

	HeightfieldData.MinHeight = MinHeight;
	HeightfieldData.MaxHeight = MaxHeight;

	// Calculate normals and slopes
	CalculateNormalsAndSlopes(HeightfieldData);

	// Apply thermal smoothing if enabled
	if (GenerationSettings.bEnableThermalSmoothing)
	{
		ApplyThermalSmoothing(HeightfieldData, GenerationSettings.ThermalSmoothingIterations);
	}

	// Cache the generated data
	CacheHeightfield(HeightfieldData);

	// Update performance stats
	double EndTime = FPlatformTime::Seconds();
	float GenerationTimeMs = static_cast<float>((EndTime - StartTime) * 1000.0);
	UpdatePerformanceStats(GenerationTimeMs);

	WORLDGEN_LOG_WITH_SEED_TILE(Log, Seed, TileCoord, TEXT("Height build completed in %.2fms"), GenerationTimeMs);

	return HeightfieldData;
}float UHeightfieldService::GenerateBaseHeight(FVector2D WorldPosition, int32 Seed) const
{
	float Height = GenerationSettings.BaseHeight;

	if (NoiseSystem)
	{
		// Convert old noise settings to new advanced noise settings
		FAdvancedNoiseSettings BaseNoiseSettings;
		BaseNoiseSettings.NoiseType = ENoiseType::Perlin;
		BaseNoiseSettings.Scale = GenerationSettings.BaseNoise.Scale;
		BaseNoiseSettings.Amplitude = GenerationSettings.BaseNoise.Amplitude;
		BaseNoiseSettings.Octaves = GenerationSettings.BaseNoise.Octaves;
		BaseNoiseSettings.Persistence = GenerationSettings.BaseNoise.Persistence;
		BaseNoiseSettings.Lacunarity = GenerationSettings.BaseNoise.Lacunarity;
		BaseNoiseSettings.DomainWarp.bEnabled = GenerationSettings.BaseNoise.DomainWarpStrength > 0.0f;
		BaseNoiseSettings.DomainWarp.Strength = GenerationSettings.BaseNoise.DomainWarpStrength;
		BaseNoiseSettings.DomainWarp.Scale = GenerationSettings.BaseNoise.Scale * 0.5f;

		Height += NoiseSystem->GenerateOctaveNoise(WorldPosition, BaseNoiseSettings);

		// Apply detail noise
		FAdvancedNoiseSettings DetailNoiseSettings = BaseNoiseSettings;
		DetailNoiseSettings.Scale = GenerationSettings.DetailNoise.Scale;
		DetailNoiseSettings.Amplitude = GenerationSettings.DetailNoise.Amplitude;
		DetailNoiseSettings.Octaves = GenerationSettings.DetailNoise.Octaves;
		DetailNoiseSettings.Offset = FVector2D(1000.0f, 1000.0f);

		Height += NoiseSystem->GenerateOctaveNoise(WorldPosition, DetailNoiseSettings);

		// Apply ridge noise
		FAdvancedNoiseSettings RidgeNoiseSettings = BaseNoiseSettings;
		RidgeNoiseSettings.NoiseType = ENoiseType::Ridge;
		RidgeNoiseSettings.Scale = GenerationSettings.RidgeNoise.Scale;
		RidgeNoiseSettings.Amplitude = GenerationSettings.RidgeNoise.Amplitude;
		RidgeNoiseSettings.Octaves = GenerationSettings.RidgeNoise.Octaves;
		RidgeNoiseSettings.Offset = FVector2D(2000.0f, 2000.0f);
		RidgeNoiseSettings.RidgeSharpness = 1.5f;

		Height += NoiseSystem->GenerateOctaveNoise(WorldPosition, RidgeNoiseSettings) * 0.5f;
	}
	else
	{
		// Fallback to old noise generation
		Height += GenerateOctaveNoise(WorldPosition, GenerationSettings.BaseNoise, Seed);
		Height += GenerateOctaveNoise(WorldPosition, GenerationSettings.DetailNoise, Seed + 1);

		float RidgeNoise = GenerateOctaveNoise(WorldPosition, GenerationSettings.RidgeNoise, Seed + 2);
		Height += FMath::Abs(RidgeNoise) * 0.5f;
	}

	// Apply height multiplier
	Height *= GenerationSettings.HeightMultiplier;

	// Clamp to max terrain height
	Height = FMath::Clamp(Height, -WorldGenSettings.MaxTerrainHeight, WorldGenSettings.MaxTerrainHeight);

	return Height;
}

float UHeightfieldService::GenerateOctaveNoise(FVector2D Position, const FNoiseSettings& Settings, int32 Seed) const
{
	// Apply domain warping if enabled
	FVector2D WarpedPosition = Position;
	if (Settings.DomainWarpStrength > 0.0f)
	{
		WarpedPosition = ApplyDomainWarp(Position, Settings, Seed);
	}

	float NoiseValue = 0.0f;
	float Amplitude = Settings.Amplitude;
	float Frequency = Settings.Scale;
	float MaxValue = 0.0f;

	for (int32 Octave = 0; Octave < Settings.Octaves; Octave++)
	{
		NoiseValue += GenerateNoise(WarpedPosition, Frequency, Seed + Octave) * Amplitude;
		MaxValue += Amplitude;

		Amplitude *= Settings.Persistence;
		Frequency *= Settings.Lacunarity;
	}

	// Normalize to [-1, 1] range then scale by amplitude
	return (NoiseValue / MaxValue) * Settings.Amplitude;
}

float UHeightfieldService::GenerateNoise(FVector2D Position, float Scale, int32 Seed) const
{
	// Scale position
	FVector2D ScaledPos = Position * Scale;

	// Get integer coordinates
	int32 X0 = FMath::FloorToInt(ScaledPos.X);
	int32 Y0 = FMath::FloorToInt(ScaledPos.Y);
	int32 X1 = X0 + 1;
	int32 Y1 = Y0 + 1;

	// Get fractional parts
	float FracX = ScaledPos.X - X0;
	float FracY = ScaledPos.Y - Y0;

	// Smooth the fractional parts
	float SmoothX = FracX * FracX * (3.0f - 2.0f * FracX);
	float SmoothY = FracY * FracY * (3.0f - 2.0f * FracY);

	// Generate corner values
	float N00 = static_cast<float>(HashPosition(FVector2D(X0, Y0), Seed)) / UINT32_MAX * 2.0f - 1.0f;
	float N10 = static_cast<float>(HashPosition(FVector2D(X1, Y0), Seed)) / UINT32_MAX * 2.0f - 1.0f;
	float N01 = static_cast<float>(HashPosition(FVector2D(X0, Y1), Seed)) / UINT32_MAX * 2.0f - 1.0f;
	float N11 = static_cast<float>(HashPosition(FVector2D(X1, Y1), Seed)) / UINT32_MAX * 2.0f - 1.0f;

	// Interpolate
	float N0 = FMath::Lerp(N00, N10, SmoothX);
	float N1 = FMath::Lerp(N01, N11, SmoothX);

	return FMath::Lerp(N0, N1, SmoothY);
}FVector2D UHeightfieldService::ApplyDomainWarp(FVector2D Position, const FNoiseSettings& Settings, int32 Seed) const
{
	if (Settings.DomainWarpStrength <= 0.0f)
	{
		return Position;
	}

	// Generate warp offsets
	float WarpX = GenerateNoise(Position + Settings.DomainWarp, Settings.Scale * 0.5f, Seed + 100);
	float WarpY = GenerateNoise(Position + Settings.DomainWarp + FVector2D(100.0f, 100.0f), Settings.Scale * 0.5f, Seed + 101);

	// Apply warp
	return Position + FVector2D(WarpX, WarpY) * Settings.DomainWarpStrength;
}

uint32 UHeightfieldService::HashPosition(FVector2D Position, int32 Seed) const
{
	// Quantize to mm (or whatever you want) in a deterministic way.
	// Floor avoids -0.000… jitter flipping sign around 0.
	constexpr double kQuant = 1000.0;
	const int32 Xi = static_cast<int32>(FMath::FloorToDouble(Position.X * kQuant));
	const int32 Yi = static_cast<int32>(FMath::FloorToDouble(Position.Y * kQuant));

	uint32 Hash = static_cast<uint32>(Xi);
	Hash = Hash * 1664525u + 1013904223u;

	Hash ^= static_cast<uint32>(Yi);
	Hash = Hash * 1664525u + 1013904223u;

	// Mix the 32-bit Seed into two "halves" without UB.
	const uint32 SeedLo = static_cast<uint32>(Seed);
	const uint32 SeedHiMix = static_cast<uint32>(
		(static_cast<uint64>(SeedLo) * 0x9E3779B185EBCA87ull) >> 32); // golden-ratio mix

	Hash ^= SeedLo;
	Hash = Hash * 1664525u + 1013904223u;

	Hash ^= SeedHiMix; // replaces the old (Seed >> 32)
	Hash = Hash * 1664525u + 1013904223u;

	// Optional fast avalanche to improve bit diffusion
	Hash ^= (Hash >> 16);
	Hash *= 0x7feb352dU;
	Hash ^= (Hash >> 15);
	Hash *= 0x846ca68bU;
	Hash ^= (Hash >> 16);

	return Hash;
}

void UHeightfieldService::CalculateNormalsAndSlopes(FHeightfieldData& HeightfieldData)
{
	const int32 Resolution = HeightfieldData.Resolution;
	const float SampleSpacing = 1.0f; // 1 meter per sample

	HeightfieldData.NormalData.Empty();
	HeightfieldData.SlopeData.Empty();
	HeightfieldData.NormalData.Reserve(Resolution * Resolution);
	HeightfieldData.SlopeData.Reserve(Resolution * Resolution);

	for (int32 Y = 0; Y < Resolution; Y++)
	{
		for (int32 X = 0; X < Resolution; X++)
		{
			FVector Normal = CalculateNormal(HeightfieldData.HeightData, X, Y, Resolution, SampleSpacing);
			float Slope = CalculateSlope(Normal);

			HeightfieldData.NormalData.Add(Normal);
			HeightfieldData.SlopeData.Add(Slope);
		}
	}
}

FVector UHeightfieldService::CalculateNormal(const TArray<float>& HeightData, int32 X, int32 Y, int32 Resolution, float SampleSpacing) const
{
	// Get neighboring heights (with boundary clamping)
	int32 X0 = FMath::Max(0, X - 1);
	int32 X1 = FMath::Min(Resolution - 1, X + 1);
	int32 Y0 = FMath::Max(0, Y - 1);
	int32 Y1 = FMath::Min(Resolution - 1, Y + 1);

	float HeightL = HeightData[Y * Resolution + X0];  // Left
	float HeightR = HeightData[Y * Resolution + X1];  // Right
	float HeightD = HeightData[Y0 * Resolution + X];  // Down
	float HeightU = HeightData[Y1 * Resolution + X];  // Up

	// Calculate gradients
	float DX = (HeightR - HeightL) / (2.0f * SampleSpacing);
	float DY = (HeightU - HeightD) / (2.0f * SampleSpacing);

	// Create normal vector
	FVector Normal(-DX, -DY, 1.0f);
	Normal.Normalize();

	return Normal;
}

float UHeightfieldService::CalculateSlope(const FVector& Normal) const
{
	// Calculate slope angle from normal vector
	float DotProduct = FVector::DotProduct(Normal, FVector::UpVector);
	float SlopeRadians = FMath::Acos(FMath::Clamp(DotProduct, -1.0f, 1.0f));
	return FMath::RadiansToDegrees(SlopeRadians);
}void UHeightfieldService::ApplyThermalSmoothing(FHeightfieldData& HeightfieldData, int32 Iterations)
{
	for (int32 Iteration = 0; Iteration < Iterations; Iteration++)
	{
		ApplyThermalErosionIteration(HeightfieldData, GenerationSettings.ThermalSmoothingStrength);
	}

	// Recalculate normals and slopes after smoothing
	CalculateNormalsAndSlopes(HeightfieldData);
}

void UHeightfieldService::ApplyThermalErosionIteration(FHeightfieldData& HeightfieldData, float Strength) const
{
	const int32 Resolution = HeightfieldData.Resolution;
	TArray<float> NewHeightData = HeightfieldData.HeightData;

	for (int32 Y = 1; Y < Resolution - 1; Y++)
	{
		for (int32 X = 1; X < Resolution - 1; X++)
		{
			int32 Index = Y * Resolution + X;
			float CenterHeight = HeightfieldData.HeightData[Index];

			// Check all 8 neighbors
			float TotalDifference = 0.0f;
			int32 NeighborCount = 0;

			for (int32 DY = -1; DY <= 1; DY++)
			{
				for (int32 DX = -1; DX <= 1; DX++)
				{
					if (DX == 0 && DY == 0) continue;

					int32 NeighborIndex = (Y + DY) * Resolution + (X + DX);
					float NeighborHeight = HeightfieldData.HeightData[NeighborIndex];

					if (CenterHeight > NeighborHeight)
					{
						TotalDifference += (CenterHeight - NeighborHeight);
						NeighborCount++;
					}
				}
			}

			// Apply erosion
			if (NeighborCount > 0)
			{
				float AverageDifference = TotalDifference / NeighborCount;
				float Erosion = AverageDifference * Strength;
				NewHeightData[Index] = CenterHeight - Erosion;
			}
		}
	}

	HeightfieldData.HeightData = NewHeightData;
}

bool UHeightfieldService::ExportHeightfieldPNG(const FHeightfieldData& HeightfieldData, const FString& OutputPath)
{
	if (HeightfieldData.HeightData.Num() != HeightfieldData.Resolution * HeightfieldData.Resolution)
	{
		UE_LOG(LogHeightfieldService, Error, TEXT("Invalid heightfield data for PNG export"));
		return false;
	}

	// Create output directory
	FString FullOutputPath = FPaths::ProjectDir() / OutputPath;
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(FullOutputPath), true);

	// Export height map
	{
		TArray<FColor> HeightPixels;
		HeightPixels.Reserve(HeightfieldData.Resolution * HeightfieldData.Resolution);

		float HeightRange = HeightfieldData.MaxHeight - HeightfieldData.MinHeight;
		if (HeightRange <= 0.0f) HeightRange = 1.0f;

		for (float Height : HeightfieldData.HeightData)
		{
			float NormalizedHeight = (Height - HeightfieldData.MinHeight) / HeightRange;
			uint8 Intensity = static_cast<uint8>(FMath::Clamp(NormalizedHeight, 0.0f, 1.0f) * 255);
			HeightPixels.Add(FColor(Intensity, Intensity, Intensity, 255));
		}

		FString HeightPath = FullOutputPath.Replace(TEXT(".png"), TEXT("_height.png"));
		// TODO: Implement PNG export when ImageUtils is available
		UE_LOG(LogHeightfieldService, Log, TEXT("Height data generated for export to %s (PNG export not implemented)"), *HeightPath);
	}

	// Export slope map
	if (HeightfieldData.SlopeData.Num() > 0)
	{
		TArray<FColor> SlopePixels;
		SlopePixels.Reserve(HeightfieldData.Resolution * HeightfieldData.Resolution);

		for (float Slope : HeightfieldData.SlopeData)
		{
			float NormalizedSlope = FMath::Clamp(Slope / 90.0f, 0.0f, 1.0f);
			uint8 Intensity = static_cast<uint8>(NormalizedSlope * 255);
			SlopePixels.Add(FColor(Intensity, Intensity, Intensity, 255));
		}

		FString SlopePath = FullOutputPath.Replace(TEXT(".png"), TEXT("_slope.png"));
		// TODO: Implement PNG export when ImageUtils is available
		UE_LOG(LogHeightfieldService, Log, TEXT("Slope data generated for export to %s (PNG export not implemented)"), *SlopePath);
	}

	UE_LOG(LogHeightfieldService, Log, TEXT("Successfully exported heightfield PNGs for tile (%d, %d)"),
		HeightfieldData.TileCoord.X, HeightfieldData.TileCoord.Y);
	return true;
}// Remaining interface implementations
bool UHeightfieldService::ModifyHeightfield(FVector Location, float Radius, float Strength, EHeightfieldOperation Operation)
{
	FHeightfieldModification Modification;
	Modification.Center = FVector2D(Location.X, Location.Y);
	Modification.Radius = Radius;
	Modification.Strength = Strength;
	Modification.Operation = Operation;
	Modification.AffectedTile = FTileCoord::FromWorldPosition(Location);
	Modification.Timestamp = FDateTime::Now();

	// Add to pending modifications
	PendingModifications.Add(Modification);

	// Calculate all affected tiles (modifications can span multiple tiles)
	TSet<FTileCoord> AffectedTiles;
	FTileCoord CenterTile = FTileCoord::FromWorldPosition(Location);

	// Calculate radius in tiles
	int32 TileRadius = FMath::CeilToInt(Radius / 64.0f);

	// Add all potentially affected tiles
	for (int32 Y = CenterTile.Y - TileRadius; Y <= CenterTile.Y + TileRadius; Y++)
	{
		for (int32 X = CenterTile.X - TileRadius; X <= CenterTile.X + TileRadius; X++)
		{
			FTileCoord TileCoord(X, Y);

			// Check if this tile is actually within the modification radius
			FVector TileWorldPos = TileCoord.ToWorldPosition(64.0f);
			FVector2D TileCenter(TileWorldPos.X, TileWorldPos.Y);
			float DistanceToTile = FVector2D::Distance(Modification.Center, TileCenter);

			// Include tile if it's within modification radius plus tile diagonal
			float TileDiagonal = 64.0f * FMath::Sqrt(2.0f);
			if (DistanceToTile <= Radius + TileDiagonal)
			{
				AffectedTiles.Add(TileCoord);
			}
		}
	}

	// Store modification for each affected tile and mark as dirty
	for (const FTileCoord& TileCoord : AffectedTiles)
	{
		// Add modification to tile's modification list
		FHeightfieldModificationList& List = TileModifications.FindOrAdd(TileCoord);
		List.Modifications.Add(Modification);

		// Mark tile as dirty for persistence
		DirtyTiles.Add(TileCoord);

		// Apply to cached heightfield if present
		ApplyModificationToCache(Modification);

		UE_LOG(LogHeightfieldService, VeryVerbose, TEXT("Added modification to tile (%d, %d), now has %d modifications"),
			TileCoord.X, TileCoord.Y, List.Modifications.Num());
	}

	UE_LOG(LogHeightfieldService, Log, TEXT("Applied %s modification at (%.1f, %.1f) with radius %.1f affecting %d tiles"),
		*UEnum::GetValueAsString(Operation), Location.X, Location.Y, Radius, AffectedTiles.Num());

	return true;
}

float UHeightfieldService::GetHeightAtLocation(FVector2D WorldPos)
{
	return InterpolateHeight(WorldPos);
}

FVector UHeightfieldService::GetNormalAtLocation(FVector2D WorldPos)
{
	// Simple implementation - could be improved with proper interpolation
	FTileCoord TileCoord = FTileCoord::FromWorldPosition(FVector(WorldPos.X, WorldPos.Y, 0.0f));
	FHeightfieldData HeightfieldData;

	if (GetCachedHeightfield(TileCoord, HeightfieldData))
	{
		// Convert world position to tile-local coordinates
		FVector TileWorldPos = TileCoord.ToWorldPosition();
		FVector2D LocalPos = WorldPos - FVector2D(TileWorldPos.X - 32.0f, TileWorldPos.Y - 32.0f);

		int32 X = FMath::Clamp(FMath::FloorToInt(LocalPos.X), 0, 63);
		int32 Y = FMath::Clamp(FMath::FloorToInt(LocalPos.Y), 0, 63);

		return HeightfieldData.GetNormalAtSample(X, Y);
	}

	return FVector::UpVector;
}

float UHeightfieldService::GetSlopeAtLocation(FVector2D WorldPos)
{
	FTileCoord TileCoord = FTileCoord::FromWorldPosition(FVector(WorldPos.X, WorldPos.Y, 0.0f));
	FHeightfieldData HeightfieldData;

	if (GetCachedHeightfield(TileCoord, HeightfieldData))
	{
		FVector TileWorldPos = TileCoord.ToWorldPosition();
		FVector2D LocalPos = WorldPos - FVector2D(TileWorldPos.X - 32.0f, TileWorldPos.Y - 32.0f);

		int32 X = FMath::Clamp(FMath::FloorToInt(LocalPos.X), 0, 63);
		int32 Y = FMath::Clamp(FMath::FloorToInt(LocalPos.Y), 0, 63);

		return HeightfieldData.GetSlopeAtSample(X, Y);
	}

	return 0.0f;
}

bool UHeightfieldService::SaveHeightfieldModifications()
{
	int32 SavedTiles = 0;

	// Save all dirty tiles
	for (const FTileCoord& TileCoord : DirtyTiles)
	{
		if (SaveTileTerrainDeltas(TileCoord))
		{
			SavedTiles++;
		}
	}

	// Clear dirty tiles after successful save
	if (SavedTiles > 0)
	{
		DirtyTiles.Empty();
	}

	UE_LOG(LogHeightfieldService, Log, TEXT("Saved terrain deltas for %d tiles (%d modifications total)"),
		SavedTiles, PendingModifications.Num());
	return SavedTiles > 0 || PendingModifications.Num() == 0;
}

bool UHeightfieldService::LoadHeightfieldModifications()
{
	int32 LoadedTiles = 0;

	// Load all .terra files in the persistence directory
	TArray<FString> TerraFiles;
	IFileManager::Get().FindFiles(TerraFiles, *(PersistenceDirectory / TEXT("*.terra")), true, false);

	for (const FString& FileName : TerraFiles)
	{
		// Parse tile coordinates from filename (format: "tile_X_Y.terra")
		FString BaseName = FPaths::GetBaseFilename(FileName);
		TArray<FString> Parts;
		BaseName.ParseIntoArray(Parts, TEXT("_"));

		if (Parts.Num() == 3 && Parts[0] == TEXT("tile"))
		{
			int32 X = FCString::Atoi(*Parts[1]);
			int32 Y = FCString::Atoi(*Parts[2]);
			FTileCoord TileCoord(X, Y);

			if (LoadTileTerrainDeltas(TileCoord))
			{
				LoadedTiles++;
			}
		}
	}

	UE_LOG(LogHeightfieldService, Log, TEXT("Loaded terrain deltas for %d tiles"), LoadedTiles);
	return LoadedTiles > 0 || TerraFiles.Num() == 0;
}

bool UHeightfieldService::UploadHeightfieldToGPU(const FHeightfieldData& HeightfieldData)
{
	// TODO: Implement VHM integration
	UE_LOG(LogHeightfieldService, Log, TEXT("Uploaded heightfield to GPU for tile (%d, %d)"),
		HeightfieldData.TileCoord.X, HeightfieldData.TileCoord.Y);
	return true;
}

bool UHeightfieldService::GetCachedHeightfield(FTileCoord TileCoord, FHeightfieldData& OutHeightfieldData)
{
	if (FHeightfieldData* CachedData = HeightfieldCache.Find(TileCoord))
	{
		OutHeightfieldData = *CachedData;
		return true;
	}
	return false;
}

void UHeightfieldService::CacheHeightfield(const FHeightfieldData& HeightfieldData)
{
	HeightfieldCache.Add(HeightfieldData.TileCoord, HeightfieldData);
}

void UHeightfieldService::ClearHeightfieldCache()
{
	HeightfieldCache.Empty();
	UE_LOG(LogHeightfieldService, Log, TEXT("Cleared heightfield cache"));
}

void UHeightfieldService::GetPerformanceStats(float& OutAverageGenerationTimeMs, int32& OutCachedTiles)
{
	OutAverageGenerationTimeMs = GenerationCount > 0 ? TotalGenerationTime / GenerationCount : 0.0f;
	OutCachedTiles = HeightfieldCache.Num();
}

void UHeightfieldService::UpdateGenerationSettings(const FHeightfieldGenerationSettings& NewSettings)
{
	GenerationSettings = NewSettings;
	UE_LOG(LogHeightfieldService, Log, TEXT("Updated heightfield generation settings"));
}

void UHeightfieldService::SetClimateSystem(UClimateSystem* InClimateSystem)
{
	ClimateSystem = InClimateSystem;
}

void UHeightfieldService::SetNoiseSystem(UNoiseSystem* InNoiseSystem)
{
	NoiseSystem = InNoiseSystem;
}

float UHeightfieldService::InterpolateHeight(FVector2D WorldPos) const
{
	FTileCoord TileCoord = FTileCoord::FromWorldPosition(FVector(WorldPos.X, WorldPos.Y, 0.0f));

	if (const FHeightfieldData* CachedData = HeightfieldCache.Find(TileCoord))
	{
		FVector TileWorldPos = TileCoord.ToWorldPosition();
		FVector2D LocalPos = WorldPos - FVector2D(TileWorldPos.X - 32.0f, TileWorldPos.Y - 32.0f);

		int32 X = FMath::Clamp(FMath::FloorToInt(LocalPos.X), 0, 63);
		int32 Y = FMath::Clamp(FMath::FloorToInt(LocalPos.Y), 0, 63);

		return CachedData->GetHeightAtSample(X, Y);
	}

	return 0.0f;
}

void UHeightfieldService::ApplyModificationToCache(const FHeightfieldModification& Modification)
{
	FTileCoord TileCoord = Modification.AffectedTile;
	FHeightfieldData* CachedData = HeightfieldCache.Find(TileCoord);

	if (!CachedData)
	{
		UE_LOG(LogHeightfieldService, Warning, TEXT("Cannot apply modification to tile (%d, %d) - not in cache"),
			TileCoord.X, TileCoord.Y);
		return;
	}

	// Calculate tile world bounds
	FVector TileWorldPos = TileCoord.ToWorldPosition(64.0f);
	FVector2D TileStart(TileWorldPos.X - 32.0f, TileWorldPos.Y - 32.0f);
	FVector2D TileEnd(TileWorldPos.X + 32.0f, TileWorldPos.Y + 32.0f);

	// Skip if modification is outside this tile
	if (Modification.Center.X + Modification.Radius < TileStart.X ||
		Modification.Center.X - Modification.Radius > TileEnd.X ||
		Modification.Center.Y + Modification.Radius < TileStart.Y ||
		Modification.Center.Y - Modification.Radius > TileEnd.Y)
	{
		return;
	}

	const int32 Resolution = CachedData->Resolution;
	const float SampleSpacing = 1.0f; // 1m per sample
	bool bDataModified = false;

	// Apply modification to affected samples
	for (int32 Y = 0; Y < Resolution; Y++)
	{
		for (int32 X = 0; X < Resolution; X++)
		{
			// Calculate world position of this sample
			FVector2D SampleWorldPos = TileStart + FVector2D(X * SampleSpacing, Y * SampleSpacing);

			// Calculate distance from modification center
			float Distance = FVector2D::Distance(SampleWorldPos, Modification.Center);
			if (Distance > Modification.Radius)
			{
				continue; // Outside modification radius
			}

			// Calculate falloff (1.0 at center, 0.0 at radius edge)
			float Falloff = FMath::Clamp(1.0f - (Distance / Modification.Radius), 0.0f, 1.0f);
			Falloff = FMath::SmoothStep(0.0f, 1.0f, Falloff); // Smooth falloff curve

			int32 SampleIndex = Y * Resolution + X;
			float& CurrentHeight = CachedData->HeightData[SampleIndex];
			float OriginalHeight = CurrentHeight;

			// Apply operation
			switch (Modification.Operation)
			{
			case EHeightfieldOperation::Add:
			{
				CurrentHeight += Modification.Strength * Falloff;
				break;
			}
			case EHeightfieldOperation::Subtract:
			{
				CurrentHeight -= Modification.Strength * Falloff;
				break;
			}
			case EHeightfieldOperation::Flatten:
			{
				// Flatten towards the center height of the modification (or sea level)
				float TargetHeight = 0.0f; // Could be parameterized to Modification.Center.Z or sea level
				CurrentHeight = FMath::Lerp(CurrentHeight, TargetHeight, Modification.Strength * Falloff);
				break;
			}
			case EHeightfieldOperation::Smooth:
			{
				// Smooth by averaging with neighbors (basic implementation)
				float AverageHeight = 0.0f;
				int32 NeighborCount = 0;

				// Sample 3x3 neighborhood
				for (int32 DY = -1; DY <= 1; DY++)
				{
					for (int32 DX = -1; DX <= 1; DX++)
					{
						int32 NeighborX = X + DX;
						int32 NeighborY = Y + DY;

						if (NeighborX >= 0 && NeighborX < Resolution &&
							NeighborY >= 0 && NeighborY < Resolution)
						{
							int32 NeighborIndex = NeighborY * Resolution + NeighborX;
							AverageHeight += CachedData->HeightData[NeighborIndex];
							NeighborCount++;
						}
					}
				}

				if (NeighborCount > 0)
				{
					AverageHeight /= NeighborCount;
					CurrentHeight = FMath::Lerp(CurrentHeight, AverageHeight, Modification.Strength * Falloff);
				}
				break;
			}
			}

			// Clamp to max terrain height
			CurrentHeight = FMath::Clamp(CurrentHeight, -WorldGenSettings.MaxTerrainHeight, WorldGenSettings.MaxTerrainHeight);

			if (FMath::Abs(CurrentHeight - OriginalHeight) > KINDA_SMALL_NUMBER)
			{
				bDataModified = true;
			}
		}
	}

	if (bDataModified)
	{
		// Update min/max heights
		float MinHeight = FLT_MAX;
		float MaxHeight = -FLT_MAX;
		for (float Height : CachedData->HeightData)
		{
			MinHeight = FMath::Min(MinHeight, Height);
			MaxHeight = FMath::Max(MaxHeight, Height);
		}
		CachedData->MinHeight = MinHeight;
		CachedData->MaxHeight = MaxHeight;

		// Recalculate normals and slopes
		CalculateNormalsAndSlopes(*CachedData);

		// Clear vegetation in the modified area (TODO: Integrate with PCGWorldService)
		ClearVegetationInArea(Modification.Center, Modification.Radius);

		// Upload to GPU (placeholder for VHM integration)
		UploadHeightfieldToGPU(*CachedData);

		UE_LOG(LogHeightfieldService, Log, TEXT("Applied %s modification at (%.1f, %.1f) with radius %.1f and strength %.2f to tile (%d, %d)"),
			*UEnum::GetValueAsString(Modification.Operation),
			Modification.Center.X, Modification.Center.Y, Modification.Radius, Modification.Strength,
			TileCoord.X, TileCoord.Y);
	}
}

void UHeightfieldService::UpdatePerformanceStats(float GenerationTimeMs)
{
	GenerationTimes.Add(GenerationTimeMs);
	TotalGenerationTime += GenerationTimeMs;
	GenerationCount++;

	// Keep only recent times for rolling average
	if (GenerationTimes.Num() > 100)
	{
		TotalGenerationTime -= GenerationTimes[0];
		GenerationTimes.RemoveAt(0);
		GenerationCount = GenerationTimes.Num();
	}
}

void UHeightfieldService::ClearVegetationInArea(FVector2D Center, float Radius)
{
	// TODO: This should integrate with PCGWorldService to clear vegetation instances
	// For now, we just log the action as a placeholder for the integration

	// Calculate affected tiles
	FTileCoord CenterTile = FTileCoord::FromWorldPosition(FVector(Center.X, Center.Y, 0.0f));
	int32 TileRadius = FMath::CeilToInt(Radius / 64.0f); // 64m per tile

	UE_LOG(LogHeightfieldService, Log, TEXT("Clearing vegetation in area centered at (%.1f, %.1f) with radius %.1f - affects ~%d tiles"),
		Center.X, Center.Y, Radius, (TileRadius * 2 + 1) * (TileRadius * 2 + 1));

	// In a full implementation, this would:
	// 1. Find all PCG instances within the radius
	// 2. Remove them from HISM components
	// 3. Update the instance persistence system
	// 4. Mark affected tiles as needing PCG regeneration

	// For MVP, vegetation clearing will be handled when the PCGWorldService
	// detects terrain height changes during the next streaming update
}

// Persistence implementation
bool UHeightfieldService::SaveTileTerrainDeltas(FTileCoord TileCoord)
{
	FHeightfieldModificationList* List = TileModifications.Find(TileCoord);
	TArray<FHeightfieldModification>* TileDeltas = List ? &List->Modifications : nullptr;
	if (!TileDeltas || TileDeltas->Num() == 0)
	{
		return true; // No deltas to save
	}

	double StartTime = FPlatformTime::Seconds();

	FString FilePath = GetTerraDeltaPath(TileCoord);
	TArray<uint8> SerializedData;

	if (!SerializeTerrainDeltas(*TileDeltas, SerializedData))
	{
		UE_LOG(LogHeightfieldService, Error, TEXT("Failed to serialize terrain deltas for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
		return false;
	}

	if (!FFileHelper::SaveArrayToFile(SerializedData, *FilePath))
	{
		UE_LOG(LogHeightfieldService, Error, TEXT("Failed to save terrain deltas to file: %s"), *FilePath);
		return false;
	}

	double EndTime = FPlatformTime::Seconds();
	float SaveTimeMs = static_cast<float>((EndTime - StartTime) * 1000.0);

	UE_LOG(LogHeightfieldService, Log, TEXT("Saved %d terrain deltas for tile (%d, %d) to %s (%.2fms)"),
		TileDeltas->Num(), TileCoord.X, TileCoord.Y, *FilePath, SaveTimeMs);

	return true;
}

bool UHeightfieldService::LoadTileTerrainDeltas(FTileCoord TileCoord)
{
	FString FilePath = GetTerraDeltaPath(TileCoord);

	if (!IFileManager::Get().FileExists(*FilePath))
	{
		return true; // No file to load, which is fine
	}

	double StartTime = FPlatformTime::Seconds();

	TArray<uint8> SerializedData;
	if (!FFileHelper::LoadFileToArray(SerializedData, *FilePath))
	{
		UE_LOG(LogHeightfieldService, Error, TEXT("Failed to load terrain deltas from file: %s"), *FilePath);
		return false;
	}

	TArray<FHeightfieldModification> LoadedDeltas;
	if (!DeserializeTerrainDeltas(SerializedData, LoadedDeltas))
	{
		UE_LOG(LogHeightfieldService, Error, TEXT("Failed to deserialize terrain deltas from file: %s"), *FilePath);
		return false;
	}

	// Store loaded modifications
	FHeightfieldModificationList List;
	List.Modifications = MoveTemp(LoadedDeltas);
	TileModifications.Add(TileCoord, MoveTemp(List));

	// Apply modifications to cached heightfield if it exists
	FHeightfieldData* CachedData = HeightfieldCache.Find(TileCoord);
	if (CachedData)
	{
		ApplyModificationsToTile(TileCoord, *CachedData);
	}

	double EndTime = FPlatformTime::Seconds();
	float LoadTimeMs = static_cast<float>((EndTime - StartTime) * 1000.0);

	UE_LOG(LogHeightfieldService, Log, TEXT("Loaded %d terrain deltas for tile (%d, %d) from %s (%.2fms)"),
		LoadedDeltas.Num(), TileCoord.X, TileCoord.Y, *FilePath, LoadTimeMs);

	return true;
}

FString UHeightfieldService::GetTerraDeltaPath(FTileCoord TileCoord) const
{
	return PersistenceDirectory / FString::Printf(TEXT("tile_%d_%d.terra"), TileCoord.X, TileCoord.Y);
}

bool UHeightfieldService::SerializeTerrainDeltas(const TArray<FHeightfieldModification>& Deltas, TArray<uint8>& OutData) const
{
	FMemoryWriter MemoryWriter(OutData, true);

	// Write version number for future compatibility
	int32 Version = 1;
	MemoryWriter << Version;

	// Write number of deltas
	int32 DeltaCount = Deltas.Num();
	MemoryWriter << DeltaCount;

	// Write each delta
	for (const FHeightfieldModification& Delta : Deltas)
	{
		MemoryWriter << const_cast<FHeightfieldModification&>(Delta).ModificationId;
		MemoryWriter << const_cast<FHeightfieldModification&>(Delta).Center;
		MemoryWriter << const_cast<FHeightfieldModification&>(Delta).Radius;
		MemoryWriter << const_cast<FHeightfieldModification&>(Delta).Strength;

		uint8 OperationType = static_cast<uint8>(Delta.Operation);
		MemoryWriter << OperationType;

		MemoryWriter << const_cast<FHeightfieldModification&>(Delta).AffectedTile;

		// Serialize timestamp as Unix timestamp
		int64 UnixTimestamp = Delta.Timestamp.ToUnixTimestamp();
		MemoryWriter << UnixTimestamp;
	}

	return !MemoryWriter.IsError();
}

bool UHeightfieldService::DeserializeTerrainDeltas(const TArray<uint8>& InData, TArray<FHeightfieldModification>& OutDeltas) const
{
	if (InData.Num() == 0)
	{
		return true; // Empty data is valid
	}

	FMemoryReader MemoryReader(InData, true);

	// Read version number
	int32 Version = 0;
	MemoryReader << Version;

	if (Version != 1)
	{
		UE_LOG(LogHeightfieldService, Error, TEXT("Unsupported terrain delta version: %d"), Version);
		return false;
	}

	// Read number of deltas
	int32 DeltaCount = 0;
	MemoryReader << DeltaCount;

	if (DeltaCount < 0 || DeltaCount > 10000) // Sanity check
	{
		UE_LOG(LogHeightfieldService, Error, TEXT("Invalid delta count: %d"), DeltaCount);
		return false;
	}

	OutDeltas.Reserve(DeltaCount);

	// Read each delta
	for (int32 i = 0; i < DeltaCount; i++)
	{
		FHeightfieldModification Delta;

		MemoryReader << Delta.ModificationId;
		MemoryReader << Delta.Center;
		MemoryReader << Delta.Radius;
		MemoryReader << Delta.Strength;

		uint8 OperationType = 0;
		MemoryReader << OperationType;
		Delta.Operation = static_cast<EHeightfieldOperation>(OperationType);

		MemoryReader << Delta.AffectedTile;

		// Deserialize timestamp from Unix timestamp
		int64 UnixTimestamp = 0;
		MemoryReader << UnixTimestamp;
		Delta.Timestamp = FDateTime::FromUnixTimestamp(UnixTimestamp);

		OutDeltas.Add(Delta);
	}

	return !MemoryReader.IsError();
}

void UHeightfieldService::ApplyModificationsToTile(FTileCoord TileCoord, FHeightfieldData& HeightfieldData)
{
	FHeightfieldModificationList* List = TileModifications.Find(TileCoord);
	TArray<FHeightfieldModification>* TileDeltas = List ? &List->Modifications : nullptr;

	if (!TileDeltas || TileDeltas->Num() == 0)
	{
		return; // No modifications to apply
	}

	// Sort modifications by timestamp to apply them in chronological order
	TileDeltas->Sort([](const FHeightfieldModification& A, const FHeightfieldModification& B)
		{
			return A.Timestamp < B.Timestamp;
		});

	int32 AppliedCount = 0;
	for (const FHeightfieldModification& Modification : *TileDeltas)
	{
		// Create a temporary modification to apply to the cache
		FHeightfieldModification TempMod = Modification;

		// Apply the modification directly to the heightfield data
		ApplyModificationToHeightfield(HeightfieldData, TempMod);
		AppliedCount++;
	}

	if (AppliedCount > 0)
	{
		// Recalculate normals and slopes after applying all modifications
		CalculateNormalsAndSlopes(HeightfieldData);

		UE_LOG(LogHeightfieldService, Log, TEXT("Applied %d terrain modifications to tile (%d, %d)"),
			AppliedCount, TileCoord.X, TileCoord.Y);
	}
}

TArray<FHeightfieldModification> UHeightfieldService::GetTileModifications(FTileCoord TileCoord) const
{
	if (const FHeightfieldModificationList* List = TileModifications.Find(TileCoord))
	{
		return List->Modifications;
	}
	return {};
}

void UHeightfieldService::ApplyModificationToHeightfield(FHeightfieldData& HeightfieldData, const FHeightfieldModification& Modification)
{
	// Calculate tile world bounds
	FVector TileWorldPos = HeightfieldData.TileCoord.ToWorldPosition(64.0f);
	FVector2D TileStart(TileWorldPos.X - 32.0f, TileWorldPos.Y - 32.0f);

	const int32 Resolution = HeightfieldData.Resolution;
	const float SampleSpacing = 1.0f; // 1m per sample
	bool bDataModified = false;

	// Apply modification to affected samples
	for (int32 Y = 0; Y < Resolution; Y++)
	{
		for (int32 X = 0; X < Resolution; X++)
		{
			// Calculate world position of this sample
			FVector2D SampleWorldPos = TileStart + FVector2D(X * SampleSpacing, Y * SampleSpacing);

			// Calculate distance from modification center
			float Distance = FVector2D::Distance(SampleWorldPos, Modification.Center);
			if (Distance > Modification.Radius)
			{
				continue; // Outside modification radius
			}

			// Calculate falloff (1.0 at center, 0.0 at radius edge)
			float Falloff = FMath::Clamp(1.0f - (Distance / Modification.Radius), 0.0f, 1.0f);
			Falloff = FMath::SmoothStep(0.0f, 1.0f, Falloff); // Smooth falloff curve

			int32 SampleIndex = Y * Resolution + X;
			float& CurrentHeight = HeightfieldData.HeightData[SampleIndex];
			float OriginalHeight = CurrentHeight;

			// Apply operation
			switch (Modification.Operation)
			{
			case EHeightfieldOperation::Add:
			{
				CurrentHeight += Modification.Strength * Falloff;
				break;
			}
			case EHeightfieldOperation::Subtract:
			{
				CurrentHeight -= Modification.Strength * Falloff;
				break;
			}
			case EHeightfieldOperation::Flatten:
			{
				float TargetHeight = 0.0f; // Sea level
				CurrentHeight = FMath::Lerp(CurrentHeight, TargetHeight, Modification.Strength * Falloff);
				break;
			}
			case EHeightfieldOperation::Smooth:
			{
				// Smooth by averaging with neighbors
				float AverageHeight = 0.0f;
				int32 NeighborCount = 0;

				// Sample 3x3 neighborhood
				for (int32 DY = -1; DY <= 1; DY++)
				{
					for (int32 DX = -1; DX <= 1; DX++)
					{
						int32 NeighborX = X + DX;
						int32 NeighborY = Y + DY;

						if (NeighborX >= 0 && NeighborX < Resolution &&
							NeighborY >= 0 && NeighborY < Resolution)
						{
							int32 NeighborIndex = NeighborY * Resolution + NeighborX;
							AverageHeight += HeightfieldData.HeightData[NeighborIndex];
							NeighborCount++;
						}
					}
				}

				if (NeighborCount > 0)
				{
					AverageHeight /= NeighborCount;
					CurrentHeight = FMath::Lerp(CurrentHeight, AverageHeight, Modification.Strength * Falloff);
				}
				break;
			}
			}

			// Clamp to max terrain height
			CurrentHeight = FMath::Clamp(CurrentHeight, -WorldGenSettings.MaxTerrainHeight, WorldGenSettings.MaxTerrainHeight);

			if (FMath::Abs(CurrentHeight - OriginalHeight) > KINDA_SMALL_NUMBER)
			{
				bDataModified = true;
			}
		}
	}

	if (bDataModified)
	{
		// Update min/max heights
		float MinHeight = FLT_MAX;
		float MaxHeight = -FLT_MAX;
		for (float Height : HeightfieldData.HeightData)
		{
			MinHeight = FMath::Min(MinHeight, Height);
			MaxHeight = FMath::Max(MaxHeight, Height);
		}
		HeightfieldData.MinHeight = MinHeight;
		HeightfieldData.MaxHeight = MaxHeight;
	}
}
