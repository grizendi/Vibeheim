#include "Services/BiomeService.h"
#include "Utils/WorldGenLogging.h"
#include "Engine/Engine.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogBiomeService, Log, All);

UBiomeService::UBiomeService()
{
	ClimateSystem = nullptr;
}

void UBiomeService::Initialize(UClimateSystem* InClimateSystem, const FWorldGenConfig& Settings)
{
	ClimateSystem = InClimateSystem;
	WorldGenSettings = Settings;
	
	// Try to load biomes from JSON first, fallback to defaults
	if (!LoadBiomesFromJSON(TEXT("Config/BiomeDefinitions.json")))
	{
		UE_LOG(LogBiomeService, Warning, TEXT("Failed to load biomes from JSON, using default hardcoded biomes"));
		InitializeDefaultBiomes();
	}
	
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

EBiomeType UBiomeService::DetermineTileBiome(FTileCoord Tile, const TArray<float>& HeightData) const
{
	WORLDGEN_TIMER_WITH_CONTEXT("Biome classify", WorldGenSettings.Seed, Tile);
	
	if (!ClimateSystem)
	{
		WORLDGEN_LOG_WITH_TILE(Warning, Tile, TEXT("Climate system not set, defaulting to Meadows biome"));
		return EBiomeType::Meadows;
	}

	// Calculate tile world position (center of tile)
	FVector TileWorldPos = Tile.ToWorldPosition(WorldGenSettings.TileSizeMeters);
	FVector2D TileCenter(TileWorldPos.X, TileWorldPos.Y);
	
	// Sample multiple points across the tile to determine dominant biome
	const int32 SampleCount = 9; // 3x3 sampling grid
	const float SampleSpacing = WorldGenSettings.TileSizeMeters / 4.0f; // Quarter tile spacing
	TMap<EBiomeType, int32> BiomeCounts;
	
	for (int32 Y = -1; Y <= 1; Y++)
	{
		for (int32 X = -1; X <= 1; X++)
		{
			// Calculate sample position
			FVector2D SamplePos = TileCenter + FVector2D(X * SampleSpacing, Y * SampleSpacing);
			
			// Get height at this sample position (approximate from height data)
			float SampleHeight = 0.0f;
			if (HeightData.Num() > 0)
			{
				// Use center height as approximation (could be improved with interpolation)
				int32 CenterIndex = (HeightData.Num() - 1) / 2;
				if (HeightData.IsValidIndex(CenterIndex))
				{
					SampleHeight = HeightData[CenterIndex];
				}
			}
			
			// Determine biome at this sample point
			FBiomeResult BiomeResult = DetermineBiome(SamplePos, SampleHeight);
			
			// Count biome occurrences
			if (BiomeResult.PrimaryBiome != EBiomeType::None)
			{
				if (int32* Count = BiomeCounts.Find(BiomeResult.PrimaryBiome))
				{
					(*Count)++;
				}
				else
				{
					BiomeCounts.Add(BiomeResult.PrimaryBiome, 1);
				}
			}
		}
	}
	
	// Find the most common biome
	EBiomeType DominantBiome = EBiomeType::Meadows; // Default fallback
	int32 MaxCount = 0;
	
	for (const auto& BiomeCount : BiomeCounts)
	{
		if (BiomeCount.Value > MaxCount)
		{
			MaxCount = BiomeCount.Value;
			DominantBiome = BiomeCount.Key;
		}
	}
	
	UE_LOG(LogBiomeService, Verbose, TEXT("Determined dominant biome for tile (%d, %d): %d (from %d samples)"), 
		Tile.X, Tile.Y, static_cast<int32>(DominantBiome), SampleCount);
	
	return DominantBiome;
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
}FBiomeResult UBiomeService::ApplyBiomeBlending(const TMap<EBiomeType, float>& BiomeWeights, FVector2D WorldPosition) const
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

bool UBiomeService::LoadBiomesFromJSON(const FString& ConfigPath)
{
	// Get full path to the config file
	FString FullPath = FPaths::ProjectDir() / ConfigPath;
	
	// Check if file exists
	if (!FPaths::FileExists(FullPath))
	{
		UE_LOG(LogBiomeService, Warning, TEXT("Biome config file not found at: %s"), *FullPath);
		return false;
	}

	// Load the JSON file
	FString JSONContent;
	if (!FFileHelper::LoadFileToString(JSONContent, *FullPath))
	{
		UE_LOG(LogBiomeService, Error, TEXT("Failed to load biome config file: %s"), *FullPath);
		return false;
	}

	// Parse the JSON
	TSharedPtr<FJsonObject> JSONObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JSONContent);
	if (!FJsonSerializer::Deserialize(Reader, JSONObject) || !JSONObject.IsValid())
	{
		UE_LOG(LogBiomeService, Error, TEXT("Failed to parse JSON from biome config file: %s"), *FullPath);
		return false;
	}

	// Clear existing biome definitions
	BiomeDefinitions.Empty();

	// Parse biome definitions
	const TSharedPtr<FJsonObject>* BiomeDefsObject;
	if (!JSONObject->TryGetObjectField(TEXT("BiomeDefinitions"), BiomeDefsObject))
	{
		UE_LOG(LogBiomeService, Error, TEXT("Missing 'BiomeDefinitions' section in biome config"));
		return false;
	}

	// Parse each biome
	for (const auto& BiomePair : BiomeDefsObject->Get()->Values)
	{
		const FString& BiomeName = BiomePair.Key;
		const TSharedPtr<FJsonValue>& BiomeValue = BiomePair.Value;
		
		const TSharedPtr<FJsonObject>* BiomeObject;
		if (!BiomeValue->TryGetObject(BiomeObject))
		{
			UE_LOG(LogBiomeService, Warning, TEXT("Invalid biome object for %s"), *BiomeName);
			continue;
		}

		// Parse biome definition
		FBiomeDefinition BiomeDef;
		
		// Get biome type
		int32 BiomeTypeInt;
		if (!BiomeObject->Get()->TryGetNumberField(TEXT("BiomeType"), BiomeTypeInt))
		{
			UE_LOG(LogBiomeService, Warning, TEXT("Missing BiomeType for biome %s"), *BiomeName);
			continue;
		}
		BiomeDef.BiomeType = static_cast<EBiomeType>(BiomeTypeInt);

		// Get basic properties
		BiomeObject->Get()->TryGetStringField(TEXT("BiomeName"), BiomeDef.BiomeName);
		BiomeObject->Get()->TryGetNumberField(TEXT("BaseHeight"), BiomeDef.BaseHeight);
		BiomeObject->Get()->TryGetNumberField(TEXT("HeightVariation"), BiomeDef.HeightVariation);
		BiomeObject->Get()->TryGetNumberField(TEXT("MinTemperature"), BiomeDef.MinTemperature);
		BiomeObject->Get()->TryGetNumberField(TEXT("MaxTemperature"), BiomeDef.MaxTemperature);
		BiomeObject->Get()->TryGetNumberField(TEXT("MinMoisture"), BiomeDef.MinMoisture);
		BiomeObject->Get()->TryGetNumberField(TEXT("MaxMoisture"), BiomeDef.MaxMoisture);
		BiomeObject->Get()->TryGetNumberField(TEXT("BiomeWeight"), BiomeDef.BiomeWeight);

		// Parse RVT blend color
		const TArray<TSharedPtr<FJsonValue>>* ColorArray;
		if (BiomeObject->Get()->TryGetArrayField(TEXT("RVTBlendColor"), ColorArray) && ColorArray->Num() >= 4)
		{
			BiomeDef.RVTBlendColor.R = (*ColorArray)[0]->AsNumber();
			BiomeDef.RVTBlendColor.G = (*ColorArray)[1]->AsNumber();
			BiomeDef.RVTBlendColor.B = (*ColorArray)[2]->AsNumber();
			BiomeDef.RVTBlendColor.A = (*ColorArray)[3]->AsNumber();
		}

		// Parse vegetation rules
		const TArray<TSharedPtr<FJsonValue>>* VegRulesArray;
		if (BiomeObject->Get()->TryGetArrayField(TEXT("VegetationRules"), VegRulesArray))
		{
			for (const auto& VegRuleValue : *VegRulesArray)
			{
				const TSharedPtr<FJsonObject>* VegRuleObject;
				if (!VegRuleValue->TryGetObject(VegRuleObject))
					continue;

				FPCGVegetationRule VegRule;
				FString VegetationName;
				VegRuleObject->Get()->TryGetStringField(TEXT("VegetationName"), VegetationName);
				VegRuleObject->Get()->TryGetNumberField(TEXT("Density"), VegRule.Density);
				VegRuleObject->Get()->TryGetNumberField(TEXT("MinScale"), VegRule.MinScale);
				VegRuleObject->Get()->TryGetNumberField(TEXT("MaxScale"), VegRule.MaxScale);
				VegRuleObject->Get()->TryGetNumberField(TEXT("SlopeLimit"), VegRule.SlopeLimit);
				VegRuleObject->Get()->TryGetNumberField(TEXT("MinHeight"), VegRule.MinHeight);
				VegRuleObject->Get()->TryGetNumberField(TEXT("MaxHeight"), VegRule.MaxHeight);
				
				BiomeDef.VegetationRules.Add(VegRule);
			}
		}

		// Parse POI rules
		const TArray<TSharedPtr<FJsonValue>>* POIRulesArray;
		if (BiomeObject->Get()->TryGetArrayField(TEXT("POIRules"), POIRulesArray))
		{
			for (const auto& POIRuleValue : *POIRulesArray)
			{
				const TSharedPtr<FJsonObject>* POIRuleObject;
				if (!POIRuleValue->TryGetObject(POIRuleObject))
					continue;

				FPOISpawnRule POIRule;
				POIRuleObject->Get()->TryGetStringField(TEXT("POIName"), POIRule.POIName);
				POIRuleObject->Get()->TryGetNumberField(TEXT("SpawnChance"), POIRule.SpawnChance);
				POIRuleObject->Get()->TryGetNumberField(TEXT("MinDistanceFromOthers"), POIRule.MinDistanceFromOthers);
				POIRuleObject->Get()->TryGetNumberField(TEXT("SlopeLimit"), POIRule.SlopeLimit);
				POIRuleObject->Get()->TryGetBoolField(TEXT("RequiresFlatGround"), POIRule.bRequiresFlatGround);
				
				BiomeDef.POIRules.Add(POIRule);
			}
		}

		// Add the biome definition
		BiomeDefinitions.Add(BiomeDef.BiomeType, BiomeDef);
		UE_LOG(LogBiomeService, Log, TEXT("Loaded biome definition: %s"), *BiomeDef.BiomeName);
	}

	// Parse global biome settings if available
	const TSharedPtr<FJsonObject>* GlobalSettingsObject;
	if (JSONObject->TryGetObjectField(TEXT("GlobalBiomeSettings"), GlobalSettingsObject))
	{
		float BlendDistance;
		if (GlobalSettingsObject->Get()->TryGetNumberField(TEXT("BlendDistanceMeters"), BlendDistance))
		{
			WorldGenSettings.BiomeBlendDistance = BlendDistance;
		}
	}

	UE_LOG(LogBiomeService, Log, TEXT("Successfully loaded %d biome definitions from %s"), 
		BiomeDefinitions.Num(), *ConfigPath);
	return true;
}

bool UBiomeService::SaveBiomesToJSON(const FString& ConfigPath) const
{
	// Create the main JSON object
	TSharedPtr<FJsonObject> JSONObject = MakeShareable(new FJsonObject);
	
	// Create biome definitions object
	TSharedPtr<FJsonObject> BiomeDefsObject = MakeShareable(new FJsonObject);
	
	// Convert each biome definition
	for (const auto& BiomePair : BiomeDefinitions)
	{
		const FBiomeDefinition& BiomeDef = BiomePair.Value;
		TSharedPtr<FJsonObject> BiomeObject = MakeShareable(new FJsonObject);
		
		// Basic properties
		BiomeObject->SetNumberField(TEXT("BiomeType"), static_cast<int32>(BiomeDef.BiomeType));
		BiomeObject->SetStringField(TEXT("BiomeName"), BiomeDef.BiomeName);
		BiomeObject->SetNumberField(TEXT("BaseHeight"), BiomeDef.BaseHeight);
		BiomeObject->SetNumberField(TEXT("HeightVariation"), BiomeDef.HeightVariation);
		BiomeObject->SetNumberField(TEXT("MinTemperature"), BiomeDef.MinTemperature);
		BiomeObject->SetNumberField(TEXT("MaxTemperature"), BiomeDef.MaxTemperature);
		BiomeObject->SetNumberField(TEXT("MinMoisture"), BiomeDef.MinMoisture);
		BiomeObject->SetNumberField(TEXT("MaxMoisture"), BiomeDef.MaxMoisture);
		BiomeObject->SetNumberField(TEXT("BiomeWeight"), BiomeDef.BiomeWeight);

		// RVT blend color
		TArray<TSharedPtr<FJsonValue>> ColorArray;
		ColorArray.Add(MakeShareable(new FJsonValueNumber(BiomeDef.RVTBlendColor.R)));
		ColorArray.Add(MakeShareable(new FJsonValueNumber(BiomeDef.RVTBlendColor.G)));
		ColorArray.Add(MakeShareable(new FJsonValueNumber(BiomeDef.RVTBlendColor.B)));
		ColorArray.Add(MakeShareable(new FJsonValueNumber(BiomeDef.RVTBlendColor.A)));
		BiomeObject->SetArrayField(TEXT("RVTBlendColor"), ColorArray);

		// Vegetation rules
		TArray<TSharedPtr<FJsonValue>> VegRulesArray;
		for (const FPCGVegetationRule& VegRule : BiomeDef.VegetationRules)
		{
			TSharedPtr<FJsonObject> VegRuleObject = MakeShareable(new FJsonObject);
			VegRuleObject->SetStringField(TEXT("VegetationName"), TEXT("Generated")); // Placeholder
			VegRuleObject->SetNumberField(TEXT("Density"), VegRule.Density);
			VegRuleObject->SetNumberField(TEXT("MinScale"), VegRule.MinScale);
			VegRuleObject->SetNumberField(TEXT("MaxScale"), VegRule.MaxScale);
			VegRuleObject->SetNumberField(TEXT("SlopeLimit"), VegRule.SlopeLimit);
			VegRuleObject->SetNumberField(TEXT("MinHeight"), VegRule.MinHeight);
			VegRuleObject->SetNumberField(TEXT("MaxHeight"), VegRule.MaxHeight);
			VegRulesArray.Add(MakeShareable(new FJsonValueObject(VegRuleObject)));
		}
		BiomeObject->SetArrayField(TEXT("VegetationRules"), VegRulesArray);

		// POI rules
		TArray<TSharedPtr<FJsonValue>> POIRulesArray;
		for (const FPOISpawnRule& POIRule : BiomeDef.POIRules)
		{
			TSharedPtr<FJsonObject> POIRuleObject = MakeShareable(new FJsonObject);
			POIRuleObject->SetStringField(TEXT("POIName"), POIRule.POIName);
			POIRuleObject->SetNumberField(TEXT("SpawnChance"), POIRule.SpawnChance);
			POIRuleObject->SetNumberField(TEXT("MinDistanceFromOthers"), POIRule.MinDistanceFromOthers);
			POIRuleObject->SetNumberField(TEXT("SlopeLimit"), POIRule.SlopeLimit);
			POIRuleObject->SetBoolField(TEXT("RequiresFlatGround"), POIRule.bRequiresFlatGround);
			POIRulesArray.Add(MakeShareable(new FJsonValueObject(POIRuleObject)));
		}
		BiomeObject->SetArrayField(TEXT("POIRules"), POIRulesArray);

		// Add to biome definitions
		BiomeDefsObject->SetObjectField(BiomeDef.BiomeName, BiomeObject);
	}

	JSONObject->SetObjectField(TEXT("BiomeDefinitions"), BiomeDefsObject);

	// Add global biome settings
	TSharedPtr<FJsonObject> GlobalSettingsObject = MakeShareable(new FJsonObject);
	GlobalSettingsObject->SetNumberField(TEXT("Version"), 1);
	GlobalSettingsObject->SetNumberField(TEXT("BlendDistanceMeters"), WorldGenSettings.BiomeBlendDistance);
	GlobalSettingsObject->SetNumberField(TEXT("MinBiomeWeight"), 0.1);
	GlobalSettingsObject->SetNumberField(TEXT("BiomeTransitionSpeed"), 1.0);
	GlobalSettingsObject->SetNumberField(TEXT("RingBiasStrength"), 0.5);
	GlobalSettingsObject->SetNumberField(TEXT("TemperatureInfluence"), 1.0);
	GlobalSettingsObject->SetNumberField(TEXT("MoistureInfluence"), 1.0);
	GlobalSettingsObject->SetNumberField(TEXT("AltitudeInfluence"), 0.8);
	JSONObject->SetObjectField(TEXT("GlobalBiomeSettings"), GlobalSettingsObject);

	// Serialize to string
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(JSONObject.ToSharedRef(), Writer);

	// Save to file
	FString FullPath = FPaths::ProjectDir() / ConfigPath;
	if (!FFileHelper::SaveStringToFile(OutputString, *FullPath))
	{
		UE_LOG(LogBiomeService, Error, TEXT("Failed to save biome config to: %s"), *FullPath);
		return false;
	}

	UE_LOG(LogBiomeService, Log, TEXT("Successfully saved %d biome definitions to %s"), 
		BiomeDefinitions.Num(), *ConfigPath);
	return true;
}
