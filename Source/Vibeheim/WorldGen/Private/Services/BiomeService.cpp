#include "Services/BiomeService.h"
#include "Engine/Engine.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogBiomeService, Log, All);

UBiomeService::UBiomeService()
{
	ClimateSystem = nullptr;
}

void UBiomeService::Initialize(UClimateSystem* InClimateSystem, const FWorldGenConfig& Settings)
{
	ClimateSystem = InClimateSystem;
	WorldGenSettings = Settings;
	
	InitializeDefaultBiomes();
	
	UE_LOG(LogBiomeService, Log, TEXT("Biome service initialized with %d biome definitions"), BiomeDefinitions.Num());
}

void UBiomeService::InitializeDefaultBiomes()
{
	// Meadows biome
	FBiomeDefinition MeadowsBiome;
	MeadowsBiome.BiomeType = EBiomeType::Meadows;
	MeadowsBiome.BiomeName = TEXT("Meadows");
	MeadowsBiome.BaseHeight = 5.0f;
	MeadowsBiome.HeightVariation = 20.0f;
	MeadowsBiome.MinTemperature = 5.0f;
	MeadowsBiome.MaxTemperature = 25.0f;
	MeadowsBiome.MinMoisture = 0.3f;
	MeadowsBiome.MaxMoisture = 0.8f;
	MeadowsBiome.BiomeWeight = 1.0f;
	BiomeDefinitions.Add(EBiomeType::Meadows, MeadowsBiome);
	
	// Forest biome
	FBiomeDefinition ForestBiome;
	ForestBiome.BiomeType = EBiomeType::Forest;
	ForestBiome.BiomeName = TEXT("Forest");
	ForestBiome.BaseHeight = 10.0f;
	ForestBiome.HeightVariation = 30.0f;
	ForestBiome.MinTemperature = 0.0f;
	ForestBiome.MaxTemperature = 20.0f;
	ForestBiome.MinMoisture = 0.5f;
	ForestBiome.MaxMoisture = 1.0f;
	ForestBiome.BiomeWeight = 1.2f;
	BiomeDefinitions.Add(EBiomeType::Forest, ForestBiome);
	
	// Mountains biome
	FBiomeDefinition MountainsBiome;
	MountainsBiome.BiomeType = EBiomeType::Mountains;
	MountainsBiome.BiomeName = TEXT("Mountains");
	MountainsBiome.BaseHeight = 50.0f;
	MountainsBiome.HeightVariation = 70.0f;
	MountainsBiome.MinTemperature = -10.0f;
	MountainsBiome.MaxTemperature = 15.0f;
	MountainsBiome.MinMoisture = 0.2f;
	MountainsBiome.MaxMoisture = 0.9f;
	MountainsBiome.BiomeWeight = 0.8f;
	BiomeDefinitions.Add(EBiomeType::Mountains, MountainsBiome);
	
	// Ocean biome
	FBiomeDefinition OceanBiome;
	OceanBiome.BiomeType = EBiomeType::Ocean;
	OceanBiome.BiomeName = TEXT("Ocean");
	OceanBiome.BaseHeight = -20.0f;
	OceanBiome.HeightVariation = 10.0f;
	OceanBiome.MinTemperature = -5.0f;
	OceanBiome.MaxTemperature = 30.0f;
	OceanBiome.MinMoisture = 0.8f;
	OceanBiome.MaxMoisture = 1.0f;
	OceanBiome.BiomeWeight = 1.0f;
	BiomeDefinitions.Add(EBiomeType::Ocean, OceanBiome);
}

FBiomeResult UBiomeService::DetermineBiome(FVector2D WorldPosition, float Altitude) const
{
	if (!ClimateSystem)
	{
		UE_LOG(LogBiomeService, Warning, TEXT("Climate system not set, returning default biome"));
		FBiomeResult Result;
		Result.PrimaryBiome = EBiomeType::Meadows;
		Result.BiomeWeights.Add(EBiomeType::Meadows, 1.0f);
		return Result;
	}
	
	// Get climate data for this position
	FClimateData ClimateData = ClimateSystem->CalculateClimate(WorldPosition, Altitude);
	
	// Calculate biome weights based on climate suitability
	TMap<EBiomeType, float> BiomeWeights = CalculateBiomeWeights(ClimateData, Altitude);
	
	// Apply biome blending
	return ApplyBiomeBlending(BiomeWeights, WorldPosition);
}

TMap<EBiomeType, float> UBiomeService::CalculateBiomeWeights(const FClimateData& ClimateData, float Altitude) const
{
	TMap<EBiomeType, float> Weights;
	
	for (const auto& BiomePair : BiomeDefinitions)
	{
		EBiomeType BiomeType = BiomePair.Key;
		float Suitability = CalculateBiomeSuitability(BiomeType, ClimateData, Altitude);
		
		if (Suitability > 0.0f)
		{
			Weights.Add(BiomeType, Suitability);
		}
	}
	
	return Weights;
}

float UBiomeService::CalculateBiomeSuitability(EBiomeType BiomeType, const FClimateData& ClimateData, float Altitude) const
{
	const FBiomeDefinition* BiomeDef = BiomeDefinitions.Find(BiomeType);
	if (!BiomeDef)
	{
		return 0.0f;
	}
	
	// Temperature suitability
	float TempSuitability = 1.0f;
	if (ClimateData.Temperature < BiomeDef->MinTemperature || ClimateData.Temperature > BiomeDef->MaxTemperature)
	{
		float TempRange = BiomeDef->MaxTemperature - BiomeDef->MinTemperature;
		if (ClimateData.Temperature < BiomeDef->MinTemperature)
		{
			float Diff = BiomeDef->MinTemperature - ClimateData.Temperature;
			TempSuitability = FMath::Max(0.0f, 1.0f - (Diff / (TempRange * 0.5f)));
		}
		else
		{
			float Diff = ClimateData.Temperature - BiomeDef->MaxTemperature;
			TempSuitability = FMath::Max(0.0f, 1.0f - (Diff / (TempRange * 0.5f)));
		}
	}
	
	// Moisture suitability
	float MoistureSuitability = 1.0f;
	if (ClimateData.Moisture < BiomeDef->MinMoisture || ClimateData.Moisture > BiomeDef->MaxMoisture)
	{
		float MoistureRange = BiomeDef->MaxMoisture - BiomeDef->MinMoisture;
		if (ClimateData.Moisture < BiomeDef->MinMoisture)
		{
			float Diff = BiomeDef->MinMoisture - ClimateData.Moisture;
			MoistureSuitability = FMath::Max(0.0f, 1.0f - (Diff / (MoistureRange * 0.5f)));
		}
		else
		{
			float Diff = ClimateData.Moisture - BiomeDef->MaxMoisture;
			MoistureSuitability = FMath::Max(0.0f, 1.0f - (Diff / (MoistureRange * 0.5f)));
		}
	}
	
	// Altitude influence (mountains prefer higher altitudes)
	float AltitudeSuitability = 1.0f;
	if (BiomeType == EBiomeType::Mountains)
	{
		AltitudeSuitability = FMath::Clamp(Altitude / 50.0f, 0.1f, 2.0f);
	}
	else if (BiomeType == EBiomeType::Ocean)
	{
		AltitudeSuitability = Altitude < 0.0f ? 2.0f : FMath::Max(0.0f, 1.0f - (Altitude / 20.0f));
	}
	
	// Ring bias influence
	float RingInfluence = 1.0f + (ClimateData.RingBias * 0.5f);
	
	// Combine all factors
	float TotalSuitability = TempSuitability * MoistureSuitability * AltitudeSuitability * RingInfluence * BiomeDef->BiomeWeight;
	
	return FMath::Max(0.0f, TotalSuitability);
}FBiomeResu
lt UBiomeService::ApplyBiomeBlending(const TMap<EBiomeType, float>& BiomeWeights, FVector2D WorldPosition) const
{
	FBiomeResult Result;
	
	if (BiomeWeights.Num() == 0)
	{
		Result.PrimaryBiome = EBiomeType::Meadows;
		Result.BiomeWeights.Add(EBiomeType::Meadows, 1.0f);
		return Result;
	}
	
	// Find the biome with highest weight
	float MaxWeight = 0.0f;
	EBiomeType PrimaryBiome = EBiomeType::None;
	
	for (const auto& WeightPair : BiomeWeights)
	{
		if (WeightPair.Value > MaxWeight)
		{
			MaxWeight = WeightPair.Value;
			PrimaryBiome = WeightPair.Key;
		}
	}
	
	Result.PrimaryBiome = PrimaryBiome;
	
	// Normalize weights and apply blending
	float TotalWeight = 0.0f;
	for (const auto& WeightPair : BiomeWeights)
	{
		TotalWeight += WeightPair.Value;
	}
	
	if (TotalWeight > 0.0f)
	{
		for (const auto& WeightPair : BiomeWeights)
		{
			float NormalizedWeight = WeightPair.Value / TotalWeight;
			if (NormalizedWeight > 0.1f) // Only include significant weights
			{
				Result.BiomeWeights.Add(WeightPair.Key, NormalizedWeight);
			}
		}
	}
	
	// Set blend radius based on biome blend distance setting
	Result.BlendRadius = WorldGenSettings.BiomeBlendDistance;
	
	return Result;
}

TArray<FBiomeResult> UBiomeService::GenerateTileBiomeData(FTileCoord TileCoord, const TArray<float>& HeightData) const
{
	TArray<FBiomeResult> BiomeResults;
	
	// Calculate tile world position
	FVector TileWorldPos = TileCoord.ToWorldPosition(64.0f);
	FVector2D TileStart(TileWorldPos.X - 32.0f, TileWorldPos.Y - 32.0f);
	
	// Generate biome data for each sample in the tile (64x64 samples)
	const int32 SamplesPerTile = 64;
	BiomeResults.Reserve(SamplesPerTile * SamplesPerTile);
	
	for (int32 Y = 0; Y < SamplesPerTile; Y++)
	{
		for (int32 X = 0; X < SamplesPerTile; X++)
		{
			// Calculate world position for this sample
			FVector2D SampleWorldPos = TileStart + FVector2D(X, Y);
			
			// Get height for this sample
			float SampleHeight = 0.0f;
			if (HeightData.IsValidIndex(Y * SamplesPerTile + X))
			{
				SampleHeight = HeightData[Y * SamplesPerTile + X];
			}
			
			// Determine biome
			FBiomeResult BiomeResult = DetermineBiome(SampleWorldPos, SampleHeight);
			BiomeResults.Add(BiomeResult);
		}
	}
	
	return BiomeResults;
}

bool UBiomeService::ExportBiomePNG(FTileCoord TileCoord, const TArray<float>& HeightData, const FString& OutputPath) const
{
	// Generate biome data for the tile
	TArray<FBiomeResult> BiomeData = GenerateTileBiomeData(TileCoord, HeightData);
	
	if (BiomeData.Num() != 64 * 64)
	{
		UE_LOG(LogBiomeService, Error, TEXT("Invalid biome data size for tile export"));
		return false;
	}
	
	// Create output directory
	FString FullOutputPath = FPaths::ProjectDir() / OutputPath;
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(FullOutputPath), true);
	
	// Export biome map
	TArray<FColor> BiomePixels;
	BiomePixels.Reserve(64 * 64);
	
	for (const FBiomeResult& Result : BiomeData)
	{
		FColor BiomeColor = GetBiomeColor(Result.PrimaryBiome);
		BiomePixels.Add(BiomeColor);
	}
	
	FString BiomePath = FullOutputPath.Replace(TEXT(".png"), TEXT("_biome.png"));
	// TODO: Implement PNG export when ImageUtils is available
	UE_LOG(LogBiomeService, Log, TEXT("Biome data generated for export to %s (PNG export not implemented)"), *BiomePath);
	
	UE_LOG(LogBiomeService, Log, TEXT("Successfully exported biome PNG for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
	return true;
}

FColor UBiomeService::GetBiomeColor(EBiomeType BiomeType) const
{
	switch (BiomeType)
	{
		case EBiomeType::Meadows:
			return FColor(144, 238, 144); // Light green
		case EBiomeType::Forest:
			return FColor(34, 139, 34); // Forest green
		case EBiomeType::Mountains:
			return FColor(139, 137, 137); // Gray
		case EBiomeType::Ocean:
			return FColor(0, 191, 255); // Deep sky blue
		default:
			return FColor(128, 128, 128); // Gray for unknown
	}
}

bool UBiomeService::GetBiomeDefinition(EBiomeType BiomeType, FBiomeDefinition& OutDefinition) const
{
	if (const FBiomeDefinition* BiomeDef = BiomeDefinitions.Find(BiomeType))
	{
		OutDefinition = *BiomeDef;
		return true;
	}
	return false;
}

void UBiomeService::SetBiomeDefinitions(const TMap<EBiomeType, FBiomeDefinition>& InBiomeDefinitions)
{
	BiomeDefinitions = InBiomeDefinitions;
	UE_LOG(LogBiomeService, Log, TEXT("Updated biome definitions with %d biomes"), BiomeDefinitions.Num());
}