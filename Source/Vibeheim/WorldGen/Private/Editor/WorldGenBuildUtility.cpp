#include "Editor/WorldGenBuildUtility.h"
#include "Data/WorldGenAssets.h"
#include "Data/WorldGenTerrainResource.h"
#include "Data/WorldGenTypes.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "PCGWorldActor.h"
#include "Services/BiomeService.h"
#include "Services/ClimateSystem.h"
#include "Services/HeightfieldService.h"
#include "Services/PCGWorldService.h"
#include "UObject/Package.h"
#include "VHMTerrainRendering/VHMTypes.h"
#include "WorldGenSettings.h"
#include <cfloat>

bool UWorldGenBuildUtility::BuildWorldFromSeed(int32 Seed) {
#if !WITH_EDITOR
  UE_LOG(LogTemp, Warning,
         TEXT("BuildWorldFromSeed is editor-only and unavailable in this build"));
  return false;
#else
  UE_LOG(LogTemp, Log, TEXT("Starting World Build (Editor Mode)..."));

  // Ensure we are in a valid editor world
  UWorld *World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
  if (!World) {
    UE_LOG(LogTemp, Error, TEXT("No active editor world found. Cannot build."));
    return false;
  }

  // Initialize settings
  UWorldGenSettings *Settings = UWorldGenSettings::GetWorldGenSettings();
  if (!Settings) {
    UE_LOG(LogTemp, Error, TEXT("Failed to load WorldGenSettings."));
    return false;
  }

  if (Seed != 0) {
    Settings->Settings.Seed = Seed;
  }

  UE_LOG(LogTemp, Log, TEXT("Building with Seed: %d"), Settings->Settings.Seed);

  // Initialize services transiently
  UHeightfieldService *HeightfieldService = nullptr;
  UBiomeService *BiomeService = nullptr;
  UClimateSystem *ClimateService = nullptr;
  UPCGWorldService *PCGService = nullptr;

  UObject *ServiceOuter = World;

  if (!InitializeBuildServices(ServiceOuter, HeightfieldService, BiomeService,
                               ClimateService, PCGService)) {
    UE_LOG(LogTemp, Error, TEXT("Failed to initialize build services."));
    return false;
  }

  // Create or load persistence asset
  FString PackageName = TEXT("/Game/WorldGen/Baked/BakedTerrainData");
  UPackage *Package = CreatePackage(*PackageName);
  Package->FullyLoad();

  UWorldGenTerrainResource *TerrainResource =
      FindObject<UWorldGenTerrainResource>(Package, TEXT("BakedTerrainData"));
  if (!TerrainResource) {
    TerrainResource = NewObject<UWorldGenTerrainResource>(
        Package, UWorldGenTerrainResource::StaticClass(),
        TEXT("BakedTerrainData"),
        EObjectFlags::RF_Public | RF_Standalone | RF_Transactional);
  }

  // Initialize resource metadata
  TerrainResource->WorldOrigin = FVector2D::ZeroVector;
  TerrainResource->TileSizeMeters = Settings->Settings.TileSizeMeters;
  TerrainResource->SampleSpacingMeters = Settings->Settings.SampleSpacingMeters;
  TerrainResource->SeaLevel = Settings->Settings.SeaLevel;
  TerrainResource->HeightTextures.Empty(); // Clear old data
  TerrainResource->BiomeCache.Empty();

  // Define build bounds
  const int32 BuildRadius = Settings->Settings.GenerateRadius;
  UE_LOG(LogTemp, Log,
         TEXT("Building Tiles in Radius: %d based on GenerateRadius"),
         BuildRadius);

  const int32 Resolution =
      Settings->VHMSettings.IsSet()
          ? Settings->VHMSettings.GetValue().HeightTextureResolution
          : 64;

  // Iterate and generate
  int32 TotalTiles = 0;
  float MinWorldHeight = FLT_MAX;
  float MaxWorldHeight = -FLT_MAX;

  for (int32 X = -BuildRadius; X <= BuildRadius; ++X) {
    for (int32 Y = -BuildRadius; Y <= BuildRadius; ++Y) {
      FTileCoord TileCoord(X, Y);

      // Generate Heightfield
      FHeightfieldData HeightData = HeightfieldService->GenerateHeightfield(
          Settings->Settings.Seed, TileCoord);

      // Update min/max
      for (float H : HeightData.HeightData) {
        MinWorldHeight = FMath::Min(MinWorldHeight, H);
        MaxWorldHeight = FMath::Max(MaxWorldHeight, H);
      }

      // Create Texture from height data
      const FString TexName = FString::Printf(TEXT("Height_%d_%d"), X, Y);
      UTexture2D *Texture =
          NewObject<UTexture2D>(Package, *TexName, RF_Public | RF_Standalone);

      // Init texture data (R32F for precision)
      Texture->Source.Init(Resolution, Resolution, 1, 1,
                           ETextureSourceFormat::TSF_R32F);

      // Lock and copy
      uint8 *MipData = Texture->Source.LockMip(0);
      float *FloatMipData = reinterpret_cast<float *>(MipData);

      if (HeightData.HeightData.Num() == Resolution * Resolution) {
        FMemory::Memcpy(FloatMipData, HeightData.HeightData.GetData(),
                        HeightData.HeightData.Num() * sizeof(float));
      } else {
        UE_LOG(LogTemp, Warning,
               TEXT("HeightData size mismatch for tile (%d, %d). Expected %d, got %d"),
               X, Y, Resolution * Resolution, HeightData.HeightData.Num());
        FMemory::Memzero(FloatMipData, Resolution * Resolution * sizeof(float));
      }

      Texture->Source.UnlockMip(0);

      Texture->CompressionSettings = TC_HDR; // Use HDR for floats
      Texture->Filter = TF_Bilinear;
      Texture->SRGB = 0;
      Texture->PostEditChange();

      // Store in resource
      TerrainResource->HeightTextures.Add(FIntPoint(TileCoord.X, TileCoord.Y),
                                          Texture);
      TotalTiles++;
    }
  }

  TerrainResource->MinHeight = MinWorldHeight;
  TerrainResource->MaxHeight = MaxWorldHeight;
  TerrainResource->MarkPackageDirty();

  UE_LOG(LogTemp, Log,
         TEXT("World Build Complete. Processed %d tiles. MinH: %.2f, MaxH: %.2f"),
         TotalTiles, MinWorldHeight, MaxWorldHeight);

  return true;
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

bool UWorldGenBuildUtility::AlignPCGGridWithSettings() {
#if !WITH_EDITOR
  return false;
#else
  UE_LOG(LogTemp, Log,
         TEXT("Aligning PCG World Actor to WorldGen Settings..."));

  UWorld *World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
  if (!World) {
    UE_LOG(LogTemp, Error, TEXT("No active editor world found."));
    return false;
  }

  UWorldGenSettings *Settings = UWorldGenSettings::GetWorldGenSettings();
  if (!Settings) {
    UE_LOG(LogTemp, Error, TEXT("Failed to load WorldGenSettings."));
    return false;
  }

  APCGWorldActor *PCGActor = Cast<APCGWorldActor>(
      UGameplayStatics::GetActorOfClass(World, APCGWorldActor::StaticClass()));

  // Spawn if missing
  if (!PCGActor) {
    UE_LOG(LogTemp, Warning,
           TEXT("No APCGWorldActor found. Spawning a new one."));
    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = TEXT("PCGWorldActor");
    PCGActor = World->SpawnActor<APCGWorldActor>(APCGWorldActor::StaticClass(),
                                                 SpawnParams);
  }

  if (PCGActor) {
    const int32 TileSizeMeters = Settings->Settings.TileSizeMeters;
    const uint32 GridSizeCm = TileSizeMeters * 100;

    if (PCGActor->PartitionGridSize != GridSizeCm) {
      UE_LOG(LogTemp, Log, TEXT("Updating PartitionGridSize from %d to %d"),
             PCGActor->PartitionGridSize, GridSizeCm);
      PCGActor->PartitionGridSize = GridSizeCm;
      PCGActor->Modify();
    }

    return true;
  }

  UE_LOG(LogTemp, Error, TEXT("Failed to spawn or find PCGWorldActor."));
  return false;
#endif // WITH_EDITOR
}
