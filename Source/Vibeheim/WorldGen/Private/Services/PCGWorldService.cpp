#include "Services/PCGWorldService.h"
#include "Engine/Engine.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/DateTime.h"

// PCG includes (conditional)
#if WITH_PCG
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGSubsystem.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogPCGWorldService, Log, All);

UPCGWorldService::UPCGWorldService()
{
	bRuntimeOperationsEnabled = true;
	PerformanceStats = FPCGPerformanceStats();
	CurrentPCGGraph = nullptr;
}

bool UPCGWorldService::Initialize(const FWorldGenConfig& Settings)
{
	WorldGenSettings = Settings;
	InitializeDefaultBiomes();

#if WITH_PCG
	UE_LOG(LogPCGWorldService, Log, TEXT("PCG World Service initialized with PCG support"));
#else
	UE_LOG(LogPCGWorldService, Warning, TEXT("PCG World Service initialized without PCG support - using fallback generation"));
#endif

	return true;
}

bool UPCGWorldService::InitializePCGGraph(UObject* BiomeGraph)
{
	if (!BiomeGraph)
	{
		UE_LOG(LogPCGWorldService, Error, TEXT("Cannot initialize with null PCG graph"));
		return false;
	}

#if WITH_PCG
	// Validate that it's actually a PCG graph when PCG is available
	UPCGGraph* PCGGraph = Cast<UPCGGraph>(BiomeGraph);
	if (!PCGGraph)
	{
		UE_LOG(LogPCGWorldService, Error, TEXT("Provided object is not a valid PCG graph"));
		return false;
	}
	CurrentPCGGraph = BiomeGraph;
	UE_LOG(LogPCGWorldService, Log, TEXT("PCG graph initialized: %s"), *BiomeGraph->GetName());
#else
	// Store the object but log that PCG is not available
	CurrentPCGGraph = BiomeGraph;
	UE_LOG(LogPCGWorldService, Warning, TEXT("PCG graph provided but PCG system not available - stored for future use"));
#endif

	return true;
}

FPCGGenerationData UPCGWorldService::GenerateBiomeContent(FTileCoord TileCoord, EBiomeType BiomeType, const TArray<float>& HeightData)
{
	double StartTime = FPlatformTime::Seconds();

	// Check cache first
	if (FPCGGenerationData* CachedData = GenerationCache.Find(TileCoord))
	{
		return *CachedData;
	}

	// Generate new content
	FPCGGenerationData GenerationData = GenerateContentInternal(TileCoord, BiomeType, HeightData);

	// Cache the result
	GenerationCache.Add(TileCoord, GenerationData);

	// Update performance stats
	double EndTime = FPlatformTime::Seconds();
	float GenerationTimeMs = static_cast<float>((EndTime - StartTime) * 1000.0);
	UpdatePerformanceStats(GenerationTimeMs, GenerationData.TotalInstanceCount);

	UE_LOG(LogPCGWorldService, Log, TEXT("Generated biome content for tile (%d, %d) - %d instances in %.2fms"), 
		TileCoord.X, TileCoord.Y, GenerationData.TotalInstanceCount, GenerationTimeMs);

	return GenerationData;
}

FPCGGenerationData UPCGWorldService::GenerateContentInternal(FTileCoord TileCoord, EBiomeType BiomeType, const TArray<float>& HeightData)
{
#if WITH_PCG
	// Try PCG generation first
	if (CurrentPCGGraph && bRuntimeOperationsEnabled)
	{
		UPCGGraph* PCGGraph = Cast<UPCGGraph>(CurrentPCGGraph);
		if (PCGGraph)
		{
			// TODO: Implement actual PCG generation when PCG system is fully available
			UE_LOG(LogPCGWorldService, Log, TEXT("PCG generation not yet implemented - using fallback"));
		}
	}
#endif

	// Use fallback generation
	return GenerateFallbackContent(TileCoord, BiomeType, HeightData);
}

FPCGGenerationData UPCGWorldService::GenerateFallbackContent(FTileCoord TileCoord, EBiomeType BiomeType, const TArray<float>& HeightData)
{
	FPCGGenerationData GenerationData;
	GenerationData.TileCoord = TileCoord;
	GenerationData.BiomeType = BiomeType;

	// Get biome definition
	const FBiomeDefinition* BiomeDef = BiomeDefinitions.Find(BiomeType);
	if (!BiomeDef)
	{
		UE_LOG(LogPCGWorldService, Warning, TEXT("No biome definition found for biome type %d"), static_cast<int32>(BiomeType));
		return GenerationData;
	}

	// Generate vegetation instances
	TArray<FPCGInstanceData> VegetationInstances = GenerateVegetationInstances(TileCoord, *BiomeDef, HeightData);
	GenerationData.GeneratedInstances.Append(VegetationInstances);

	// Generate POI instances
	TArray<FPOIData> POIInstances = GeneratePOIInstances(TileCoord, *BiomeDef, HeightData);
	
	// Convert POI data to PCG instance data
	for (const FPOIData& POI : POIInstances)
	{
		FPCGInstanceData InstanceData;
		InstanceData.Location = POI.Location;
		InstanceData.Rotation = POI.Rotation;
		InstanceData.Scale = POI.Scale;
		InstanceData.OwningTile = TileCoord;
		GenerationData.GeneratedInstances.Add(InstanceData);
	}

	GenerationData.TotalInstanceCount = GenerationData.GeneratedInstances.Num();

	return GenerationData;
}

TArray<FPCGInstanceData> UPCGWorldService::GenerateVegetationInstances(FTileCoord TileCoord, const FBiomeDefinition& BiomeDef, const TArray<float>& HeightData)
{
	TArray<FPCGInstanceData> Instances;

	// Calculate tile world position
	FVector TileWorldPos = TileCoord.ToWorldPosition(64.0f);
	FVector2D TileStart(TileWorldPos.X - 32.0f, TileWorldPos.Y - 32.0f);

	// Simple vegetation placement based on biome rules
	for (const FPCGVegetationRule& VegRule : BiomeDef.VegetationRules)
	{
		// Calculate number of instances based on density
		int32 InstanceCount = FMath::RoundToInt(VegRule.Density * WorldGenSettings.VegetationDensity * 100.0f);
		InstanceCount = FMath::Min(InstanceCount, WorldGenSettings.MaxHISMInstances / BiomeDef.VegetationRules.Num());

		for (int32 i = 0; i < InstanceCount; i++)
		{
			// Random position within tile
			float RandomX = FMath::FRandRange(0.0f, 64.0f);
			float RandomY = FMath::FRandRange(0.0f, 64.0f);
			FVector2D LocalPos(RandomX, RandomY);
			FVector WorldPos = FVector(TileStart + LocalPos, 0.0f);

			// Get height at this position
			int32 HeightIndex = FMath::FloorToInt(RandomY) * 64 + FMath::FloorToInt(RandomX);
			if (HeightData.IsValidIndex(HeightIndex))
			{
				float Height = HeightData[HeightIndex];
				WorldPos.Z = Height;

				// Check height and slope constraints
				if (Height >= VegRule.MinHeight && Height <= VegRule.MaxHeight)
				{
					// TODO: Add slope checking when normal data is available

					FPCGInstanceData InstanceData;
					InstanceData.Location = WorldPos;
					InstanceData.Rotation = FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f);
					InstanceData.Scale = FVector(FMath::FRandRange(VegRule.MinScale, VegRule.MaxScale));
					InstanceData.Mesh = VegRule.VegetationMesh;
					InstanceData.OwningTile = TileCoord;

					Instances.Add(InstanceData);
				}
			}
		}
	}

	return Instances;
}

TArray<FPOIData> UPCGWorldService::GeneratePOIInstances(FTileCoord TileCoord, const FBiomeDefinition& BiomeDef, const TArray<float>& HeightData)
{
	TArray<FPOIData> POIs;

	// Calculate tile world position
	FVector TileWorldPos = TileCoord.ToWorldPosition(64.0f);

	// Simple POI placement based on biome rules
	for (const FPOISpawnRule& POIRule : BiomeDef.POIRules)
	{
		// Check spawn chance
		if (FMath::FRand() <= POIRule.SpawnChance * WorldGenSettings.POIDensity)
		{
			// Find suitable location
			FVector POILocation = TileWorldPos;
			
			// Get height at tile center
			int32 CenterIndex = 32 * 64 + 32; // Center of 64x64 tile
			if (HeightData.IsValidIndex(CenterIndex))
			{
				POILocation.Z = HeightData[CenterIndex];

				FPOIData POIData;
				POIData.POIName = POIRule.POIName;
				POIData.Location = POILocation;
				POIData.Rotation = FRotator::ZeroRotator;
				POIData.Scale = FVector::OneVector;
				POIData.POIBlueprint = POIRule.POIBlueprint;
				POIData.OriginBiome = BiomeDef.BiomeType;

				POIs.Add(POIData);
			}
		}
	}

	return POIs;
}bool 
UPCGWorldService::SpawnPOI(FVector Location, const FPOIData& POIData)
{
	// TODO: Implement actual POI spawning
	UE_LOG(LogPCGWorldService, Log, TEXT("POI spawn requested: %s at (%.1f, %.1f, %.1f)"), 
		*POIData.POIName, Location.X, Location.Y, Location.Z);
	return true;
}

bool UPCGWorldService::UpdateHISMInstances(FTileCoord TileCoord)
{
	// TODO: Implement HISM optimization
	UE_LOG(LogPCGWorldService, Log, TEXT("HISM update requested for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
	return true;
}

bool UPCGWorldService::RemoveContentInArea(FBox Area)
{
	// TODO: Implement content removal
	UE_LOG(LogPCGWorldService, Log, TEXT("Content removal requested for area"));
	return true;
}

FPCGPerformanceStats UPCGWorldService::GetPerformanceStats()
{
	return PerformanceStats;
}

void UPCGWorldService::SetRuntimeOperationsEnabled(bool bEnabled)
{
	bRuntimeOperationsEnabled = bEnabled;
	UE_LOG(LogPCGWorldService, Log, TEXT("Runtime PCG operations %s"), bEnabled ? TEXT("enabled") : TEXT("disabled"));
}

void UPCGWorldService::ClearPCGCache()
{
	GenerationCache.Empty();
	HISMComponents.Empty();
	UE_LOG(LogPCGWorldService, Log, TEXT("PCG cache cleared"));
}

bool UPCGWorldService::ValidatePCGGraph(const FString& GraphPath, TArray<FString>& OutErrors)
{
	OutErrors.Empty();

#if WITH_PCG
	// TODO: Implement PCG graph validation
	UE_LOG(LogPCGWorldService, Log, TEXT("PCG graph validation not yet implemented"));
#else
	OutErrors.Add(TEXT("PCG system not available - using fallback generation"));
#endif

	return OutErrors.Num() == 0;
}

void UPCGWorldService::SetBiomeDefinitions(const TMap<EBiomeType, FBiomeDefinition>& InBiomeDefinitions)
{
	BiomeDefinitions = InBiomeDefinitions;
	UE_LOG(LogPCGWorldService, Log, TEXT("Updated biome definitions with %d biomes"), BiomeDefinitions.Num());
}

UHierarchicalInstancedStaticMeshComponent* UPCGWorldService::CreateHISMComponent(FTileCoord TileCoord, UStaticMesh* Mesh)
{
	// TODO: Implement HISM component creation
	UE_LOG(LogPCGWorldService, Log, TEXT("HISM component creation requested for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
	return nullptr;
}

void UPCGWorldService::UpdatePerformanceStats(float GenerationTimeMs, int32 InstanceCount)
{
	PerformanceStats.LastGenerationTimeMs = GenerationTimeMs;
	PerformanceStats.TotalInstancesGenerated += InstanceCount;
	
	// Update average (simple moving average)
	static int32 SampleCount = 0;
	SampleCount++;
	PerformanceStats.AverageGenerationTimeMs = 
		(PerformanceStats.AverageGenerationTimeMs * (SampleCount - 1) + GenerationTimeMs) / SampleCount;
}

void UPCGWorldService::InitializeDefaultBiomes()
{
	// Initialize with basic biome definitions (these would normally come from data tables)
	
	// Meadows biome
	FBiomeDefinition MeadowsBiome;
	MeadowsBiome.BiomeType = EBiomeType::Meadows;
	MeadowsBiome.BiomeName = TEXT("Meadows");
	
	// Add basic vegetation rule
	FPCGVegetationRule GrassRule;
	GrassRule.Density = 0.5f;
	GrassRule.MinScale = 0.8f;
	GrassRule.MaxScale = 1.2f;
	GrassRule.MinHeight = -10.0f;
	GrassRule.MaxHeight = 50.0f;
	MeadowsBiome.VegetationRules.Add(GrassRule);
	
	BiomeDefinitions.Add(EBiomeType::Meadows, MeadowsBiome);
	
	// Forest biome
	FBiomeDefinition ForestBiome;
	ForestBiome.BiomeType = EBiomeType::Forest;
	ForestBiome.BiomeName = TEXT("Forest");
	
	// Add tree vegetation rule
	FPCGVegetationRule TreeRule;
	TreeRule.Density = 0.3f;
	TreeRule.MinScale = 0.9f;
	TreeRule.MaxScale = 1.5f;
	TreeRule.MinHeight = 0.0f;
	TreeRule.MaxHeight = 80.0f;
	ForestBiome.VegetationRules.Add(TreeRule);
	
	BiomeDefinitions.Add(EBiomeType::Forest, ForestBiome);
	
	UE_LOG(LogPCGWorldService, Log, TEXT("Initialized %d default biome definitions"), BiomeDefinitions.Num());
}