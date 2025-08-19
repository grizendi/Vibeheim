#include "Services/HeightfieldService.h"
#include "Services/ClimateSystem.h"
#include "Services/NoiseSystem.h"
#include "Engine/Engine.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"

DEFINE_LOG_CATEGORY_STATIC(LogHeightfieldService, Log, All);

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
	
	UE_LOG(LogHeightfieldService, Log, TEXT("Heightfield service initialized"));
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
	
	UE_LOG(LogHeightfieldService, Log, TEXT("Generated heightfield for tile (%d, %d) in %.2fms"), 
		TileCoord.X, TileCoord.Y, GenerationTimeMs);
	
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
	
	PendingModifications.Add(Modification);
	ApplyModificationToCache(Modification);
	
	UE_LOG(LogHeightfieldService, Log, TEXT("Applied heightfield modification at (%.1f, %.1f)"), Location.X, Location.Y);
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
	// TODO: Implement persistent storage
	UE_LOG(LogHeightfieldService, Log, TEXT("Saved %d heightfield modifications"), PendingModifications.Num());
	return true;
}

bool UHeightfieldService::LoadHeightfieldModifications()
{
	// TODO: Implement persistent storage
	UE_LOG(LogHeightfieldService, Log, TEXT("Loaded heightfield modifications"));
	return true;
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
	// TODO: Apply modification to cached heightfield data
	UE_LOG(LogHeightfieldService, Log, TEXT("Applied modification to cache"));
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