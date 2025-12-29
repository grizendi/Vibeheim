#include "Editor/WorldGenBuildUtility.h"

#include "Data/WorldGenAssets.h"
#include "Data/WorldGenBuildState.h"
#include "Data/WorldGenTerrainResource.h"
#include "Data/WorldGenTypes.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Framework/Notifications/NotificationManager.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "PCGWorldActor.h"
#include "Services/BiomeService.h"
#include "Services/ClimateSystem.h"
#include "Services/HeightfieldService.h"
#include "Services/PCGWorldService.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "VHMTerrainRendering/VHMTypes.h"
#include "WorldGenExternalDataProvider.h"
#include "WorldGenSettings.h"
#include <cfloat>

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Widgets/Notifications/SNotificationList.h"
#endif

#include "PCGVersionGuard.h"
#if VHM_PCG_ENABLED
#include "PCGSubsystem.h"
#endif

#if WITH_EDITOR && __has_include("WorldPartition/DataLayer/DataLayerManager.h")
#include "WorldPartition/DataLayer/DataLayerManager.h"
#include "DataLayer/DataLayerEditorSubsystem.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"
#define VHM_HAS_DATA_LAYERS 1
#else
#define VHM_HAS_DATA_LAYERS 0
#endif

#if WITH_EDITOR && __has_include("WorldPartition/HLOD/HLODLayer.h")
#include "WorldPartition/HLOD/HLODLayer.h"
#define VHM_HAS_HLOD_LAYER 1
#else
#define VHM_HAS_HLOD_LAYER 0
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWorldGenBuildUtility, Log, All);

namespace
{
FString SanitizeIdentifier(const FString& Identifier)
{
  FString Safe = Identifier;
  Safe.ReplaceInline(TEXT("."), TEXT("_"));
  Safe.ReplaceInline(TEXT("/"), TEXT("_"));
  Safe.ReplaceInline(TEXT("\\"), TEXT("_"));
  Safe.ReplaceInline(TEXT(" "), TEXT("_"));
  Safe.ReplaceInline(TEXT(":"), TEXT("_"));
  return Safe.IsEmpty() ? TEXT("World") : Safe;
}

FString ResolveMapIdentifier(const FString& MapPath, const UWorld* World)
{
  if (!MapPath.IsEmpty())
  {
    const FString PackageName = FPackageName::ObjectPathToPackageName(MapPath);
    return SanitizeIdentifier(FPackageName::GetShortName(PackageName));
  }

  return SanitizeIdentifier(World ? World->GetMapName() : FString(TEXT("World")));
}

#if WITH_EDITOR
void RegisterAsset(UObject* Asset)
{
  if (Asset)
  {
    FAssetRegistryModule::AssetCreated(Asset);
  }
}
#endif

#if WITH_EDITOR && VHM_HAS_DATA_LAYERS
UDataLayerInstance* ResolveDataLayerInstance(UWorld* World,
                                             UDataLayerEditorSubsystem* EditorSubsystem,
                                             const FName& LayerName)
{
  if (!World || LayerName.IsNone())
  {
    return nullptr;
  }

  if (EditorSubsystem)
  {
    if (UDataLayerInstance* Instance = EditorSubsystem->GetDataLayerInstance(LayerName))
    {
      return Instance;
    }
  }

  if (UDataLayerManager* DataLayerManager =
          UDataLayerManager::GetDataLayerManager(World))
  {
    return const_cast<UDataLayerInstance*>(
        DataLayerManager->GetDataLayerInstanceFromName(LayerName));
  }

  return nullptr;
}

void ValidateDataLayerNames(UWorld* World, const FWorldPartitionPCGDataLayers& Layers,
                            TArray<FString>& OutErrors)
{
  UDataLayerEditorSubsystem* EditorSubsystem = UDataLayerEditorSubsystem::Get();
  if (!EditorSubsystem)
  {
    OutErrors.Add(TEXT("DataLayerEditorSubsystem unavailable; cannot validate PCG data layers."));
    return;
  }

  const struct
  {
    FName Name;
    const TCHAR* Label;
  } RequiredLayers[] = {
      {Layers.TerrainClutter, TEXT("TerrainClutter")},
      {Layers.Trees, TEXT("Trees")},
      {Layers.Rocks, TEXT("Rocks")},
      {Layers.POIs, TEXT("POIs")},
      {Layers.Dynamic, TEXT("Dynamic")},
  };

  for (const auto& Entry : RequiredLayers)
  {
    if (Entry.Name.IsNone())
    {
      OutErrors.Add(FString::Printf(
          TEXT("PCG data layer '%s' is not configured; set WorldGenSettings.PCGDataLayers.%s."),
          Entry.Label, Entry.Label));
      continue;
    }

    if (!ResolveDataLayerInstance(World, EditorSubsystem, Entry.Name))
    {
      OutErrors.Add(FString::Printf(
          TEXT("PCG data layer '%s' (%s) was not found in the current world."),
          Entry.Label, *Entry.Name.ToString()));
    }
  }
}

#if VHM_HAS_HLOD_LAYER
UHLODLayer* ResolveHLODLayer(const FName& LayerName, TArray<FString>& OutErrors)
{
  if (LayerName.IsNone())
  {
    return nullptr;
  }

  const FString LayerPath = LayerName.ToString();
  UHLODLayer* Layer = LoadObject<UHLODLayer>(nullptr, *LayerPath);
  if (!Layer && !LayerPath.Contains(TEXT("/")))
  {
    Layer = FindObject<UHLODLayer>(nullptr, *LayerPath);
  }

  if (!Layer)
  {
    OutErrors.Add(FString::Printf(TEXT("HLOD layer '%s' could not be resolved."),
                                  *LayerPath));
  }

  return Layer;
}
#endif

void ApplyHLODLayerForDataLayer(UWorld* World,
                                UDataLayerEditorSubsystem* EditorSubsystem,
                                UDataLayerInstance* DataLayer,
                                UHLODLayer* HLODLayer,
                                const TCHAR* Label,
                                TArray<FString>& OutErrors)
{
  if (!World || !EditorSubsystem || !DataLayer || !HLODLayer)
  {
    return;
  }

  TArray<AActor*> Actors = EditorSubsystem->GetActorsFromDataLayer(DataLayer);
  if (Actors.IsEmpty())
  {
    return;
  }

  int32 UpdatedCount = 0;
  for (AActor* Actor : Actors)
  {
    if (!Actor)
    {
      continue;
    }

    Actor->Modify();
    Actor->SetHLODLayer(HLODLayer);
    ++UpdatedCount;
  }

  UE_LOG(LogWorldGenBuildUtility, Log,
         TEXT("Applied HLOD layer '%s' to %d actor(s) in Data Layer %s."),
         *HLODLayer->GetName(), UpdatedCount, Label);
}

void ApplyHLODLayerAssignments(UWorld* World, const FWorldPartitionPCGDataLayers& Layers,
                               TArray<FString>& OutErrors)
{
#if VHM_HAS_HLOD_LAYER
  UDataLayerEditorSubsystem* EditorSubsystem = UDataLayerEditorSubsystem::Get();
  if (!EditorSubsystem)
  {
    OutErrors.Add(TEXT("DataLayerEditorSubsystem unavailable; cannot assign HLOD layers."));
    return;
  }

  const struct
  {
    FName DataLayerName;
    FName HLODLayerName;
    const TCHAR* Label;
  } HLODTargets[] = {
      {Layers.Trees, Layers.TreesHLODLayer, TEXT("Trees")},
      {Layers.Rocks, Layers.RocksHLODLayer, TEXT("Rocks")},
      {Layers.POIs, Layers.POIsHLODLayer, TEXT("POIs")},
  };

  for (const auto& Target : HLODTargets)
  {
    if (Target.HLODLayerName.IsNone())
    {
      continue;
    }

    UDataLayerInstance* DataLayer =
        ResolveDataLayerInstance(World, EditorSubsystem, Target.DataLayerName);
    if (!DataLayer)
    {
      OutErrors.Add(FString::Printf(
          TEXT("Cannot apply HLOD layer for %s; Data Layer '%s' was not found."),
          Target.Label, *Target.DataLayerName.ToString()));
      continue;
    }

    UHLODLayer* HLODLayer = ResolveHLODLayer(Target.HLODLayerName, OutErrors);
    if (!HLODLayer)
    {
      continue;
    }

    ApplyHLODLayerForDataLayer(World, EditorSubsystem, DataLayer, HLODLayer,
                               Target.Label, OutErrors);
  }
#else
  if (!Layers.TreesHLODLayer.IsNone() || !Layers.RocksHLODLayer.IsNone() ||
      !Layers.POIsHLODLayer.IsNone())
  {
    OutErrors.Add(TEXT("HLOD layer assignments requested but HLOD support is unavailable."));
  }
#endif
}
#endif // WITH_EDITOR && VHM_HAS_DATA_LAYERS
} // namespace

bool UWorldGenBuildUtility::BuildWorldFromSeed(int32 Seed,
                                               const FString& MapPath,
                                               bool bBuildTerrain,
                                               bool bBuildPCG) {
#if !WITH_EDITOR
  UE_LOG(LogWorldGenBuildUtility, Warning,
         TEXT("BuildWorldFromSeed is editor-only and unavailable in this build"));
  return false;
#else
  UWorldGenBuildUtility* Utility =
      NewObject<UWorldGenBuildUtility>(GetTransientPackage());
  if (!Utility) {
    UE_LOG(LogWorldGenBuildUtility, Error,
           TEXT("Failed to allocate build utility instance."));
    return false;
  }

  UWorld* EditorWorld = nullptr;
  FString ContextError;
  if (!Utility->ValidateEditorContext(EditorWorld, ContextError)) {
    UE_LOG(LogWorldGenBuildUtility, Error, TEXT("%s"), *ContextError);
    return false;
  }

  return Utility->RunBuild(EditorWorld, Seed, MapPath, bBuildTerrain,
                           bBuildPCG, /*bUpdateBuildState=*/true);
#endif // WITH_EDITOR
}

bool UWorldGenBuildUtility::BuildWorldFromSeedInstance(
    int32 Seed, const FString& MapPath, bool bBuildTerrain, bool bBuildPCG) {
#if !WITH_EDITOR
  return false;
#else
  UWorld* EditorWorld = nullptr;
  FString ContextError;
  if (!ValidateEditorContext(EditorWorld, ContextError)) {
    UE_LOG(LogWorldGenBuildUtility, Error, TEXT("%s"), *ContextError);
    BroadcastCompletion(false, ContextError);
    return false;
  }

  return RunBuild(EditorWorld, Seed, MapPath, bBuildTerrain, bBuildPCG,
                  /*bUpdateBuildState=*/true);
#endif // WITH_EDITOR
}

bool UWorldGenBuildUtility::InitializeBuildServices(
    UObject *ContextObject, UHeightfieldService *&OutHeightfield,
    UBiomeService *&OutBiome, UClimateSystem *&OutClimate,
    UPCGWorldService *&OutPCG) {
#if !WITH_EDITOR
  return false;
#else
  UWorldGenSettings *Settings = UWorldGenSettings::GetWorldGenSettings();
  if (!Settings) {
    return false;
  }

  // Climate
  OutClimate = NewObject<UClimateSystem>(ContextObject);
  if (!OutClimate) {
    return false;
  }

  FClimateSettings ClimateSettings;
  OutClimate->Initialize(ClimateSettings, Settings->Settings.Seed,
                         &Settings->Settings);

  // Biome
  OutBiome = NewObject<UBiomeService>(ContextObject);
  if (!OutBiome) {
    return false;
  }

  OutBiome->Initialize(OutClimate, Settings->Settings);
  if (!Settings->SelectedBiomeDefinitionsAsset.IsNull()) {
    if (UBiomeDefinitionsAsset *BiomeAsset =
            Settings->SelectedBiomeDefinitionsAsset.LoadSynchronous()) {
      OutBiome->SetBiomeDefinitions(BiomeAsset->Biomes);
      OutBiome->SetBiomeRingDefinitions(BiomeAsset->BiomeRings);
    }
  }

  // Heightfield
  OutHeightfield = NewObject<UHeightfieldService>(ContextObject);
  if (!OutHeightfield || !OutHeightfield->Initialize(Settings->Settings)) {
    return false;
  }
  OutHeightfield->SetClimateSystem(OutClimate);

  // PCG
  OutPCG = NewObject<UPCGWorldService>(ContextObject);
  if (!OutPCG || !OutPCG->Initialize(Settings->Settings)) {
    return false;
  }
  OutPCG->SetHeightfieldService(OutHeightfield);
  OutPCG->SetBiomeService(OutBiome);
  if (!Settings->SelectedBiomeDefinitionsAsset.IsNull()) {
    if (UBiomeDefinitionsAsset *BiomeAsset =
            Settings->SelectedBiomeDefinitionsAsset.LoadSynchronous()) {
      OutPCG->SetBiomeDefinitions(BiomeAsset->Biomes);
    }
  }

  return true;
#endif // WITH_EDITOR
}

bool UWorldGenBuildUtility::ValidateEditorContext(UWorld *&OutWorld,
                                                  FString &OutError) const {
#if !WITH_EDITOR
  OutError = TEXT("World builds are editor-only.");
  return false;
#else
  if (!GIsEditor || IsRunningGame()) {
    OutError =
        TEXT("World builds must run from the editor (not PIE or packaged).");
    return false;
  }

  if (!GEditor) {
    OutError = TEXT("Editor subsystem unavailable.");
    return false;
  }

  UWorld *EditorWorld = GEditor->GetEditorWorldContext().World();
  if (!EditorWorld) {
    OutError = TEXT("No active editor world found.");
    return false;
  }

#if WITH_AUTOMATION_TESTS
  if (!EvaluateContextForTest(GIsEditor, IsRunningGame(),
                              EditorWorld->WorldType)) {
    OutError = TEXT("World builds are only allowed in editor worlds.");
    return false;
  }
#else
  const bool bEditorWorld = EditorWorld->WorldType == EWorldType::Editor ||
                            EditorWorld->WorldType == EWorldType::EditorPreview;
  if (!(GIsEditor && !IsRunningGame() && bEditorWorld)) {
    OutError = TEXT("World builds are only allowed in editor worlds.");
    return false;
  }
#endif

  OutWorld = EditorWorld;
  return true;
#endif // WITH_EDITOR
}

bool UWorldGenBuildUtility::RunBuild(UWorld *World, int32 Seed,
                                     const FString &MapPath,
                                     bool bBuildTerrain, bool bBuildPCG,
                                     bool bUpdateBuildState) {
#if !WITH_EDITOR
  return false;
#else
  if (!World) {
    UE_LOG(LogWorldGenBuildUtility, Error, TEXT("Missing editor world."));
    return false;
  }

  UWorldGenSettings *Settings = UWorldGenSettings::GetWorldGenSettings();
  if (!Settings) {
    UE_LOG(LogWorldGenBuildUtility, Error, TEXT("Failed to load settings."));
    return false;
  }

  if (Seed != 0) {
    Settings->Settings.Seed = Seed;
  }

  const FWorldGenConfig Config = Settings->Settings;
  UE_LOG(LogWorldGenBuildUtility, Log, TEXT("Building with Seed: %d"),
         Config.Seed);

  AlignPCGGridWithSettings();

  // Initialize services transiently
  UHeightfieldService *HeightfieldService = nullptr;
  UBiomeService *BiomeService = nullptr;
  UClimateSystem *ClimateService = nullptr;
  UPCGWorldService *PCGService = nullptr;

  if (!InitializeBuildServices(World, HeightfieldService, BiomeService,
                               ClimateService, PCGService)) {
    UE_LOG(LogWorldGenBuildUtility, Error,
           TEXT("Failed to initialize build services."));
    return false;
  }

  const FString MapIdentifier = ResolveMapIdentifier(MapPath, World);
  const FString TerrainPackagePath = MakePackagePath(MapIdentifier, TEXT("TerrainData"));
  const FString TerrainObjectName = MakeObjectName(MapIdentifier, TEXT("TerrainData"));

  UPackage *Package = CreatePackage(*TerrainPackagePath);
  if (Package) {
    Package->FullyLoad();
  }

  UWorldGenTerrainResource *TerrainResource =
      FindObject<UWorldGenTerrainResource>(Package, *TerrainObjectName);
  if (!TerrainResource && bBuildTerrain) {
    TerrainResource = NewObject<UWorldGenTerrainResource>(
        Package, UWorldGenTerrainResource::StaticClass(), *TerrainObjectName,
        EObjectFlags::RF_Public | RF_Standalone | RF_Transactional);
#if WITH_EDITOR
    RegisterAsset(TerrainResource);
#endif
  }

  if (!TerrainResource) {
    UE_LOG(LogWorldGenBuildUtility, Error,
           TEXT("Failed to resolve terrain resource for baked data."));
    return false;
  }

  TerrainResource->WorldOrigin = FVector2D::ZeroVector;
  TerrainResource->TileSizeMeters = Config.TileSizeMeters;
  TerrainResource->SampleSpacingMeters = Config.SampleSpacingMeters;
  TerrainResource->SeaLevel = Config.SeaLevel;

  const int32 BuildRadius = Config.GenerateRadius;
  const int32 TilesPerSide = (BuildRadius * 2) + 1;
  const int32 TotalTiles = TilesPerSide * TilesPerSide;
  int32 ProcessedTiles = 0;
  float MinWorldHeight = FLT_MAX;
  float MaxWorldHeight = -FLT_MAX;
  TArray<FString> Errors;

#if WITH_EDITOR && VHM_HAS_DATA_LAYERS
  if (Config.bUseWorldPartitionStreaming)
  {
    if (World->IsPartitionedWorld() && World->GetWorldPartition())
    {
      ValidateDataLayerNames(World, Config.PCGDataLayers, Errors);
    }
    else
    {
      UE_LOG(LogWorldGenBuildUtility, Warning,
             TEXT("World Partition is unavailable for this map; skipping PCG data layer validation."));
    }
  }
#endif

#if VHM_PCG_ENABLED
  if (bBuildPCG && PCGService)
  {
    UBiomeDefinitionsAsset* BiomeAsset =
        Settings->SelectedBiomeDefinitionsAsset.IsNull()
            ? nullptr
            : Settings->SelectedBiomeDefinitionsAsset.LoadSynchronous();
    if (BiomeAsset)
    {
      TArray<FString> GraphPaths;
      for (const TPair<EBiomeType, FBiomeDefinition>& Pair : BiomeAsset->Biomes)
      {
        const FBiomeDefinition& BiomeDef = Pair.Value;
        if (!BiomeDef.BiomePCGGraph.IsNull())
        {
          GraphPaths.AddUnique(BiomeDef.BiomePCGGraph.ToString());
        }
      }

      for (const FString& GraphPath : GraphPaths)
      {
        const FPCGGraphValidationResult Result =
            PCGService->ValidatePCGGraph(GraphPath);
        for (const FString& Error : Result.Errors)
        {
          Errors.Add(
              FString::Printf(TEXT("PCG graph %s: %s"),
                              Result.GraphName.IsEmpty()
                                  ? *Result.GraphPath
                                  : *Result.GraphName,
                              *Error));
        }

        for (const FString& Warning : Result.Warnings)
        {
          UE_LOG(LogWorldGenBuildUtility, Warning,
                 TEXT("PCG graph %s: %s"),
                 Result.GraphName.IsEmpty() ? *Result.GraphPath
                                            : *Result.GraphName,
                 *Warning);
        }
      }
    }
  }
#endif

  if (bBuildTerrain) {
    TerrainResource->HeightTextures.Empty(); // Clear old data
    TerrainResource->BiomeCache.Empty();

    ReportProgress(0, TotalTiles, TEXT("Starting terrain bake"));

    for (int32 X = -BuildRadius; X <= BuildRadius; ++X) {
      for (int32 Y = -BuildRadius; Y <= BuildRadius; ++Y) {
        FTileCoord TileCoord(X, Y);
        FString TileError;
        float TileMin = FLT_MAX;
        float TileMax = -FLT_MAX;
        if (!BuildTerrainForTile(TerrainResource, HeightfieldService, BiomeService,
                                 Config, TileCoord, TileError, TileMin, TileMax)) {
          Errors.Add(TileError);
        } else {
          MinWorldHeight = FMath::Min(MinWorldHeight, TileMin);
          MaxWorldHeight = FMath::Max(MaxWorldHeight, TileMax);
        }

        ++ProcessedTiles;
        if (ProcessedTiles == TotalTiles ||
            ProcessedTiles % FMath::Max(1, TotalTiles / 10) == 0) {
          ReportProgress(
              ProcessedTiles, TotalTiles,
              FString::Printf(
                  TEXT("Terrain %d/%d (Tile %d,%d)"), ProcessedTiles, TotalTiles,
                  TileCoord.X, TileCoord.Y));
        }
      }
    }

    TerrainResource->MinHeight = (MinWorldHeight == FLT_MAX) ? 0.0f : MinWorldHeight;
    TerrainResource->MaxHeight =
        (MaxWorldHeight == -FLT_MAX) ? 0.0f : MaxWorldHeight;
    TerrainResource->MarkPackageDirty();
    if (Package) {
      Package->MarkPackageDirty();
    }
  } else {
    UE_LOG(LogWorldGenBuildUtility, Log,
           TEXT("Skipping terrain bake (bBuildTerrain=false). Using existing prebaked data."));
  }

  UWorldGenExternalDataProvider *ExternalProvider =
      NewObject<UWorldGenExternalDataProvider>(this);
  if (ExternalProvider) {
    ExternalProvider->Initialize(Config, HeightfieldService, BiomeService,
                                 ClimateService, TerrainResource);
  }

  FString PCGHash;
  bool bPCGBuildSucceeded = false;
  if (bBuildPCG) {
    bPCGBuildSucceeded = TriggerPCGOfflineBuild(World, Config, TerrainResource,
                                                PCGService, ExternalProvider,
                                                PCGHash, Errors);
    if (!bPCGBuildSucceeded) {
      UE_LOG(LogWorldGenBuildUtility, Warning,
             TEXT("PCG offline build was skipped or failed; see log for details."));
    }
  } else {
    UE_LOG(LogWorldGenBuildUtility, Log,
           TEXT("Skipping PCG offline build (bBuildPCG=false)."));
    PCGHash = TEXT("PCG_SKIPPED");
  }

#if WITH_EDITOR && VHM_HAS_DATA_LAYERS
  if (bPCGBuildSucceeded && Config.bUseWorldPartitionStreaming)
  {
    ApplyHLODLayerAssignments(World, Config.PCGDataLayers, Errors);
  }
#endif

  if (Errors.IsEmpty() && bUpdateBuildState && bBuildPCG) {
    FString SaveError;
    if (!SaveBuildState(World, Config.Seed, Config, PCGHash, SaveError)) {
      Errors.Add(SaveError);
    }
  } else if (bUpdateBuildState && !bBuildPCG) {
    UE_LOG(LogWorldGenBuildUtility, Warning,
           TEXT("Build state not updated because PCG phase was skipped."));
  }

  const bool bSuccess = Errors.IsEmpty();
  if (!bSuccess) {
    for (const FString &Error : Errors) {
      UE_LOG(LogWorldGenBuildUtility, Error, TEXT("%s"), *Error);
    }
  }

  BroadcastCompletion(
      bSuccess, bSuccess ? TEXT("World build completed")
                         : FString::Printf(TEXT("World build completed with %d issues"),
                                           Errors.Num()));
  return bSuccess;
#endif // WITH_EDITOR
}

bool UWorldGenBuildUtility::BuildTerrainForTile(
    UWorldGenTerrainResource *TerrainResource,
    UHeightfieldService *HeightfieldService, UBiomeService *BiomeService,
    const FWorldGenConfig &Config, const FTileCoord &TileCoord,
    FString &OutError, float &OutTileMin, float &OutTileMax) {
#if !WITH_EDITOR
  return false;
#else
  OutError.Reset();
  OutTileMin = FLT_MAX;
  OutTileMax = -FLT_MAX;

  if (!TerrainResource || !HeightfieldService) {
    OutError = TEXT("Missing terrain resource or heightfield service.");
    return false;
  }

  FHeightfieldData HeightData =
      HeightfieldService->GenerateHeightfield(Config.Seed, TileCoord);
  HeightfieldService->CacheHeightfield(HeightData);

  const int32 Resolution = HeightData.Resolution;
  const int32 ExpectedSamples = Resolution * Resolution;
  if (HeightData.HeightData.Num() != ExpectedSamples) {
    OutError = FString::Printf(
        TEXT("HeightData size mismatch for tile (%d,%d). Expected %d, got %d"),
        TileCoord.X, TileCoord.Y, ExpectedSamples,
        HeightData.HeightData.Num());
    return false;
  }

  for (float Sample : HeightData.HeightData) {
    OutTileMin = FMath::Min(OutTileMin, Sample);
    OutTileMax = FMath::Max(OutTileMax, Sample);
  }

  const FString TexName = FString::Printf(
      TEXT("%s_Height_%d_%d"), *TerrainResource->GetName(), TileCoord.X,
      TileCoord.Y);
  UPackage *Package = Cast<UPackage>(TerrainResource->GetOutermost());
  UTexture2D *Texture =
      NewObject<UTexture2D>(Package ? Package : GetTransientPackage(),
                            *TexName, RF_Public | RF_Standalone | RF_Transactional);
  if (!Texture) {
    OutError = FString::Printf(
        TEXT("Failed to allocate height texture for tile (%d,%d)"), TileCoord.X,
        TileCoord.Y);
    return false;
  }

  Texture->Source.Init(Resolution, Resolution, 1, 1, TSF_R32F);
  uint8 *MipData = Texture->Source.LockMip(0);
  FMemory::Memcpy(MipData, HeightData.HeightData.GetData(),
                  HeightData.HeightData.Num() * sizeof(float));
  Texture->Source.UnlockMip(0);

  Texture->CompressionSettings = TC_HDR; // Use HDR for floats
  Texture->Filter = TF_Bilinear;
  Texture->SRGB = 0;
  Texture->UpdateResource();
  Texture->MarkPackageDirty();
  if (Package) {
    Package->MarkPackageDirty();
  }

  TerrainResource->HeightTextures.Add(FIntPoint(TileCoord.X, TileCoord.Y),
                                      TSoftObjectPtr<UTexture2D>(Texture));

  if (BiomeService) {
    const FVector TileCenter = TileCoord.ToWorldPosition(Config);
    const FVector2D TileCenter2D(TileCenter.X, TileCenter.Y);
    const float Altitude = (OutTileMin + OutTileMax) * 0.5f;
    const FBiomeResult Biome =
        BiomeService->DetermineBiome(TileCenter2D, Altitude);
    TerrainResource->BiomeCache.Add(FIntPoint(TileCoord.X, TileCoord.Y), Biome);
  }

  return true;
#endif // WITH_EDITOR
}

bool UWorldGenBuildUtility::TriggerPCGOfflineBuild(
    UWorld *World, const FWorldGenConfig &Config,
    UWorldGenTerrainResource *TerrainResource, UPCGWorldService *PCGService,
    UWorldGenExternalDataProvider *DataProvider, FString &OutPCGHash,
    TArray<FString> &OutErrors) {
#if !WITH_EDITOR
  return false;
#else
#if !VHM_PCG_ENABLED
  OutPCGHash = TEXT("PCG_DISABLED");
  OutErrors.Add(
      TEXT("PCG plugin disabled; offline build skipped for EditorBuildOnce."));
  return false;
#else
  if (DataProvider) {
    UE_LOG(LogWorldGenBuildUtility, Log,
           TEXT("External data provider prepared for PCG graphs."));
  }
  if (PCGService) {
    // PCG service already seeded with worldgen data; no additional wiring here.
  }
  if (TerrainResource) {
    UE_LOG(LogWorldGenBuildUtility, Log,
           TEXT("Terrain resource ready for offline PCG build."));
  }

  // Invoke UE5.7 PCG offline builder (PCG World Partition Builder / pcg.BuildComponents)
  const bool bExecResult =
      (GEditor && World) && GEditor->Exec(World, TEXT("pcg.BuildComponents -All"));
  if (!bExecResult) {
    UE_LOG(LogWorldGenBuildUtility, Warning,
           TEXT("pcg.BuildComponents command failed or returned false; PCG content not rebuilt."));
    OutPCGHash = TEXT("PCG_FAILED");
    return false; // Non-fatal: terrain is still baked
  }

  uint32 HashValue =
      HashCombine(GetTypeHash(Config.Seed), GetTypeHash(Config.WorldGenVersion));
  HashValue = HashCombine(HashValue, GetTypeHash(Config.GenerateRadius));
  OutPCGHash = FString::Printf(TEXT("%08x"), HashValue);
  UE_LOG(LogWorldGenBuildUtility, Log,
         TEXT("PCG offline build completed with hash %s"), *OutPCGHash);
  return true;
#endif // VHM_PCG_ENABLED
#endif // WITH_EDITOR
}

bool UWorldGenBuildUtility::SaveBuildState(UWorld *World, int32 Seed,
                                           const FWorldGenConfig &Config,
                                           const FString &PCGHash,
                                           FString &OutError) const {
#if !WITH_EDITOR
  return false;
#else
  const FString MapIdentifier = ResolveMapIdentifier(FString(), World);
  const FString PackagePath = MakePackagePath(MapIdentifier, TEXT("BuildState"));
  const FString ObjectName = MakeObjectName(MapIdentifier, TEXT("BuildState"));

  UPackage *Package = CreatePackage(*PackagePath);
  if (Package) {
    Package->FullyLoad();
  }

  UWorldGenBuildStateAsset *BuildAsset =
      FindObject<UWorldGenBuildStateAsset>(Package, *ObjectName);
  if (!BuildAsset) {
    BuildAsset = NewObject<UWorldGenBuildStateAsset>(
        Package, UWorldGenBuildStateAsset::StaticClass(), *ObjectName,
        EObjectFlags::RF_Public | RF_Standalone | RF_Transactional);
#if WITH_EDITOR
    RegisterAsset(BuildAsset);
#endif
  }

  if (!BuildAsset) {
    OutError =
        TEXT("Failed to allocate WorldGenBuildStateAsset for baked world.");
    return false;
  }

  BuildAsset->UpdateFromBuild(Seed, Config.WorldGenVersion, PCGHash);
  BuildAsset->MarkPackageDirty();
  if (Package) {
    Package->MarkPackageDirty();
  }

  UE_LOG(LogWorldGenBuildUtility, Log,
         TEXT("Saved build state (Seed=%d, Version=%d, Hash=%s)"),
         Seed, Config.WorldGenVersion, *PCGHash);
  return true;
#endif // WITH_EDITOR
}

void UWorldGenBuildUtility::ReportProgress(int32 Current, int32 Total,
                                           const FString &Status) {
  OnBuildProgress.Broadcast(Current, Total, Status);
  UE_LOG(LogWorldGenBuildUtility, Log, TEXT("%s"), *Status);

#if WITH_EDITOR
  if (Total <= 1 || Current == 0 || Current == Total) {
    FNotificationInfo Info(FText::FromString(Status));
    Info.bFireAndForget = true;
    Info.FadeOutDuration = 0.2f;
    Info.ExpireDuration = 1.5f;
    FSlateNotificationManager::Get().AddNotification(Info);
  }
#endif
}

void UWorldGenBuildUtility::BroadcastCompletion(bool bSuccess,
                                                const FString &Message) {
  OnBuildComplete.Broadcast(bSuccess, Message);
  if (bSuccess) {
    UE_LOG(LogWorldGenBuildUtility, Log, TEXT("%s"), *Message);
  } else {
    UE_LOG(LogWorldGenBuildUtility, Error, TEXT("%s"), *Message);
  }

#if WITH_EDITOR
  FNotificationInfo Info(FText::FromString(Message));
  Info.bFireAndForget = true;
  Info.ExpireDuration = 3.0f;
  Info.bUseSuccessFailIcons = true;
  if (TSharedPtr<SNotificationItem> Notification =
          FSlateNotificationManager::Get().AddNotification(Info)) {
    Notification->SetCompletionState(bSuccess ? SNotificationItem::CS_Success
                                              : SNotificationItem::CS_Fail);
  }
#endif
}

FString UWorldGenBuildUtility::MakePackagePath(const FString &MapIdentifier,
                                               const FString &Suffix) {
  const FString SafeMap = SanitizeIdentifier(MapIdentifier);
  return FString::Printf(TEXT("/Game/WorldGen/Baked/%s_%s"), *SafeMap,
                         *Suffix);
}

FString UWorldGenBuildUtility::MakeObjectName(const FString &MapIdentifier,
                                              const FString &BaseName) {
  const FString SafeMap = SanitizeIdentifier(MapIdentifier);
  return FString::Printf(TEXT("%s_%s"), *SafeMap, *BaseName);
}

bool UWorldGenBuildUtility::AlignPCGGridWithSettings() {
#if !WITH_EDITOR
  return false;
#else
  UE_LOG(LogWorldGenBuildUtility, Log,
         TEXT("Aligning PCG World Actor to WorldGen Settings..."));

  UWorld *World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
  if (!World) {
    UE_LOG(LogWorldGenBuildUtility, Error,
           TEXT("No active editor world found."));
    return false;
  }

  UWorldGenSettings *Settings = UWorldGenSettings::GetWorldGenSettings();
  if (!Settings) {
    UE_LOG(LogWorldGenBuildUtility, Error,
           TEXT("Failed to load WorldGenSettings."));
    return false;
  }

  APCGWorldActor *PCGActor = Cast<APCGWorldActor>(
      UGameplayStatics::GetActorOfClass(World, APCGWorldActor::StaticClass()));

  // Spawn if missing
  if (!PCGActor) {
    UE_LOG(LogWorldGenBuildUtility, Warning,
           TEXT("No APCGWorldActor found. Spawning a new one."));
    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = TEXT("PCGWorldActor");
    PCGActor = World->SpawnActor<APCGWorldActor>(APCGWorldActor::StaticClass(),
                                                 SpawnParams);
  }

  if (PCGActor) {
    const int32 TileSizeMeters = Settings->Settings.TileSizeMeters;
    const uint32 GridSizeCm = TileSizeMeters * 100;
    const bool bHasGridSize = PCGActor->PartitionGridSize > 0;
    const float PartitionGridMeters =
        bHasGridSize
            ? static_cast<float>(PCGActor->PartitionGridSize) / 100.0f
            : 0.0f;
    if (bHasGridSize &&
        !FTileCoord::IsAlignedWithPCGGrid(TileSizeMeters, PartitionGridMeters)) {
      const float Ratio = PartitionGridMeters > KINDA_SMALL_NUMBER
                              ? TileSizeMeters / PartitionGridMeters
                              : 0.0f;
      UE_LOG(
          LogWorldGenBuildUtility, Warning,
          TEXT("PCG grid misaligned with world tiles: TileSize=%.2fm, "
               "PartitionGrid=%.2fm (ratio=%.3f). Recommended grid is %d cm "
               "or another integer divisor/multiple of the tile size."),
          static_cast<float>(TileSizeMeters), PartitionGridMeters, Ratio,
          GridSizeCm);
    }

    if (PCGActor->PartitionGridSize != GridSizeCm) {
      UE_LOG(LogWorldGenBuildUtility, Log,
             TEXT("Updating PartitionGridSize from %d to %d"),
             PCGActor->PartitionGridSize, GridSizeCm);
      PCGActor->PartitionGridSize = GridSizeCm;
      PCGActor->Modify();
    }

    return true;
  }

  UE_LOG(LogWorldGenBuildUtility, Error,
         TEXT("Failed to spawn or find PCGWorldActor."));
  return false;
#endif // WITH_EDITOR
}

#if WITH_AUTOMATION_TESTS
bool UWorldGenBuildUtility::EvaluateContextForTest(
    bool bIsEditor, bool bIsRunningGame, EWorldType::Type WorldType) {
  const bool bEditorWorld = WorldType == EWorldType::Editor ||
                            WorldType == EWorldType::EditorPreview;
  return bIsEditor && !bIsRunningGame && bEditorWorld;
}
#endif // WITH_AUTOMATION_TESTS
