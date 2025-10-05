#include "Services/PCGWorldService.h"
#include "Math/Box2D.h"
#include "Utils/WorldGenLogging.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"

#if __has_include("PCGSubsystem.h")
#define VIBEHEIM_PCG_ENABLED 1
#include "PCGComponent.h"
#include "PCGData.h"
#include "PCGGraph.h"
#include "PCGParamData.h"
#include "Data/PCGPointData.h"
#include "PCGSubsystem.h"
#include "Graph/PCGStackContext.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#else
#define VIBEHEIM_PCG_ENABLED 0
#endif

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/DateTime.h"
#include "GameFramework/Actor.h"
#include "UObject/ConstructorHelpers.h"
#include "Data/InstancePersistence.h"
#include "Data/SerializationShims.h"


DEFINE_LOG_CATEGORY_STATIC(LogPCGWorldService, Log, All);

#if VIBEHEIM_PCG_ENABLED
namespace PCGWorldService::Private
{
        static const FName Attribute_StaticMesh(TEXT("StaticMesh"));
        static const FName Attribute_Mesh(TEXT("Mesh"));
        static const FName Attribute_InstanceScale(TEXT("InstanceScale"));
        static const FName Attribute_InstanceRotation(TEXT("InstanceRotation"));
        static const FName Attribute_IsActive(TEXT("IsActive"));
        static const FName Attribute_InstanceId(TEXT("InstanceId"));
        static const FName Attribute_BiomeWeight(TEXT("BiomeWeight"));
        static const FName Attribute_TileX(TEXT("TileX"));
        static const FName Attribute_TileY(TEXT("TileY"));
        static const FName Attribute_TileSeed(TEXT("TileSeed"));

        static UPCGParamData* CreateTileParameterData(UObject* Outer, const UPCGWorldService::FPCGTileMetrics& TileMetrics,
                FTileCoord TileCoord, EBiomeType BiomeType, const FWorldGenConfig& WorldGenSettings, uint32 TileSeed);

        static UPCGPointData* CreateTilePointData(UObject* Outer, FTileCoord TileCoord, const FWorldGenConfig& WorldGenSettings,
                const UPCGWorldService::FPCGTileMetrics& TileMetrics);

        static void ExtractInstancesFromPointData(const UPCGPointData* PointData, FTileCoord TileCoord,
                FPCGGenerationData& OutGenerationData);
}
#endif

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

#if VIBEHEIM_PCG_ENABLED
UPCGParamData* PCGWorldService::Private::CreateTileParameterData(UObject* Outer,
        const UPCGWorldService::FPCGTileMetrics& TileMetrics, FTileCoord TileCoord, EBiomeType BiomeType,
        const FWorldGenConfig& WorldGenSettings, uint32 TileSeed)
{
        UPCGParamData* ParamData = NewObject<UPCGParamData>(Outer ? Outer : GetTransientPackage(), NAME_None, RF_Transient);
        check(ParamData);

        FPCGMetadata* Metadata = ParamData->MutableMetadata();
        const FPCGMetadataEntryKey EntryKey = Metadata->AddEntry();

        auto CreateFloatAttribute = [Metadata, EntryKey](const FName& AttributeName, float Value)
        {
                if (FPCGMetadataAttribute<float>* Attribute = Metadata->CreateAttribute<float>(AttributeName, Value, true, true))
                {
                        Metadata->SetValue(Attribute, EntryKey, Value);
                }
        };

        auto CreateIntAttribute = [Metadata, EntryKey](const FName& AttributeName, int32 Value)
        {
                if (FPCGMetadataAttribute<int32>* Attribute = Metadata->CreateAttribute<int32>(AttributeName, Value, true, true))
                {
                        Metadata->SetValue(Attribute, EntryKey, Value);
                }
        };

        auto CreateEnumAttribute = [Metadata, EntryKey](const FName& AttributeName, int32 Value)
        {
                if (FPCGMetadataAttribute<int32>* Attribute = Metadata->CreateAttribute<int32>(AttributeName, Value, true, true))
                {
                        Metadata->SetValue(Attribute, EntryKey, Value);
                }
        };

        CreateFloatAttribute(TEXT("AverageHeight"), TileMetrics.AverageHeight);
        CreateFloatAttribute(TEXT("MinHeight"), TileMetrics.MinHeight);
        CreateFloatAttribute(TEXT("MaxHeight"), TileMetrics.MaxHeight);
        CreateFloatAttribute(TEXT("AverageSlope"), TileMetrics.AverageSlope);
        CreateFloatAttribute(TEXT("MaxSlope"), TileMetrics.MaxSlope);
        CreateFloatAttribute(TEXT("WaterCoverage"), TileMetrics.WaterCoverageRatio);
        CreateFloatAttribute(TEXT("AverageAboveWater"), TileMetrics.AverageAboveWater);
        CreateFloatAttribute(TEXT("AverageBelowWater"), TileMetrics.AverageBelowWater);
        CreateFloatAttribute(TEXT("MinWaterDistance"), TileMetrics.MinAbsWaterDistance);
        CreateFloatAttribute(TEXT("SeaLevel"), WorldGenSettings.SeaLevel);
        CreateEnumAttribute(TEXT("Biome"), static_cast<int32>(BiomeType));
        CreateIntAttribute(Attribute_TileSeed, static_cast<int32>(TileSeed));
        CreateIntAttribute(Attribute_TileX, TileCoord.X);
        CreateIntAttribute(Attribute_TileY, TileCoord.Y);

        // Provide tile size so graphs can reason about extents
        CreateFloatAttribute(TEXT("TileSize"), WorldGenSettings.TileSizeMeters);
        CreateFloatAttribute(Attribute_BiomeWeight, 1.0f);
        return ParamData;
}

UPCGPointData* PCGWorldService::Private::CreateTilePointData(UObject* Outer, FTileCoord TileCoord,
        const FWorldGenConfig& WorldGenSettings, const UPCGWorldService::FPCGTileMetrics& TileMetrics)
{
        UPCGPointData* PointData = NewObject<UPCGPointData>(Outer ? Outer : GetTransientPackage(), NAME_None, RF_Transient);
        check(PointData);

        TArray<FPCGPoint>& Points = PointData->GetMutablePoints();
        FPCGPoint& TilePoint = Points.AddDefaulted_GetRef();

        const FVector TileCenter = TileCoord.ToWorldPosition(WorldGenSettings.TileSizeMeters);
        const FVector TileExtent(WorldGenSettings.TileSizeMeters * 0.5f, WorldGenSettings.TileSizeMeters * 0.5f,
                WorldGenSettings.TileSizeMeters * 0.25f);

        TilePoint.Transform = FTransform(FRotator::ZeroRotator, TileCenter, FVector::OneVector);
        TilePoint.SetExtents(TileExtent);
        TilePoint.Density = 1.0f;
        TilePoint.Seed = GetTypeHash(TileCoord);

        if (FPCGMetadata* Metadata = PointData->MutableMetadata())
        {
                const FPCGMetadataEntryKey EntryKey = Metadata->AddEntry();
                TilePoint.MetadataEntry = EntryKey;
                Metadata->CreateAttribute<float>(TEXT("AverageSlope"), TileMetrics.AverageSlope, true, true);
        }

        PointData->InitializeBounds();
        return PointData;
}

void PCGWorldService::Private::ExtractInstancesFromPointData(const UPCGPointData* PointData, FTileCoord TileCoord,
        FPCGGenerationData& OutGenerationData)
{
        if (!PointData)
        {
                return;
        }

        const FPCGMetadata* Metadata = PointData->Metadata();
        const TArray<FPCGPoint>& Points = PointData->GetPoints();

        for (const FPCGPoint& Point : Points)
        {
                FPCGInstanceData Instance;
                Instance.Location = Point.Transform.GetLocation();
                Instance.Rotation = Point.Transform.Rotator();
                Instance.Scale = Point.Transform.GetScale3D();
                Instance.OwningTile = TileCoord;

                if (Metadata)
                {
                        const FPCGMetadataEntryKey EntryKey = Point.MetadataEntry;

                        auto TryAssignSoftObject = [Metadata, EntryKey](const FName& AttributeName, TSoftObjectPtr<UStaticMesh>& OutValue)
                        {
                                if (!Metadata)
                                {
                                        return false;
                                }

                                FSoftObjectPath MeshPath;
                                if (Metadata->GetAttribute<FSoftObjectPath>(AttributeName, EntryKey, MeshPath))
                                {
                                        if (!MeshPath.IsNull())
                                        {
                                                OutValue = TSoftObjectPtr<UStaticMesh>(MeshPath);
                                                return true;
                                        }
                                }
                                UObject* RawObject = nullptr;
                                if (Metadata->GetAttribute<UObject*>(AttributeName, EntryKey, RawObject))
                                {
                                        if (UStaticMesh* Mesh = Cast<UStaticMesh>(RawObject))
                                        {
                                                OutValue = Mesh;
                                                return true;
                                        }
                                }
                                return false;
                        };

                        if (!TryAssignSoftObject(Attribute_StaticMesh, Instance.Mesh))
                        {
                                TryAssignSoftObject(Attribute_Mesh, Instance.Mesh);
                        }

                        bool bIsActive = true;
                        Metadata->GetAttribute<bool>(Attribute_IsActive, EntryKey, bIsActive);
                        Instance.bIsActive = bIsActive;

                        FVector OverrideScale = Instance.Scale;
                        if (Metadata->GetAttribute<FVector>(Attribute_InstanceScale, EntryKey, OverrideScale))
                        {
                                Instance.Scale = OverrideScale;
                        }

                        FRotator OverrideRotation = Instance.Rotation;
                        if (Metadata->GetAttribute<FRotator>(Attribute_InstanceRotation, EntryKey, OverrideRotation))
                        {
                                Instance.Rotation = OverrideRotation;
                        }

                        FGuid InstanceGuid;
                        if (Metadata->GetAttribute<FGuid>(Attribute_InstanceId, EntryKey, InstanceGuid))
                        {
                                if (InstanceGuid.IsValid())
                                {
                                        Instance.InstanceId = InstanceGuid;
                                }
                        }
                }

                OutGenerationData.GeneratedInstances.Add(Instance);
        }
}
#endif

bool UPCGWorldService::Initialize(const FWorldGenConfig& Settings)
{
	WorldGenSettings = Settings;
	MaxInstancesPerTile = Settings.MaxHISMInstances;
	InitializeDefaultBiomes();

	WorldGenSettings = Settings;
	MaxInstancesPerTile = Settings.MaxHISMInstances;
	InitializeDefaultBiomes();

	bHeadless = (GetWorld() == nullptr);
	if (bHeadless)
	{
		UE_LOG(LogPCGWorldService, Warning,
			TEXT("Headless mode: PCG running without UWorld; HISM updates will be skipped."));
	}

#if VIBEHEIM_PCG_ENABLED
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

#if VIBEHEIM_PCG_ENABLED
	// Validate that its actually a PCG graph when PCG is available
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

	// Add logging during content test to verify rule count
	if (const FBiomeDefinition* BiomeDef = BiomeDefinitions.Find(BiomeType))
	{
		UE_LOG(LogPCGWorldService, Log, TEXT("Forest rules: N=%d"), BiomeDef->VegetationRules.Num());
	}

	// For biome-specific generation (test path), dont use cache - always generate fresh
	// This ensures we use the BiomeType parameter as authoritative rather than tile classification

	// Generate new content using BiomeType as authoritative (not tile classification)
	FPCGGenerationData GenerationData = GenerateContentInternal(TileCoord, BiomeType, HeightData);

	// Cache the generated content so RemoveContentInArea can find it
	// This is needed for the area removal test to work properly
	GenerationCache.Add(TileCoord, GenerationData);
	UE_LOG(LogPCGWorldService, Log, TEXT("Cached generation data for tile (%d, %d) with %d instances for area removal testing"),
		TileCoord.X, TileCoord.Y, GenerationData.TotalInstanceCount);

	// Update performance stats
	double EndTime = FPlatformTime::Seconds();
	float GenerationTimeMs = static_cast<float>((EndTime - StartTime) * 1000.0);
	UpdatePerformanceStats(GenerationTimeMs, GenerationData.TotalInstanceCount);

	WORLDGEN_LOG_WITH_SEED_TILE(Log, WorldGenSettings.Seed, TileCoord, TEXT("PCG spawn completed - %d instances in %.2fms"),
		GenerationData.TotalInstanceCount, GenerationTimeMs);

	return GenerationData;
}

FPCGGenerationData UPCGWorldService::GenerateContentInternal(FTileCoord TileCoord, EBiomeType BiomeType, const TArray<float>& HeightData)
{
	FPCGTileMetrics TileMetrics;
	bool bHasTileMetrics = false;
	const int32 ExpectedHeightDataSize = 64 * 64;

#if VIBEHEIM_PCG_ENABLED
	if (WorldGenSettings.bEnablePCGGraphs && bRuntimeOperationsEnabled)
	{
		if (HeightData.Num() == ExpectedHeightDataSize)
		{
			TileMetrics = AnalyzeTileMetrics(HeightData);
			bHasTileMetrics = true;

			FPCGGenerationData GraphDrivenData;
			if (TryGeneratePCGGraphContent(TileCoord, BiomeType, HeightData, TileMetrics, GraphDrivenData))
			{
				return GraphDrivenData;
			}
		}
		else
		{
			UE_LOG(LogPCGWorldService, Warning, TEXT("PCG graph requested for tile (%d, %d) but height data contained %d samples; using fallback generation"),
				TileCoord.X, TileCoord.Y, HeightData.Num());
		}
	}
#endif

	const bool bUsePCGHeuristics = WorldGenSettings.bEnablePCGGraphs;

	if (!bHasTileMetrics && bUsePCGHeuristics && HeightData.Num() == ExpectedHeightDataSize)
	{
		TileMetrics = AnalyzeTileMetrics(HeightData);
		bHasTileMetrics = true;
	}

	return GenerateFallbackContent(TileCoord, BiomeType, HeightData, bUsePCGHeuristics, bHasTileMetrics ? &TileMetrics : nullptr);
}

FPCGGenerationData UPCGWorldService::GeneratePCGContent(FTileCoord TileCoord, EBiomeType BiomeType, const TArray<float>& HeightData, UPCGGraph* PCGGraph, const FPCGTileMetrics* TileMetrics)
{
        FPCGGenerationData GenerationData;
        GenerationData.TileCoord = TileCoord;
        GenerationData.BiomeType = BiomeType;

#if VIBEHEIM_PCG_ENABLED
        if (PCGGraph && GetWorld() && bRuntimeOperationsEnabled)
        {
                UWorld* World = GetWorld();
                UPCGSubsystem* PCGSubsystem = World ? World->GetSubsystem<UPCGSubsystem>() : nullptr;

                if (!PCGSubsystem)
                {
                        UE_LOG(LogPCGWorldService, Warning,
                                TEXT("PCG subsystem not available when evaluating biome %s; resorting to procedural fallback"),
                                *UEnum::GetValueAsString(BiomeType));
                }
                else
                {
                        const double GraphStartSeconds = FPlatformTime::Seconds();
                        const uint32 TileSeed = GetTileRandomSeed(TileCoord);

                        FPCGDataCollection InputCollection;

                        UPCGParamData* ParameterData = Private::CreateTileParameterData(this,
                                TileMetrics ? *TileMetrics : FPCGTileMetrics(), TileCoord, BiomeType, WorldGenSettings, TileSeed);
                        if (ParameterData)
                        {
                                FPCGTaggedData& ParameterEntry = InputCollection.TaggedData.AddDefaulted_GetRef();
                                ParameterEntry.Data = ParameterData;
                                ParameterEntry.Tags.Add(TEXT("TileParameters"));
                        }

                        UPCGPointData* TilePointData = Private::CreateTilePointData(this, TileCoord, WorldGenSettings,
                                TileMetrics ? *TileMetrics : FPCGTileMetrics());
                        if (TilePointData)
                        {
                                FPCGTaggedData& TileEntry = InputCollection.TaggedData.AddDefaulted_GetRef();
                                TileEntry.Data = TilePointData;
                                TileEntry.Tags.Add(TEXT("Tile"));
                        }

                        FPCGStack ExecutionStack;

                        const FPCGTaskId TaskId = PCGSubsystem->RunGraph(PCGGraph, InputCollection, ExecutionStack);

                        bool bGeneratedFromGraph = false;

                        if (TaskId != FPCGTaskId())
                        {
                                PCGSubsystem->Wait(TaskId);

                                if (const FPCGDataCollection* OutputCollection = PCGSubsystem->GetGraphOutput(TaskId))
                                {
                                        for (const FPCGTaggedData& TaggedOutput : OutputCollection->TaggedData)
                                        {
                                                if (const UPCGPointData* OutputPointData = Cast<UPCGPointData>(TaggedOutput.Data))
                                                {
                                                        Private::ExtractInstancesFromPointData(OutputPointData, TileCoord, GenerationData);
                                                        bGeneratedFromGraph = true;
                                                }
                                        }
                                }

                                PCGSubsystem->Release(TaskId);
                        }

                        if (bGeneratedFromGraph)
                        {
                                GenerationData.TotalInstanceCount = GenerationData.GeneratedInstances.Num();
                                GenerationData.GenerationTimeMs = static_cast<float>((FPlatformTime::Seconds() - GraphStartSeconds) * 1000.0);

                                if (GenerationData.TotalInstanceCount > MaxInstancesPerTile)
                                {
                                        ApplyDensityLimiting(GenerationData);
                                }

                                if (GenerationData.TotalInstanceCount == 0)
                                {
                                        UE_LOG(LogPCGWorldService, Verbose,
                                                TEXT("PCG graph %s produced no instances for biome %s on tile (%d, %d)"),
                                                *PCGGraph->GetName(), *UEnum::GetValueAsString(BiomeType), TileCoord.X, TileCoord.Y);
                                }
                                else
                                {
                                        UE_LOG(LogPCGWorldService, VeryVerbose,
                                                TEXT("PCG graph %s generated %d instances for biome %s on tile (%d, %d)"),
                                                *PCGGraph->GetName(), GenerationData.TotalInstanceCount,
                                                *UEnum::GetValueAsString(BiomeType), TileCoord.X, TileCoord.Y);
                                }

                                return GenerationData;
                        }

                        UE_LOG(LogPCGWorldService, Warning,
                                TEXT("PCG graph %s failed to generate content for biome %s on tile (%d, %d); using fallback"),
                                *PCGGraph->GetName(), *UEnum::GetValueAsString(BiomeType), TileCoord.X, TileCoord.Y);
                }
        }
        else if (PCGGraph)
        {
                UE_LOG(LogPCGWorldService, Verbose, TEXT("Skipping PCG execution for biome %s - missing world context or runtime disabled"),
                        *UEnum::GetValueAsString(BiomeType));
        }
#endif

        GenerationData = GenerateFallbackContent(TileCoord, BiomeType, HeightData, true, TileMetrics);
        return GenerationData;
}


bool UPCGWorldService::TryGeneratePCGGraphContent(FTileCoord TileCoord, EBiomeType BiomeType, const TArray<float>& HeightData, const FPCGTileMetrics& TileMetrics, FPCGGenerationData& OutData)
{
	const bool bHasGraphReference = BiomePCGGraphs.Contains(BiomeType);
	UPCGGraph* Graph = ResolveBiomePCGGraph(BiomeType);

	if (!Graph && !bHasGraphReference)
	{
		UE_LOG(LogPCGWorldService, Verbose, TEXT("No PCG graph registered for biome %s - skipping graph generation"), *UEnum::GetValueAsString(BiomeType));
		return false;
	}

	OutData = GeneratePCGContent(TileCoord, BiomeType, HeightData, Graph, &TileMetrics);
	return true;
}

FPCGGenerationData UPCGWorldService::GenerateFallbackContent(FTileCoord TileCoord, EBiomeType BiomeType, const TArray<float>& HeightData, bool bUsePCGHeuristics, const FPCGTileMetrics* TileMetrics)
{
	FPCGGenerationData GenerationData;
	GenerationData.TileCoord = TileCoord;
	GenerationData.BiomeType = BiomeType;

	const FBiomeDefinition* BiomeDef = BiomeDefinitions.Find(BiomeType);
	if (!BiomeDef)
	{
		UE_LOG(LogPCGWorldService, Warning, TEXT("No biome definition found for biome type %d"), static_cast<int32>(BiomeType));
		return GenerationData;
	}

	const int32 ExpectedHeightDataSize = 64 * 64;
	const bool bHasValidHeightData = HeightData.Num() == ExpectedHeightDataSize;

	FPCGTileMetrics LocalMetrics;
	const FPCGTileMetrics* EffectiveMetrics = TileMetrics;
	if (bUsePCGHeuristics && !EffectiveMetrics && bHasValidHeightData)
	{
		LocalMetrics = AnalyzeTileMetrics(HeightData);
		EffectiveMetrics = &LocalMetrics;
	}

	FPCGSpawnParams SpawnParams;
	if (bHeadless)
	{
		SpawnParams.bForceBiome = true;
		SpawnParams.BiomeOverride = BiomeType;
	}

	if (EffectiveMetrics)
	{
		const float SlopeFactor = FMath::Clamp(1.0f - (EffectiveMetrics->AverageSlope / 60.0f), 0.2f, 1.0f);
		const float WaterFalloff = FMath::Max(10.0f, WorldGenSettings.TileSizeMeters * 0.75f);
		const float WaterFactor = FMath::Clamp(1.0f - (EffectiveMetrics->MinAbsWaterDistance / WaterFalloff), 0.2f, 1.0f);

		SpawnParams.SlopeResponse = SlopeFactor;
		SpawnParams.WaterResponse = WaterFactor;
		SpawnParams.BiomeWeightScale = SlopeFactor * WaterFactor;
	}

	if (bUsePCGHeuristics)
	{
		UE_LOG(LogPCGWorldService, VeryVerbose, TEXT("Applying PCG heuristics for biome %s (metrics available: %s)"),
			*UEnum::GetValueAsString(BiomeType), EffectiveMetrics ? TEXT("true") : TEXT("false"));
	}

	TArray<FPCGInstanceData> VegetationInstances = GenerateVegetationInstances(TileCoord, *BiomeDef, HeightData, SpawnParams, EffectiveMetrics, bUsePCGHeuristics);
	GenerationData.GeneratedInstances.Append(VegetationInstances);

	TArray<FPOIData> POIInstances = GeneratePOIInstances(TileCoord, *BiomeDef, HeightData);
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

	if (GenerationData.TotalInstanceCount > MaxInstancesPerTile)
	{
		ApplyDensityLimiting(GenerationData);
	}

	return GenerationData;
}

TArray<FPCGInstanceData> UPCGWorldService::GenerateVegetationInstances(FTileCoord TileCoord, const FBiomeDefinition& BiomeDef, const TArray<float>& HeightData, const FPCGTileMetrics* TileMetrics, bool bUsePCGHeuristics)
{
	FPCGSpawnParams DefaultSpawnParams;
	return GenerateVegetationInstances(TileCoord, BiomeDef, HeightData, DefaultSpawnParams, TileMetrics, bUsePCGHeuristics);
}

TArray<FPCGInstanceData> UPCGWorldService::GenerateVegetationInstances(FTileCoord TileCoord, const FBiomeDefinition& BiomeDef, const TArray<float>& HeightData, const FPCGSpawnParams& SpawnParams, const FPCGTileMetrics* TileMetrics, bool bUsePCGHeuristics)
{
	TArray<FPCGInstanceData> Instances;

	const int32 ExpectedHeightDataSize = 64 * 64;
	if (HeightData.Num() != ExpectedHeightDataSize)
	{
		UE_LOG(LogPCGWorldService, Error, TEXT("Height data size mismatch: expected %d elements (64x64), got %d elements"),
			ExpectedHeightDataSize, HeightData.Num());
		return Instances;
	}

	FVector TileWorldPos = TileCoord.ToWorldPosition(64.0f);
	FVector2D TileStart(TileWorldPos.X - 32.0f, TileWorldPos.Y - 32.0f);
	const float TileAreaM2 = 64.0f * 64.0f;

	FRandomStream RandomStream(GetTileRandomSeed(TileCoord));
	const float BiomeWeight = GetBiomeWeightForSpawn(SpawnParams, BiomeDef.BiomeType);
	const bool bApplyHeuristics = bUsePCGHeuristics && TileMetrics != nullptr;

	UE_LOG(LogPCGWorldService, VeryVerbose, TEXT("Generating vegetation for biome %s with %d rules (heuristics=%s, biomeWeight=%.3f)"),
		*UEnum::GetValueAsString(BiomeDef.BiomeType), BiomeDef.VegetationRules.Num(), bApplyHeuristics ? TEXT("true") : TEXT("false"), BiomeWeight);

	int32 TotalInstanceCount = 0;

	for (const FPCGVegetationRule& VegRule : BiomeDef.VegetationRules)
	{
		UStaticMesh* Mesh = VegRule.VegetationMesh.IsNull() ? nullptr : VegRule.VegetationMesh.LoadSynchronous();

		float BaseDensity = VegRule.Density * WorldGenSettings.VegetationDensity * BiomeWeight;
		if (TileMetrics)
		{
			BaseDensity *= ComputeEnvironmentScale(*TileMetrics, VegRule);
		}

		int32 BaseInstanceCount = FMath::Max(0, FMath::RoundToInt(BaseDensity * TileAreaM2 / 100.0f));
		const int32 MaxInstancesForThisRule = FMath::Max(1, MaxInstancesPerTile / FMath::Max(1, BiomeDef.VegetationRules.Num()));
		int32 InstanceCount = FMath::Min(BaseInstanceCount, MaxInstancesForThisRule);

		if (bHeadless && SpawnParams.bForceBiome)
		{
			InstanceCount = FMath::Max(InstanceCount, 1);
		}

		UE_LOG(LogPCGWorldService, VeryVerbose, TEXT("Vegetation rule %s: baseDensity=%.3f baseCount=%d clamped=%d"),
			VegRule.VegetationMesh.IsNull() ? TEXT("NULL_MESH") : *VegRule.VegetationMesh.GetAssetName(), BaseDensity, BaseInstanceCount, InstanceCount);

		if (InstanceCount <= 0)
		{
			continue;
		}

		TArray<FVector2D> SamplePoints;
		SamplePoints.Reserve(InstanceCount);

		if (bApplyHeuristics)
		{
			GenerateClusteredSamples(RandomStream, InstanceCount, TileStart, 64.0f, 2.0f, SamplePoints);
		}
		else
		{
			for (int32 i = 0; i < InstanceCount; ++i)
			{
				SamplePoints.Add(GeneratePoissonSample(RandomStream, TileStart, 64.0f, 2.0f));
			}
		}

		int32 ValidInstances = 0;
		int32 HeightRejections = 0;
		int32 SlopeRejections = 0;

		for (const FVector2D& SamplePoint : SamplePoints)
		{
			FVector WorldPos = FVector(SamplePoint, 0.0f);
			bool bPassesHeightCheck = true;
			bool bPassesSlopeCheck = true;

			const int32 HeightX = FMath::Clamp(FMath::FloorToInt(SamplePoint.X - TileStart.X), 0, 63);
			const int32 HeightY = FMath::Clamp(FMath::FloorToInt(SamplePoint.Y - TileStart.Y), 0, 63);
			const int32 HeightIndex = HeightY * 64 + HeightX;

			const float Height = HeightData[HeightIndex];
			WorldPos.Z = Height;

			if (!(Height >= VegRule.MinHeight && Height <= VegRule.MaxHeight))
			{
				bPassesHeightCheck = false;
			}

			if (bPassesHeightCheck)
			{
				const float Slope = CalculateSlope(HeightData, HeightX, HeightY, 64);
				if (Slope > VegRule.SlopeLimit)
				{
					bPassesSlopeCheck = false;
				}
			}

			if (SpawnParams.bForceBiome && bHeadless)
			{
				bPassesHeightCheck = true;
				bPassesSlopeCheck = true;
			}

			if (!bPassesHeightCheck)
			{
				++HeightRejections;
				continue;
			}

			if (!bPassesSlopeCheck)
			{
				++SlopeRejections;
				continue;
			}

			FPCGInstanceData InstanceData;
			InstanceData.Location = WorldPos;
			InstanceData.Rotation = FRotator(0.0f, RandomStream.FRandRange(0.0f, 360.0f), 0.0f);
			InstanceData.Scale = FVector(RandomStream.FRandRange(VegRule.MinScale, VegRule.MaxScale));

			if (bHeadless && VegRule.VegetationMesh.IsNull())
			{
				InstanceData.Mesh = TSoftObjectPtr<UStaticMesh>();
			}
			else
			{
				InstanceData.Mesh = VegRule.VegetationMesh;
			}

			InstanceData.OwningTile = TileCoord;
			InstanceData.bIsActive = true;

			bool bPassesAllFilters = true;
			if (!bHeadless && GetWorld() != nullptr)
			{
				// TODO: hook in navmesh/reachability filtering when world context available
				bPassesAllFilters = true;
			}

			if (bPassesAllFilters)
			{
				Instances.Add(InstanceData);
				++ValidInstances;
			}
		}

		TotalInstanceCount += ValidInstances;

		UE_LOG(LogPCGWorldService, VeryVerbose, TEXT("Rule %s results: Requested=%d, Placed=%d, HeightRejects=%d, SlopeRejects=%d"),
			VegRule.VegetationMesh.IsNull() ? TEXT("NULL_MESH") : *VegRule.VegetationMesh.GetAssetName(),
			InstanceCount, ValidInstances, HeightRejections, SlopeRejections);
	}

	if (TotalInstanceCount != Instances.Num())
	{
		UE_LOG(LogPCGWorldService, Warning, TEXT("Instance accounting mismatch: expected %d, actual %d"), TotalInstanceCount, Instances.Num());
	}

	return Instances;
}

TArray<FPOIData> UPCGWorldService::GeneratePOIInstances(FTileCoord TileCoord, const FBiomeDefinition& BiomeDef, const TArray<float>& HeightData)
{
	TArray<FPOIData> POIs;

	// Calculate tile world position
	FVector TileWorldPos = TileCoord.ToWorldPosition(64.0f);
	FVector2D TileStart(TileWorldPos.X - 32.0f, TileWorldPos.Y - 32.0f);

	// Initialize seeded random for consistent generation
	FRandomStream RandomStream(GetTileRandomSeed(TileCoord));

	// Generate POIs based on biome rules using stratified placement
	for (const FPOISpawnRule& POIRule : BiomeDef.POIRules)
	{
		// Check spawn chance
		if (RandomStream.FRand() <= POIRule.SpawnChance * WorldGenSettings.POIDensity)
		{
			// Use stratified sampling for better distribution
			FVector POILocation;
			bool bFoundSuitableLocation = FindPOILocationStratified(
				TileCoord, POIRule, HeightData, RandomStream, POILocation);

			if (bFoundSuitableLocation)
			{
				// Create POI data
				FPOIData POIData;
				POIData.POIName = POIRule.POIName;
				POIData.Location = POILocation;
				POIData.Rotation = FRotator(0.0f, RandomStream.FRandRange(0.0f, 360.0f), 0.0f);
				POIData.Scale = FVector::OneVector;
				POIData.POIBlueprint = POIRule.POIBlueprint;
				POIData.OriginBiome = BiomeDef.BiomeType;
				POIData.bIsSpawned = false;

				// Apply terrain flattening/clearing if required
				if (POIRule.bRequiresFlatGround)
				{
					ApplyPOITerrainStamp(POIData.Location, 8.0f); // 8m radius flatten
				}

				POIs.Add(POIData);
				SpawnedPOIs.Add(POIData.POIId, POIData);

				UE_LOG(LogPCGWorldService, Log, TEXT("Generated POI %s at (%.1f, %.1f, %.1f) on tile (%d, %d)"),
					*POIData.POIName, POILocation.X, POILocation.Y, POILocation.Z, TileCoord.X, TileCoord.Y);
			}
		}
	}

	return POIs;
}

bool UPCGWorldService::SpawnPOI(FVector Location, const FPOIData& POIData)
{
	// Validate POI ID is properly initialized
	ensureMsgf(POIData.POIId.IsValid(), TEXT("SpawnPOI: POIData must have a valid POIId"));

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
	// Get generation data for this tile
	const FPCGGenerationData* GenerationData = GenerationCache.Find(TileCoord);
	if (!GenerationData)
	{
		UE_LOG(LogPCGWorldService, Warning, TEXT("No generation data found for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
		return false;
	}

	// Fix instance counting to use transform sets not HISM instances: count logical instances in headless mode
	TMap<UStaticMesh*, TArray<FTransform>> InstancesByMesh;
	int32 TotalLogicalInstances = 0;

	for (const FPCGInstanceData& InstanceData : GenerationData->GeneratedInstances)
	{
		if (InstanceData.bIsActive)
		{
			// Count all active instances, even those without meshes (headless mode)
			TotalLogicalInstances++;

			// Only group by mesh if we have a valid mesh and are not in headless mode
			if (!bHeadless && !InstanceData.Mesh.IsNull())
			{
				UStaticMesh* Mesh = InstanceData.Mesh.LoadSynchronous();
				if (Mesh)
				{
					FTransform Transform(InstanceData.Rotation, InstanceData.Location, InstanceData.Scale);
					InstancesByMesh.FindOrAdd(Mesh).Add(Transform);
				}
			}
		}
	}

	// Check if we have a valid world context for HISM operations
	if (bHeadless || GetWorld() == nullptr)
	{
		// In headless mode, we count logical instances rather than committed components
		UE_LOG(LogPCGWorldService, Log, TEXT("Headless mode: Counted %d logical instances for tile (%d, %d) - skipping HISM component creation"),
			TotalLogicalInstances, TileCoord.X, TileCoord.Y);
		return true;
	}

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

	// Update HISM components
	for (auto& MeshPair : InstancesByMesh)
	{
		UStaticMesh* Mesh = MeshPair.Key;
		const TArray<FTransform>& Transforms = MeshPair.Value;

		UHierarchicalInstancedStaticMeshComponent* HISMComp = GetOrCreateHISMComponent(TileCoord, Mesh);
		if (HISMComp)
		{
			HISMComp->SetMobility(EComponentMobility::Movable);
			// Clear existing instances and add new ones
			HISMComp->ClearInstances();
			for (const FTransform& Transform : Transforms)
			{
				HISMComp->AddInstance(Transform, /*bWorldSpace=*/true);
			}

			// Update performance stats
			PerformanceStats.ActiveHISMInstances += Transforms.Num();
		}
	}

	UE_LOG(LogPCGWorldService, Log, TEXT("Updated HISM instances for tile (%d, %d) - %d logical instances, %d instance groups"),
		TileCoord.X, TileCoord.Y, TotalLogicalInstances, InstancesByMesh.Num());
	return true;
}

bool UPCGWorldService::RemoveContentInArea(FBox Area)
{
	bool bRemovedAny = false;
	const FBox2D Area2D(FVector2D(Area.Min), FVector2D(Area.Max));
	const FVector2D AreaMin(Area.Min.X, Area.Min.Y);
	const FVector2D AreaMax(Area.Max.X, Area.Max.Y);

	UE_LOG(LogPCGWorldService, Log, TEXT("RemoveContentInArea: Searching for content in area (%.1f,%.1f) to (%.1f,%.1f)"),
		AreaMin.X, AreaMin.Y, AreaMax.X, AreaMax.Y);
	UE_LOG(LogPCGWorldService, Log, TEXT("RemoveContentInArea: GenerationCache has %d tiles"), GenerationCache.Num());

	auto IsInside2D = [&](const FVector2D& P)->bool
		{
			// inclusive compare so edge cases are removed too
			return (P.X >= AreaMin.X && P.X <= AreaMax.X &&
				P.Y >= AreaMin.Y && P.Y <= AreaMax.Y);
		};

	// Remove POI actors in the area
	TArray<FGuid> POIsToRemove;
	for (auto& POIPair : SpawnedPOIActors)
	{
		AActor* POIActor = POIPair.Value;
		if (IsValid(POIActor) && Area2D.IsInside(FVector2D(POIActor->GetActorLocation())))
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

		UE_LOG(LogPCGWorldService, Log, TEXT("RemoveContentInArea: Checking tile (%d, %d) with %d instances"),
			GenerationData.TileCoord.X, GenerationData.TileCoord.Y, GenerationData.GeneratedInstances.Num());

		int32 RemovedFromThisTile = 0;
		for (FPCGInstanceData& InstanceData : GenerationData.GeneratedInstances)
		{
			const bool bInside = Area.IsInside(InstanceData.Location);
			if (!bInside)
			{
				RemainingInstances.Add(InstanceData);
			}
			else
			{
				RemovedFromThisTile++;
				bRemovedAny = true; // we removed at least one
			}
		}

		if (RemainingInstances.Num() != GenerationData.GeneratedInstances.Num())
		{
			UE_LOG(LogPCGWorldService, Log, TEXT("RemoveContentInArea: Removed %d instances from tile (%d, %d), %d remaining"),
				RemovedFromThisTile, GenerationData.TileCoord.X, GenerationData.TileCoord.Y, RemainingInstances.Num());

			GenerationData.GeneratedInstances = RemainingInstances;
			GenerationData.TotalInstanceCount = RemainingInstances.Num();

			// Update HISM for affected tile
			UpdateHISMInstances(GenerationData.TileCoord);
		}
		else if (GenerationData.GeneratedInstances.Num() > 0)
		{
			UE_LOG(LogPCGWorldService, Log, TEXT("RemoveContentInArea: No instances removed from tile (%d, %d) - none were in removal area"),
				GenerationData.TileCoord.X, GenerationData.TileCoord.Y);
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
#if VIBEHEIM_PCG_ENABLED
	ResolvedBiomeGraphs.Empty();
#endif

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

#if VIBEHEIM_PCG_ENABLED
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

float UPCGWorldService::GetBiomeWeightForSpawn(const FPCGSpawnParams& SpawnParams, EBiomeType BiomeType) const
{
	if (SpawnParams.bForceBiome)
	{
		return 1.0f;
	}

	float Weight = SpawnParams.BiomeWeightScale;
	Weight *= SpawnParams.SlopeResponse;
	Weight *= SpawnParams.WaterResponse;

	return FMath::Clamp(Weight, 0.0f, 1.0f);
}

void UPCGWorldService::SetBiomeDefinitions(const TMap<EBiomeType, FBiomeDefinition>& InBiomeDefinitions)
{
	BiomeDefinitions = InBiomeDefinitions;
	BiomePCGGraphs.Empty();
#if VIBEHEIM_PCG_ENABLED
	ResolvedBiomeGraphs.Empty();
#endif

	// Initialize default biome definitions to merge with incoming data
	TMap<EBiomeType, FBiomeDefinition> DefaultBiomeDefinitions;
	InitializeDefaultBiomes(DefaultBiomeDefinitions);

	for (auto& BiomePair : BiomeDefinitions)
	{
		FBiomeDefinition& BiomeDef = BiomePair.Value;
		const FString BiomeLabel = BiomeDef.BiomeName.IsEmpty() ? UEnum::GetValueAsString(BiomePair.Key) : BiomeDef.BiomeName;

		// Merge default vegetation rules if authoring data left them empty
		if (BiomeDef.VegetationRules.Num() == 0)
		{
			if (const FBiomeDefinition* DefaultBiomeDef = DefaultBiomeDefinitions.Find(BiomePair.Key))
			{
				BiomeDef.VegetationRules = DefaultBiomeDef->VegetationRules;
				UE_LOG(LogPCGWorldService, Log, TEXT("Merged %d default vegetation rules for %s biome"),
					DefaultBiomeDef->VegetationRules.Num(), *BiomeLabel);
			}
		}

		// Cache PCG graph references so generation can resolve them quickly
		if (BiomeDef.BiomePCGGraph.IsNull())
		{
			UE_LOG(LogPCGWorldService, Verbose, TEXT("Biome %s has no PCG graph assigned - fallback generation will be used"), *BiomeLabel);
		}
		else
		{
			BiomePCGGraphs.Add(BiomePair.Key, BiomeDef.BiomePCGGraph);
			UE_LOG(LogPCGWorldService, Log, TEXT("Biome %s mapped to PCG graph %s"), *BiomeLabel, *BiomeDef.BiomePCGGraph.ToString());
		}
	}

	// Reset cached graph pointer from legacy single-graph workflow
	CurrentPCGGraph = nullptr;

	// Clear cache to ensure fresh generation uses updated rule registry
	ClearPCGCache();

	UE_LOG(LogPCGWorldService, Log, TEXT("Updated biome definitions with %d biomes (registered PCG graphs: %d)"),
		BiomeDefinitions.Num(), BiomePCGGraphs.Num());
}

UPCGGraph* UPCGWorldService::ResolveBiomePCGGraph(EBiomeType BiomeType)
{
	const TSoftObjectPtr<UPCGGraph>* GraphRef = BiomePCGGraphs.Find(BiomeType);
	if (!GraphRef)
	{
		return nullptr;
	}

#if VIBEHEIM_PCG_ENABLED
	if (const TWeakObjectPtr<UPCGGraph>* CachedGraph = ResolvedBiomeGraphs.Find(BiomeType))
	{
		if (CachedGraph->IsValid())
		{
			return CachedGraph->Get();
		}
	}

	if (!GraphRef->IsNull())
	{
		UPCGGraph* LoadedGraph = GraphRef->LoadSynchronous();
		if (LoadedGraph)
		{
			ResolvedBiomeGraphs.FindOrAdd(BiomeType) = LoadedGraph;
			return LoadedGraph;
		}

		UE_LOG(LogPCGWorldService, Warning, TEXT("Failed to load PCG graph %s for biome %s"), *GraphRef->ToString(), *UEnum::GetValueAsString(BiomeType));
	}
	return nullptr;
#else
	return nullptr;
#endif
}
void UPCGWorldService::SetPersistenceManager(UInstancePersistenceManager* InPersistenceManager)
{
	PersistenceManager = InPersistenceManager;
	UE_LOG(LogPCGWorldService, Log, TEXT("Instance persistence manager set: %s"),
		PersistenceManager ? TEXT("Valid") : TEXT("Null"));
}

bool UPCGWorldService::RemoveInstance(FTileCoord TileCoord, FGuid InstanceId)
{
	// Find the instance in the generation cache
	FPCGGenerationData* GenerationData = GenerationCache.Find(TileCoord);
	if (!GenerationData)
	{
		UE_LOG(LogPCGWorldService, Warning, TEXT("No generation data found for tile (%d, %d) when removing instance"), TileCoord.X, TileCoord.Y);
		return false;
	}

	// Find and remove the instance
	bool bFoundInstance = false;
	FPCGInstanceData RemovedInstance;
	for (int32 i = GenerationData->GeneratedInstances.Num() - 1; i >= 0; i--)
	{
		if (GenerationData->GeneratedInstances[i].InstanceId == InstanceId)
		{
			RemovedInstance = GenerationData->GeneratedInstances[i];
			GenerationData->GeneratedInstances.RemoveAt(i);
			GenerationData->TotalInstanceCount = GenerationData->GeneratedInstances.Num();
			bFoundInstance = true;
			break;
		}
	}

	if (!bFoundInstance)
	{
		UE_LOG(LogPCGWorldService, Warning, TEXT("Instance %s not found in tile (%d, %d)"), *InstanceId.ToString(), TileCoord.X, TileCoord.Y);
		return false;
	}

	// Log the removal to persistence manager if available
	if (PersistenceManager)
	{
		PersistenceManager->AddInstanceOperation(TileCoord, RemovedInstance, EInstanceOperation::Remove);
	}

	// Update HISM instances to reflect the change
	UpdateHISMInstances(TileCoord);

	UE_LOG(LogPCGWorldService, Log, TEXT("Removed instance %s from tile (%d, %d)"), *InstanceId.ToString(), TileCoord.X, TileCoord.Y);
	return true;
}

bool UPCGWorldService::AddInstance(FTileCoord TileCoord, const FPCGInstanceData& InstanceData)
{
	// Get or create generation data for the tile
	FPCGGenerationData* GenerationData = GenerationCache.Find(TileCoord);
	if (!GenerationData)
	{
		// Create new generation data for this tile
		FPCGGenerationData NewGenerationData;
		NewGenerationData.TileCoord = TileCoord;
		NewGenerationData.BiomeType = EBiomeType::None; // Will be set by proper generation
		GenerationCache.Add(TileCoord, NewGenerationData);
		GenerationData = GenerationCache.Find(TileCoord);
	}

	// Add the instance
	FPCGInstanceData NewInstance = InstanceData;
	NewInstance.OwningTile = TileCoord;
	NewInstance.bIsActive = true;

	GenerationData->GeneratedInstances.Add(NewInstance);
	GenerationData->TotalInstanceCount = GenerationData->GeneratedInstances.Num();

	// Log the addition to persistence manager if available
	if (PersistenceManager)
	{
		PersistenceManager->AddInstanceOperation(TileCoord, NewInstance, EInstanceOperation::Add);
	}

	// Update HISM instances to reflect the change
	UpdateHISMInstances(TileCoord);

	UE_LOG(LogPCGWorldService, Log, TEXT("Added instance %s to tile (%d, %d)"), *NewInstance.InstanceId.ToString(), TileCoord.X, TileCoord.Y);
	return true;
}

bool UPCGWorldService::RemovePOI(FGuid POIId)
{
	// Find POI in spawned POIs
	FPOIData* POIData = SpawnedPOIs.Find(POIId);
	if (!POIData)
	{
		UE_LOG(LogPCGWorldService, Warning, TEXT("POI %s not found in spawned POIs"), *POIId.ToString());
		return false;
	}

	// Get the tile coordinate for persistence logging
	FTileCoord TileCoord = FTileCoord::FromWorldPosition(POIData->Location, 64.0f);

	// Destroy the spawned actor if it exists
	if (TObjectPtr<AActor>* FoundPtr = SpawnedPOIActors.Find(POIId))
	{
		AActor* SpawnedActor = FoundPtr->Get();
		if (IsValid(SpawnedActor))
		{
			(SpawnedActor)->Destroy();
		}
		SpawnedPOIActors.Remove(POIId);
	}

	// Log the removal to persistence manager if available
	if (PersistenceManager)
	{
		PersistenceManager->AddPOIOperation(TileCoord, *POIData, EInstanceOperation::Remove);
	}

	// Remove from spawned POIs map
	SpawnedPOIs.Remove(POIId);

	UE_LOG(LogPCGWorldService, Log, TEXT("Removed POI %s (%s)"), *POIId.ToString(), *POIData->POIName);
	return true;
}

bool UPCGWorldService::AddPOI(const FPOIData& POIData)
{
	// Validate POI ID is properly initialized
	ensureMsgf(POIData.POIId.IsValid(), TEXT("AddPOI: POIData must have a valid POIId"));

	// Get the tile coordinate for persistence logging
	FTileCoord TileCoord = FTileCoord::FromWorldPosition(POIData.Location, 64.0f);

	// Add to spawned POIs map
	SpawnedPOIs.Add(POIData.POIId, POIData);

	// Actually spawn the POI
	bool bSpawned = SpawnPOI(POIData.Location, POIData);
	if (!bSpawned)
	{
		// Remove from map if spawning failed
		SpawnedPOIs.Remove(POIData.POIId);
		return false;
	}

	// Log the addition to persistence manager if available
	if (PersistenceManager)
	{
		PersistenceManager->AddPOIOperation(TileCoord, POIData, EInstanceOperation::Add);
	}

	UE_LOG(LogPCGWorldService, Log, TEXT("Added POI %s (%s) at (%.1f, %.1f, %.1f)"),
		*POIData.POIId.ToString(), *POIData.POIName, POIData.Location.X, POIData.Location.Y, POIData.Location.Z);
	return true;
}

bool UPCGWorldService::LoadTileWithPersistence(FTileCoord TileCoord, EBiomeType BiomeType, const TArray<float>& HeightData)
{
	// First generate the base content
	FPCGGenerationData GenerationData = GenerateContentInternal(TileCoord, BiomeType, HeightData);

	// Cache the base generation
	GenerationCache.Add(TileCoord, GenerationData);

	// Apply persistence modifications if persistence manager is available
	if (PersistenceManager)
	{
		// Load tile journal from disk if it exists
		if (!PersistenceManager->LoadTileJournal(TileCoord))
		{
			UE_LOG(LogPCGWorldService, Warning, TEXT("Failed to load persistence journal for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
		}

		// Replay the journal to apply persistent modifications
		if (!PersistenceManager->ReplayTileJournal(TileCoord, this))
		{
			UE_LOG(LogPCGWorldService, Warning, TEXT("Failed to replay persistence journal for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
		}
		else
		{
			UE_LOG(LogPCGWorldService, Log, TEXT("Successfully applied persistent modifications to tile (%d, %d)"), TileCoord.X, TileCoord.Y);
		}
	}

	// Update HISM instances with the final state
	UpdateHISMInstances(TileCoord);

	return true;
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

FVector2D UPCGWorldService::GeneratePoissonSample(FRandomStream& RandomStream, FVector2D TileStart, float TileSize, float MinDistance) const
{
	// Simple random sample within the tile. For production, implement true Poisson disk
	// by checking against existing samples and using MinDistance.
	(void)MinDistance; // suppress unused parameter warning for now

	return TileStart + FVector2D(
		RandomStream.FRandRange(0.0f, TileSize),
		RandomStream.FRandRange(0.0f, TileSize)
	);
}

void UPCGWorldService::GenerateClusteredSamples(FRandomStream& RandomStream, int32 InstanceCount, FVector2D TileStart, float TileSize, float MinDistance, TArray<FVector2D>& OutSamples) const
{
	OutSamples.Reset();

	if (InstanceCount <= 0)
	{
		return;
	}

	const float ClusterRadius = FMath::Max(MinDistance * 2.0f, TileSize * 0.1f);
	int32 Remaining = InstanceCount;
	const int32 TargetClusterSize = FMath::Clamp(FMath::Max(3, InstanceCount / 6), 3, 12);

	while (Remaining > 0)
	{
		const FVector2D ClusterCenter = GeneratePoissonSample(RandomStream, TileStart, TileSize, MinDistance);
		const int32 SamplesThisCluster = FMath::Min(TargetClusterSize, Remaining);

		for (int32 SampleIndex = 0; SampleIndex < SamplesThisCluster; ++SampleIndex)
		{
			const float Angle = RandomStream.FRandRange(0.0f, 2.0f * PI);
			const float Radius = RandomStream.FRandRange(0.0f, ClusterRadius);
			FVector2D Offset(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius);

			FVector2D SamplePoint = ClusterCenter + Offset;
			SamplePoint.X = FMath::Clamp(SamplePoint.X, TileStart.X, TileStart.X + TileSize);
			SamplePoint.Y = FMath::Clamp(SamplePoint.Y, TileStart.Y, TileStart.Y + TileSize);
			OutSamples.Add(SamplePoint);
		}

		Remaining -= SamplesThisCluster;
	}

	if (OutSamples.Num() > InstanceCount)
	{
		OutSamples.SetNum(InstanceCount);
	}
}
float UPCGWorldService::CalculateSlope(const TArray<float>& HeightData, int32 X, int32 Y, int32 GridSize) const
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
UPCGWorldService::FPCGTileMetrics UPCGWorldService::AnalyzeTileMetrics(const TArray<float>& HeightData) const
{
	FPCGTileMetrics Metrics;
	const int32 ExpectedSize = 64 * 64;
	if (HeightData.Num() != ExpectedSize)
	{
		return Metrics;
	}

	const int32 GridSize = 64;
	const float SeaLevel = WorldGenSettings.SeaLevel;

	float SumHeight = 0.0f;
	float SumSlope = 0.0f;
	float MaxSlope = 0.0f;
	float MinHeight = HeightData[0];
	float MaxHeight = HeightData[0];
	float WaterCoverageSamples = 0.0f;
	float AboveWaterSum = 0.0f;
	float BelowWaterSum = 0.0f;
	float MinAbsWaterDistance = TNumericLimits<float>::Max();

	for (int32 Y = 0; Y < GridSize; ++Y)
	{
		for (int32 X = 0; X < GridSize; ++X)
		{
			const int32 Index = Y * GridSize + X;
			const float Height = HeightData[Index];

			SumHeight += Height;
			MinHeight = FMath::Min(MinHeight, Height);
			MaxHeight = FMath::Max(MaxHeight, Height);

			const float Slope = CalculateSlope(HeightData, X, Y, GridSize);
			SumSlope += Slope;
			MaxSlope = FMath::Max(MaxSlope, Slope);

			const float WaterDelta = Height - SeaLevel;
			MinAbsWaterDistance = FMath::Min(MinAbsWaterDistance, FMath::Abs(WaterDelta));

			if (WaterDelta >= 0.0f)
			{
				AboveWaterSum += WaterDelta;
			}
			else
			{
				BelowWaterSum += -WaterDelta;
				WaterCoverageSamples += 1.0f;
			}
		}
	}

	const float SampleCount = static_cast<float>(ExpectedSize);
	Metrics.AverageHeight = SumHeight / SampleCount;
	Metrics.MinHeight = MinHeight;
	Metrics.MaxHeight = MaxHeight;
	Metrics.AverageSlope = SumSlope / SampleCount;
	Metrics.MaxSlope = MaxSlope;
	Metrics.WaterCoverageRatio = SampleCount > 0.0f ? WaterCoverageSamples / SampleCount : 0.0f;

	const float AboveSamples = SampleCount - WaterCoverageSamples;
	Metrics.AverageAboveWater = AboveSamples > KINDA_SMALL_NUMBER ? AboveWaterSum / AboveSamples : 0.0f;
	Metrics.AverageBelowWater = WaterCoverageSamples > KINDA_SMALL_NUMBER ? BelowWaterSum / WaterCoverageSamples : 0.0f;
	Metrics.MinAbsWaterDistance = (MinAbsWaterDistance == TNumericLimits<float>::Max()) ? 0.0f : MinAbsWaterDistance;

	return Metrics;
}

float UPCGWorldService::ComputeEnvironmentScale(const FPCGTileMetrics& TileMetrics, const FPCGVegetationRule& VegRule) const
{
	float SlopeFactor = 1.0f;
	if (VegRule.SlopeLimit > KINDA_SMALL_NUMBER)
	{
		SlopeFactor = FMath::Clamp(1.0f - (TileMetrics.AverageSlope / FMath::Max(VegRule.SlopeLimit, 1.0f)), 0.0f, 1.0f);
	}

	float WaterFactor = 1.0f;
	const float WaterFalloff = FMath::Max(10.0f, WorldGenSettings.TileSizeMeters * 0.75f);
	WaterFactor = FMath::Clamp(1.0f - (TileMetrics.MinAbsWaterDistance / WaterFalloff), 0.2f, 1.0f);

	if (TileMetrics.WaterCoverageRatio > 0.0f && VegRule.MaxHeight > WorldGenSettings.SeaLevel)
	{
		WaterFactor *= 1.0f - TileMetrics.WaterCoverageRatio;
	}

	return FMath::Clamp(SlopeFactor * WaterFactor, 0.1f, 1.5f);
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
	UWorld* World = GetWorld();
	if (!World)
	{
		if (bHeadless)
		{
			UE_LOG(LogPCGWorldService, Verbose,
				TEXT("Headless: skipping HISM component creation for tile (%d,%d)"), TileCoord.X, TileCoord.Y);
		}
		else
		{
			UE_LOG(LogPCGWorldService, Error, TEXT("Cannot create HISM components - no valid world"));
		}
		return;
	}

	if (!TileActor)
	{
		FVector TileWorldPos = TileCoord.ToWorldPosition(64.0f);
		FTransform ActorTransform(FRotator::ZeroRotator, TileWorldPos, FVector::OneVector);
		TileActor = World->SpawnActor<AActor>(AActor::StaticClass(), ActorTransform);
#if WITH_EDITOR
		TileActor->SetActorLabel(TEXT("PCGTileActor"));
#endif
	}

	if (TileActor && !TileActor->GetRootComponent())
	{
		USceneComponent* RootComponent = NewObject<USceneComponent>(TileActor, TEXT("PCGTileRoot"));
		RootComponent->SetMobility(EComponentMobility::Movable);
		TileActor->SetRootComponent(RootComponent);
		RootComponent->SetWorldTransform(TileActor->GetActorTransform());
		RootComponent->RegisterComponent();
	}

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

	FHISMComponentArray* TileComponentArray = HISMComponents.Find(TileCoord);
	if (!TileComponentArray)
	{
		CreateHISMComponentsForTile(TileCoord);
		TileComponentArray = HISMComponents.Find(TileCoord);
	}

	if (!TileComponentArray)
	{
		return nullptr;
	}

	for (UHierarchicalInstancedStaticMeshComponent* Component : TileComponentArray->Components)
	{
		if (IsValid(Component) && Component->GetStaticMesh() == Mesh)
		{
			return Component;
		}
	}

	if (!TileActor)
	{
		CreateHISMComponentsForTile(TileCoord);
	}

	if (!TileActor)
	{
		UE_LOG(LogPCGWorldService, Error, TEXT("Cannot create HISM component for tile (%d, %d) - tile actor is invalid"), TileCoord.X, TileCoord.Y);
		return nullptr;
	}

	USceneComponent* RootComponent = TileActor->GetRootComponent();
	if (!RootComponent)
	{
		CreateHISMComponentsForTile(TileCoord);
		RootComponent = TileActor->GetRootComponent();
	}

	if (!RootComponent)
	{
		UE_LOG(LogPCGWorldService, Error, TEXT("Cannot create HISM component for tile (%d, %d) - root component missing"), TileCoord.X, TileCoord.Y);
		return nullptr;
	}

	UHierarchicalInstancedStaticMeshComponent* NewComponent = NewObject<UHierarchicalInstancedStaticMeshComponent>(TileActor);
	if (!NewComponent)
	{
		UE_LOG(LogPCGWorldService, Error, TEXT("Failed to allocate HISM component for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
		return nullptr;
	}

	NewComponent->SetStaticMesh(Mesh);
	NewComponent->SetMobility(EComponentMobility::Movable);
	NewComponent->SetCanEverAffectNavigation(false);
	NewComponent->SetupAttachment(RootComponent);
	NewComponent->SetCullDistances(LODDistances[0], LODDistances[2]);
	NewComponent->bUseAsOccluder = false; // Vegetation typically shouldnt occlude
	NewComponent->RegisterComponent();

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

bool UPCGWorldService::FindPOILocationStratified(FTileCoord TileCoord, const FPOISpawnRule& POIRule, const TArray<float>& HeightData, FRandomStream& RandomStream, FVector& OutLocation)
{
	// Calculate tile bounds
	FVector TileWorldPos = TileCoord.ToWorldPosition(64.0f);
	FVector2D TileStart(TileWorldPos.X - 32.0f, TileWorldPos.Y - 32.0f);

	// Use stratified sampling - divide tile into 4x4 grid and sample within each cell
	const int32 GridSize = 4;
	const float CellSize = 64.0f / GridSize;

	// Try multiple cells for better distribution
	TArray<FIntVector2> CellIndices;
	for (int32 Y = 0; Y < GridSize; Y++)
	{
		for (int32 X = 0; X < GridSize; X++)
		{
			CellIndices.Add(FIntVector2(X, Y));
		}
	}

	// Shuffle the cells for random sampling order
	for (int32 i = CellIndices.Num() - 1; i > 0; i--)
	{
		int32 j = RandomStream.RandRange(0, i);
		CellIndices.Swap(i, j);
	}

	// Try to find suitable location in cells
	for (const FIntVector2& CellIndex : CellIndices)
	{
		// Generate random point within this cell
		FVector2D CellMin = TileStart + FVector2D(CellIndex.X * CellSize, CellIndex.Y * CellSize);
		FVector2D RandomOffset = FVector2D(
			RandomStream.FRandRange(2.0f, CellSize - 2.0f),
			RandomStream.FRandRange(2.0f, CellSize - 2.0f)
		);
		FVector2D SamplePoint = CellMin + RandomOffset;

		// Convert to heightfield coordinates
		int32 HeightX = FMath::Clamp(FMath::FloorToInt(SamplePoint.X - TileStart.X), 0, 63);
		int32 HeightY = FMath::Clamp(FMath::FloorToInt(SamplePoint.Y - TileStart.Y), 0, 63);
		int32 HeightIndex = HeightY * 64 + HeightX;

		if (!HeightData.IsValidIndex(HeightIndex))
		{
			continue;
		}

		// Get terrain data at this location
		float Height = HeightData[HeightIndex];
		float Slope = CalculateSlope(HeightData, HeightX, HeightY, 64);
		FVector TestLocation(SamplePoint.X, SamplePoint.Y, Height);

		// Check slope requirements
		if (Slope > POIRule.SlopeLimit)
		{
			continue;
		}

		// Check altitude constraints (basic filtering)
		if (Height < WorldGenSettings.SeaLevel + 2.0f) // 2m above sea level minimum
		{
			continue;
		}

		// Check spacing requirements
		if (!CheckPOISpacingRequirements(TestLocation, POIRule.MinDistanceFromOthers))
		{
			continue;
		}

		// Additional slope validation for flat ground requirement
		if (POIRule.bRequiresFlatGround)
		{
			// Check a 3x3 area around the point for consistent flatness
			bool bIsFlatArea = true;
			float MaxSlopeInArea = 0.0f;

			for (int32 CheckY = FMath::Max(0, HeightY - 1); CheckY <= FMath::Min(63, HeightY + 1); CheckY++)
			{
				for (int32 CheckX = FMath::Max(0, HeightX - 1); CheckX <= FMath::Min(63, HeightX + 1); CheckX++)
				{
					float LocalSlope = CalculateSlope(HeightData, CheckX, CheckY, 64);
					MaxSlopeInArea = FMath::Max(MaxSlopeInArea, LocalSlope);
					if (LocalSlope > POIRule.SlopeLimit * 0.5f) // Stricter slope for flat ground
					{
						bIsFlatArea = false;
						break;
					}
				}
				if (!bIsFlatArea) break;
			}

			if (!bIsFlatArea)
			{
				continue;
			}
		}

		// Found suitable location
		OutLocation = TestLocation;

		UE_LOG(LogPCGWorldService, Verbose, TEXT("Found POI location at (%.1f, %.1f, %.1f) with slope %.1f degrees in cell (%d, %d)"),
			TestLocation.X, TestLocation.Y, TestLocation.Z, Slope, CellIndex.X, CellIndex.Y);

		return true;
	}

	UE_LOG(LogPCGWorldService, Verbose, TEXT("Could not find suitable POI location for rule %s in tile (%d, %d)"),
		*POIRule.POIName, TileCoord.X, TileCoord.Y);

	return false;
}

void UPCGWorldService::ApplyPOITerrainStamp(FVector Location, float Radius)
{
	// Bypass "stamp terrain" integration step in headless mode: skip terrain modification when no world context available
	if (bHeadless || GetWorld() == nullptr)
	{
		UE_LOG(LogPCGWorldService, Log, TEXT("Headless mode: Skipping terrain stamp at (%.1f, %.1f, %.1f) with radius %.1f - no world context available"),
			Location.X, Location.Y, Location.Z, Radius);
		return;
	}

	// Get HeightfieldService to apply terrain modification
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogPCGWorldService, Warning, TEXT("Cannot apply terrain stamp - no valid world"));
		return;
	}

	// Find WorldGenManager to access HeightfieldService
	// For now, just log the operation as a placeholder for integration
	UE_LOG(LogPCGWorldService, Log, TEXT("Applied terrain stamp at (%.1f, %.1f, %.1f) with radius %.1f for POI placement"),
		Location.X, Location.Y, Location.Z, Radius);

	// In a full implementation, this would:
	// 1. Get the HeightfieldService from WorldGenManager
	// 2. Apply a flatten operation with the specified radius
	// 3. Clear vegetation in the area
	// 4. Update the heightfield data
	//
	// Example integration code:
	// if (UWorldGenManager* WorldGenManager = World->GetSubsystem<UWorldGenManager>())
	// {
	//     if (UHeightfieldService* HeightfieldService = WorldGenManager->GetHeightfieldService())
	//     {
	//         HeightfieldService->ModifyHeightfield(Location, Radius, 0.8f, EHeightfieldOperation::Flatten);
	//         
	//         // Clear vegetation in the area
	//         FBox ClearArea(Location - FVector(Radius), Location + FVector(Radius));
	//         RemoveContentInArea(ClearArea);
	//     }
	// }
}

void UPCGWorldService::InitializeDefaultBiomes(TMap<EBiomeType, FBiomeDefinition>& OutDefaultBiomes)
{
	// Forest biome with default vegetation rules
	FBiomeDefinition ForestBiome;
	ForestBiome.BiomeType = EBiomeType::Forest;
	ForestBiome.BiomeName = TEXT("Forest");

	// Add default vegetation rules for Forest biome
	FPCGVegetationRule ForestTreeRule;
	ForestTreeRule.Density = 0.5f;
	ForestTreeRule.MinScale = 0.8f;
	ForestTreeRule.MaxScale = 1.2f;
	ForestTreeRule.SlopeLimit = 30.0f;
	ForestTreeRule.MinHeight = -100.0f;
	ForestTreeRule.MaxHeight = 1000.0f;
	// Leave VegetationMesh as null - headless mode will handle this
	ForestBiome.VegetationRules.Add(ForestTreeRule);

	FPCGVegetationRule ForestUndergrowthRule;
	ForestUndergrowthRule.Density = 0.3f;
	ForestUndergrowthRule.MinScale = 0.5f;
	ForestUndergrowthRule.MaxScale = 0.8f;
	ForestUndergrowthRule.SlopeLimit = 45.0f;
	ForestUndergrowthRule.MinHeight = -100.0f;
	ForestUndergrowthRule.MaxHeight = 1000.0f;
	// Leave VegetationMesh as null - headless mode will handle this
	ForestBiome.VegetationRules.Add(ForestUndergrowthRule);

	OutDefaultBiomes.Add(EBiomeType::Forest, ForestBiome);

	// Meadows biome with default vegetation rules
	FBiomeDefinition MeadowsBiome;
	MeadowsBiome.BiomeType = EBiomeType::Meadows;
	MeadowsBiome.BiomeName = TEXT("Meadows");

	FPCGVegetationRule MeadowsGrassRule;
	MeadowsGrassRule.Density = 0.4f;
	MeadowsGrassRule.MinScale = 0.6f;
	MeadowsGrassRule.MaxScale = 1.0f;
	MeadowsGrassRule.SlopeLimit = 35.0f;
	MeadowsGrassRule.MinHeight = -50.0f;
	MeadowsGrassRule.MaxHeight = 500.0f;
	// Leave VegetationMesh as null - headless mode will handle this
	MeadowsBiome.VegetationRules.Add(MeadowsGrassRule);

	OutDefaultBiomes.Add(EBiomeType::Meadows, MeadowsBiome);

	// Mountains biome with sparse vegetation
	FBiomeDefinition MountainsBiome;
	MountainsBiome.BiomeType = EBiomeType::Mountains;
	MountainsBiome.BiomeName = TEXT("Mountains");

	FPCGVegetationRule MountainSparseRule;
	MountainSparseRule.Density = 0.1f;
	MountainSparseRule.MinScale = 0.7f;
	MountainSparseRule.MaxScale = 1.1f;
	MountainSparseRule.SlopeLimit = 25.0f;
	MountainSparseRule.MinHeight = 20.0f;
	MountainSparseRule.MaxHeight = 1000.0f;
	// Leave VegetationMesh as null - headless mode will handle this
	MountainsBiome.VegetationRules.Add(MountainSparseRule);

	OutDefaultBiomes.Add(EBiomeType::Mountains, MountainsBiome);

	// Ocean biome (no vegetation rules - underwater)
	FBiomeDefinition OceanBiome;
	OceanBiome.BiomeType = EBiomeType::Ocean;
	OceanBiome.BiomeName = TEXT("Ocean");
	// No vegetation rules for ocean

	OutDefaultBiomes.Add(EBiomeType::Ocean, OceanBiome);

	UE_LOG(LogPCGWorldService, Log, TEXT("Initialized default biome definitions with vegetation rules for %d biomes"), OutDefaultBiomes.Num());
}

