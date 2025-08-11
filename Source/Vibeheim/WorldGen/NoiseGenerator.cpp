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