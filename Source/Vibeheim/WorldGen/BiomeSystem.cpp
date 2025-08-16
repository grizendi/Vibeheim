#include "BiomeSystem.h"
#include "Engine/Engine.h"
#include "Math/UnrealMathUtility.h"

void FBiomeSystem::Initialize(const FWorldGenSettings& InSettings)
{
    Settings = InSettings;
    NoiseGenerator.Initialize(InSettings);
    InitializeDefaultBiomeData();
    InitializeEnhancedBiomeData();
}

FBiomeEvaluation FBiomeSystem::EvaluateBiome(float WorldX, float WorldY, float TerrainHeight, const FIntVector& ChunkCoord) const
{
    FBiomeEvaluation Result;
    Result.TerrainHeight = TerrainHeight;
    
    // Calculate base biome weights from noise
    Result.BiomeWeights = CalculateBiomeWeights(WorldX, WorldY, ChunkCoord);
    
    // Apply height-based biome override if applicable
    Result.bHeightBasedOverride = ApplyHeightBasedBiomeOverride(Result.BiomeWeights, TerrainHeight, WorldX, WorldY);
    
    // Apply blending
    ApplyBiomeBlending(Result.BiomeWeights, WorldX, WorldY, ChunkCoord);
    
    // Normalize weights
    NormalizeBiomeWeights(Result.BiomeWeights);
    
    // Calculate blended properties
    Result.BlendedHeightOffset = CalculateBlendedHeightOffset(Result.BiomeWeights);
    Result.BlendedDebugColor = CalculateBlendedDebugColor(Result.BiomeWeights);
    
    // Calculate enhanced material properties
    Result.BlendedMaterial = CalculateBlendedMaterial(Result.BiomeWeights);
    Result.BlendedBiomeColor = CalculateBlendedBiomeColor(Result.BiomeWeights);
    CalculateBlendedMaterialParameters(Result.BiomeWeights, Result.BlendedRoughness, Result.BlendedMetallic);
    
    // Set dominant biome information
    Result.DominantBiome = GetEnhancedBiomeData(Result.BiomeWeights.DominantBiome).BiomeName;
    if (Result.bHeightBasedOverride)
    {
        Result.HeightBasedBiomeType = Result.BiomeWeights.DominantBiome;
    }
    
    return Result;
}

const FEnhancedBiomeData& FBiomeSystem::GetEnhancedBiomeData(EBiomeType BiomeType) const
{
    int32 Index = static_cast<int32>(BiomeType);
    if (Index >= 0 && Index < EnhancedBiomeDataArray.Num())
    {
        return EnhancedBiomeDataArray[Index];
    }
    
    // Return first biome as fallback
    return EnhancedBiomeDataArray[0];
}

const FBiomeData& FBiomeSystem::GetBiomeData(EBiomeType BiomeType) const
{
    int32 Index = static_cast<int32>(BiomeType);
    if (Index >= 0 && Index < LegacyBiomeDataArray.Num())
    {
        return LegacyBiomeDataArray[Index];
    }
    
    // Return first biome as fallback
    return LegacyBiomeDataArray[0];
}

float FBiomeSystem::GenerateBiomeNoise(EBiomeType BiomeType, float WorldX, float WorldY, const FIntVector& ChunkCoord) const
{
    // Height-based biomes don't use noise generation
    if (BiomeType == EBiomeType::Mountains || BiomeType == EBiomeType::Ocean)
    {
        return 0.0f;
    }
    
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
        const FEnhancedBiomeData& BiomeData = GetEnhancedBiomeData(BiomeType);
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
        const FEnhancedBiomeData& BiomeData = GetEnhancedBiomeData(BiomeType);
        BlendedColor += BiomeData.DebugColor * BiomeWeights.NormalizedWeights[i];
    }
    
    return BlendedColor;
}

void FBiomeSystem::InitializeDefaultBiomeData()
{
    LegacyBiomeDataArray.Empty();
    LegacyBiomeDataArray.SetNum(static_cast<int32>(EBiomeType::Count));
    
    // Initialize Meadows biome
    LegacyBiomeDataArray[static_cast<int32>(EBiomeType::Meadows)] = FBiomeData(
        TEXT("Meadows"),
        0.0f,           // Height offset
        Settings.MeadowsScale,
        FLinearColor(0.3f, 0.8f, 0.3f, 1.0f) // Bright green
    );
    
    // Initialize BlackForest biome
    LegacyBiomeDataArray[static_cast<int32>(EBiomeType::BlackForest)] = FBiomeData(
        TEXT("BlackForest"),
        50.0f,          // Height offset (elevated)
        Settings.BlackForestScale,
        FLinearColor(0.1f, 0.3f, 0.1f, 1.0f) // Very dark green
    );
    
    // Initialize Swamp biome
    LegacyBiomeDataArray[static_cast<int32>(EBiomeType::Swamp)] = FBiomeData(
        TEXT("Swamp"),
        -25.0f,         // Height offset (lowered)
        Settings.SwampScale,
        FLinearColor(0.5f, 0.4f, 0.2f, 1.0f) // Muddy brown
    );
    
    // Initialize Mountains biome (height-based)
    LegacyBiomeDataArray[static_cast<int32>(EBiomeType::Mountains)] = FBiomeData(
        TEXT("Mountains"),
        100.0f,         // Height offset (elevated)
        0.0f,           // No noise scale (height-based)
        FLinearColor(0.8f, 0.8f, 0.9f, 1.0f) // Light gray/blue
    );
    
    // Initialize Ocean biome (height-based)
    LegacyBiomeDataArray[static_cast<int32>(EBiomeType::Ocean)] = FBiomeData(
        TEXT("Ocean"),
        -50.0f,         // Height offset (lowered)
        0.0f,           // No noise scale (height-based)
        FLinearColor(0.2f, 0.4f, 0.8f, 1.0f) // Blue
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
        case EBiomeType::Mountains:
            return ENoiseFeatureTag::BiomeMeadows; // Mountains don't use noise
        case EBiomeType::Ocean:
            return ENoiseFeatureTag::BiomeMeadows; // Ocean doesn't use noise
        default:
            return ENoiseFeatureTag::BiomeMeadows; // Fallback
    }
}


void FBiomeSystem::InitializeEnhancedBiomeData()
{
    EnhancedBiomeDataArray.Empty();
    EnhancedBiomeDataArray.SetNum(static_cast<int32>(EBiomeType::Count));
    
    // Initialize Meadows biome with vegetation
    FEnhancedBiomeData& MeadowsBiome = EnhancedBiomeDataArray[static_cast<int32>(EBiomeType::Meadows)];
    MeadowsBiome = FEnhancedBiomeData(TEXT("Meadows"), 0.0f, Settings.MeadowsScale, FLinearColor(0.3f, 0.8f, 0.3f, 1.0f));
    MeadowsBiome.BiomeColor = FLinearColor(0.4f, 0.8f, 0.3f, 1.0f); // Bright green
    MeadowsBiome.MaterialRoughness = 0.8f;
    MeadowsBiome.MaterialMetallic = 0.0f;
    
    // Add meadows vegetation
    FTreeSpeciesData OakTree;
    OakTree.SpeciesName = TEXT("Oak");
    OakTree.SpawnProbability = 0.7f;
    OakTree.SizeRange = FVector2D(0.8f, 1.2f);
    OakTree.HarvestableResources.Add(TEXT("Wood"));
    OakTree.HarvestableResources.Add(TEXT("Acorns"));
    MeadowsBiome.VegetationProfile.TreeSpecies.Add(OakTree);
    
    FFoliageTypeData Grass;
    Grass.FoliageTypeName = TEXT("Grass");
    Grass.SpawnProbability = 1.0f;
    Grass.DensityMultiplier = 2.0f;
    MeadowsBiome.VegetationProfile.FoliageTypes.Add(Grass);
    
    FResourceData Berries;
    Berries.ResourceName = TEXT("Berries");
    Berries.ResourceType = TEXT("Food");
    Berries.SpawnRate = 0.3f;
    Berries.MinQuantity = 1;
    Berries.MaxQuantity = 3;
    MeadowsBiome.VegetationProfile.HarvestableResources.Add(Berries);
    
    // Initialize BlackForest biome with vegetation
    FEnhancedBiomeData& BlackForestBiome = EnhancedBiomeDataArray[static_cast<int32>(EBiomeType::BlackForest)];
    BlackForestBiome = FEnhancedBiomeData(TEXT("BlackForest"), 50.0f, Settings.BlackForestScale, FLinearColor(0.1f, 0.3f, 0.1f, 1.0f));
    BlackForestBiome.BiomeColor = FLinearColor(0.1f, 0.4f, 0.1f, 1.0f); // Dark green
    BlackForestBiome.MaterialRoughness = 0.9f;
    BlackForestBiome.MaterialMetallic = 0.0f;
    
    // Add black forest vegetation
    FTreeSpeciesData PineTree;
    PineTree.SpeciesName = TEXT("Pine");
    PineTree.SpawnProbability = 0.9f;
    PineTree.SizeRange = FVector2D(1.0f, 1.5f);
    PineTree.HarvestableResources.Add(TEXT("Pine Wood"));
    PineTree.HarvestableResources.Add(TEXT("Resin"));
    BlackForestBiome.VegetationProfile.TreeSpecies.Add(PineTree);
    
    FFoliageTypeData Ferns;
    Ferns.FoliageTypeName = TEXT("Ferns");
    Ferns.SpawnProbability = 0.8f;
    Ferns.DensityMultiplier = 1.5f;
    BlackForestBiome.VegetationProfile.FoliageTypes.Add(Ferns);
    
    FResourceData Mushrooms;
    Mushrooms.ResourceName = TEXT("Mushrooms");
    Mushrooms.ResourceType = TEXT("Food");
    Mushrooms.SpawnRate = 0.4f;
    Mushrooms.MinQuantity = 1;
    Mushrooms.MaxQuantity = 2;
    BlackForestBiome.VegetationProfile.HarvestableResources.Add(Mushrooms);
    
    BlackForestBiome.VegetationProfile.TreeDensity = 2.0f; // Dense forest
    
    // Initialize Swamp biome with vegetation
    FEnhancedBiomeData& SwampBiome = EnhancedBiomeDataArray[static_cast<int32>(EBiomeType::Swamp)];
    SwampBiome = FEnhancedBiomeData(TEXT("Swamp"), -25.0f, Settings.SwampScale, FLinearColor(0.5f, 0.4f, 0.2f, 1.0f));
    SwampBiome.BiomeColor = FLinearColor(0.4f, 0.3f, 0.2f, 1.0f); // Muddy brown
    SwampBiome.MaterialRoughness = 0.7f;
    SwampBiome.MaterialMetallic = 0.1f;
    
    // Add swamp vegetation
    FTreeSpeciesData WillowTree;
    WillowTree.SpeciesName = TEXT("Willow");
    WillowTree.SpawnProbability = 0.6f;
    WillowTree.SizeRange = FVector2D(0.7f, 1.1f);
    WillowTree.HarvestableResources.Add(TEXT("Willow Wood"));
    SwampBiome.VegetationProfile.TreeSpecies.Add(WillowTree);
    
    FFoliageTypeData Reeds;
    Reeds.FoliageTypeName = TEXT("Reeds");
    Reeds.SpawnProbability = 0.9f;
    Reeds.DensityMultiplier = 3.0f;
    SwampBiome.VegetationProfile.FoliageTypes.Add(Reeds);
    
    FResourceData SwampOre;
    SwampOre.ResourceName = TEXT("Swamp Ore");
    SwampOre.ResourceType = TEXT("Metal");
    SwampOre.SpawnRate = 0.2f;
    SwampOre.MinQuantity = 1;
    SwampOre.MaxQuantity = 1;
    SwampBiome.VegetationProfile.HarvestableResources.Add(SwampOre);
    
    // Initialize Mountains biome (height-based)
    FEnhancedBiomeData& MountainsBiome = EnhancedBiomeDataArray[static_cast<int32>(EBiomeType::Mountains)];
    MountainsBiome = FEnhancedBiomeData::CreateHeightBasedBiome(
        TEXT("Mountains"), 
        Settings.MountainHeightThreshold, 
        true, // Override other biomes
        FLinearColor(0.7f, 0.7f, 0.8f, 1.0f), // Light gray
        FLinearColor(0.8f, 0.8f, 0.9f, 1.0f)  // Debug color
    );
    MountainsBiome.MaterialRoughness = 0.9f;
    MountainsBiome.MaterialMetallic = 0.2f;
    MountainsBiome.HeightOffset = 100.0f;
    
    // Add sparse mountain vegetation
    FTreeSpeciesData SpruceTree;
    SpruceTree.SpeciesName = TEXT("Spruce");
    SpruceTree.SpawnProbability = 0.3f; // Sparse
    SpruceTree.SizeRange = FVector2D(0.6f, 0.9f); // Smaller due to harsh conditions
    SpruceTree.HarvestableResources.Add(TEXT("Spruce Wood"));
    MountainsBiome.VegetationProfile.TreeSpecies.Add(SpruceTree);
    
    FResourceData Stone;
    Stone.ResourceName = TEXT("Stone");
    Stone.ResourceType = TEXT("Building");
    Stone.SpawnRate = 0.8f;
    Stone.MinQuantity = 2;
    Stone.MaxQuantity = 5;
    MountainsBiome.VegetationProfile.HarvestableResources.Add(Stone);
    
    FResourceData IronOre;
    IronOre.ResourceName = TEXT("Iron Ore");
    IronOre.ResourceType = TEXT("Metal");
    IronOre.SpawnRate = 0.3f;
    IronOre.MinQuantity = 1;
    IronOre.MaxQuantity = 2;
    MountainsBiome.VegetationProfile.HarvestableResources.Add(IronOre);
    
    MountainsBiome.VegetationProfile.TreeDensity = 0.3f; // Very sparse
    MountainsBiome.VegetationProfile.ResourceDensity = 1.5f; // Rich in minerals
    
    // Initialize Ocean biome (height-based)
    FEnhancedBiomeData& OceanBiome = EnhancedBiomeDataArray[static_cast<int32>(EBiomeType::Ocean)];
    OceanBiome = FEnhancedBiomeData::CreateHeightBasedBiome(
        TEXT("Ocean"), 
        Settings.WaterHeightThreshold, 
        true, // Override other biomes
        FLinearColor(0.2f, 0.4f, 0.8f, 1.0f), // Blue
        FLinearColor(0.2f, 0.4f, 0.8f, 1.0f)  // Debug color
    );
    OceanBiome.MaterialRoughness = 0.1f; // Very smooth water
    OceanBiome.MaterialMetallic = 0.0f;
    OceanBiome.HeightOffset = -50.0f;
    
    // Add ocean vegetation (minimal)
    FFoliageTypeData Kelp;
    Kelp.FoliageTypeName = TEXT("Kelp");
    Kelp.SpawnProbability = 0.4f;
    Kelp.DensityMultiplier = 1.0f;
    OceanBiome.VegetationProfile.FoliageTypes.Add(Kelp);
    
    FResourceData Fish;
    Fish.ResourceName = TEXT("Fish");
    Fish.ResourceType = TEXT("Food");
    Fish.SpawnRate = 0.6f;
    Fish.MinQuantity = 1;
    Fish.MaxQuantity = 3;
    OceanBiome.VegetationProfile.HarvestableResources.Add(Fish);
    
    OceanBiome.VegetationProfile.TreeDensity = 0.0f; // No trees in ocean
    OceanBiome.VegetationProfile.FoliageDensity = 0.5f; // Some underwater vegetation
}

TPair<EBiomeType, float> FBiomeSystem::EvaluateHeightBasedBiome(float TerrainHeight, float WorldX, float WorldY) const
{
    // Check for mountain biome override
    if (TerrainHeight >= Settings.MountainHeightThreshold)
    {
        const FEnhancedBiomeData& MountainBiome = GetEnhancedBiomeData(EBiomeType::Mountains);
        float BlendDistance = MountainBiome.HeightBlendDistance;
        float HeightDiff = TerrainHeight - Settings.MountainHeightThreshold;
        float BlendFactor = FMath::Clamp(HeightDiff / BlendDistance, 0.0f, 1.0f);
        return TPair<EBiomeType, float>(EBiomeType::Mountains, BlendFactor);
    }
    
    // Check for ocean biome override
    if (TerrainHeight <= Settings.WaterHeightThreshold)
    {
        const FEnhancedBiomeData& OceanBiome = GetEnhancedBiomeData(EBiomeType::Ocean);
        float BlendDistance = OceanBiome.HeightBlendDistance;
        float HeightDiff = Settings.WaterHeightThreshold - TerrainHeight;
        float BlendFactor = FMath::Clamp(HeightDiff / BlendDistance, 0.0f, 1.0f);
        return TPair<EBiomeType, float>(EBiomeType::Ocean, BlendFactor);
    }
    
    // No height-based override
    return TPair<EBiomeType, float>(EBiomeType::Count, 0.0f);
}

bool FBiomeSystem::ApplyHeightBasedBiomeOverride(FBiomeWeights& InOutWeights, float TerrainHeight, float WorldX, float WorldY) const
{
    TPair<EBiomeType, float> HeightBiome = EvaluateHeightBasedBiome(TerrainHeight, WorldX, WorldY);
    
    if (HeightBiome.Key != EBiomeType::Count)
    {
        const FEnhancedBiomeData& HeightBiomeData = GetEnhancedBiomeData(HeightBiome.Key);
        
        if (HeightBiomeData.bOverrideOtherBiomes)
        {
            // Clear all existing weights
            for (int32 i = 0; i < static_cast<int32>(EBiomeType::Count); ++i)
            {
                InOutWeights.Weights[i] = 0.0f;
            }
            
            // Set height-based biome weight
            int32 HeightBiomeIndex = static_cast<int32>(HeightBiome.Key);
            InOutWeights.Weights[HeightBiomeIndex] = HeightBiome.Value;
            InOutWeights.DominantBiome = HeightBiome.Key;
            
            return true;
        }
        else
        {
            // Blend with existing weights
            int32 HeightBiomeIndex = static_cast<int32>(HeightBiome.Key);
            InOutWeights.Weights[HeightBiomeIndex] = FMath::Max(InOutWeights.Weights[HeightBiomeIndex], HeightBiome.Value);
            
            // Update dominant biome if this height-based biome is stronger
            if (HeightBiome.Value > InOutWeights.Weights[static_cast<int32>(InOutWeights.DominantBiome)])
            {
                InOutWeights.DominantBiome = HeightBiome.Key;
            }
            
            return true;
        }
    }
    
    return false;
}

TSoftObjectPtr<UMaterialInterface> FBiomeSystem::CalculateBlendedMaterial(const FBiomeWeights& BiomeWeights) const
{
    // Find the dominant biome's material
    const FEnhancedBiomeData& DominantBiomeData = GetEnhancedBiomeData(BiomeWeights.DominantBiome);
    
    if (DominantBiomeData.BiomeMaterial.IsValid())
    {
        return DominantBiomeData.BiomeMaterial;
    }
    
    // If dominant biome has no material, try to find any biome with a material
    for (int32 i = 0; i < static_cast<int32>(EBiomeType::Count); ++i)
    {
        if (BiomeWeights.NormalizedWeights[i] > 0.1f) // Only consider biomes with significant weight
        {
            const FEnhancedBiomeData& BiomeData = GetEnhancedBiomeData(static_cast<EBiomeType>(i));
            if (BiomeData.BiomeMaterial.IsValid())
            {
                return BiomeData.BiomeMaterial;
            }
        }
    }
    
    // No material found
    return nullptr;
}

FLinearColor FBiomeSystem::CalculateBlendedBiomeColor(const FBiomeWeights& BiomeWeights) const
{
    FLinearColor BlendedColor = FLinearColor::Black;
    
    // Blend biome colors based on normalized weights
    for (int32 i = 0; i < static_cast<int32>(EBiomeType::Count); ++i)
    {
        const FEnhancedBiomeData& BiomeData = GetEnhancedBiomeData(static_cast<EBiomeType>(i));
        BlendedColor += BiomeData.BiomeColor * BiomeWeights.NormalizedWeights[i];
    }
    
    return BlendedColor;
}

void FBiomeSystem::CalculateBlendedMaterialParameters(const FBiomeWeights& BiomeWeights, float& OutRoughness, float& OutMetallic) const
{
    OutRoughness = 0.0f;
    OutMetallic = 0.0f;
    
    // Blend material parameters based on normalized weights
    for (int32 i = 0; i < static_cast<int32>(EBiomeType::Count); ++i)
    {
        const FEnhancedBiomeData& BiomeData = GetEnhancedBiomeData(static_cast<EBiomeType>(i));
        OutRoughness += BiomeData.MaterialRoughness * BiomeWeights.NormalizedWeights[i];
        OutMetallic += BiomeData.MaterialMetallic * BiomeWeights.NormalizedWeights[i];
    }
}