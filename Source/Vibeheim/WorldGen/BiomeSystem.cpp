#include "BiomeSystem.h"
#include "Engine/Engine.h"
#include "Math/UnrealMathUtility.h"

void FBiomeSystem::Initialize(const FWorldGenSettings& InSettings)
{
    Settings = InSettings;
    NoiseGenerator.Initialize(InSettings);
    InitializeDefaultBiomeData();
}

FBiomeEvaluation FBiomeSystem::EvaluateBiome(float WorldX, float WorldY, const FIntVector& ChunkCoord) const
{
    FBiomeEvaluation Result;
    
    // Calculate biome weights
    Result.BiomeWeights = CalculateBiomeWeights(WorldX, WorldY, ChunkCoord);
    
    // Apply blending
    ApplyBiomeBlending(Result.BiomeWeights, WorldX, WorldY, ChunkCoord);
    
    // Normalize weights
    NormalizeBiomeWeights(Result.BiomeWeights);
    
    // Calculate blended properties
    Result.BlendedHeightOffset = CalculateBlendedHeightOffset(Result.BiomeWeights);
    Result.BlendedDebugColor = CalculateBlendedDebugColor(Result.BiomeWeights);
    
    return Result;
}

const FBiomeData& FBiomeSystem::GetBiomeData(EBiomeType BiomeType) const
{
    int32 Index = static_cast<int32>(BiomeType);
    if (Index >= 0 && Index < BiomeDataArray.Num())
    {
        return BiomeDataArray[Index];
    }
    
    // Return first biome as fallback
    return BiomeDataArray[0];
}

float FBiomeSystem::GenerateBiomeNoise(EBiomeType BiomeType, float WorldX, float WorldY, const FIntVector& ChunkCoord) const
{
    float Scale = 0.0025f; // Default scale
    
    // Get biome-specific scale from settings
    switch (BiomeType)
    {
        case EBiomeType::Meadows:
            Scale = Settings.MeadowsScale;
            break;
        case EBiomeType::BlackForest:
            Scale = Settings.BlackForestScale;
            break;
        case EBiomeType::Swamp:
            Scale = Settings.SwampScale;
            break;
        default:
            break;
    }
    
    // Get the appropriate feature tag for this biome
    ENoiseFeatureTag FeatureTag = GetBiomeFeatureTag(BiomeType);
    
    // Use NoiseGenerator for deterministic Perlin noise
    return NoiseGenerator.GeneratePerlinNoise(WorldX, WorldY, Scale, FeatureTag, ChunkCoord);
}

FBiomeWeights FBiomeSystem::CalculateBiomeWeights(float WorldX, float WorldY, const FIntVector& ChunkCoord) const
{
    FBiomeWeights Weights;
    
    // Generate noise for each biome
    for (int32 i = 0; i < static_cast<int32>(EBiomeType::Count); ++i)
    {
        EBiomeType BiomeType = static_cast<EBiomeType>(i);
        Weights.Weights[i] = GenerateBiomeNoise(BiomeType, WorldX, WorldY, ChunkCoord);
    }
    
    // Find dominant biome (max weight wins)
    float MaxWeight = 0.0f;
    int32 DominantIndex = 0;
    
    for (int32 i = 0; i < static_cast<int32>(EBiomeType::Count); ++i)
    {
        if (Weights.Weights[i] > MaxWeight)
        {
            MaxWeight = Weights.Weights[i];
            DominantIndex = i;
        }
    }
    
    Weights.DominantBiome = static_cast<EBiomeType>(DominantIndex);
    
    return Weights;
}

void FBiomeSystem::NormalizeBiomeWeights(FBiomeWeights& InOutWeights) const
{
    // Calculate sum of all weights
    float WeightSum = 0.0f;
    for (int32 i = 0; i < static_cast<int32>(EBiomeType::Count); ++i)
    {
        WeightSum += InOutWeights.Weights[i];
    }
    
    // Normalize weights to sum to 1.0
    if (WeightSum > 0.0f)
    {
        for (int32 i = 0; i < static_cast<int32>(EBiomeType::Count); ++i)
        {
            InOutWeights.NormalizedWeights[i] = InOutWeights.Weights[i] / WeightSum;
        }
    }
    else
    {
        // Fallback: equal weights
        float EqualWeight = 1.0f / static_cast<float>(static_cast<int32>(EBiomeType::Count));
        for (int32 i = 0; i < static_cast<int32>(EBiomeType::Count); ++i)
        {
            InOutWeights.NormalizedWeights[i] = EqualWeight;
        }
    }
}

void FBiomeSystem::ApplyBiomeBlending(FBiomeWeights& InOutWeights, float WorldX, float WorldY, const FIntVector& ChunkCoord) const
{
    // Apply 24m blend zones between biomes
    const float BlendDistance = Settings.BiomeBlendMeters;
    
    // For each biome, check distance to other biomes and apply blending
    for (int32 i = 0; i < static_cast<int32>(EBiomeType::Count); ++i)
    {
        float BlendFactor = 1.0f;
        
        // Sample neighboring points to determine blend factor
        const float SampleDistance = BlendDistance * 0.5f;
        const int32 NumSamples = 8;
        
        for (int32 Sample = 0; Sample < NumSamples; ++Sample)
        {
            float Angle = (2.0f * UE_PI * Sample) / NumSamples;
            float SampleX = WorldX + FMath::Cos(Angle) * SampleDistance;
            float SampleY = WorldY + FMath::Sin(Angle) * SampleDistance;
            
            // Generate noise for current biome at sample point
            EBiomeType CurrentBiome = static_cast<EBiomeType>(i);
            float SampleNoise = GenerateBiomeNoise(CurrentBiome, SampleX, SampleY, ChunkCoord);
            
            // If sample noise is significantly different, apply blending
            float NoiseDifference = FMath::Abs(InOutWeights.Weights[i] - SampleNoise);
            if (NoiseDifference > 0.1f) // Threshold for blending
            {
                float Distance = FVector2D(SampleX - WorldX, SampleY - WorldY).Size();
                float BlendWeight = FMath::Clamp(Distance / BlendDistance, 0.0f, 1.0f);
                BlendFactor = FMath::Min(BlendFactor, BlendWeight);
            }
        }
        
        // Apply blend factor to weight
        InOutWeights.Weights[i] *= BlendFactor;
    }
}

float FBiomeSystem::CalculateBlendedHeightOffset(const FBiomeWeights& BiomeWeights) const
{
    float BlendedOffset = 0.0f;
    
    // Blend height offsets based on normalized weights
    for (int32 i = 0; i < static_cast<int32>(EBiomeType::Count); ++i)
    {
        EBiomeType BiomeType = static_cast<EBiomeType>(i);
        const FBiomeData& BiomeData = GetBiomeData(BiomeType);
        BlendedOffset += BiomeData.HeightOffset * BiomeWeights.NormalizedWeights[i];
    }
    
    return BlendedOffset;
}

FLinearColor FBiomeSystem::CalculateBlendedDebugColor(const FBiomeWeights& BiomeWeights) const
{
    FLinearColor BlendedColor = FLinearColor::Black;
    
    // Blend debug colors based on normalized weights
    for (int32 i = 0; i < static_cast<int32>(EBiomeType::Count); ++i)
    {
        EBiomeType BiomeType = static_cast<EBiomeType>(i);
        const FBiomeData& BiomeData = GetBiomeData(BiomeType);
        BlendedColor += BiomeData.DebugColor * BiomeWeights.NormalizedWeights[i];
    }
    
    return BlendedColor;
}

void FBiomeSystem::InitializeDefaultBiomeData()
{
    BiomeDataArray.Empty();
    BiomeDataArray.SetNum(static_cast<int32>(EBiomeType::Count));
    
    // Initialize Meadows biome
    BiomeDataArray[static_cast<int32>(EBiomeType::Meadows)] = FBiomeData(
        TEXT("Meadows"),
        0.0f,           // Height offset
        Settings.MeadowsScale,
        FLinearColor(0.3f, 0.8f, 0.3f, 1.0f) // Bright green
    );
    
    // Initialize BlackForest biome
    BiomeDataArray[static_cast<int32>(EBiomeType::BlackForest)] = FBiomeData(
        TEXT("BlackForest"),
        50.0f,          // Height offset (elevated)
        Settings.BlackForestScale,
        FLinearColor(0.1f, 0.3f, 0.1f, 1.0f) // Very dark green
    );
    
    // Initialize Swamp biome
    BiomeDataArray[static_cast<int32>(EBiomeType::Swamp)] = FBiomeData(
        TEXT("Swamp"),
        -25.0f,         // Height offset (lowered)
        Settings.SwampScale,
        FLinearColor(0.5f, 0.4f, 0.2f, 1.0f) // Muddy brown
    );
}

ENoiseFeatureTag FBiomeSystem::GetBiomeFeatureTag(EBiomeType BiomeType) const
{
    switch (BiomeType)
    {
        case EBiomeType::Meadows:
            return ENoiseFeatureTag::BiomeMeadows;
        case EBiomeType::BlackForest:
            return ENoiseFeatureTag::BiomeBlackForest;
        case EBiomeType::Swamp:
            return ENoiseFeatureTag::BiomeSwamp;
        default:
            return ENoiseFeatureTag::BiomeMeadows; // Fallback
    }
}

