#include "Services/PCGWorldService.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/DateTime.h"
#include "GameFramework/Actor.h"
#include "UObject/ConstructorHelpers.h"

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
	TileActor = nullptr;
	MaxInstancesPerTile = 10000;
	LODDistances.Add(500.0f);  // LOD 0-1 transition
	LODDistances.Add(1500.0f); // LOD 1-2 transition
	LODDistances.Add(5000.0f); // LOD 2-3 transition
}

bool UPCGWorldService::Initialize(const FWorldGenConfig& Settings)
{
	WorldGenSettings = Settings;
	MaxInstancesPerTile = Settings.MaxHISMInstances;
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
			return GeneratePCGContent(TileCoord, BiomeType, HeightData, PCGGraph);
		}
	}
#endif

	// Use fallback generation
	return GenerateFallbackContent(TileCoord, BiomeType, HeightData);
}

FPCGGenerationData UPCGWorldService::GeneratePCGContent(FTileCoord TileCoord, EBiomeType BiomeType, const TArray<float>& HeightData, UPCGGraph* PCGGraph)
{
	FPCGGenerationData GenerationData;
	GenerationData.TileCoord = TileCoord;
	GenerationData.BiomeType = BiomeType;

#if WITH_PCG
	// TODO: Implement full PCG generation when UE5.6 PCG is fully available
	// For now, log and fall back to procedural generation
	UE_LOG(LogPCGWorldService, Log, TEXT("PCG generation with graph %s - falling back to procedural"), *PCGGraph->GetName());
#endif

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

	// Generate vegetation instances with density management
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

	// Apply density limiting
	if (GenerationData.TotalInstanceCount > MaxInstancesPerTile)
	{
		ApplyDensityLimiting(GenerationData);
	}

	return GenerationData;
}

TArray<FPCGInstanceData> UPCGWorldService::GenerateVegetationInstances(FTileCoord TileCoord, const FBiomeDefinition& BiomeDef, const TArray<float>& HeightData)
{
	TArray<FPCGInstanceData> Instances;

	// Calculate tile world position
	FVector TileWorldPos = TileCoord.ToWorldPosition(64.0f);
	FVector2D TileStart(TileWorldPos.X - 32.0f, TileWorldPos.Y - 32.0f);

	// Initialize seeded random for consistent generation
	FRandomStream RandomStream(GetTileRandomSeed(TileCoord));

	// Generate vegetation based on biome rules
	for (const FPCGVegetationRule& VegRule : BiomeDef.VegetationRules)
	{
		// Skip if no mesh is specified
		if (VegRule.VegetationMesh.IsNull())
		{
			continue;
		}

		// Calculate number of instances based on density and biome settings
		int32 BaseInstanceCount = FMath::RoundToInt(VegRule.Density * WorldGenSettings.VegetationDensity * 100.0f);
		int32 MaxInstancesForThisRule = MaxInstancesPerTile / FMath::Max(1, BiomeDef.VegetationRules.Num());
		int32 InstanceCount = FMath::Min(BaseInstanceCount, MaxInstancesForThisRule);

		for (int32 i = 0; i < InstanceCount; i++)
		{
			// Use Poisson disc sampling for better distribution
			FVector2D SamplePoint = GeneratePoissonSample(RandomStream, TileStart, 64.0f, 2.0f);
			FVector WorldPos = FVector(SamplePoint, 0.0f);

			// Get height at this position
			int32 HeightX = FMath::Clamp(FMath::FloorToInt(SamplePoint.X - TileStart.X), 0, 63);
			int32 HeightY = FMath::Clamp(FMath::FloorToInt(SamplePoint.Y - TileStart.Y), 0, 63);
			int32 HeightIndex = HeightY * 64 + HeightX;
			
			if (HeightData.IsValidIndex(HeightIndex))
			{
				float Height = HeightData[HeightIndex];
				WorldPos.Z = Height;

				// Check height constraints
				if (Height >= VegRule.MinHeight && Height <= VegRule.MaxHeight)
				{
					// Check slope constraints (estimate from neighboring heights)
					float Slope = CalculateSlope(HeightData, HeightX, HeightY, 64);
					if (Slope <= VegRule.SlopeLimit)
					{
						FPCGInstanceData InstanceData;
						InstanceData.Location = WorldPos;
						InstanceData.Rotation = FRotator(0.0f, RandomStream.FRandRange(0.0f, 360.0f), 0.0f);
						InstanceData.Scale = FVector(RandomStream.FRandRange(VegRule.MinScale, VegRule.MaxScale));
						InstanceData.Mesh = VegRule.VegetationMesh;
						InstanceData.OwningTile = TileCoord;
						InstanceData.bIsActive = true;

						Instances.Add(InstanceData);
					}
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
	
	// Initialize seeded random for consistent generation
	FRandomStream RandomStream(GetTileRandomSeed(TileCoord));

	// Generate POIs based on biome rules
	for (const FPOISpawnRule& POIRule : BiomeDef.POIRules)
	{
		// Check spawn chance
		if (RandomStream.FRand() <= POIRule.SpawnChance * WorldGenSettings.POIDensity)
		{
			// Find suitable location within tile
			bool bFoundSuitableLocation = false;
			int32 MaxAttempts = 10;
			
			for (int32 Attempt = 0; Attempt < MaxAttempts && !bFoundSuitableLocation; Attempt++)
			{
				FVector2D RandomOffset = FVector2D(
					RandomStream.FRandRange(-30.0f, 30.0f),
					RandomStream.FRandRange(-30.0f, 30.0f)
				);
				
				FVector POILocation = TileWorldPos + FVector(RandomOffset, 0.0f);
				
				// Get height at this position
				int32 HeightX = FMath::Clamp(FMath::FloorToInt(32.0f + RandomOffset.X), 0, 63);
				int32 HeightY = FMath::Clamp(FMath::FloorToInt(32.0f + RandomOffset.Y), 0, 63);
				int32 HeightIndex = HeightY * 64 + HeightX;
				
				if (HeightData.IsValidIndex(HeightIndex))
				{
					float Height = HeightData[HeightIndex];
					POILocation.Z = Height;

					// Check slope requirements
					float Slope = CalculateSlope(HeightData, HeightX, HeightY, 64);
					if (Slope <= POIRule.SlopeLimit)
					{
						// Check distance from other POIs if required
						if (CheckPOISpacingRequirements(POILocation, POIRule.MinDistanceFromOthers))
						{
							FPOIData POIData;
							POIData.POIName = POIRule.POIName;
							POIData.Location = POILocation;
							POIData.Rotation = FRotator::ZeroRotator;
							POIData.Scale = FVector::OneVector;
							POIData.POIBlueprint = POIRule.POIBlueprint;
							POIData.OriginBiome = BiomeDef.BiomeType;
							POIData.bIsSpawned = false;

							POIs.Add(POIData);
							SpawnedPOIs.Add(POIData.POIId, POIData);
							bFoundSuitableLocation = true;
						}
					}
				}
			}
		}
	}

	return POIs;
}

bool UPCGWorldService::SpawnPOI(FVector Location, const FPOIData& POIData)
{
	if (!GetWorld())
	{
		UE_LOG(LogPCGWorldService, Error, TEXT("Cannot spawn POI - no valid world"));
		return false;
	}

	// Check if POI blueprint is valid
	if (POIData.POIBlueprint.IsNull())
	{
		UE_LOG(LogPCGWorldService, Warning, TEXT("POI blueprint is null for POI: %s"), *POIData.POIName);
		return false;
	}

	// Load the blueprint if needed
	UBlueprint* Blueprint = POIData.POIBlueprint.LoadSynchronous();
	if (!Blueprint || !Blueprint->GeneratedClass)
	{
		UE_LOG(LogPCGWorldService, Error, TEXT("Failed to load POI blueprint: %s"), *POIData.POIBlueprint.GetAssetName());
		return false;
	}

	// Spawn the actor
	FTransform SpawnTransform(POIData.Rotation, Location, POIData.Scale);
	AActor* SpawnedActor = GetWorld()->SpawnActor<AActor>(Blueprint->GeneratedClass, SpawnTransform);
	
	if (SpawnedActor)
	{
		// Store reference for management
		SpawnedPOIActors.Add(POIData.POIId, SpawnedActor);
		
		UE_LOG(LogPCGWorldService, Log, TEXT("Successfully spawned POI: %s at (%.1f, %.1f, %.1f)"), 
			*POIData.POIName, Location.X, Location.Y, Location.Z);
		return true;
	}

	UE_LOG(LogPCGWorldService, Error, TEXT("Failed to spawn POI actor: %s"), *POIData.POIName);
	return false;
}

bool UPCGWorldService::UpdateHISMInstances(FTileCoord TileCoord)
{
	// Get or create HISM components for this tile
	FHISMComponentArray* TileComponents = HISMComponents.Find(TileCoord);
	if (!TileComponents)
	{
		// Create new HISM components for this tile
		CreateHISMComponentsForTile(TileCoord);
		TileComponents = HISMComponents.Find(TileCoord);
	}

	if (!TileComponents)
	{
		UE_LOG(LogPCGWorldService, Error, TEXT("Failed to create HISM components for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
		return false;
	}

	// Get generation data for this tile
	const FPCGGenerationData* GenerationData = GenerationCache.Find(TileCoord);
	if (!GenerationData)
	{
		UE_LOG(LogPCGWorldService, Warning, TEXT("No generation data found for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
		return false;
	}

	// Group instances by mesh
	TMap<UStaticMesh*, TArray<FTransform>> InstancesByMesh;
	for (const FPCGInstanceData& InstanceData : GenerationData->GeneratedInstances)
	{
		if (InstanceData.bIsActive && !InstanceData.Mesh.IsNull())
		{
			UStaticMesh* Mesh = InstanceData.Mesh.LoadSynchronous();
			if (Mesh)
			{
				FTransform Transform(InstanceData.Rotation, InstanceData.Location, InstanceData.Scale);
				InstancesByMesh.FindOrAdd(Mesh).Add(Transform);
			}
		}
	}

	// Update HISM components
	for (auto& MeshPair : InstancesByMesh)
	{
		UStaticMesh* Mesh = MeshPair.Key;
		const TArray<FTransform>& Transforms = MeshPair.Value;

		UHierarchicalInstancedStaticMeshComponent* HISMComp = GetOrCreateHISMComponent(TileCoord, Mesh);
		if (HISMComp)
		{
			// Clear existing instances and add new ones
			HISMComp->ClearInstances();
			for (const FTransform& Transform : Transforms)
			{
				HISMComp->AddInstance(Transform);
			}

			// Update performance stats
			PerformanceStats.ActiveHISMInstances += Transforms.Num();
		}
	}

	UE_LOG(LogPCGWorldService, Log, TEXT("Updated HISM instances for tile (%d, %d) - %d instance groups"), 
		TileCoord.X, TileCoord.Y, InstancesByMesh.Num());
	return true;
}

bool UPCGWorldService::RemoveContentInArea(FBox Area)
{
	bool bRemovedAny = false;

	// Remove POI actors in the area
	TArray<FGuid> POIsToRemove;
	for (auto& POIPair : SpawnedPOIActors)
	{
		AActor* POIActor = POIPair.Value;
		if (IsValid(POIActor) && Area.IsInside(POIActor->GetActorLocation()))
		{
			POIActor->Destroy();
			POIsToRemove.Add(POIPair.Key);
			bRemovedAny = true;
		}
	}

	// Clean up POI references
	for (const FGuid& POIId : POIsToRemove)
	{
		SpawnedPOIActors.Remove(POIId);
		SpawnedPOIs.Remove(POIId);
	}

	// Remove vegetation instances in the area
	for (auto& CachePair : GenerationCache)
	{
		FPCGGenerationData& GenerationData = CachePair.Value;
		TArray<FPCGInstanceData> RemainingInstances;

		for (FPCGInstanceData& InstanceData : GenerationData.GeneratedInstances)
		{
			if (!Area.IsInside(InstanceData.Location))
			{
				RemainingInstances.Add(InstanceData);
			}
			else
			{
				bRemovedAny = true;
			}
		}

		if (RemainingInstances.Num() != GenerationData.GeneratedInstances.Num())
		{
			GenerationData.GeneratedInstances = RemainingInstances;
			GenerationData.TotalInstanceCount = RemainingInstances.Num();
			
			// Update HISM for affected tile
			UpdateHISMInstances(GenerationData.TileCoord);
		}
	}

	if (bRemovedAny)
	{
		UE_LOG(LogPCGWorldService, Log, TEXT("Removed content in area (%.1f,%.1f,%.1f) to (%.1f,%.1f,%.1f)"), 
			Area.Min.X, Area.Min.Y, Area.Min.Z, Area.Max.X, Area.Max.Y, Area.Max.Z);
	}

	return bRemovedAny;
}

FPCGPerformanceStats UPCGWorldService::GetPerformanceStats()
{
	// Update memory usage estimate
	PerformanceStats.MemoryUsageMB = EstimateMemoryUsage();
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
	
	// Clean up HISM components
	for (auto& TilePair : HISMComponents)
	{
		for (UHierarchicalInstancedStaticMeshComponent* Component : TilePair.Value.Components)
		{
			if (IsValid(Component))
			{
				Component->ClearInstances();
			}
		}
	}
	HISMComponents.Empty();

	// Clean up spawned POIs
	for (auto& POIPair : SpawnedPOIActors)
	{
		if (IsValid(POIPair.Value))
		{
			POIPair.Value->Destroy();
		}
	}
	SpawnedPOIActors.Empty();
	SpawnedPOIs.Empty();

	// Reset performance stats
	PerformanceStats = FPCGPerformanceStats();
	
	UE_LOG(LogPCGWorldService, Log, TEXT("PCG cache cleared"));
}

bool UPCGWorldService::ValidatePCGGraph(const FString& GraphPath, TArray<FString>& OutErrors)
{
	OutErrors.Empty();

#if WITH_PCG
	// Load and validate the PCG graph
	UObject* GraphObject = LoadObject<UObject>(nullptr, *GraphPath);
	if (!GraphObject)
	{
		OutErrors.Add(FString::Printf(TEXT("Failed to load PCG graph at path: %s"), *GraphPath));
		return false;
	}

	UPCGGraph* PCGGraph = Cast<UPCGGraph>(GraphObject);
	if (!PCGGraph)
	{
		OutErrors.Add(FString::Printf(TEXT("Object at path is not a valid PCG graph: %s"), *GraphPath));
		return false;
	}

	// TODO: Add more detailed PCG graph validation
	UE_LOG(LogPCGWorldService, Log, TEXT("PCG graph validation passed: %s"), *GraphPath);
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
	return GetOrCreateHISMComponent(TileCoord, Mesh);
}

// Private helper methods

void UPCGWorldService::UpdatePerformanceStats(float GenerationTimeMs, int32 InstanceCount)
{
	PerformanceStats.LastGenerationTimeMs = GenerationTimeMs;
	PerformanceStats.TotalInstancesGenerated += InstanceCount;
	
	// Update average (simple moving average)
	static int32 SampleCount = 0;
	SampleCount++;
	if (SampleCount > 0)
	{
		PerformanceStats.AverageGenerationTimeMs = 
			(PerformanceStats.AverageGenerationTimeMs * (SampleCount - 1) + GenerationTimeMs) / SampleCount;
	}
}

void UPCGWorldService::InitializeDefaultBiomes()
{
	// Initialize with enhanced biome definitions
	
	// Meadows biome
	FBiomeDefinition MeadowsBiome;
	MeadowsBiome.BiomeType = EBiomeType::Meadows;
	MeadowsBiome.BiomeName = TEXT("Meadows");
	
	// Add grass vegetation rule
	FPCGVegetationRule GrassRule;
	GrassRule.Density = 0.8f;
	GrassRule.MinScale = 0.8f;
	GrassRule.MaxScale = 1.2f;
	GrassRule.MinHeight = -10.0f;
	GrassRule.MaxHeight = 50.0f;
	GrassRule.SlopeLimit = 35.0f;
	MeadowsBiome.VegetationRules.Add(GrassRule);
	
	// Add flower vegetation rule
	FPCGVegetationRule FlowerRule;
	FlowerRule.Density = 0.3f;
	FlowerRule.MinScale = 0.5f;
	FlowerRule.MaxScale = 1.0f;
	FlowerRule.MinHeight = -5.0f;
	FlowerRule.MaxHeight = 40.0f;
	FlowerRule.SlopeLimit = 25.0f;
	MeadowsBiome.VegetationRules.Add(FlowerRule);
	
	// Add POI rule for meadows
	FPOISpawnRule MeadowPOI;
	MeadowPOI.POIName = TEXT("MeadowShrine");
	MeadowPOI.SpawnChance = 0.05f;
	MeadowPOI.MinDistanceFromOthers = 1000.0f;
	MeadowPOI.SlopeLimit = 15.0f;
	MeadowPOI.bRequiresFlatGround = true;
	MeadowsBiome.POIRules.Add(MeadowPOI);
	
	BiomeDefinitions.Add(EBiomeType::Meadows, MeadowsBiome);
	
	// Forest biome
	FBiomeDefinition ForestBiome;
	ForestBiome.BiomeType = EBiomeType::Forest;
	ForestBiome.BiomeName = TEXT("Forest");
	
	// Add tree vegetation rule
	FPCGVegetationRule TreeRule;
	TreeRule.Density = 0.4f;
	TreeRule.MinScale = 0.9f;
	TreeRule.MaxScale = 1.8f;
	TreeRule.MinHeight = 0.0f;
	TreeRule.MaxHeight = 100.0f;
	TreeRule.SlopeLimit = 45.0f;
	ForestBiome.VegetationRules.Add(TreeRule);
	
	// Add undergrowth vegetation rule
	FPCGVegetationRule UndergrowthRule;
	UndergrowthRule.Density = 0.6f;
	UndergrowthRule.MinScale = 0.7f;
	UndergrowthRule.MaxScale = 1.3f;
	UndergrowthRule.MinHeight = 0.0f;
	UndergrowthRule.MaxHeight = 80.0f;
	UndergrowthRule.SlopeLimit = 40.0f;
	ForestBiome.VegetationRules.Add(UndergrowthRule);
	
	// Add POI rule for forests
	FPOISpawnRule ForestPOI;
	ForestPOI.POIName = TEXT("AbandonedCamp");
	ForestPOI.SpawnChance = 0.08f;
	ForestPOI.MinDistanceFromOthers = 800.0f;
	ForestPOI.SlopeLimit = 30.0f;
	ForestPOI.bRequiresFlatGround = false;
	ForestBiome.POIRules.Add(ForestPOI);
	
	BiomeDefinitions.Add(EBiomeType::Forest, ForestBiome);
	
	// Mountains biome
	FBiomeDefinition MountainBiome;
	MountainBiome.BiomeType = EBiomeType::Mountains;
	MountainBiome.BiomeName = TEXT("Mountains");
	
	// Sparse vegetation for mountains
	FPCGVegetationRule MountainTreeRule;
	MountainTreeRule.Density = 0.1f;
	MountainTreeRule.MinScale = 0.6f;
	MountainTreeRule.MaxScale = 1.2f;
	MountainTreeRule.MinHeight = 30.0f;
	MountainTreeRule.MaxHeight = 120.0f;
	MountainTreeRule.SlopeLimit = 50.0f;
	MountainBiome.VegetationRules.Add(MountainTreeRule);
	
	// Add POI rule for mountains
	FPOISpawnRule MountainPOI;
	MountainPOI.POIName = TEXT("MountainCave");
	MountainPOI.SpawnChance = 0.03f;
	MountainPOI.MinDistanceFromOthers = 1500.0f;
	MountainPOI.SlopeLimit = 60.0f;
	MountainPOI.bRequiresFlatGround = false;
	MountainBiome.POIRules.Add(MountainPOI);
	
	BiomeDefinitions.Add(EBiomeType::Mountains, MountainBiome);
	
	UE_LOG(LogPCGWorldService, Log, TEXT("Initialized %d default biome definitions"), BiomeDefinitions.Num());
}

uint32 UPCGWorldService::GetTileRandomSeed(FTileCoord TileCoord) const
{
	// Generate deterministic seed based on tile coordinates and world seed
	return HashCombine(HashCombine(GetTypeHash(TileCoord.X), GetTypeHash(TileCoord.Y)), GetTypeHash(WorldGenSettings.Seed));
}

FVector2D UPCGWorldService::GeneratePoissonSample(FRandomStream& RandomStream, FVector2D TileStart, float TileSize, float MinDistance)
{
	// Simple random sample within the tile. For production, implement true Poisson disk
	// by checking against existing samples and using MinDistance.
	(void)MinDistance; // suppress unused parameter warning for now

	return TileStart + FVector2D(
		RandomStream.FRandRange(0.0f, TileSize),
		RandomStream.FRandRange(0.0f, TileSize)
	);
}

float UPCGWorldService::CalculateSlope(const TArray<float>& HeightData, int32 X, int32 Y, int32 GridSize)
{
	if (!HeightData.IsValidIndex(Y * GridSize + X))
	{
		return 0.0f;
	}

	float CenterHeight = HeightData[Y * GridSize + X];
	
	// Calculate slope using neighboring heights
	float MaxSlope = 0.0f;
	for (int32 DX = -1; DX <= 1; DX++)
	{
		for (int32 DY = -1; DY <= 1; DY++)
		{
			if (DX == 0 && DY == 0) continue;
			
			int32 NeighborX = X + DX;
			int32 NeighborY = Y + DY;
			
			if (NeighborX >= 0 && NeighborX < GridSize && NeighborY >= 0 && NeighborY < GridSize)
			{
				int32 NeighborIndex = NeighborY * GridSize + NeighborX;
				if (HeightData.IsValidIndex(NeighborIndex))
				{
					float NeighborHeight = HeightData[NeighborIndex];
					float HeightDiff = FMath::Abs(NeighborHeight - CenterHeight);
					float Distance = FMath::Sqrt(static_cast<float>(DX * DX + DY * DY)); // Grid distance
					float Slope = FMath::RadiansToDegrees(FMath::Atan2(HeightDiff, Distance));
					MaxSlope = FMath::Max(MaxSlope, Slope);
				}
			}
		}
	}
	
	return MaxSlope;
}

bool UPCGWorldService::CheckPOISpacingRequirements(FVector Location, float MinDistance)
{
	// Check against existing POIs
	for (const auto& POIPair : SpawnedPOIs)
	{
		const FPOIData& ExistingPOI = POIPair.Value;
		float Distance = FVector::Dist(Location, ExistingPOI.Location);
		if (Distance < MinDistance)
		{
			return false;
		}
	}
	
	return true;
}

void UPCGWorldService::ApplyDensityLimiting(FPCGGenerationData& GenerationData)
{
	if (GenerationData.TotalInstanceCount <= MaxInstancesPerTile)
	{
		return;
	}
	
	// Sort instances by some priority (e.g., distance from tile center, or keep first N instances)
	FVector TileCenter = GenerationData.TileCoord.ToWorldPosition(64.0f);
	
	GenerationData.GeneratedInstances.Sort([TileCenter](const FPCGInstanceData& A, const FPCGInstanceData& B)
	{
		float DistA = FVector::DistSquared(A.Location, TileCenter);
		float DistB = FVector::DistSquared(B.Location, TileCenter);
		return DistA < DistB; // Keep instances closer to tile center
	});
	
	// Truncate to max instances
	if (GenerationData.GeneratedInstances.Num() > MaxInstancesPerTile)
	{
		GenerationData.GeneratedInstances.SetNum(MaxInstancesPerTile);
		GenerationData.TotalInstanceCount = MaxInstancesPerTile;
		
		UE_LOG(LogPCGWorldService, Warning, TEXT("Applied density limiting to tile (%d, %d) - reduced to %d instances"), 
			GenerationData.TileCoord.X, GenerationData.TileCoord.Y, MaxInstancesPerTile);
	}
}

void UPCGWorldService::CreateHISMComponentsForTile(FTileCoord TileCoord)
{
	if (!GetWorld())
	{
		UE_LOG(LogPCGWorldService, Error, TEXT("Cannot create HISM components - no valid world"));
		return;
	}
	
	// Create or get tile actor to hold HISM components
	if (!TileActor)
	{
		FVector TileWorldPos = TileCoord.ToWorldPosition(64.0f);
		FTransform ActorTransform(FRotator::ZeroRotator, TileWorldPos, FVector::OneVector);
		TileActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), ActorTransform);
#if WITH_EDITOR
		TileActor->SetActorLabel(TEXT("PCGTileActor"));
#endif
	}
	
	// Initialize empty array for this tile
	FHISMComponentArray ComponentArray;
	ComponentArray.Components = TArray<UHierarchicalInstancedStaticMeshComponent*>();
	HISMComponents.Add(TileCoord, ComponentArray);
	
	UE_LOG(LogPCGWorldService, Log, TEXT("Created HISM component array for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
}

UHierarchicalInstancedStaticMeshComponent* UPCGWorldService::GetOrCreateHISMComponent(FTileCoord TileCoord, UStaticMesh* Mesh)
{
	if (!Mesh || !GetWorld())
	{
		return nullptr;
	}
	
	// Get components for this tile
	FHISMComponentArray* TileComponentArray = HISMComponents.Find(TileCoord);
	if (!TileComponentArray)
	{
		CreateHISMComponentsForTile(TileCoord);
		TileComponentArray = HISMComponents.Find(TileCoord);
	}
	
	// Look for existing component with this mesh
	for (UHierarchicalInstancedStaticMeshComponent* Component : TileComponentArray->Components)
	{
		if (IsValid(Component) && Component->GetStaticMesh() == Mesh)
		{
			return Component;
		}
	}
	
	// Create new HISM component
	if (!TileActor)
	{
		FVector TileWorldPos = TileCoord.ToWorldPosition(64.0f);
		FTransform ActorTransform(FRotator::ZeroRotator, TileWorldPos, FVector::OneVector);
		TileActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), ActorTransform);
#if WITH_EDITOR
		TileActor->SetActorLabel(TEXT("PCGTileActor"));
#endif
	}
	
	UHierarchicalInstancedStaticMeshComponent* NewComponent = NewObject<UHierarchicalInstancedStaticMeshComponent>(TileActor);
	NewComponent->SetStaticMesh(Mesh);
	NewComponent->SetWorldLocation(TileCoord.ToWorldPosition(64.0f));
	NewComponent->AttachToComponent(TileActor->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
	NewComponent->RegisterComponent();
	
	// Configure HISM settings for performance
	NewComponent->SetCullDistances(LODDistances[0], LODDistances[2]);
	NewComponent->bUseAsOccluder = false; // Vegetation typically shouldn't occlude
	
	TileComponentArray->Components.Add(NewComponent);
	
	UE_LOG(LogPCGWorldService, Log, TEXT("Created new HISM component for mesh %s on tile (%d, %d)"), 
		*Mesh->GetName(), TileCoord.X, TileCoord.Y);
	
	return NewComponent;
}

float UPCGWorldService::EstimateMemoryUsage()
{
	float TotalMemoryMB = 0.0f;
	
	// Estimate cache memory usage
	TotalMemoryMB += GenerationCache.Num() * 0.1f; // Rough estimate per generation data entry
	
	// Estimate HISM memory usage
	int32 TotalInstances = 0;
	for (const auto& TilePair : HISMComponents)
	{
		for (UHierarchicalInstancedStaticMeshComponent* Component : TilePair.Value.Components)
		{
			if (IsValid(Component))
			{
				TotalInstances += Component->GetInstanceCount();
			}
		}
	}
	TotalMemoryMB += TotalInstances * 0.001f; // Rough estimate per instance
	
	// Estimate POI memory usage
	TotalMemoryMB += SpawnedPOIs.Num() * 0.05f; // Rough estimate per POI
	
	return TotalMemoryMB;
}
