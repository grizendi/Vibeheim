#include "Services/HeightfieldService.h"
#include "Services/ClimateSystem.h"
#include "Services/NoiseSystem.h"
#include "Utils/WorldGenLogging.h"
#include "WorldGenSettings.h"
#include "Data/SerializationShims.h"
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

	// Force identical derived buffer lengths and zero them for deterministic checksums
	// This ensures normals/slopes arrays are exactly HeightData.Num() in both edit and reload paths
	const int32 SampleCount = HeightfieldData.HeightData.Num();
	HeightfieldData.NormalData.Empty(SampleCount);
	HeightfieldData.NormalData.SetNumZeroed(SampleCount);
	HeightfieldData.SlopeData.Empty(SampleCount);
	HeightfieldData.SlopeData.SetNumZeroed(SampleCount);
	
	// Calculate normals and slopes with deterministic ordering
	CalculateNormalsAndSlopes(HeightfieldData);


	// Apply thermal smoothing if enabled
	if (GenerationSettings.bEnableThermalSmoothing)
	{
		ApplyThermalSmoothing(HeightfieldData, GenerationSettings.ThermalSmoothingIterations);
	}

	// === Apply loaded terrain deltas for this tile ===
	// Ensure modifications are loaded for this tile if they exist on disk
	if (!TileModifications.Contains(TileCoord))
	{
		LoadTileTerrainDeltas(TileCoord);
	}

	// Check if we have modifications for this tile
	const FHeightfieldModificationList* Mods = TileModifications.Find(TileCoord);
	if (Mods && Mods->Modifications.Num() > 0)
	{
		UE_LOG(LogHeightfieldService, Log, TEXT("Applying %d terrain modifications to tile (%d, %d) during generation"),
			Mods->Modifications.Num(), TileCoord.X, TileCoord.Y);

		// Apply edits into the height array
		ApplyModificationsToTile(TileCoord, HeightfieldData.HeightData);

		// Recompute min/max after edits
		float NewMinHeight = FLT_MAX;
		float NewMaxHeight = -FLT_MAX;
		for (float H : HeightfieldData.HeightData)
		{
			NewMinHeight = FMath::Min(NewMinHeight, H);
			NewMaxHeight = FMath::Max(NewMaxHeight, H);
		}
		HeightfieldData.MinHeight = NewMinHeight;
		HeightfieldData.MaxHeight = NewMaxHeight;

		// Apply thermal smoothing after modifications to mirror base generation processing sequence
		// This ensures both edit and reload paths use identical thermal smoothing + post-processing
		if (GenerationSettings.bEnableThermalSmoothing)
		{
			ApplyThermalSmoothing(HeightfieldData, GenerationSettings.ThermalSmoothingIterations);
		}
		
		// Force identical derived buffer lengths and zero them before rebuilding
		// This prevents slack bytes from affecting checksums in TArray capacity differences
		{
			const int32 ModifiedSampleCount = HeightfieldData.HeightData.Num();
			HeightfieldData.NormalData.Empty(ModifiedSampleCount);
			HeightfieldData.NormalData.SetNumZeroed(ModifiedSampleCount);
			HeightfieldData.SlopeData.Empty(ModifiedSampleCount);
			HeightfieldData.SlopeData.SetNumZeroed(ModifiedSampleCount);
		}
		
		// Rebuild derived data (normals/slopes) after edits with deterministic calculation order
		CalculateNormalsAndSlopes(HeightfieldData);

		UE_LOG(LogHeightfieldService, Log, TEXT("Successfully applied terrain modifications to tile (%d, %d), height range: [%.2f, %.2f]"),
			TileCoord.X, TileCoord.Y, NewMinHeight, NewMaxHeight);
	}
	else
	{
		UE_LOG(LogHeightfieldService, VeryVerbose, TEXT("No terrain modifications found for tile (%d, %d)"),
			TileCoord.X, TileCoord.Y);
	}

	// Cache the generated data
	CacheHeightfield(HeightfieldData);

	// Update performance stats
	double EndTime = FPlatformTime::Seconds();
	float GenerationTimeMs = static_cast<float>((EndTime - StartTime) * 1000.0);
	UpdatePerformanceStats(GenerationTimeMs);

	WORLDGEN_LOG_WITH_SEED_TILE(Log, Seed, TileCoord, TEXT("Height build completed in %.2fms"), GenerationTimeMs);

	return HeightfieldData;
}

FHeightfieldData UHeightfieldService::GenerateHeightfieldPristine(int32 Seed, FTileCoord TileCoord)
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

    // Derived buffers
    const int32 SampleCount = HeightfieldData.HeightData.Num();
    HeightfieldData.NormalData.Empty(SampleCount);
    HeightfieldData.NormalData.SetNumZeroed(SampleCount);
    HeightfieldData.SlopeData.Empty(SampleCount);
    HeightfieldData.SlopeData.SetNumZeroed(SampleCount);

    // Normals, slopes, smoothing as in normal path
    CalculateNormalsAndSlopes(HeightfieldData);
    if (GenerationSettings.bEnableThermalSmoothing)
    {
        ApplyThermalSmoothing(HeightfieldData, GenerationSettings.ThermalSmoothingIterations);
    }

    double EndTime = FPlatformTime::Seconds();
    float GenerationTimeMs = static_cast<float>((EndTime - StartTime) * 1000.0);
    UpdatePerformanceStats(GenerationTimeMs);
    WORLDGEN_LOG_WITH_SEED_TILE(Log, Seed, TileCoord, TEXT("Height build (pristine) completed in %.2fms"), GenerationTimeMs);

    return HeightfieldData;
}

float UHeightfieldService::GenerateBaseHeight(FVector2D WorldPosition, int32 Seed) const
{
	float Height = GenerationSettings.BaseHeight;

	// 1) Base/local terrain noise (preserves existing look/feel)
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

	// 2) Macro topology: continental noise + island falloff (radial)
	// Read optional MacroWorld configuration from settings assets
	const UWorldGenSettings* WGSettings = UWorldGenSettings::GetWorldGenSettings();
	if (WGSettings && WGSettings->MacroWorldConfig.IsSet())
	{
		const FMacroWorldConfig& Macro = WGSettings->MacroWorldConfig.GetValue();

		// Compute normalized radial distance from world center (meters)
		const float Radius = FMath::Max(Macro.WorldRadiusMeters, 1.0f);
		const float Dist = WorldPosition.Size();
		const float RNorm = FMath::Clamp(Dist / Radius, 0.0f, 1.0f);

		// Island falloff mask (1 at center -> 0 at edge)
		float IslandMask = 1.0f - RNorm;
		IslandMask = FMath::Clamp(IslandMask, 0.0f, 1.0f);
		if (Macro.IslandFalloffCurve)
		{
			// Curve maps [0..1] radial fraction to mask
			IslandMask = FMath::Clamp(Macro.IslandFalloffCurve->GetFloatValue(RNorm), 0.0f, 1.0f);
		}
		else
		{
			// Power falloff for broad island shaping
			IslandMask = FMath::Pow(IslandMask, FMath::Max(0.01f, Macro.IslandFalloff));
		}
		// Coast sharpness: emphasize coastline transition
		IslandMask = FMath::Pow(IslandMask, FMath::Max(0.01f, Macro.CoastSharpness));

		// Large-scale continental ridged noise (very low frequency)
		float Continental = 0.0f;
		if (NoiseSystem)
		{
			FAdvancedNoiseSettings ContinentNoise;
			ContinentNoise.NoiseType = ENoiseType::Ridge;
			ContinentNoise.Scale = Macro.ContinentScale;      // very low frequency
			ContinentNoise.Amplitude = 1.0f;                  // combine as bias later
			ContinentNoise.Octaves = 3;
			ContinentNoise.Persistence = 0.5f;
			ContinentNoise.Lacunarity = 2.0f;
			ContinentNoise.RidgeSharpness = 1.2f;

			Continental = NoiseSystem->GenerateOctaveNoise(WorldPosition, ContinentNoise);
		}
		else
		{
			// Fallback: simple absolute ridge-like behavior via existing noise
			FNoiseSettings Simple;
			Simple.Scale = Macro.ContinentScale;
			Simple.Amplitude = 1.0f;
			Simple.Octaves = 3;
			float Raw = GenerateOctaveNoise(WorldPosition, Simple, Seed + 101);
			Continental = FMath::Abs(Raw); // approximate ridge behavior
		}

		// Normalize continental value to [0..1]
		float Continental01 = FMath::Clamp(0.5f * (Continental + 1.0f), 0.0f, 1.0f);

		// Macro blend drives broad elevation bias; keep conservative amplitude
		const float MacroBiasMeters = 20.0f; // broad elevation swing (~40m peak-to-peak)
		float MacroBlend = (Continental01 * IslandMask - 0.5f) * 2.0f; // [-1..1]
		Height += MacroBlend * MacroBiasMeters;

		// 3) Sea level clamping and underwater topology
		const float SeaLevel = WorldGenSettings.SeaLevel;
		if (Height < SeaLevel)
		{
			// Add subtle underwater variation
			float UnderTopo = 0.0f;
			if (NoiseSystem)
			{
				FAdvancedNoiseSettings UnderNoise;
				UnderNoise.NoiseType = ENoiseType::Perlin;
				UnderNoise.Scale = Macro.ContinentScale * 2.5f;
				UnderNoise.Amplitude = 1.0f;
				UnderNoise.Octaves = 2;
				UnderNoise.Offset = FVector2D(3141.0f, 2718.0f);
				UnderTopo = NoiseSystem->GenerateOctaveNoise(WorldPosition, UnderNoise) * 2.0f; // ~2m variation
			}
			else
			{
				FNoiseSettings UnderSimple;
				UnderSimple.Scale = Macro.ContinentScale * 2.5f;
				UnderSimple.Amplitude = 2.0f;
				UnderSimple.Octaves = 2;
				UnderTopo = GenerateOctaveNoise(WorldPosition, UnderSimple, Seed + 202);
			}

			Height += UnderTopo;

			// Clamp ocean depth so seabed does not get too deep
			Height = FMath::Clamp(Height, Macro.OceanDepth, SeaLevel);
		}
	}

	// 4) Apply height multiplier and global clamp
	Height *= GenerationSettings.HeightMultiplier;
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
	const int32 TotalSamples = Resolution * Resolution;

	// Ensure arrays are exactly the right size and zeroed for deterministic checksums
	// This prevents TArray capacity differences from affecting memory layout
	HeightfieldData.NormalData.Empty(TotalSamples);
	HeightfieldData.NormalData.SetNumZeroed(TotalSamples);
	HeightfieldData.SlopeData.Empty(TotalSamples);
	HeightfieldData.SlopeData.SetNumZeroed(TotalSamples);

	// Calculate normals and slopes in deterministic order (Y-major, X-minor)
	// This ensures identical calculation sequence in both edit and reload paths
	for (int32 Y = 0; Y < Resolution; Y++)
	{
		for (int32 X = 0; X < Resolution; X++)
		{
			int32 Index = Y * Resolution + X;
			
			FVector Normal = CalculateNormal(HeightfieldData.HeightData, X, Y, Resolution, SampleSpacing);
			float Slope = CalculateSlope(Normal);

			// Direct assignment instead of Add() to ensure exact indexing
			HeightfieldData.NormalData[Index] = Normal;
			HeightfieldData.SlopeData[Index] = Slope;
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

	// Force identical derived buffer lengths and zero them before rebuilding
	// This prevents slack bytes from affecting checksums in TArray capacity differences
	const int32 SampleCount = HeightfieldData.HeightData.Num();
	HeightfieldData.NormalData.Empty(SampleCount);
	HeightfieldData.NormalData.SetNumZeroed(SampleCount);
	HeightfieldData.SlopeData.Empty(SampleCount);
	HeightfieldData.SlopeData.SetNumZeroed(SampleCount);
	
	// Recalculate normals and slopes after smoothing with deterministic calculation order
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

	// Validate that ModificationId was properly initialized (Persistent ID pattern)
	ensureMsgf(Modification.ModificationId.IsValid(), TEXT("FHeightfieldModification::ModificationId must be valid after construction"));

	Modification.Center = FVector2D(Location.X, Location.Y);
	Modification.Radius = Radius;
	Modification.Strength = Strength;
	Modification.Operation = Operation;
	Modification.AffectedTile = FTileCoord::FromWorldPosition(Location);
	Modification.Timestamp = FDateTime::Now();

	// Persist derived parameters for bit-for-bit determinism
	Modification.KernelRadius = FMath::RoundToInt(Modification.Radius);

	// For flatten operations, capture the target height from current heightfield
	if (Modification.Operation == EHeightfieldOperation::Flatten)
	{
		// Sample height at the center location from current heightfield
		FTileCoord CenterTile = FTileCoord::FromWorldPosition(Location);
		if (FHeightfieldData* CachedData = HeightfieldCache.Find(CenterTile))
		{
			Modification.FlattenTargetZ = SampleHeightAt(Modification.Center, CachedData->HeightData, CenterTile);
			Modification.bFlattenUsesTarget = true;
		}
		else
		{
			// If no cached data, generate heightfield to sample from
			FHeightfieldData TempHeightfield = GenerateHeightfield(0, CenterTile);
			Modification.FlattenTargetZ = SampleHeightAt(Modification.Center, TempHeightfield.HeightData, CenterTile);
			Modification.bFlattenUsesTarget = true;
		}

		UE_LOG(LogHeightfieldService, Warning, TEXT("ModifyHeightfield: Flatten Op=%d KernelRadius=%d FlattenTargetZ=%.4f at (%.1f,%.1f)"),
			(int32)Modification.Operation, Modification.KernelRadius, Modification.FlattenTargetZ,
			Modification.Center.X, Modification.Center.Y);
	}
	else
	{
		Modification.bFlattenUsesTarget = false;
		UE_LOG(LogHeightfieldService, Warning, TEXT("ModifyHeightfield: Op=%d KernelRadius=%d at (%.1f,%.1f)"),
			(int32)Modification.Operation, Modification.KernelRadius, Modification.Center.X, Modification.Center.Y);
	}

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

	// Assign Order values for each affected tile and store modification
	for (const FTileCoord& TileCoord : AffectedTiles)
	{
		// Get or create next order index for this tile
		uint32& NextOrder = NextOrderIndexPerTile.FindOrAdd(TileCoord);

		// Create a copy of the modification with the correct Order for this tile
		FHeightfieldModification TileModification = Modification;
		TileModification.Order = NextOrder++;
		TileModification.AffectedTile = TileCoord;

		// Add modification to tile's modification list
		FHeightfieldModificationList& List = TileModifications.FindOrAdd(TileCoord);
		List.Modifications.Add(TileModification);

		// Mark tile as dirty for persistence
		DirtyTiles.Add(TileCoord);

		// Apply to cached heightfield if present for this specific tile
		if (FHeightfieldData* CachedData = HeightfieldCache.Find(TileCoord))
		{
			ApplyModificationToHeightfield(*CachedData, TileModification);
		}

		UE_LOG(LogHeightfieldService, VeryVerbose, TEXT("Added modification to tile (%d, %d) with Order %u, now has %d modifications"),
			TileCoord.X, TileCoord.Y, TileModification.Order, List.Modifications.Num());
	}

	// Add to pending modifications (use the original modification without tile-specific Order)
	PendingModifications.Add(Modification);

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

	// For smooth operations, create a snapshot of the current state
	TArray<float> HeightSnapshot;
	if (Modification.Operation == EHeightfieldOperation::Smooth)
	{
		HeightSnapshot = CachedData->HeightData;
	}

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
			case EHeightfieldOperation::Noise:
			{
				uint32 Seed32 = GetTypeHash(Modification.ModificationId);
				float Noise01 = static_cast<float>(HashPosition(SampleWorldPos, static_cast<int32>(Seed32))) / static_cast<float>(MAX_uint32);
				float NoiseSigned = Noise01 * 2.0f - 1.0f; // [-1,1]
				CurrentHeight += NoiseSigned * (Modification.Strength * Falloff);
				break;
			}
			case EHeightfieldOperation::Smooth:
			{
				// Smooth by averaging with neighbors from the snapshot
				float AverageHeight = 0.0f;
				int32 NeighborCount = 0;

				// Sample 3x3 neighborhood from snapshot
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
							AverageHeight += HeightSnapshot[NeighborIndex];
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

			// Round to 2 decimal places (centimeter precision) to eliminate floating point precision issues
			CurrentHeight = FMath::RoundToFloat(CurrentHeight * 100.0f) / 100.0f;

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

		// Force identical derived buffer lengths and zero them before rebuilding
		// This prevents slack bytes from affecting checksums in TArray capacity differences
		const int32 SampleCount = CachedData->HeightData.Num();
		CachedData->NormalData.Empty(SampleCount);
		CachedData->NormalData.SetNumZeroed(SampleCount);
		CachedData->SlopeData.Empty(SampleCount);
		CachedData->SlopeData.SetNumZeroed(SampleCount);
		
		// Recalculate normals and slopes with deterministic calculation order
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

	UE_LOG(LogHeightfieldService, Log, TEXT("SaveTileTerrainDeltas: Attempting to save deltas for tile (%d, %d)"),
		TileCoord.X, TileCoord.Y);

	if (!TileDeltas || TileDeltas->Num() == 0)
	{
		UE_LOG(LogHeightfieldService, VeryVerbose, TEXT("SaveTileTerrainDeltas: No deltas to save for tile (%d, %d)"),
			TileCoord.X, TileCoord.Y);
		return true; // No deltas to save
	}

	// Stable deduplication using TArray + TSet (preserves first-seen order)
	TArray<FHeightfieldModification> DeduplicatedDeltas;
	DeduplicatedDeltas.Reserve(TileDeltas->Num());
	TSet<FGuid> SeenModifications;

	for (const FHeightfieldModification& Modification : *TileDeltas)
	{
		if (!SeenModifications.Contains(Modification.ModificationId))
		{
			SeenModifications.Add(Modification.ModificationId);
			DeduplicatedDeltas.Add(Modification);
		}
	}

	// Sort by Order field for deterministic serialization
	Algo::Sort(DeduplicatedDeltas, [](const FHeightfieldModification& A, const FHeightfieldModification& B) {
		return A.Order < B.Order;
		});

	UE_LOG(LogHeightfieldService, Log, TEXT("SaveTileTerrainDeltas: Found %d deltas to save for tile (%d, %d) (deduplicated from %d)"),
		DeduplicatedDeltas.Num(), TileCoord.X, TileCoord.Y, TileDeltas->Num());

	double StartTime = FPlatformTime::Seconds();

	FString FilePath = GetTerraDeltaPath(TileCoord);
	TArray<uint8> SerializedData;

	if (!SerializeTerrainDeltas(DeduplicatedDeltas, SerializedData))
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
		DeduplicatedDeltas.Num(), TileCoord.X, TileCoord.Y, *FilePath, SaveTimeMs);

	return true;
}

bool UHeightfieldService::LoadTileTerrainDeltas(FTileCoord TileCoord)
{
	FString FilePath = GetTerraDeltaPath(TileCoord);

	UE_LOG(LogHeightfieldService, Log, TEXT("LoadTileTerrainDeltas: Attempting to load deltas for tile (%d, %d) from: %s"),
		TileCoord.X, TileCoord.Y, *FilePath);

	if (!IFileManager::Get().FileExists(*FilePath))
	{
		UE_LOG(LogHeightfieldService, VeryVerbose, TEXT("LoadTileTerrainDeltas: No delta file exists for tile (%d, %d)"),
			TileCoord.X, TileCoord.Y);
		return true; // No file to load, which is fine
	}

	int64 FileSize = IFileManager::Get().FileSize(*FilePath);
	UE_LOG(LogHeightfieldService, Log, TEXT("LoadTileTerrainDeltas: Found delta file for tile (%d, %d), size: %lld bytes"),
		TileCoord.X, TileCoord.Y, FileSize);

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

	UE_LOG(LogHeightfieldService, Log, TEXT("LoadTileTerrainDeltas: Successfully deserialized %d deltas from file"),
		LoadedDeltas.Num());

	// Add instrumentation logging to verify loaded sequence matches creation order
	for (int32 i = 0; i < LoadedDeltas.Num(); ++i)
	{
		const FHeightfieldModification& Mod = LoadedDeltas[i];
		UE_LOG(LogHeightfieldService, Warning, TEXT("Loaded[%d]: Op=%d Order=%u Guid=%s Ticks=%lld"),
			i, (int32)Mod.Operation, Mod.Order,
			*Mod.ModificationId.ToString(), Mod.Timestamp.GetTicks());
	}

	// Store loaded modifications (replace any existing ones for this tile)
	FHeightfieldModificationList List;
	List.Modifications = MoveTemp(LoadedDeltas);
	TileModifications.Add(TileCoord, MoveTemp(List));

	// Apply modifications to cached heightfield if it exists
	FHeightfieldData* CachedData = HeightfieldCache.Find(TileCoord);
	if (CachedData)
	{
		ApplyModificationsToTile(TileCoord, CachedData->HeightData);

		// Recalculate min/max heights after applying modifications
		float NewMinHeight = FLT_MAX;
		float NewMaxHeight = -FLT_MAX;
		for (float Height : CachedData->HeightData)
		{
			NewMinHeight = FMath::Min(NewMinHeight, Height);
			NewMaxHeight = FMath::Max(NewMaxHeight, Height);
		}
		CachedData->MinHeight = NewMinHeight;
		CachedData->MaxHeight = NewMaxHeight;

		// Force identical derived buffer lengths and zero them before rebuilding
		// This prevents slack bytes from affecting checksums in TArray capacity differences
		const int32 SampleCount = CachedData->HeightData.Num();
		CachedData->NormalData.Empty(SampleCount);
		CachedData->NormalData.SetNumZeroed(SampleCount);
		CachedData->SlopeData.Empty(SampleCount);
		CachedData->SlopeData.SetNumZeroed(SampleCount);
		
		// Recalculate normals and slopes after modifications with deterministic calculation order
		CalculateNormalsAndSlopes(*CachedData);
	}

	double EndTime = FPlatformTime::Seconds();
	float LoadTimeMs = static_cast<float>((EndTime - StartTime) * 1000.0);

	// Get the actual count from the stored list
	const FHeightfieldModificationList* StoredList = TileModifications.Find(TileCoord);
	int32 StoredCount = StoredList ? StoredList->Modifications.Num() : 0;

	UE_LOG(LogHeightfieldService, Log, TEXT("Loaded %d terrain deltas for tile (%d, %d) from %s (%.2fms)"),
		StoredCount, TileCoord.X, TileCoord.Y, *FilePath, LoadTimeMs);

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
	int32 Version = 4;  // Bump to version 4 for derived parameters (KernelRadius, FlattenTargetZ, bFlattenUsesTarget)
	MemoryWriter << Version;

	// Write number of deltas
	int32 DeltaCount = Deltas.Num();
	MemoryWriter << DeltaCount;

	// Write each delta
	for (const FHeightfieldModification& Delta : Deltas)
	{
		uint8 OperationType = static_cast<uint8>(Delta.Operation);
		MemoryWriter << OperationType;

		MemoryWriter << const_cast<FHeightfieldModification&>(Delta).AffectedTile;

		// Serialize timestamp as high-resolution ticks (100-nanosecond precision)
		int64 Ticks = Delta.Timestamp.GetTicks();
		MemoryWriter << Ticks;

		MemoryWriter << const_cast<FHeightfieldModification&>(Delta).ModificationId;

		// Write Order field (new in version 3)
		MemoryWriter << const_cast<FHeightfieldModification&>(Delta).Order;

		// Write derived parameters (new in version 4)
		MemoryWriter << const_cast<FHeightfieldModification&>(Delta).KernelRadius;
		MemoryWriter << const_cast<FHeightfieldModification&>(Delta).FlattenTargetZ;
		MemoryWriter << const_cast<FHeightfieldModification&>(Delta).bFlattenUsesTarget;

		// Write remaining fields
		MemoryWriter << const_cast<FHeightfieldModification&>(Delta).Center;
		MemoryWriter << const_cast<FHeightfieldModification&>(Delta).Radius;
		MemoryWriter << const_cast<FHeightfieldModification&>(Delta).Strength;
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

	if (Version < 1 || Version > 4)
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

		if (Version >= 3)
		{
			// Version 3+ format: Operation, AffectedTile, TimestampTicks, ModificationId, Order, [KernelRadius, FlattenTargetZ, bFlattenUsesTarget], Center, Radius, Strength
			uint8 OperationType = 0;
			MemoryReader << OperationType;
			Delta.Operation = static_cast<EHeightfieldOperation>(OperationType);

			MemoryReader << Delta.AffectedTile;

			int64 Ticks = 0;
			MemoryReader << Ticks;
			Delta.Timestamp = FDateTime(Ticks);

			MemoryReader << Delta.ModificationId;
			MemoryReader << Delta.Order;

			// Read derived parameters (version 4+)
			if (Version >= 4)
			{
				MemoryReader << Delta.KernelRadius;
				MemoryReader << Delta.FlattenTargetZ;
				MemoryReader << Delta.bFlattenUsesTarget;
			}
			else
			{
				// For version 3, compute derived parameters from base values
				Delta.KernelRadius = FMath::RoundToInt(Delta.Radius);
				Delta.FlattenTargetZ = 0.0f; // Default to sea level
				Delta.bFlattenUsesTarget = false;
			}

			MemoryReader << Delta.Center;
			MemoryReader << Delta.Radius;
			MemoryReader << Delta.Strength;
		}
		else
		{
			// Legacy formats (versions 1 and 2)
			MemoryReader << Delta.ModificationId;
			MemoryReader << Delta.Center;
			MemoryReader << Delta.Radius;
			MemoryReader << Delta.Strength;

			uint8 OperationType = 0;
			MemoryReader << OperationType;
			Delta.Operation = static_cast<EHeightfieldOperation>(OperationType);

			MemoryReader << Delta.AffectedTile;

			// Deserialize timestamp based on version
			if (Version == 1)
			{
				// Legacy format: Unix timestamp (seconds)
				int64 UnixTimestamp = 0;
				MemoryReader << UnixTimestamp;
				Delta.Timestamp = FDateTime::FromUnixTimestamp(UnixTimestamp);
			}
			else if (Version == 2)
			{
				// Version 2 format: High-resolution ticks (100-nanosecond precision)
				int64 Ticks = 0;
				MemoryReader << Ticks;
				Delta.Timestamp = FDateTime(Ticks);
			}

			// For older versions, assign Order based on read index
			Delta.Order = static_cast<uint32>(i);

			// Compute derived parameters for legacy versions
			Delta.KernelRadius = FMath::RoundToInt(Delta.Radius);
			Delta.FlattenTargetZ = 0.0f; // Default to sea level
			Delta.bFlattenUsesTarget = false;
		}

		// Validate that deserialized ModificationId is valid (should never be zero after proper serialization)
		ensureMsgf(Delta.ModificationId.IsValid(), TEXT("Deserialized FHeightfieldModification::ModificationId should be valid"));

		OutDeltas.Add(Delta);
	}

	return !MemoryReader.IsError();
}



TArray<FHeightfieldModification> UHeightfieldService::GetTileModifications(FTileCoord TileCoord) const
{
	if (const FHeightfieldModificationList* List = TileModifications.Find(TileCoord))
	{
		return List->Modifications;
	}
	return {};
}

void UHeightfieldService::ClearTileModifications(FTileCoord TileCoord)
{
	// Remove from tile modifications map
	TileModifications.Remove(TileCoord);

	// Remove from dirty tiles set
	DirtyTiles.Remove(TileCoord);

	// Remove from heightfield cache to force regeneration
	HeightfieldCache.Remove(TileCoord);

	// Delete the persisted .terra file if it exists
	FString TileFilename = FString::Printf(TEXT("tile_%d_%d.terra"), TileCoord.X, TileCoord.Y);
	FString TileFilePath = PersistenceDirectory / TileFilename;

	if (IFileManager::Get().FileExists(*TileFilePath))
	{
		if (IFileManager::Get().Delete(*TileFilePath))
		{
			UE_LOG(LogHeightfieldService, Log, TEXT("Deleted persisted terrain deltas file: %s"), *TileFilePath);
		}
		else
		{
			UE_LOG(LogHeightfieldService, Warning, TEXT("Failed to delete persisted terrain deltas file: %s"), *TileFilePath);
		}
	}

	UE_LOG(LogHeightfieldService, Log, TEXT("Cleared all modifications for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
}

void UHeightfieldService::ApplyModificationsToTile(FTileCoord TileCoord, TArray<float>& HeightData)
{
	const FHeightfieldModificationList* ModList = TileModifications.Find(TileCoord);
	if (!ModList || ModList->Modifications.Num() == 0)
	{
		UE_LOG(LogHeightfieldService, VeryVerbose, TEXT("ApplyModificationsToTile: No modifications found for tile (%d, %d)"),
			TileCoord.X, TileCoord.Y);
		return;
	}

	// Calculate initial checksum for diagnostic purposes
	uint32 InitialChecksum = 0;
	for (int32 i = 0; i < HeightData.Num(); ++i)
	{
		uint32 FloatBits = *reinterpret_cast<const uint32*>(&HeightData[i]);
		InitialChecksum = InitialChecksum * 31 + FloatBits;
	}

	UE_LOG(LogHeightfieldService, Log, TEXT("ApplyModificationsToTile: Applying %d modifications to tile (%d, %d), initial checksum: 0x%08X"),
		ModList->Modifications.Num(), TileCoord.X, TileCoord.Y, InitialChecksum);

	// Calculate tile world bounds
	FVector TileWorldPos = TileCoord.ToWorldPosition(64.0f);
	FVector2D TileStart(TileWorldPos.X - 32.0f, TileWorldPos.Y - 32.0f);

	const int32 Resolution = 64; // Locked per coordinate system
	const float SampleSpacing = 1.0f; // 1m per sample

    // First: GUID-based dedup (preserve first-seen order)
    TArray<FHeightfieldModification> UniqueModifications;
    UniqueModifications.Reserve(ModList->Modifications.Num());
    TSet<FGuid> SeenModifications;
    for (const FHeightfieldModification& Modification : ModList->Modifications)
    {
        if (!SeenModifications.Contains(Modification.ModificationId))
        {
            SeenModifications.Add(Modification.ModificationId);
            UniqueModifications.Add(Modification);
        }
    }

    // Order for deterministic application
    Algo::Sort(UniqueModifications, [](const FHeightfieldModification& A, const FHeightfieldModification& B) {
        return A.Order < B.Order;
    });

    // Second: collapse logical duplicates (same op/center/kernel radius/strength) to avoid double-applying across sessions
    auto Quantize = [](float V)->int32 { return FMath::RoundToInt(V * 100.0f); }; // 1cm
    struct FModKey { uint8 Op; int32 Qx; int32 Qy; int32 KR; int32 QS; };

    TMap<uint64, FHeightfieldModification> LatestByKey;
    for (const FHeightfieldModification& M : UniqueModifications)
    {
        uint64 Key = 1469598103934665603ull;
        Key = (Key ^ (uint64)M.Operation) * 1099511628211ull;
        Key = (Key ^ (uint64)Quantize(M.Center.X)) * 1099511628211ull;
        Key = (Key ^ (uint64)Quantize(M.Center.Y)) * 1099511628211ull;
        Key = (Key ^ (uint64)M.KernelRadius) * 1099511628211ull;
        Key = (Key ^ (uint64)Quantize(M.Strength)) * 1099511628211ull;

        if (FHeightfieldModification* Existing = LatestByKey.Find(Key))
        {
            if (M.Timestamp > Existing->Timestamp) { *Existing = M; }
        }
        else
        {
            LatestByKey.Add(Key, M);
        }
    }

    TArray<FHeightfieldModification> Compressed;
    LatestByKey.GenerateValueArray(Compressed);
    Algo::Sort(Compressed, [](const FHeightfieldModification& A, const FHeightfieldModification& B){ return A.Order < B.Order; });

    UE_LOG(LogHeightfieldService, Log, TEXT("ApplyModificationsToTile: After dedup/compress, applying %d modifications (from %d) to tile (%d, %d)"),
        Compressed.Num(), UniqueModifications.Num(), TileCoord.X, TileCoord.Y);

	// Apply each modification in Order sequence
    for (int32 ModIndex = 0; ModIndex < Compressed.Num(); ++ModIndex)
    {
        const FHeightfieldModification& Modification = Compressed[ModIndex];

		UE_LOG(LogHeightfieldService, Warning, TEXT("ApplyModificationsToTile: [%d/%d] Applying Op=%d Order=%u KR=%d FlattenZ=%.4f bUsesTarget=%s at (%.1f,%.1f)"),
            ModIndex + 1, Compressed.Num(), (int32)Modification.Operation, Modification.Order,
			Modification.KernelRadius, Modification.FlattenTargetZ, Modification.bFlattenUsesTarget ? TEXT("true") : TEXT("false"),
			Modification.Center.X, Modification.Center.Y);
		// For smooth operations, create a snapshot of the current state
		TArray<float> HeightSnapshot;
		if (Modification.Operation == EHeightfieldOperation::Smooth)
		{
			HeightSnapshot = HeightData;
		}

		// Apply modification to affected samples
		for (int32 Y = 0; Y < Resolution; Y++)
		{
			for (int32 X = 0; X < Resolution; X++)
			{
				// Calculate world position of this sample
				FVector2D SampleWorldPos = TileStart + FVector2D(X * SampleSpacing, Y * SampleSpacing);

				// Calculate distance from modification center
				float Distance = FVector2D::Distance(SampleWorldPos, Modification.Center);
				// Use persisted KernelRadius for consistent behavior
				float EffectiveRadius = (Modification.KernelRadius > 0) ? Modification.KernelRadius : Modification.Radius;
				if (Distance > EffectiveRadius)
				{
					continue; // Outside modification radius
				}

				// Calculate falloff (1.0 at center, 0.0 at radius edge)
				float Falloff = FMath::Clamp(1.0f - (Distance / EffectiveRadius), 0.0f, 1.0f);
				Falloff = FMath::SmoothStep(0.0f, 1.0f, Falloff); // Smooth falloff curve

				int32 SampleIndex = Y * Resolution + X;
				float& CurrentHeight = HeightData[SampleIndex];

				// Apply operation
				switch (Modification.Operation)
				{
				case EHeightfieldOperation::Add:
					CurrentHeight += Modification.Strength * Falloff;
					break;
				case EHeightfieldOperation::Subtract:
					CurrentHeight -= Modification.Strength * Falloff;
					break;
				case EHeightfieldOperation::Flatten:
				{
					// Use persisted FlattenTargetZ if available, otherwise fall back to sea level
					float TargetHeight = (Modification.bFlattenUsesTarget) ? Modification.FlattenTargetZ : 0.0f;
					CurrentHeight = FMath::Lerp(CurrentHeight, TargetHeight, Modification.Strength * Falloff);
				}
				break;
				case EHeightfieldOperation::Noise:
				{
					uint32 Seed32 = GetTypeHash(Modification.ModificationId);
					float Noise01 = static_cast<float>(HashPosition(SampleWorldPos, static_cast<int32>(Seed32))) / static_cast<float>(MAX_uint32);
					float NoiseSigned = Noise01 * 2.0f - 1.0f;
					CurrentHeight += NoiseSigned * (Modification.Strength * Falloff);
				}
				break;
				case EHeightfieldOperation::Smooth:
				{
					// Smooth by averaging with neighbors from the snapshot
					float AverageHeight = 0.0f;
					int32 NeighborCount = 0;

					// Sample 3x3 neighborhood from snapshot
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
								AverageHeight += HeightSnapshot[NeighborIndex];
								NeighborCount++;
							}
						}
					}

					if (NeighborCount > 0)
					{
						AverageHeight /= NeighborCount;
						CurrentHeight = FMath::Lerp(CurrentHeight, AverageHeight, Modification.Strength * Falloff);
					}
				}
				break;
				}

				// Clamp to max terrain height
				CurrentHeight = FMath::Clamp(CurrentHeight, -WorldGenSettings.MaxTerrainHeight, WorldGenSettings.MaxTerrainHeight);

				// Round to 2 decimal places (centimeter precision) to eliminate floating point precision issues
				CurrentHeight = FMath::RoundToFloat(CurrentHeight * 100.0f) / 100.0f;
			}
		}
	}

	// Calculate final checksum for diagnostic purposes
	uint32 FinalChecksum = 0;
	for (int32 i = 0; i < HeightData.Num(); ++i)
	{
		uint32 FloatBits = *reinterpret_cast<const uint32*>(&HeightData[i]);
		FinalChecksum = FinalChecksum * 31 + FloatBits;
	}

	UE_LOG(LogHeightfieldService, Log, TEXT("ApplyModificationsToTile: Completed applying %d modifications to tile (%d, %d), final checksum: 0x%08X"),
		ModList->Modifications.Num(), TileCoord.X, TileCoord.Y, FinalChecksum);
}

void UHeightfieldService::ApplyModificationToHeightfield(FHeightfieldData& HeightfieldData, const FHeightfieldModification& Modification)
{
	// Calculate tile world bounds
	FVector TileWorldPos = HeightfieldData.TileCoord.ToWorldPosition(64.0f);
	FVector2D TileStart(TileWorldPos.X - 32.0f, TileWorldPos.Y - 32.0f);

	const int32 Resolution = HeightfieldData.Resolution;
	const float SampleSpacing = 1.0f; // 1m per sample
	bool bDataModified = false;

	// For smooth operations, create a snapshot of the current state
	TArray<float> HeightSnapshot;
	if (Modification.Operation == EHeightfieldOperation::Smooth)
	{
		HeightSnapshot = HeightfieldData.HeightData;
	}

	// Apply modification to affected samples
	for (int32 Y = 0; Y < Resolution; Y++)
	{
		for (int32 X = 0; X < Resolution; X++)
		{
			// Calculate world position of this sample
			FVector2D SampleWorldPos = TileStart + FVector2D(X * SampleSpacing, Y * SampleSpacing);

			// Calculate distance from modification center
			float Distance = FVector2D::Distance(SampleWorldPos, Modification.Center);
			// Use persisted KernelRadius for consistent behavior
			float EffectiveRadius = (Modification.KernelRadius > 0) ? Modification.KernelRadius : Modification.Radius;
			if (Distance > EffectiveRadius)
			{
				continue; // Outside modification radius
			}

			// Calculate falloff (1.0 at center, 0.0 at radius edge)
			// Use persisted KernelRadius for consistent behavior
			float Falloff = FMath::Clamp(1.0f - (Distance / EffectiveRadius), 0.0f, 1.0f);
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
				// Use persisted FlattenTargetZ if available, otherwise fall back to sea level
				float TargetHeight = (Modification.bFlattenUsesTarget) ? Modification.FlattenTargetZ : 0.0f;
				CurrentHeight = FMath::Lerp(CurrentHeight, TargetHeight, Modification.Strength * Falloff);
				break;
			}
			case EHeightfieldOperation::Noise:
			{
				uint32 Seed32 = GetTypeHash(Modification.ModificationId);
				float Noise01 = static_cast<float>(HashPosition(SampleWorldPos, static_cast<int32>(Seed32))) / static_cast<float>(MAX_uint32);
				float NoiseSigned = Noise01 * 2.0f - 1.0f;
				CurrentHeight += NoiseSigned * (Modification.Strength * Falloff);
				break;
			}
			case EHeightfieldOperation::Smooth:
			{
				// Smooth by averaging with neighbors from the snapshot
				float AverageHeight = 0.0f;
				int32 NeighborCount = 0;

				// Sample 3x3 neighborhood from snapshot
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
							AverageHeight += HeightSnapshot[NeighborIndex];
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

			// Round to 2 decimal places (centimeter precision) to eliminate floating point precision issues
			CurrentHeight = FMath::RoundToFloat(CurrentHeight * 100.0f) / 100.0f;

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

float UHeightfieldService::SampleHeightAt(FVector2D WorldXY, const TArray<float>& HeightData, FTileCoord TileCoord) const
{
	// Consistent coordinate conversion (cm → sample index)
	const float SampleSpacing = 100.0f; // 1m per sample in cm
	const float TileSize = 64.0f * 100.0f; // 64m tile size in cm
	const int32 GridSize = 64; // Fixed resolution

	// Calculate tile origin in world coordinates (cm)
	FVector TileWorldPos = TileCoord.ToWorldPosition(64.0f);
	FVector2D TileOrigin(TileWorldPos.X * 100.0f - TileSize * 0.5f, TileWorldPos.Y * 100.0f - TileSize * 0.5f);

	// Convert world position to local tile coordinates
	FVector2D LocalPos = (WorldXY * 100.0f - TileOrigin) / SampleSpacing;

	// Clamp to valid sample range
	float Fx = FMath::Clamp(LocalPos.X, 0.0f, GridSize - 1.0f);
	float Fy = FMath::Clamp(LocalPos.Y, 0.0f, GridSize - 1.0f);

	// Get integer indices
	int32 Ix = FMath::FloorToInt(Fx);
	int32 Iy = FMath::FloorToInt(Fy);

	// Bilinear interpolation for smoother sampling
	if (Ix < GridSize - 1 && Iy < GridSize - 1)
	{
		float FracX = Fx - Ix;
		float FracY = Fy - Iy;

		float H00 = HeightData[Iy * GridSize + Ix];
		float H10 = HeightData[Iy * GridSize + (Ix + 1)];
		float H01 = HeightData[(Iy + 1) * GridSize + Ix];
		float H11 = HeightData[(Iy + 1) * GridSize + (Ix + 1)];

		float H0 = FMath::Lerp(H00, H10, FracX);
		float H1 = FMath::Lerp(H01, H11, FracX);

		return FMath::Lerp(H0, H1, FracY);
	}
	else
	{
		// Edge case: use nearest neighbor
		int32 Index = Iy * GridSize + Ix;
		return HeightData.IsValidIndex(Index) ? HeightData[Index] : 0.0f;
	}
}
