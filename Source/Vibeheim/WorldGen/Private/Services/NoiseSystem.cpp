#include "Services/NoiseSystem.h"
#include "Engine/Engine.h"

DEFINE_LOG_CATEGORY_STATIC(LogNoiseSystem, Log, All);

UNoiseSystem::UNoiseSystem()
{
	Seed = 1337;
}

void UNoiseSystem::Initialize(int32 InSeed)
{
	Seed = InSeed;
	UE_LOG(LogNoiseSystem, Log, TEXT("Noise system initialized with seed %llu"), Seed);
}

float UNoiseSystem::GenerateNoise(FVector2D Position, const FAdvancedNoiseSettings& Settings) const
{
	// Apply domain warping if enabled
	FVector2D WarpedPosition = Position;
	if (Settings.DomainWarp.bEnabled)
	{
		WarpedPosition = ApplyDomainWarp(Position, Settings.DomainWarp);
	}

	// Add offset
	WarpedPosition += Settings.Offset;

	// Generate noise based on type
	float NoiseValue = 0.0f;
	switch (Settings.NoiseType)
	{
		case ENoiseType::Perlin:
			NoiseValue = GeneratePerlinNoise(WarpedPosition, Settings.Scale);
			break;
		case ENoiseType::Simplex:
			NoiseValue = GenerateSimplexNoise(WarpedPosition, Settings.Scale);
			break;
		case ENoiseType::Ridge:
			NoiseValue = GenerateRidgeNoise(WarpedPosition, Settings.Scale, Settings.RidgeSharpness);
			break;
		case ENoiseType::Billow:
			NoiseValue = GenerateBillowNoise(WarpedPosition, Settings.Scale, Settings.BillowBias);
			break;
		case ENoiseType::Voronoi:
			NoiseValue = GenerateVoronoiNoise(WarpedPosition, Settings.Scale);
			break;
		default:
			NoiseValue = GeneratePerlinNoise(WarpedPosition, Settings.Scale);
			break;
	}

	return NoiseValue * Settings.Amplitude;
}

float UNoiseSystem::GenerateSimpleNoise(FVector2D Position, float Scale, float Amplitude, int32 NoiseTypeIndex) const
{
	FAdvancedNoiseSettings Settings;
	Settings.NoiseType = static_cast<ENoiseType>(FMath::Clamp(NoiseTypeIndex, 0u, static_cast<int32>(ENoiseType::MAX) - 1));
	Settings.Scale = Scale;
	Settings.Amplitude = Amplitude;
	Settings.Octaves = 1;

	return GenerateNoise(Position, Settings);
}

float UNoiseSystem::GenerateSimpleNoise(FVector2D Position, float Scale, float Amplitude) const
{
	// Call the full version with default noise type 0 (Perlin)
	return GenerateSimpleNoise(Position, Scale, Amplitude, 0);
}

float UNoiseSystem::GenerateOctaveNoise(FVector2D Position, const FAdvancedNoiseSettings& Settings) const
{
	float NoiseValue = 0.0f;
	float Amplitude = Settings.Amplitude;
	float Frequency = Settings.Scale;
	float MaxValue = 0.0f;

	FAdvancedNoiseSettings OctaveSettings = Settings;
	OctaveSettings.Octaves = 1; // Process one octave at a time

	for (int32 Octave = 0; Octave < Settings.Octaves; Octave++)
	{
		OctaveSettings.Scale = Frequency;
		OctaveSettings.Amplitude = Amplitude;
		OctaveSettings.Offset = Settings.Offset + FVector2D(Octave * 100.0f, Octave * 100.0f); // Offset each octave

		NoiseValue += GenerateNoise(Position, OctaveSettings);
		MaxValue += Amplitude;

		Amplitude *= Settings.Persistence;
		Frequency *= Settings.Lacunarity;
	}

	// Normalize to [-1, 1] range
	return MaxValue > 0.0f ? NoiseValue / MaxValue : 0.0f;
}

FVector2D UNoiseSystem::ApplyDomainWarp(FVector2D Position, const FDomainWarpSettings& WarpSettings) const
{
	if (!WarpSettings.bEnabled || WarpSettings.Strength <= 0.0f)
	{
		return Position;
	}

	// Generate warp offsets using simple noise
	FAdvancedNoiseSettings WarpNoiseSettings;
	WarpNoiseSettings.NoiseType = ENoiseType::Perlin;
	WarpNoiseSettings.Scale = WarpSettings.Scale;
	WarpNoiseSettings.Amplitude = 1.0f;
	WarpNoiseSettings.Octaves = WarpSettings.Octaves;
	WarpNoiseSettings.Offset = WarpSettings.Offset;

	float WarpX = GenerateOctaveNoise(Position, WarpNoiseSettings);
	
	WarpNoiseSettings.Offset = WarpSettings.Offset + FVector2D(1000.0f, 1000.0f);
	float WarpY = GenerateOctaveNoise(Position, WarpNoiseSettings);

	return Position + FVector2D(WarpX, WarpY) * WarpSettings.Strength;
}

TArray<float> UNoiseSystem::GenerateNoiseField(FVector2D StartPosition, int32 Width, int32 Height, float SampleSpacing, const FAdvancedNoiseSettings& Settings) const
{
	TArray<float> NoiseField;
	NoiseField.Reserve(Width * Height);

	for (int32 Y = 0; Y < Height; Y++)
	{
		for (int32 X = 0; X < Width; X++)
		{
			FVector2D SamplePosition = StartPosition + FVector2D(X * SampleSpacing, Y * SampleSpacing);
			float NoiseValue = GenerateOctaveNoise(SamplePosition, Settings);
			NoiseField.Add(NoiseValue);
		}
	}

	return NoiseField;
}float
 UNoiseSystem::GeneratePerlinNoise(FVector2D Position, float Scale) const
{
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
	float SmoothX = SmoothStep(FracX);
	float SmoothY = SmoothStep(FracY);
	
	// Generate gradients at corners
	float N00 = Gradient(Hash(X0, Y0, 0), FracX, FracY);
	float N10 = Gradient(Hash(X1, Y0, 0), FracX - 1.0f, FracY);
	float N01 = Gradient(Hash(X0, Y1, 0), FracX, FracY - 1.0f);
	float N11 = Gradient(Hash(X1, Y1, 0), FracX - 1.0f, FracY - 1.0f);
	
	// Interpolate
	float N0 = Lerp(N00, N10, SmoothX);
	float N1 = Lerp(N01, N11, SmoothX);
	
	return Lerp(N0, N1, SmoothY);
}

float UNoiseSystem::GenerateSimplexNoise(FVector2D Position, float Scale) const
{
	// Simplified 2D simplex noise implementation
	FVector2D ScaledPos = Position * Scale;
	
	// Skew the input space to determine which simplex cell we're in
	const float F2 = 0.5f * (FMath::Sqrt(3.0f) - 1.0f);
	float s = (ScaledPos.X + ScaledPos.Y) * F2;
	int32 i = FMath::FloorToInt(ScaledPos.X + s);
	int32 j = FMath::FloorToInt(ScaledPos.Y + s);
	
	const float G2 = (3.0f - FMath::Sqrt(3.0f)) / 6.0f;
	float t = (i + j) * G2;
	float X0 = i - t;
	float Y0 = j - t;
	float x0 = ScaledPos.X - X0;
	float y0 = ScaledPos.Y - Y0;
	
	// Determine which simplex we are in
	int32 i1, j1;
	if (x0 > y0)
	{
		i1 = 1; j1 = 0; // Lower triangle, XY order: (0,0)->(1,0)->(1,1)
	}
	else
	{
		i1 = 0; j1 = 1; // Upper triangle, YX order: (0,0)->(0,1)->(1,1)
	}
	
	// Calculate the three corners
	float x1 = x0 - i1 + G2;
	float y1 = y0 - j1 + G2;
	float x2 = x0 - 1.0f + 2.0f * G2;
	float y2 = y0 - 1.0f + 2.0f * G2;
	
	// Calculate the contribution from the three corners
	float n0 = 0.0f, n1 = 0.0f, n2 = 0.0f;
	
	float t0 = 0.5f - x0 * x0 - y0 * y0;
	if (t0 >= 0.0f)
	{
		t0 *= t0;
		n0 = t0 * t0 * Gradient(Hash(i, j, 1), x0, y0);
	}
	
	float t1 = 0.5f - x1 * x1 - y1 * y1;
	if (t1 >= 0.0f)
	{
		t1 *= t1;
		n1 = t1 * t1 * Gradient(Hash(i + i1, j + j1, 1), x1, y1);
	}
	
	float t2 = 0.5f - x2 * x2 - y2 * y2;
	if (t2 >= 0.0f)
	{
		t2 *= t2;
		n2 = t2 * t2 * Gradient(Hash(i + 1, j + 1, 1), x2, y2);
	}
	
	// Add contributions from each corner to get the final noise value
	return 70.0f * (n0 + n1 + n2);
}

float UNoiseSystem::GenerateRidgeNoise(FVector2D Position, float Scale, float Sharpness) const
{
	float NoiseValue = GeneratePerlinNoise(Position, Scale);
	
	// Create ridges by taking absolute value and inverting
	NoiseValue = FMath::Abs(NoiseValue);
	NoiseValue = 1.0f - NoiseValue;
	
	// Apply sharpness
	if (Sharpness != 1.0f)
	{
		NoiseValue = FMath::Pow(NoiseValue, Sharpness);
	}
	
	return NoiseValue * 2.0f - 1.0f; // Remap to [-1, 1]
}

float UNoiseSystem::GenerateBillowNoise(FVector2D Position, float Scale, float Bias) const
{
	float NoiseValue = GeneratePerlinNoise(Position, Scale);
	
	// Create billowy effect by taking absolute value
	NoiseValue = FMath::Abs(NoiseValue);
	
	// Apply bias
	NoiseValue += Bias;
	
	return FMath::Clamp(NoiseValue, -1.0f, 1.0f);
}

float UNoiseSystem::GenerateVoronoiNoise(FVector2D Position, float Scale) const
{
	FVector2D ScaledPos = Position * Scale;
	
	// Find the closest Voronoi point
	FVector2D ClosestPoint = FindClosestVoronoiPoint(ScaledPos, 1.0f);
	
	// Calculate distance to closest point
	float Distance = FVector2D::Distance(ScaledPos, ClosestPoint);
	
	// Normalize and remap to [-1, 1]
	return FMath::Clamp(Distance * 2.0f - 1.0f, -1.0f, 1.0f);
}

uint32 UNoiseSystem::Hash(int32 X, int32 Y, uint32 NoiseType) const
{
	uint32 Hash = static_cast<uint32>(X);
	Hash = Hash * 1664525u + 1013904223u;

	Hash ^= static_cast<uint32>(Y);
	Hash = Hash * 1664525u + 1013904223u;

	// Mix the (presumably 32-bit) Seed into a pseudo "high" part safely.
	const uint64 Seed64 = static_cast<uint64>(static_cast<uint32>(Seed));
	const uint32 SeedHiMix = static_cast<uint32>((Seed64 * 0x9E3779B185EBCA87ull) >> 32); // golden ratio mix

	Hash ^= static_cast<uint32>(Seed);   // low 32
	Hash = Hash * 1664525u + 1013904223u;

	Hash ^= SeedHiMix;                    // replaces the old (Seed >> 32)
	Hash = Hash * 1664525u + 1013904223u;

	Hash ^= NoiseType;
	Hash = Hash * 1664525u + 1013904223u;

	// Optional final avalanche to improve bit diffusion (fast, 32-bit friendly)
	Hash ^= Hash >> 16;
	Hash *= 0x7feb352dU;
	Hash ^= Hash >> 15;
	Hash *= 0x846ca68bU;
	Hash ^= Hash >> 16;

	return Hash;
}

uint32 UNoiseSystem::HashFloat(float X, float Y, uint32 NoiseType) const
{
	return Hash(static_cast<int32>(X * 1000.0f), static_cast<int32>(Y * 1000.0f), NoiseType);
}

float UNoiseSystem::SmoothStep(float t) const
{
	return t * t * (3.0f - 2.0f * t);
}

float UNoiseSystem::Lerp(float a, float b, float t) const
{
	return a + t * (b - a);
}

float UNoiseSystem::Gradient(uint32 Hash, float X, float Y) const
{
	// Use hash to select gradient vector
	uint32 h = Hash & 3;
	float u = h < 2 ? X : Y;
	float v = h < 2 ? Y : X;
	return ((h & 1) ? -u : u) + ((h & 2) ? -2.0f * v : 2.0f * v);
}

FVector2D UNoiseSystem::FindClosestVoronoiPoint(FVector2D Position, float Scale) const
{
	// Get the cell coordinates
	int32 CellX = FMath::FloorToInt(Position.X);
	int32 CellY = FMath::FloorToInt(Position.Y);
	
	FVector2D ClosestPoint = FVector2D::ZeroVector;
	float ClosestDistance = FLT_MAX;
	
	// Check 3x3 grid of cells around the current cell
	for (int32 DY = -1; DY <= 1; DY++)
	{
		for (int32 DX = -1; DX <= 1; DX++)
		{
			int32 CheckX = CellX + DX;
			int32 CheckY = CellY + DY;
			
			// Generate random point within this cell
			uint32 CellHash = Hash(CheckX, CheckY, 4);
			float RandomX = static_cast<float>(CellHash & 0xFFFF) / 65535.0f;
			CellHash >>= 16;
			float RandomY = static_cast<float>(CellHash & 0xFFFF) / 65535.0f;
			
			FVector2D CellPoint = FVector2D(CheckX + RandomX, CheckY + RandomY);
			float Distance = FVector2D::Distance(Position, CellPoint);
			
			if (Distance < ClosestDistance)
			{
				ClosestDistance = Distance;
				ClosestPoint = CellPoint;
			}
		}
	}
	
	return ClosestPoint;
}
