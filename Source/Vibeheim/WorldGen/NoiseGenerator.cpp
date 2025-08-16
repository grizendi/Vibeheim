#include "NoiseGenerator.h"
#include "Engine/Engine.h"
#include "Math/UnrealMathUtility.h"

void FNoiseGenerator::Initialize(const FWorldGenSettings& InSettings)
{
    BaseSeed = InSettings.Seed;
    WorldGenVersion = InSettings.WorldGenVersion;
    PluginSHA = InSettings.PluginSHA;
    
    UE_LOG(LogTemp, Log, TEXT("NoiseGenerator initialized with Seed: %lld, Version: %d, SHA: %s"), 
           BaseSeed, WorldGenVersion, *PluginSHA);
}

float FNoiseGenerator::GeneratePerlinNoise(float X, float Y, float Scale, ENoiseFeatureTag FeatureTag, const FIntVector& ChunkCoord) const
{
    // Scale coordinates
    X *= Scale;
    Y *= Scale;
    
    // Get integer grid coordinates
    int32 X0 = FMath::FloorToInt(X);
    int32 Y0 = FMath::FloorToInt(Y);
    int32 X1 = X0 + 1;
    int32 Y1 = Y0 + 1;
    
    // Get fractional parts
    float FracX = X - X0;
    float FracY = Y - Y0;
    
    // Create mixed seed for this feature and chunk
    uint32 MixedSeed = CreateMixedSeed(FeatureTag, ChunkCoord);
    
    // Generate gradients for each corner
    float Grad00 = GenerateGradient(Hash2D(X0, Y0, MixedSeed), FracX, FracY);
    float Grad10 = GenerateGradient(Hash2D(X1, Y0, MixedSeed), FracX - 1.0f, FracY);
    float Grad01 = GenerateGradient(Hash2D(X0, Y1, MixedSeed), FracX, FracY - 1.0f);
    float Grad11 = GenerateGradient(Hash2D(X1, Y1, MixedSeed), FracX - 1.0f, FracY - 1.0f);
    
    // Apply smooth step to fractional coordinates
    float SmoothX = SmoothStep(FracX);
    float SmoothY = SmoothStep(FracY);
    
    // Interpolate gradients
    float InterpolateX0 = Lerp(Grad00, Grad10, SmoothX);
    float InterpolateX1 = Lerp(Grad01, Grad11, SmoothX);
    float FinalValue = Lerp(InterpolateX0, InterpolateX1, SmoothY);
    
    // Normalize from [-1, 1] to [0, 1]
    return (FinalValue + 1.0f) * 0.5f;
}

float FNoiseGenerator::GenerateOctaveNoise(float X, float Y, float Scale, int32 Octaves, float Persistence, float Lacunarity, ENoiseFeatureTag FeatureTag, const FIntVector& ChunkCoord) const
{
    float Result = 0.0f;
    float Amplitude = 1.0f;
    float Frequency = Scale;
    float MaxValue = 0.0f;
    
    for (int32 i = 0; i < Octaves; ++i)
    {
        Result += GeneratePerlinNoise(X, Y, Frequency, FeatureTag, ChunkCoord) * Amplitude;
        MaxValue += Amplitude;
        
        Amplitude *= Persistence;
        Frequency *= Lacunarity;
    }
    
    // Normalize to [0, 1]
    return MaxValue > 0.0f ? Result / MaxValue : 0.0f;
}

float FNoiseGenerator::GenerateRandomFloat(int32 X, int32 Y, ENoiseFeatureTag FeatureTag, const FIntVector& ChunkCoord) const
{
    uint32 MixedSeed = CreateMixedSeed(FeatureTag, ChunkCoord);
    uint32 Hash = Hash2D(X, Y, MixedSeed);
    return NormalizeHash(Hash);
}

int32 FNoiseGenerator::GenerateRandomInt(int32 X, int32 Y, ENoiseFeatureTag FeatureTag, const FIntVector& ChunkCoord) const
{
    uint32 MixedSeed = CreateMixedSeed(FeatureTag, ChunkCoord);
    return static_cast<int32>(Hash2D(X, Y, MixedSeed));
}

bool FNoiseGenerator::ValidateDeterminism(float TestX, float TestY, float TestScale, ENoiseFeatureTag FeatureTag, float ExpectedValue, float Tolerance) const
{
    float GeneratedValue = GeneratePerlinNoise(TestX, TestY, TestScale, FeatureTag);
    float Difference = FMath::Abs(GeneratedValue - ExpectedValue);
    return Difference <= Tolerance;
}

uint32 FNoiseGenerator::CreateMixedSeed(ENoiseFeatureTag FeatureTag, const FIntVector& ChunkCoord) const
{
    // Formula: Seed ^ Hash(ChunkCoord) ^ FeatureTag
    uint32 Seed32 = static_cast<uint32>(BaseSeed ^ (BaseSeed >> 32)); // Mix high and low bits of 64-bit seed
    uint32 ChunkHash = HashChunkCoord(ChunkCoord);
    uint32 FeatureTagValue = static_cast<uint32>(FeatureTag);
    
    return Seed32 ^ ChunkHash ^ FeatureTagValue;
}

uint32 FNoiseGenerator::HashChunkCoord(const FIntVector& ChunkCoord) const
{
    // Hash the chunk coordinate using a simple but effective method
    uint32 Hash = 0;
    Hash ^= static_cast<uint32>(ChunkCoord.X) * 374761393U;
    Hash ^= static_cast<uint32>(ChunkCoord.Y) * 668265263U;
    Hash ^= static_cast<uint32>(ChunkCoord.Z) * 1274126177U;
    Hash = (Hash ^ (Hash >> 13)) * 1274126177U;
    Hash ^= Hash >> 16;
    return Hash;
}

uint32 FNoiseGenerator::Hash2D(int32 X, int32 Y, uint32 Seed) const
{
    // High-quality hash function for 2D coordinates
    uint32 Hash = Seed;
    Hash ^= static_cast<uint32>(X) * 374761393U;
    Hash ^= static_cast<uint32>(Y) * 668265263U;
    Hash = (Hash ^ (Hash >> 13)) * 1274126177U;
    Hash ^= Hash >> 16;
    Hash = (Hash ^ (Hash >> 13)) * 1274126177U;
    Hash ^= Hash >> 16;
    return Hash;
}

float FNoiseGenerator::GenerateGradient(uint32 Hash, float X, float Y) const
{
    // Use hash to select one of 8 gradient vectors
    // This creates the characteristic Perlin noise pattern
    switch (Hash & 7)
    {
        case 0: return  X + Y;   // (1, 1)
        case 1: return -X + Y;   // (-1, 1)
        case 2: return  X - Y;   // (1, -1)
        case 3: return -X - Y;   // (-1, -1)
        case 4: return  X;       // (1, 0)
        case 5: return -X;       // (-1, 0)
        case 6: return  Y;       // (0, 1)
        case 7: return -Y;       // (0, -1)
        default: return 0.0f;
    }
}

float FNoiseGenerator::SmoothStep(float t) const
{
    // Improved smoothstep function: 6t^5 - 15t^4 + 10t^3
    // This provides better continuity than the basic 3t^2 - 2t^3
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

float FNoiseGenerator::Lerp(float a, float b, float t) const
{
    return a + t * (b - a);
}

float FNoiseGenerator::NormalizeHash(uint32 Hash) const
{
    // Convert hash to float in range [0, 1]
    return static_cast<float>(Hash) / static_cast<float>(UINT32_MAX);
}

float FNoiseGenerator::GenerateRidgedNoise(float X, float Y, float Scale, int32 Octaves, float Persistence, float Lacunarity, ENoiseFeatureTag FeatureTag, const FIntVector& ChunkCoord) const
{
    float Result = 0.0f;
    float Amplitude = 1.0f;
    float Frequency = Scale;
    float MaxValue = 0.0f;
    
    for (int32 i = 0; i < Octaves; ++i)
    {
        // Generate Perlin noise and apply ridged transformation
        float NoiseValue = GeneratePerlinNoise(X, Y, Frequency, FeatureTag, ChunkCoord);
        float RidgedValue = ApplyRidgedTransform(NoiseValue);
        
        Result += RidgedValue * Amplitude;
        MaxValue += Amplitude;
        
        Amplitude *= Persistence;
        Frequency *= Lacunarity;
    }
    
    // Normalize to [0, 1]
    return MaxValue > 0.0f ? Result / MaxValue : 0.0f;
}

float FNoiseGenerator::GenerateDomainWarpedNoise(float X, float Y, float Scale, float WarpStrength, ENoiseFeatureTag FeatureTag, const FIntVector& ChunkCoord) const
{
    // Calculate domain warp offset
    FVector2D WarpOffset = CalculateDomainWarp(X, Y, Scale * 0.5f, WarpStrength, ENoiseFeatureTag::DomainWarp, ChunkCoord);
    
    // Apply warp offset to coordinates
    float WarpedX = X + WarpOffset.X;
    float WarpedY = Y + WarpOffset.Y;
    
    // Generate noise at warped coordinates
    return GeneratePerlinNoise(WarpedX, WarpedY, Scale, FeatureTag, ChunkCoord);
}

float FNoiseGenerator::CalculateFlowAccumulation(float X, float Y, float HeightmapScale, float FlowThreshold, ENoiseFeatureTag FeatureTag, const FIntVector& ChunkCoord) const
{
    // Sample grid size for flow calculation (in world units)
    const float GridSize = 10.0f;
    const int32 FlowSteps = 20; // Maximum flow tracing steps
    
    float FlowAccumulation = 0.0f;
    float CurrentX = X;
    float CurrentY = Y;
    
    // Trace flow path using steepest descent
    for (int32 Step = 0; Step < FlowSteps; ++Step)
    {
        // Calculate gradient at current position
        FVector2D Gradient = CalculateGradient(CurrentX, CurrentY, HeightmapScale, FeatureTag, ChunkCoord);
        
        // If gradient is too small, we've reached a local minimum
        float GradientMagnitude = Gradient.Size();
        if (GradientMagnitude < 0.001f)
        {
            break;
        }
        
        // Accumulate flow based on gradient magnitude
        FlowAccumulation += GradientMagnitude;
        
        // Move in direction of steepest descent
        FVector2D FlowDirection = Gradient.GetSafeNormal();
        CurrentX += FlowDirection.X * GridSize;
        CurrentY += FlowDirection.Y * GridSize;
    }
    
    // Normalize flow accumulation and apply threshold
    float NormalizedFlow = FMath::Clamp(FlowAccumulation / FlowSteps, 0.0f, 1.0f);
    return NormalizedFlow > FlowThreshold ? NormalizedFlow : 0.0f;
}

float FNoiseGenerator::GenerateTerrainHeight(float X, float Y, float BaseScale, float RidgedScale, float WarpStrength, ENoiseFeatureTag FeatureTag, const FIntVector& ChunkCoord) const
{
    // Generate base terrain using domain-warped Perlin noise
    float BaseHeight = GenerateDomainWarpedNoise(X, Y, BaseScale, WarpStrength, FeatureTag, ChunkCoord);
    
    // Generate ridged noise for mountain features
    float RidgedHeight = GenerateRidgedNoise(X, Y, RidgedScale, 4, 0.5f, 2.0f, ENoiseFeatureTag::RidgedTerrain, ChunkCoord);
    
    // Combine base and ridged noise with weighted blending
    // Use base noise to modulate ridged contribution
    float RidgedWeight = BaseHeight * 0.7f + 0.3f; // Weight between 0.3 and 1.0
    float CombinedHeight = BaseHeight + (RidgedHeight * RidgedWeight * 0.5f);
    
    // Apply river carving if flow accumulation is high
    float FlowAccumulation = CalculateFlowAccumulation(X, Y, BaseScale, 0.3f, ENoiseFeatureTag::Rivers, ChunkCoord);
    if (FlowAccumulation > 0.0f)
    {
        // Carve river channels by reducing height
        float RiverDepth = FlowAccumulation * 0.2f; // Rivers carve up to 20% of terrain height
        CombinedHeight = FMath::Max(0.0f, CombinedHeight - RiverDepth);
    }
    
    return FMath::Clamp(CombinedHeight, 0.0f, 1.0f);
}

ENoiseAlgorithm FNoiseGenerator::SelectNoiseAlgorithm(ETerrainFeature Feature) const
{
    switch (Feature)
    {
        case ETerrainFeature::Mountains:
            return ENoiseAlgorithm::RidgedMultifractal;
        case ETerrainFeature::Valleys:
            return ENoiseAlgorithm::RidgedMultifractal; // Inverted ridged noise creates valleys
        case ETerrainFeature::Rivers:
            return ENoiseAlgorithm::FlowAccumulation;
        case ETerrainFeature::Erosion:
            return ENoiseAlgorithm::DomainWarped;
        case ETerrainFeature::Base:
        default:
            return ENoiseAlgorithm::Perlin;
    }
}

float FNoiseGenerator::GenerateNoiseWithAlgorithm(float X, float Y, float Scale, ENoiseAlgorithm Algorithm, ENoiseFeatureTag FeatureTag, const FIntVector& ChunkCoord) const
{
    switch (Algorithm)
    {
        case ENoiseAlgorithm::RidgedMultifractal:
            return GenerateRidgedNoise(X, Y, Scale, 4, 0.5f, 2.0f, FeatureTag, ChunkCoord);
        case ENoiseAlgorithm::DomainWarped:
            return GenerateDomainWarpedNoise(X, Y, Scale, 50.0f, FeatureTag, ChunkCoord);
        case ENoiseAlgorithm::FlowAccumulation:
            return CalculateFlowAccumulation(X, Y, Scale, 0.3f, FeatureTag, ChunkCoord);
        case ENoiseAlgorithm::Perlin:
        default:
            return GeneratePerlinNoise(X, Y, Scale, FeatureTag, ChunkCoord);
    }
}

FVector2D FNoiseGenerator::CalculateGradient(float X, float Y, float HeightmapScale, ENoiseFeatureTag FeatureTag, const FIntVector& ChunkCoord) const
{
    const float Delta = 1.0f; // Small offset for gradient calculation
    
    // Sample heights at neighboring points
    float HeightLeft = SampleHeightmap(X - Delta, Y, HeightmapScale, FeatureTag, ChunkCoord);
    float HeightRight = SampleHeightmap(X + Delta, Y, HeightmapScale, FeatureTag, ChunkCoord);
    float HeightDown = SampleHeightmap(X, Y - Delta, HeightmapScale, FeatureTag, ChunkCoord);
    float HeightUp = SampleHeightmap(X, Y + Delta, HeightmapScale, FeatureTag, ChunkCoord);
    
    // Calculate gradient using central differences
    float GradientX = (HeightRight - HeightLeft) / (2.0f * Delta);
    float GradientY = (HeightUp - HeightDown) / (2.0f * Delta);
    
    return FVector2D(GradientX, GradientY);
}

float FNoiseGenerator::SampleHeightmap(float X, float Y, float Scale, ENoiseFeatureTag FeatureTag, const FIntVector& ChunkCoord) const
{
    // Use octave noise for heightmap sampling to get more realistic terrain
    return GenerateOctaveNoise(X, Y, Scale, 4, 0.5f, 2.0f, FeatureTag, ChunkCoord);
}

float FNoiseGenerator::ApplyRidgedTransform(float NoiseValue) const
{
    // Transform noise to create sharp ridges
    // Formula: 1.0 - abs(2.0 * noise - 1.0)
    // This creates sharp peaks where noise was 0.5, and valleys at 0.0 and 1.0
    float CenteredNoise = 2.0f * NoiseValue - 1.0f; // Convert [0,1] to [-1,1]
    float RidgedValue = 1.0f - FMath::Abs(CenteredNoise); // Create ridges
    
    // Square the result to sharpen the ridges further
    return RidgedValue * RidgedValue;
}

FVector2D FNoiseGenerator::CalculateDomainWarp(float X, float Y, float Scale, float Strength, ENoiseFeatureTag FeatureTag, const FIntVector& ChunkCoord) const
{
    // Generate two independent noise patterns for X and Y displacement
    float WarpX = GeneratePerlinNoise(X, Y, Scale, FeatureTag, ChunkCoord) * 2.0f - 1.0f; // Convert to [-1,1]
    float WarpY = GeneratePerlinNoise(X + 1000.0f, Y + 1000.0f, Scale, FeatureTag, ChunkCoord) * 2.0f - 1.0f; // Offset to get different pattern
    
    // Apply strength multiplier
    return FVector2D(WarpX * Strength, WarpY * Strength);
}