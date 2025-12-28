#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "WorldGenSettings.h"
#include "Data/WorldGenAssets.h"
#include "WorldGenManager.h"
#include "Services/TileStreamingService.h"
#include "Services/HeightfieldService.h"
#include "Services/BiomeService.h"
#include "Services/POIService.h"
#include "Services/RiverFlowService.h"
#include "VHMTerrainRendering/VHMTerrainRenderer.h"
#include "VHMTerrainRendering/VHMTypes.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "Editor/WorldGenBuildUtility.h"
#include "Data/WorldGenBuildState.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Modules/ModuleManager.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "Algo/Sort.h"
#include "UObject/EnumProperty.h"

DEFINE_LOG_CATEGORY_STATIC(LogWorldGenConsole, Log, All);

namespace WorldGenConsole
{
    FString ResolveExportDirectory(const FString& UserPath, const FString& DefaultSubDir)
    {
        const FString BaseSavedDir = FPaths::ProjectSavedDir();

        if (!UserPath.IsEmpty())
        {
            if (FPaths::IsRelative(UserPath))
            {
                return FPaths::ConvertRelativePathToFull(FPaths::Combine(BaseSavedDir, UserPath));
            }
            return UserPath;
        }

        return FPaths::Combine(BaseSavedDir, DefaultSubDir);
    }

    bool EnsureDirectoryExists(const FString& Directory)
    {
        return IFileManager::Get().MakeDirectory(*Directory, /*Tree*/true);
    }

    bool SaveColorPNG(const FString& AbsolutePath, const TArray<FColor>& Pixels, int32 Width, int32 Height)
    {
        if (Pixels.Num() != Width * Height || Width <= 0 || Height <= 0)
        {
            return false;
        }

        IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
        TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
        if (!ImageWrapper.IsValid())
        {
            return false;
        }

        if (!ImageWrapper->SetRaw(Pixels.GetData(), Pixels.Num() * sizeof(FColor), Width, Height, ERGBFormat::RGBA, 8))
        {
            return false;
        }

        const TArray64<uint8>& Compressed = ImageWrapper->GetCompressed();
        TArray<uint8> CompressedCopy;
        CompressedCopy.SetNumUninitialized(Compressed.Num());
        if (Compressed.Num() > 0)
        {
            FMemory::Memcpy(CompressedCopy.GetData(), Compressed.GetData(), Compressed.Num());
        }

        return FFileHelper::SaveArrayToFile(CompressedCopy, *AbsolutePath);
    }

    bool SaveGrayscalePNG(const FString& AbsolutePath, const TArray<uint8>& GrayscalePixels, int32 Width, int32 Height)
    {
        if (GrayscalePixels.Num() != Width * Height || Width <= 0 || Height <= 0)
        {
            return false;
        }

        TArray<FColor> Colors;
        Colors.Reserve(GrayscalePixels.Num());
        for (uint8 Value : GrayscalePixels)
        {
            Colors.Add(FColor(Value, Value, Value, 255));
        }

        return SaveColorPNG(AbsolutePath, Colors, Width, Height);
    }

}

static bool IsEngineReady()
{
    return GEngine != nullptr && IsInGameThread();
}

static UWorld* GetAnyWorld()
{
    if (!GEngine) return nullptr;

    const TIndirectArray<FWorldContext>& Contexts = GEngine->GetWorldContexts();
    // Prefer PIE/Game worlds first
    for (const FWorldContext& Ctx : Contexts)
    {
        UWorld* W = Ctx.World();
        if (!W) continue;
        const EWorldType::Type WT = W->WorldType;
        if (WT == EWorldType::PIE || WT == EWorldType::Game || WT == EWorldType::GameRPC || WT == EWorldType::GamePreview)
        {
            return W;
        }
    }
    // Fallback to Editor worlds
    for (const FWorldContext& Ctx : Contexts)
    {
        UWorld* W = Ctx.World();
        if (!W) continue;
        const EWorldType::Type WT = W->WorldType;
        if (WT == EWorldType::Editor || WT == EWorldType::EditorPreview)
        {
            return W;
        }
    }
    return nullptr;
}

static AWorldGenManager* FindWorldGenManager(UWorld* World)
{
    if (!World) return nullptr;
    for (TActorIterator<AWorldGenManager> It(World); It; ++It)
    {
        return *It;
    }
    return nullptr;
}

static FString SanitizeMapIdentifier(const FString& Identifier)
{
    FString Safe = Identifier;
    Safe.ReplaceInline(TEXT("."), TEXT("_"));
    Safe.ReplaceInline(TEXT("/"), TEXT("_"));
    Safe.ReplaceInline(TEXT("\\"), TEXT("_"));
    Safe.ReplaceInline(TEXT(" "), TEXT("_"));
    Safe.ReplaceInline(TEXT(":"), TEXT("_"));
    return Safe.IsEmpty() ? TEXT("World") : Safe;
}

static FString ResolveMapIdentifier(const FString& MapPath, const UWorld* World)
{
    if (!MapPath.IsEmpty())
    {
        const FString PackageName = FPackageName::ObjectPathToPackageName(MapPath);
        return SanitizeMapIdentifier(FPackageName::GetShortName(PackageName));
    }

    return SanitizeMapIdentifier(World ? World->GetMapName() : FString(TEXT("World")));
}

static FString MakeBuildStateObjectPath(const FString& MapIdentifier)
{
    return FString::Printf(TEXT("/Game/WorldGen/Baked/%s_BuildState.%s_BuildState"),
                           *MapIdentifier, *MapIdentifier);
}

static bool ResolveBuildState(UWorld* World, AWorldGenManager* Manager,
                              UWorldGenBuildStateAsset*& OutAsset,
                              FWorldBuildState& OutState, FString& OutPath)
{
    OutAsset = nullptr;
    OutState = FWorldBuildState();
    OutPath.Reset();

    // Prefer explicitly assigned asset on the manager
    if (Manager && Manager->WorldBuildStateAsset.IsValid())
    {
        OutAsset = Manager->WorldBuildStateAsset.LoadSynchronous();
        OutPath = Manager->WorldBuildStateAsset.ToString();
    }

    if (!OutAsset && World)
    {
        const FString MapId = ResolveMapIdentifier(FString(), World);
        const FString ObjectPath = MakeBuildStateObjectPath(MapId);
        OutAsset = LoadObject<UWorldGenBuildStateAsset>(nullptr, *ObjectPath);
        OutPath = ObjectPath;
    }

    if (OutAsset)
    {
        OutState = OutAsset->BuildState;
        return true;
    }

    return false;
}

static bool ParseSeedArg(const TArray<FString>& Args, int32& OutSeed)
{
    OutSeed = 0;
    if (Args.Num() <= 0)
    {
        return true;
    }

    return LexTryParseString(OutSeed, *Args[0]);
}

static bool ValidateEditorBuildContext()
{
    if (!GIsEditor || IsRunningGame())
    {
        UE_LOG(LogWorldGenConsole, Error,
               TEXT("World builds are editor-only and must run outside PIE/game."));
        return false;
    }
    if (!IsEngineReady())
    {
        UE_LOG(LogWorldGenConsole, Warning,
               TEXT("Engine not ready - try after PIE starts or in an editor world."));
        return false;
    }
    return true;
}

// Show settings (new + legacy alias)
static void ExecShowSettings()
{
    if (!IsEngineReady())
    {
        UE_LOG(LogWorldGenConsole, Warning, TEXT("Engine not ready - try after PIE starts"));
        return;
    }
    UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
    if (!Settings)
    {
        UE_LOG(LogWorldGenConsole, Error, TEXT("WorldGenSettings not available"));
        return;
    }
    const FWorldGenConfig& S = Settings->Settings;
    UE_LOG(LogWorldGenConsole, Log, TEXT("=== WorldGen Settings ==="));
    UE_LOG(LogWorldGenConsole, Log, TEXT("Seed: %d  Version: %d"), S.Seed, S.WorldGenVersion);
    UE_LOG(LogWorldGenConsole, Log, TEXT("Radii: Generate=%d Load=%d Active=%d"), S.GenerateRadius, S.LoadRadius, S.ActiveRadius);
    UE_LOG(LogWorldGenConsole, Log, TEXT("Features: Water=%s Rivers=%s Rings=%s PCGGraphs=%s"),
        S.bEnableWater ? TEXT("On") : TEXT("Off"),
        S.bEnableRivers ? TEXT("On") : TEXT("Off"),
        S.bEnableRings ? TEXT("On") : TEXT("Off"),
        S.bEnablePCGGraphs ? TEXT("On") : TEXT("Off"));
}

static FAutoConsoleCommand CmdShowSettings(
    TEXT("wg.settings.show"),
    TEXT("Display current world generation settings"),
    FConsoleCommandDelegate::CreateStatic(&ExecShowSettings)
);

// Trigger full world build (terrain + PCG)
static FAutoConsoleCommand CmdBuildWorld(
    TEXT("wg.build.world"),
    TEXT("Trigger full world build. Usage: wg.build.world [Seed]"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        int32 Seed = 0;
        if (!ParseSeedArg(Args, Seed))
        {
            UE_LOG(LogWorldGenConsole, Error, TEXT("Invalid seed value '%s'."), Args.Num() > 0 ? *Args[0] : TEXT(""));
            return;
        }

        if (!ValidateEditorBuildContext())
        {
            return;
        }

        const bool bSuccess = UWorldGenBuildUtility::BuildWorldFromSeed(Seed);
        if (bSuccess)
        {
            UE_LOG(LogWorldGenConsole, Log, TEXT("wg.build.world succeeded (Seed=%d)"), Seed);
        }
        else
        {
            UE_LOG(LogWorldGenConsole, Error, TEXT("wg.build.world failed (Seed=%d)"), Seed);
        }
    })
);

// Trigger terrain-only build (no PCG)
static FAutoConsoleCommand CmdBuildTerrain(
    TEXT("wg.build.terrain"),
    TEXT("Build terrain only (no PCG). Usage: wg.build.terrain [Seed]"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        int32 Seed = 0;
        if (!ParseSeedArg(Args, Seed))
        {
            UE_LOG(LogWorldGenConsole, Error, TEXT("Invalid seed value '%s'."), Args.Num() > 0 ? *Args[0] : TEXT(""));
            return;
        }

        if (!ValidateEditorBuildContext())
        {
            return;
        }

        const bool bSuccess = UWorldGenBuildUtility::BuildWorldFromSeed(Seed, TEXT(""), /*bBuildTerrain=*/true, /*bBuildPCG=*/false);
        if (bSuccess)
        {
            UE_LOG(LogWorldGenConsole, Log,
                   TEXT("wg.build.terrain succeeded (Seed=%d). Build state is not updated when PCG is skipped."),
                   Seed);
        }
        else
        {
            UE_LOG(LogWorldGenConsole, Error,
                   TEXT("wg.build.terrain failed (Seed=%d). Build state is not updated when PCG is skipped."),
                   Seed);
        }
    })
);

// Trigger PCG-only build (requires prebaked terrain)
static FAutoConsoleCommand CmdBuildPCG(
    TEXT("wg.build.pcg"),
    TEXT("Build PCG only (requires prebaked terrain). Usage: wg.build.pcg [Seed]"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        int32 Seed = 0;
        if (!ParseSeedArg(Args, Seed))
        {
            UE_LOG(LogWorldGenConsole, Error, TEXT("Invalid seed value '%s'."), Args.Num() > 0 ? *Args[0] : TEXT(""));
            return;
        }

        if (!ValidateEditorBuildContext())
        {
            return;
        }

        const bool bSuccess = UWorldGenBuildUtility::BuildWorldFromSeed(Seed, TEXT(""), /*bBuildTerrain=*/false, /*bBuildPCG=*/true);
        if (bSuccess)
        {
            UE_LOG(LogWorldGenConsole, Log, TEXT("wg.build.pcg succeeded (Seed=%d)"), Seed);
        }
        else
        {
            UE_LOG(LogWorldGenConsole, Error, TEXT("wg.build.pcg failed (Seed=%d)"), Seed);
        }
    })
);

// Display build state status
static FAutoConsoleCommand CmdBuildStatus(
    TEXT("wg.build.status"),
    TEXT("Display current build state (seed, version, timestamp, validity)"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        if (!IsEngineReady())
        {
            UE_LOG(LogWorldGenConsole, Warning, TEXT("Engine not ready - try after PIE starts"));
            return;
        }

        UWorld* World = GetAnyWorld();
        if (!World)
        {
            UE_LOG(LogWorldGenConsole, Error, TEXT("No world available to inspect build status."));
            return;
        }

        AWorldGenManager* Manager = FindWorldGenManager(World);
        UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
        const FWorldGenConfig Config = Settings ? Settings->Settings : FWorldGenConfig();

        UWorldGenBuildStateAsset* BuildAsset = nullptr;
        FWorldBuildState BuildState;
        FString AssetPath;
        const bool bHasAsset = ResolveBuildState(World, Manager, BuildAsset, BuildState, AssetPath);
        const bool bValid = bHasAsset && BuildState.IsValid();
        const bool bCompatible = bValid && BuildState.IsCompatibleWith(Config);

        UE_LOG(LogWorldGenConsole, Log,
               TEXT("Build Status: Mode=%d Seed=%d Version=%d Map=%s"),
               static_cast<int32>(Config.BuildMode), Config.Seed, Config.WorldGenVersion, *World->GetMapName());
        if (!bHasAsset)
        {
            UE_LOG(LogWorldGenConsole, Warning, TEXT("No build state asset found for current map."));
            return;
        }

        UE_LOG(LogWorldGenConsole, Log,
               TEXT("BuildState Asset: %s | Baked=%s Valid=%s Compatible=%s"),
               *AssetPath,
               BuildState.bIsBaked ? TEXT("true") : TEXT("false"),
               bValid ? TEXT("true") : TEXT("false"),
               bCompatible ? TEXT("true") : TEXT("false"));
        UE_LOG(LogWorldGenConsole, Log,
               TEXT("BuiltSeed=%d Version=%d PCGHash=%s Timestamp=%s"),
               BuildState.BuiltSeed, BuildState.WorldGenVersion,
               *BuildState.PCGBuildHash, *BuildState.LastBuildTime.ToString());
    })
);

// Validate build state against current config
static FAutoConsoleCommand CmdBuildValidate(
    TEXT("wg.build.validate"),
    TEXT("Validate build state against current config."),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        if (!IsEngineReady())
        {
            UE_LOG(LogWorldGenConsole, Warning, TEXT("Engine not ready - try after PIE starts"));
            return;
        }

        UWorld* World = GetAnyWorld();
        if (!World)
        {
            UE_LOG(LogWorldGenConsole, Error, TEXT("No world available to validate build state."));
            return;
        }

        AWorldGenManager* Manager = FindWorldGenManager(World);
        UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
        const FWorldGenConfig Config = Settings ? Settings->Settings : FWorldGenConfig();

        UWorldGenBuildStateAsset* BuildAsset = nullptr;
        FWorldBuildState BuildState;
        FString AssetPath;
        const bool bHasAsset = ResolveBuildState(World, Manager, BuildAsset, BuildState, AssetPath);

        if (!bHasAsset)
        {
            UE_LOG(LogWorldGenConsole, Error, TEXT("No build state asset found; world is treated as unbaked."));
            return;
        }

        const bool bValid = BuildState.IsValid();
        const bool bCompatible = BuildState.IsCompatibleWith(Config);

        if (!bValid)
        {
            UE_LOG(LogWorldGenConsole, Error,
                   TEXT("Build state is invalid (Baked=%s Seed=%d Version=%d Hash=%s Timestamp=%s)."),
                   BuildState.bIsBaked ? TEXT("true") : TEXT("false"),
                   BuildState.BuiltSeed, BuildState.WorldGenVersion,
                   *BuildState.PCGBuildHash, *BuildState.LastBuildTime.ToString());
            return;
        }

        if (!bCompatible)
        {
            UE_LOG(LogWorldGenConsole, Error,
                   TEXT("Build state mismatch: BuiltSeed=%d ConfigSeed=%d BuiltVersion=%d ConfigVersion=%d Hash=%s"),
                   BuildState.BuiltSeed, Config.Seed,
                   BuildState.WorldGenVersion, Config.WorldGenVersion,
                   *BuildState.PCGBuildHash);
            return;
        }

        UE_LOG(LogWorldGenConsole, Log,
               TEXT("Build state is valid and matches current config (Seed=%d Version=%d Hash=%s)."),
               BuildState.BuiltSeed, BuildState.WorldGenVersion,
               *BuildState.PCGBuildHash);
    })
);

static FAutoConsoleCommand CmdShowSettingsLegacy(
    TEXT("wg.ShowSettings"),
    TEXT("Display current world generation settings (legacy)"),
    FConsoleCommandDelegate::CreateStatic(&ExecShowSettings)
);

// Export per-tile performance CSV from TileStreamingService
static FAutoConsoleCommand CmdPerfExport(
    TEXT("wg.perf.export"),
    TEXT("Export per-tile perf CSV. Usage: wg.perf.export [OptionalFileName]"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        if (!IsEngineReady())
        {
            UE_LOG(LogWorldGenConsole, Warning, TEXT("Engine not ready - try after PIE starts"));
            return;
        }
        UWorld* World = GetAnyWorld();
        AWorldGenManager* Mgr = FindWorldGenManager(World);
        if (!Mgr)
        {
            UE_LOG(LogWorldGenConsole, Warning, TEXT("No AWorldGenManager found in world"));
            return;
        }
        UTileStreamingService* Streaming = Mgr->GetTileStreamingService();
        if (!Streaming)
        {
            UE_LOG(LogWorldGenConsole, Warning, TEXT("TileStreamingService unavailable"));
            return;
        }
        const FString Name = (Args.Num() > 0) ? Args[0] : TEXT("");
        const bool bOk = Streaming->ExportPerformanceCSV(Name);
        if (!bOk)
        {
            UE_LOG(LogWorldGenConsole, Error, TEXT("Export failed"));
        }
    })
);

static FAutoConsoleCommand CmdMapExport(
    TEXT("wg.map.export"),
    TEXT("Export tile height/biome debug data. Usage: wg.map.export <TileX> <TileY> [png|csv|all] [OutputDir]"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        if (Args.Num() < 2)
        {
            UE_LOG(LogWorldGenConsole, Error, TEXT("Usage: wg.map.export <TileX> <TileY> [png|csv|all] [OutputDir]"));
            return;
        }

        int32 TileX = 0;
        int32 TileY = 0;
        if (!LexTryParseString(TileX, *Args[0]) || !LexTryParseString(TileY, *Args[1]))
        {
            UE_LOG(LogWorldGenConsole, Error, TEXT("Invalid tile coordinates. Expected integers."));
            return;
        }

        const FString FormatArg = (Args.Num() > 2) ? Args[2].ToLower() : FString(TEXT("all"));
        const FString OutputArg = (Args.Num() > 3) ? Args[3] : FString();

        const bool bExportPNG = (FormatArg == TEXT("png") || FormatArg == TEXT("all"));
        const bool bExportCSV = (FormatArg == TEXT("csv") || FormatArg == TEXT("all"));

        if (!bExportPNG && !bExportCSV)
        {
            UE_LOG(LogWorldGenConsole, Error, TEXT("Unknown format '%s'. Expected png, csv, or all."), *FormatArg);
            return;
        }

        if (!IsEngineReady())
        {
            UE_LOG(LogWorldGenConsole, Warning, TEXT("Engine not ready - try after PIE starts"));
            return;
        }

        UWorld* World = GetAnyWorld();
        AWorldGenManager* Mgr = FindWorldGenManager(World);
        if (!Mgr)
        {
            UE_LOG(LogWorldGenConsole, Warning, TEXT("No AWorldGenManager found in world"));
            return;
        }

        UTileStreamingService* Streaming = Mgr->GetTileStreamingService();
        UHeightfieldService* Heightfield = Mgr->GetHeightfieldService();
        UBiomeService* BiomeService = Mgr->GetBiomeService();

        if (!Streaming || !Heightfield || !BiomeService)
        {
            UE_LOG(LogWorldGenConsole, Error, TEXT("Required services missing (Streaming=%p Height=%p Biome=%p)"),
                Streaming, Heightfield, BiomeService);
            return;
        }

        const FTileCoord Tile(TileX, TileY);
        FTileStreamingData TileData;
        if (!Streaming->GetTileData(Tile, TileData) || TileData.HeightfieldData.HeightData.Num() == 0)
        {
            UE_LOG(LogWorldGenConsole, Error, TEXT("Failed to fetch tile data for (%d, %d)"), TileX, TileY);
            return;
        }

        const FString OutputDirAbs = WorldGenConsole::ResolveExportDirectory(OutputArg, TEXT("Vibeheim/WorldGen/MapExports"));
        if (!WorldGenConsole::EnsureDirectoryExists(OutputDirAbs))
        {
            UE_LOG(LogWorldGenConsole, Error, TEXT("Unable to create export directory: %s"), *OutputDirAbs);
            return;
        }

        FString RelativeDir = OutputDirAbs;
        const bool bHasRelative = FPaths::MakePathRelativeTo(RelativeDir, *FPaths::ProjectDir());
        if (!bHasRelative)
        {
            RelativeDir = OutputDirAbs;
        }

        const FString FileBase = FString::Printf(TEXT("tile_%d_%d"), TileX, TileY);

        if (bExportPNG)
        {
            const FString RelativeBase = FPaths::Combine(RelativeDir, FileBase + TEXT(".png"));
            if (!Heightfield->ExportHeightfieldPNG(TileData.HeightfieldData, RelativeBase))
            {
                UE_LOG(LogWorldGenConsole, Error, TEXT("Failed to export heightfield PNG for tile (%d, %d)"), TileX, TileY);
            }
            if (!BiomeService->ExportBiomePNG(Tile, TileData.HeightfieldData.HeightData, RelativeBase))
            {
                UE_LOG(LogWorldGenConsole, Error, TEXT("Failed to export biome PNG for tile (%d, %d)"), TileX, TileY);
            }
        }

        if (bExportCSV)
        {
            const FString CsvPath = FPaths::Combine(OutputDirAbs, FileBase + TEXT("_map.csv"));
            const int32 Resolution = TileData.HeightfieldData.Resolution;
            const int32 SampleCount = Resolution * Resolution;

            TArray<FBiomeResult> BiomeSamples = BiomeService->GenerateTileBiomeData(Tile, TileData.HeightfieldData.HeightData);

            if (Resolution <= 0 || TileData.HeightfieldData.HeightData.Num() != SampleCount || BiomeSamples.Num() != SampleCount)
            {
                UE_LOG(LogWorldGenConsole, Error, TEXT("Tile data inconsistent for CSV export (Resolution=%d, Height=%d, Biome=%d)"),
                    Resolution, TileData.HeightfieldData.HeightData.Num(), BiomeSamples.Num());
            }
            else
            {
                FString Csv;
                Csv.Reserve(SampleCount * 32);
                Csv += TEXT("SampleX,SampleY,Height,Biome\n");

                UEnum* BiomeEnum = StaticEnum<EBiomeType>();
                for (int32 Y = 0; Y < Resolution; ++Y)
                {
                    for (int32 X = 0; X < Resolution; ++X)
                    {
                        const int32 Index = Y * Resolution + X;
                        const float HeightValue = TileData.HeightfieldData.HeightData[Index];
                        const FBiomeResult& Sample = BiomeSamples[Index];
                        const FString BiomeLabel = BiomeEnum
                            ? BiomeEnum->GetNameStringByValue(static_cast<int64>(Sample.PrimaryBiome))
                            : FString::Printf(TEXT("Biome_%d"), static_cast<int32>(Sample.PrimaryBiome));

                        Csv += FString::Printf(TEXT("%d,%d,%.2f,%s\n"), X, Y, HeightValue, *BiomeLabel);
                    }
                }

                if (FFileHelper::SaveStringToFile(Csv, *CsvPath))
                {
                    UE_LOG(LogWorldGenConsole, Log, TEXT("Exported map CSV to %s"), *CsvPath);
                }
                else
                {
                    UE_LOG(LogWorldGenConsole, Error, TEXT("Failed to write map CSV to %s"), *CsvPath);
                }
            }
        }

        UE_LOG(LogWorldGenConsole, Log, TEXT("wg.map.export completed for tile (%d, %d)"), TileX, TileY);
    })
);

static FAutoConsoleCommand CmdStreamingBudget(
    TEXT("wg.streaming.budget"),
    TEXT("Adjust streaming budget stage in milliseconds. Usage: wg.streaming.budget <total|height|biome|pcg|vhm> <ms>"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        if (Args.Num() < 2)
        {
            UE_LOG(LogWorldGenConsole, Error, TEXT("Usage: wg.streaming.budget <total|height|biome|pcg|vhm> <ms>"));
            return;
        }

        if (!IsEngineReady())
        {
            UE_LOG(LogWorldGenConsole, Warning, TEXT("Engine not ready - try after PIE starts"));
            return;
        }

        float NewBudgetMs = 0.0f;
        if (!LexTryParseString(NewBudgetMs, *Args[1]))
        {
            UE_LOG(LogWorldGenConsole, Error, TEXT("Invalid budget value '%s' - expected number of milliseconds"), *Args[1]);
            return;
        }

        UWorld* World = GetAnyWorld();
        AWorldGenManager* Mgr = FindWorldGenManager(World);
        if (!Mgr)
        {
            UE_LOG(LogWorldGenConsole, Warning, TEXT("No AWorldGenManager found in world"));
            return;
        }

        UTileStreamingService* Streaming = Mgr->GetTileStreamingService();
        if (!Streaming)
        {
            UE_LOG(LogWorldGenConsole, Warning, TEXT("TileStreamingService unavailable"));
            return;
        }

        FString Message;
        const bool bSuccess = Streaming->TryApplyStageBudget(Args[0], NewBudgetMs, Message);
        if (bSuccess)
        {
            UE_LOG(LogWorldGenConsole, Log, TEXT("%s"), *Message);
        }
        else
        {
            UE_LOG(LogWorldGenConsole, Error, TEXT("%s"), *Message);
        }
    })
);

static FAutoConsoleCommand CmdStreamingPrefetch(
    TEXT("wg.prefetch"),
    TEXT("Set streaming prefetch ring count. Usage: wg.prefetch <rings>"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        if (Args.Num() < 1)
        {
            UE_LOG(LogWorldGenConsole, Error, TEXT("Usage: wg.prefetch <rings>"));
            return;
        }

        if (!IsEngineReady())
        {
            UE_LOG(LogWorldGenConsole, Warning, TEXT("Engine not ready - try after PIE starts"));
            return;
        }

        int32 NewRings = 0;
        if (!LexTryParseString(NewRings, *Args[0]))
        {
            UE_LOG(LogWorldGenConsole, Error, TEXT("Invalid ring count '%s' - expected integer"), *Args[0]);
            return;
        }

        UWorld* World = GetAnyWorld();
        AWorldGenManager* Mgr = FindWorldGenManager(World);
        if (!Mgr)
        {
            UE_LOG(LogWorldGenConsole, Warning, TEXT("No AWorldGenManager found in world"));
            return;
        }

        UTileStreamingService* Streaming = Mgr->GetTileStreamingService();
        if (!Streaming)
        {
            UE_LOG(LogWorldGenConsole, Warning, TEXT("TileStreamingService unavailable"));
            return;
        }

        FString Message;
        const bool bSuccess = Streaming->TrySetPrefetchRings(NewRings, Message);
        if (bSuccess)
        {
            UE_LOG(LogWorldGenConsole, Log, TEXT("%s"), *Message);
        }
        else
        {
            UE_LOG(LogWorldGenConsole, Error, TEXT("%s"), *Message);
        }
    })
);

// Show feature flags
static FAutoConsoleCommand CmdFlagsShow(
    TEXT("wg.flags.show"),
    TEXT("Show feature toggles: water/rivers/rings/pcggraphs"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        if (!IsEngineReady())
        {
            UE_LOG(LogWorldGenConsole, Warning, TEXT("Engine not ready - try after PIE starts"));
            return;
        }
        const FWorldGenConfig& S = UWorldGenSettings::GetWorldGenSettings()->Settings;
        UE_LOG(LogWorldGenConsole, Log, TEXT("Water=%s Rivers=%s Rings=%s PCGGraphs=%s"),
            S.bEnableWater ? TEXT("On") : TEXT("Off"),
            S.bEnableRivers ? TEXT("On") : TEXT("Off"),
            S.bEnableRings ? TEXT("On") : TEXT("Off"),
            S.bEnablePCGGraphs ? TEXT("On") : TEXT("Off"));
    })
);

// Set feature flag: wg.flags.set <water|rivers|rings|pcggraphs> <0|1>
static FAutoConsoleCommand CmdFlagsSet(
    TEXT("wg.flags.set"),
    TEXT("Set feature flag. Usage: wg.flags.set <water|rivers|rings|pcggraphs> <0|1>"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        if (Args.Num() < 2)
        {
            UE_LOG(LogWorldGenConsole, Error, TEXT("Usage: wg.flags.set <water|rivers|rings|pcggraphs> <0|1>"));
            return;
        }
        UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
        if (!Settings)
        {
            UE_LOG(LogWorldGenConsole, Error, TEXT("WorldGenSettings not available"));
            return;
        }
        const FString Key = Args[0].ToLower();
        const bool bValue = (FCString::Atoi(*Args[1]) != 0);
        bool bHandled = true;
        if (Key == TEXT("water")) Settings->Settings.bEnableWater = bValue; else
        if (Key == TEXT("rivers")) Settings->Settings.bEnableRivers = bValue; else
        if (Key == TEXT("rings")) Settings->Settings.bEnableRings = bValue; else
        if (Key == TEXT("pcggraphs")) Settings->Settings.bEnablePCGGraphs = bValue; else
        {
            bHandled = false;
        }
        if (!bHandled)
        {
            UE_LOG(LogWorldGenConsole, Error, TEXT("Unknown flag '%s'"), *Args[0]);
            return;
        }
        UE_LOG(LogWorldGenConsole, Log, TEXT("Set '%s' = %s (takes effect for new generation)"), *Key, bValue ? TEXT("On") : TEXT("Off"));
    })
);

// Select data assets (settings/biomes) and reload
static FAutoConsoleCommand CmdSettingsSelect(
    TEXT("wg.settings.select"),
    TEXT("Select UWorldGenSettingsAsset: wg.settings.select /Game/Path/Asset.Asset"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        if (Args.Num() < 1) { UE_LOG(LogWorldGenConsole, Error, TEXT("Usage: wg.settings.select <AssetPath>")); return; }
        UWorld* World = GetAnyWorld();
        AWorldGenManager* Mgr = FindWorldGenManager(World);
        if (!Mgr) { UE_LOG(LogWorldGenConsole, Warning, TEXT("No AWorldGenManager found")); return; }
        const FString& Path = Args[0];
        FSoftObjectPath SoftPath(Path);
        if (!SoftPath.IsValid()) { UE_LOG(LogWorldGenConsole, Error, TEXT("Invalid asset path: %s"), *Path); return; }
        Mgr->WorldGenSettingsAsset = TSoftObjectPtr<UWorldGenSettingsAsset>(SoftPath);
        Mgr->ReloadWorldGenAssets();
        UE_LOG(LogWorldGenConsole, Log, TEXT("Selected WorldGenSettingsAsset: %s"), *Path);
    })
);

static FAutoConsoleCommand CmdBiomesSelect(
    TEXT("wg.biomes.select"),
    TEXT("Select UBiomeDefinitionsAsset: wg.biomes.select /Game/Path/Asset.Asset"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        if (Args.Num() < 1) { UE_LOG(LogWorldGenConsole, Error, TEXT("Usage: wg.biomes.select <AssetPath>")); return; }
        UWorld* World = GetAnyWorld();
        AWorldGenManager* Mgr = FindWorldGenManager(World);
        if (!Mgr) { UE_LOG(LogWorldGenConsole, Warning, TEXT("No AWorldGenManager found")); return; }
        const FString& Path = Args[0];
        FSoftObjectPath SoftPath(Path);
        if (!SoftPath.IsValid()) { UE_LOG(LogWorldGenConsole, Error, TEXT("Invalid asset path: %s"), *Path); return; }
        Mgr->BiomeDefinitionsAsset = TSoftObjectPtr<UBiomeDefinitionsAsset>(SoftPath);
        Mgr->ReloadWorldGenAssets();
        UE_LOG(LogWorldGenConsole, Log, TEXT("Selected BiomeDefinitionsAsset: %s"), *Path);
    })
);

static FAutoConsoleCommand CmdSettingsReload(
    TEXT("wg.settings.reload"),
    TEXT("Reload worldgen data assets (resolve soft refs and validate)"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        UWorld* World = GetAnyWorld();
        AWorldGenManager* Mgr = FindWorldGenManager(World);
        if (!Mgr) { UE_LOG(LogWorldGenConsole, Warning, TEXT("No AWorldGenManager found")); return; }
        Mgr->ReloadWorldGenAssets();
    })
);
static FAutoConsoleCommand CmdRingsValidate(
    TEXT("wg.rings.validate"),
    TEXT("Validate biome ring definitions and neighbor constraints"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        if (!IsEngineReady())
        {
            UE_LOG(LogWorldGenConsole, Warning, TEXT("Engine not ready - try after PIE starts"));
            return;
        }

        UWorld* World = GetAnyWorld();
        AWorldGenManager* Mgr = FindWorldGenManager(World);
        if (!Mgr)
        {
            UE_LOG(LogWorldGenConsole, Warning, TEXT("No AWorldGenManager found in world"));
            return;
        }

        UBiomeService* BiomeService = Mgr->GetBiomeService();
        if (!BiomeService)
        {
            UE_LOG(LogWorldGenConsole, Error, TEXT("BiomeService unavailable"));
            return;
        }

        const UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
        if (Settings && !Settings->Settings.bEnableRings)
        {
            UE_LOG(LogWorldGenConsole, Warning, TEXT("Biome rings are disabled via world settings (wg.flags rings=0)."));
        }

        const TArray<FBiomeRingDefinition>& Rings = BiomeService->GetBiomeRingDefinitions();
        if (Rings.Num() == 0)
        {
            UE_LOG(LogWorldGenConsole, Warning, TEXT("No biome ring definitions loaded."));
            return;
        }

        UEnum* BiomeEnum = StaticEnum<EBiomeType>();
        auto DescribeRing = [&](int32 Index) -> FString
        {
            const FBiomeRingDefinition& Ring = Rings[Index];
            const FString BiomeName = BiomeEnum ? BiomeEnum->GetNameStringByValue(static_cast<int64>(Ring.BiomeType)) : FString::Printf(TEXT("Biome_%d"), static_cast<int32>(Ring.BiomeType));
            return FString::Printf(TEXT("[%d] %s (Inner=%.1f Outer=%.1f Blend=%.1f)"), Index, *BiomeName, Ring.InnerRadius, Ring.OuterRadius, Ring.BlendWidth);
        };

        int32 ErrorCount = 0;
        int32 WarningCount = 0;

        auto ReportError = [&](const FString& Message)
        {
            ++ErrorCount;
            UE_LOG(LogWorldGenConsole, Error, TEXT("%s"), *Message);
        };

        auto ReportWarning = [&](const FString& Message)
        {
            ++WarningCount;
            UE_LOG(LogWorldGenConsole, Warning, TEXT("%s"), *Message);
        };

        TSet<EBiomeType> PresentBiomes;
        for (int32 Index = 0; Index < Rings.Num(); ++Index)
        {
            const FBiomeRingDefinition& Ring = Rings[Index];
            PresentBiomes.Add(Ring.BiomeType);

            if (Ring.InnerRadius < 0.0f || Ring.OuterRadius <= Ring.InnerRadius)
            {
                ReportError(FString::Printf(TEXT("%s has invalid radii"), *DescribeRing(Index)));
            }
            if (Ring.BlendWidth < 0.0f)
            {
                ReportError(FString::Printf(TEXT("%s has negative blend width"), *DescribeRing(Index)));
            }
        }

        for (int32 Index = 0; Index < Rings.Num(); ++Index)
        {
            const FBiomeRingDefinition& Ring = Rings[Index];
            for (EBiomeType Neighbor : Ring.AllowedNeighbors)
            {
                if (!PresentBiomes.Contains(Neighbor))
                {
                    const FString NeighborName = BiomeEnum ? BiomeEnum->GetNameStringByValue(static_cast<int64>(Neighbor)) : FString::Printf(TEXT("Biome_%d"), static_cast<int32>(Neighbor));
                    ReportWarning(FString::Printf(TEXT("%s lists unknown neighbor %s"), *DescribeRing(Index), *NeighborName));
                }
            }
        }

        TArray<int32> Sorted;
        Sorted.Reserve(Rings.Num());
        for (int32 i = 0; i < Rings.Num(); ++i)
        {
            Sorted.Add(i);
        }
        Sorted.Sort([&](int32 A, int32 B)
        {
            return Rings[A].InnerRadius < Rings[B].InnerRadius;
        });

        for (int32 OrderIndex = 0; OrderIndex + 1 < Sorted.Num(); ++OrderIndex)
        {
            const int32 CurrentIndex = Sorted[OrderIndex];
            const int32 NextIndex = Sorted[OrderIndex + 1];
            const FBiomeRingDefinition& CurrentRing = Rings[CurrentIndex];
            const FBiomeRingDefinition& NextRing = Rings[NextIndex];

            if (CurrentRing.OuterRadius > NextRing.InnerRadius)
            {
                ReportWarning(FString::Printf(TEXT("Ring overlap detected between %s and %s"), *DescribeRing(CurrentIndex), *DescribeRing(NextIndex)));
            }

            const float Gap = NextRing.InnerRadius - CurrentRing.OuterRadius;
            const float AllowedBlend = FMath::Max(CurrentRing.BlendWidth, NextRing.BlendWidth);
            if (Gap > AllowedBlend)
            {
                ReportWarning(FString::Printf(TEXT("Gap of %.1f exceeds blend width between %s and %s"), Gap, *DescribeRing(CurrentIndex), *DescribeRing(NextIndex)));
            }
            else if (Gap <= AllowedBlend)
            {
                const bool bForward = CurrentRing.AllowedNeighbors.Contains(NextRing.BiomeType);
                const bool bReverse = NextRing.AllowedNeighbors.Contains(CurrentRing.BiomeType);
                if (!bForward || !bReverse)
                {
                    ReportError(FString::Printf(TEXT("Neighbor permissions missing between %s and %s"), *DescribeRing(CurrentIndex), *DescribeRing(NextIndex)));
                }
            }
        }

        if (ErrorCount == 0 && WarningCount == 0)
        {
            UE_LOG(LogWorldGenConsole, Log, TEXT("Biome ring validation passed (%d rings)."), Rings.Num());
        }
        else
        {
            UE_LOG(LogWorldGenConsole, Log, TEXT("Biome ring validation complete: %d rings, %d errors, %d warnings."), Rings.Num(), ErrorCount, WarningCount);
        }
    })
);

static FAutoConsoleCommand CmdRiversExport(
    TEXT("wg.rivers.export"),
    TEXT("Export river flow data. Usage: wg.rivers.export <TileX> <TileY> [png|csv|all] [OutputDir]"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        if (Args.Num() < 2)
        {
            UE_LOG(LogWorldGenConsole, Error, TEXT("Usage: wg.rivers.export <TileX> <TileY> [png|csv|all] [OutputDir]"));
            return;
        }

        int32 TileX = 0;
        int32 TileY = 0;
        if (!LexTryParseString(TileX, *Args[0]) || !LexTryParseString(TileY, *Args[1]))
        {
            UE_LOG(LogWorldGenConsole, Error, TEXT("Invalid tile coordinates. Expected integers."));
            return;
        }

        const FString FormatArg = (Args.Num() > 2) ? Args[2].ToLower() : FString(TEXT("all"));
        const FString OutputArg = (Args.Num() > 3) ? Args[3] : FString();

        const bool bExportPNG = (FormatArg == TEXT("png") || FormatArg == TEXT("all"));
        const bool bExportCSV = (FormatArg == TEXT("csv") || FormatArg == TEXT("all"));

        if (!bExportPNG && !bExportCSV)
        {
            UE_LOG(LogWorldGenConsole, Error, TEXT("Unknown format '%s'. Expected png, csv, or all."), *FormatArg);
            return;
        }

        if (!IsEngineReady())
        {
            UE_LOG(LogWorldGenConsole, Warning, TEXT("Engine not ready - try after PIE starts"));
            return;
        }

        UWorld* World = GetAnyWorld();
        AWorldGenManager* Mgr = FindWorldGenManager(World);
        if (!Mgr)
        {
            UE_LOG(LogWorldGenConsole, Warning, TEXT("No AWorldGenManager found in world"));
            return;
        }

        UTileStreamingService* Streaming = Mgr->GetTileStreamingService();
        URiverFlowService* RiverService = Mgr->GetRiverFlowService();
        if (!Streaming || !RiverService)
        {
            UE_LOG(LogWorldGenConsole, Error, TEXT("River export requires TileStreamingService and RiverFlowService"));
            return;
        }

        const FTileCoord Tile(TileX, TileY);
        FTileStreamingData TileData;
        if (!Streaming->GetTileData(Tile, TileData))
        {
            UE_LOG(LogWorldGenConsole, Error, TEXT("Failed to fetch tile data for (%d, %d)"), TileX, TileY);
            return;
        }

        FRiverFlowTileData FlowData = TileData.RiverFlowData;
        if (!FlowData.IsValid())
        {
            if (!RiverService->ComputeFlowMap(Tile, TileData.HeightfieldData, FlowData) || !FlowData.IsValid())
            {
                UE_LOG(LogWorldGenConsole, Error, TEXT("River flow data unavailable for tile (%d, %d)"), TileX, TileY);
                return;
            }
        }

        const FString OutputDirAbs = WorldGenConsole::ResolveExportDirectory(OutputArg, TEXT("Vibeheim/WorldGen/RiverExports"));
        if (!WorldGenConsole::EnsureDirectoryExists(OutputDirAbs))
        {
            UE_LOG(LogWorldGenConsole, Error, TEXT("Unable to create export directory: %s"), *OutputDirAbs);
            return;
        }

        FString RelativeDir = OutputDirAbs;
        const bool bHasRelative = FPaths::MakePathRelativeTo(RelativeDir, *FPaths::ProjectDir());
        if (!bHasRelative)
        {
            RelativeDir = OutputDirAbs;
        }

        const FString FileBase = FString::Printf(TEXT("tile_%d_%d"), TileX, TileY);

        if (bExportPNG)
        {
            const FString PngPath = FPaths::Combine(OutputDirAbs, FileBase + TEXT("_accum.png"));
            const int32 Resolution = FlowData.Resolution;
            const int32 SampleCount = Resolution * Resolution;

            if (Resolution <= 0 || FlowData.FlowAccumulation.Num() != SampleCount)
            {
                UE_LOG(LogWorldGenConsole, Error, TEXT("Invalid flow accumulation data for tile (%d, %d)"), TileX, TileY);
            }
            else
            {
                float MinAccum = FlowData.MinAccumulation;
                float MaxAccum = FlowData.MaxAccumulation;
                if (!FMath::IsFinite(MinAccum) || !FMath::IsFinite(MaxAccum) || MaxAccum <= MinAccum)
                {
                    MinAccum = TNumericLimits<float>::Max();
                    MaxAccum = -TNumericLimits<float>::Max();
                    for (float Value : FlowData.FlowAccumulation)
                    {
                        MinAccum = FMath::Min(MinAccum, Value);
                        MaxAccum = FMath::Max(MaxAccum, Value);
                    }
                    if (!FMath::IsFinite(MinAccum) || !FMath::IsFinite(MaxAccum) || MaxAccum <= MinAccum)
                    {
                        MinAccum = 0.0f;
                        MaxAccum = 1.0f;
                    }
                }

                const float Range = FMath::Max(MaxAccum - MinAccum, KINDA_SMALL_NUMBER);
                TArray<uint8> Pixels;
                Pixels.Reserve(SampleCount);
                for (float Value : FlowData.FlowAccumulation)
                {
                    const float Normalized = FMath::Clamp((Value - MinAccum) / Range, 0.0f, 1.0f);
                    Pixels.Add(static_cast<uint8>(Normalized * 255.0f));
                }

                if (WorldGenConsole::SaveGrayscalePNG(PngPath, Pixels, Resolution, Resolution))
                {
                    UE_LOG(LogWorldGenConsole, Log, TEXT("Exported river accumulation PNG to %s"), *PngPath);
                }
                else
                {
                    UE_LOG(LogWorldGenConsole, Error, TEXT("Failed to export river accumulation PNG to %s"), *PngPath);
                }
            }
        }

        if (bExportCSV)
        {
            const FString CsvPath = FPaths::Combine(OutputDirAbs, FileBase + TEXT("_flow.csv"));
            const int32 Resolution = FlowData.Resolution;
            const int32 SampleCount = Resolution * Resolution;

            if (Resolution <= 0 || FlowData.FlowDirections.Num() != SampleCount || FlowData.FlowAccumulation.Num() != SampleCount)
            {
                UE_LOG(LogWorldGenConsole, Error, TEXT("River flow data inconsistent for tile (%d, %d)"), TileX, TileY);
            }
            else
            {
                FString Csv;
                Csv.Reserve(SampleCount * 48);
                Csv += TEXT("SampleX,SampleY,DirX,DirY,Accumulation,DownstreamX,DownstreamY\n");

                for (int32 Y = 0; Y < Resolution; ++Y)
                {
                    for (int32 X = 0; X < Resolution; ++X)
                    {
                        const int32 Index = Y * Resolution + X;
                        const FVector2D Dir = FlowData.FlowDirections[Index];
                        const float Accum = FlowData.FlowAccumulation[Index];
                        const int32 DownstreamIndex = FlowData.DownstreamIndices.IsValidIndex(Index) ? FlowData.DownstreamIndices[Index] : INDEX_NONE;
                        const int32 DownstreamX = (DownstreamIndex != INDEX_NONE) ? (DownstreamIndex % Resolution) : -1;
                        const int32 DownstreamY = (DownstreamIndex != INDEX_NONE) ? (DownstreamIndex / Resolution) : -1;

                        Csv += FString::Printf(TEXT("%d,%d,%.3f,%.3f,%.3f,%d,%d\n"), X, Y, Dir.X, Dir.Y, Accum, DownstreamX, DownstreamY);
                    }
                }

                if (FFileHelper::SaveStringToFile(Csv, *CsvPath))
                {
                    UE_LOG(LogWorldGenConsole, Log, TEXT("Exported river flow CSV to %s"), *CsvPath);
                }
                else
                {
                    UE_LOG(LogWorldGenConsole, Error, TEXT("Failed to write river flow CSV to %s"), *CsvPath);
                }
            }
        }

        UE_LOG(LogWorldGenConsole, Log, TEXT("wg.rivers.export completed for tile (%d, %d)"), TileX, TileY);
    })
);

static FAutoConsoleCommand CmdPOIValidate(
    TEXT("wg.poi.validate"),
    TEXT("Validate POI placement rules for currently loaded data"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        if (!IsEngineReady())
        {
            UE_LOG(LogWorldGenConsole, Warning, TEXT("Engine not ready - try after PIE starts"));
            return;
        }

        UWorld* World = GetAnyWorld();
        AWorldGenManager* Mgr = FindWorldGenManager(World);
        if (!Mgr)
        {
            UE_LOG(LogWorldGenConsole, Warning, TEXT("No AWorldGenManager found in world"));
            return;
        }

        UPOIService* POIService = Mgr->GetPOIService();
        if (!POIService)
        {
            UE_LOG(LogWorldGenConsole, Error, TEXT("POIService unavailable"));
            return;
        }

        TArray<FString> Errors;
        TArray<FString> Warnings;
        int32 TotalPOIs = 0;
        POIService->ValidateCurrentPOIs(Errors, Warnings, TotalPOIs);

        for (const FString& Error : Errors)
        {
            UE_LOG(LogWorldGenConsole, Error, TEXT("%s"), *Error);
        }
        for (const FString& Warning : Warnings)
        {
            UE_LOG(LogWorldGenConsole, Warning, TEXT("%s"), *Warning);
        }

        if (Errors.Num() == 0 && Warnings.Num() == 0)
        {
            UE_LOG(LogWorldGenConsole, Log, TEXT("POI validation passed (%d POIs)."), TotalPOIs);
        }
        else
        {
            UE_LOG(LogWorldGenConsole, Log, TEXT("POI validation complete: %d POIs, %d errors, %d warnings."), TotalPOIs, Errors.Num(), Warnings.Num());
        }
    })
);

static FAutoConsoleCommand CmdContinuityValidate(
    TEXT("wg.validate.continuity"),
    TEXT("Validate cross-tile continuity (rivers, shoreline, biome rings). Usage: wg.validate.continuity [riverScale=1.0] [shoreTolerance=2]"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        if (!IsEngineReady())
        {
            UE_LOG(LogWorldGenConsole, Warning, TEXT("Engine not ready - try after PIE starts"));
            return;
        }

        UWorld* World = GetAnyWorld();
        AWorldGenManager* Mgr = FindWorldGenManager(World);
        if (!Mgr)
        {
            UE_LOG(LogWorldGenConsole, Warning, TEXT("No AWorldGenManager found in world"));
            return;
        }

        UTileStreamingService* Streaming = Mgr->GetTileStreamingService();
        if (!Streaming)
        {
            UE_LOG(LogWorldGenConsole, Error, TEXT("TileStreamingService unavailable"));
            return;
        }

        float RiverScale = 1.0f;
        int32 ShoreTolerance = 2;
        if (Args.Num() >= 1)
        {
            RiverScale = FMath::Max(0.01f, static_cast<float>(FCString::Atof(*Args[0])));
        }
        if (Args.Num() >= 2)
        {
            ShoreTolerance = FMath::Max(0, FCString::Atoi(*Args[1]));
        }

        const FContinuityValidationResult Result = Streaming->ValidateContinuity(false, RiverScale, ShoreTolerance);

        UE_LOG(LogWorldGenConsole, Log,
            TEXT("Continuity validation: Edges=%d River failures=%d/%d Shoreline failures=%d/%d Biome failures=%d/%d"),
            Result.EdgesEvaluated,
            Result.RiverEdgesFailed, Result.RiverEdgesChecked,
            Result.ShorelineEdgesFailed, Result.ShorelineEdgesChecked,
            Result.BiomeEdgesFailed, Result.BiomeEdgesChecked);

        if (!Result.bAllContinuitySatisfied)
        {
            for (const FTileEdgeValidationIssue& Issue : Result.Issues)
            {
                UE_LOG(LogWorldGenConsole, Warning, TEXT("[%s] Tile (%d,%d) <-> (%d,%d): %s"),
                    *Issue.Category,
                    Issue.TileA.X, Issue.TileA.Y,
                    Issue.TileB.X, Issue.TileB.Y,
                    *Issue.Details);
            }
        }
        else
        {
            UE_LOG(LogWorldGenConsole, Log, TEXT("Continuity validation passed with no issues."));
        }
    })
);

static FAutoConsoleCommand CmdPerfValidate(
    TEXT("wg.perf.validate"),
    TEXT("Validate in-memory generation performance against targets. Usage: wg.perf.validate [p50=10] [p95=20] [spike=8]"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        if (!IsEngineReady())
        {
            UE_LOG(LogWorldGenConsole, Warning, TEXT("Engine not ready - try after PIE starts"));
            return;
        }

        UWorld* World = GetAnyWorld();
        AWorldGenManager* Mgr = FindWorldGenManager(World);
        if (!Mgr)
        {
            UE_LOG(LogWorldGenConsole, Warning, TEXT("No AWorldGenManager found in world"));
            return;
        }

        UTileStreamingService* Streaming = Mgr->GetTileStreamingService();
        if (!Streaming)
        {
            UE_LOG(LogWorldGenConsole, Error, TEXT("TileStreamingService unavailable"));
            return;
        }

        float TargetP50 = 10.0f;
        float TargetP95 = 20.0f;
        float SpikeThreshold = 8.0f;
        if (Args.Num() >= 1)
        {
            TargetP50 = FMath::Max(0.0f, static_cast<float>(FCString::Atof(*Args[0])));
        }
        if (Args.Num() >= 2)
        {
            TargetP95 = FMath::Max(TargetP50, static_cast<float>(FCString::Atof(*Args[1])));
        }
        if (Args.Num() >= 3)
        {
            SpikeThreshold = FMath::Max(0.0f, static_cast<float>(FCString::Atof(*Args[2])));
        }

        const FPerformanceValidationResult Result = Streaming->ValidatePerformanceTargets(false, TargetP50, TargetP95, SpikeThreshold);

        UE_LOG(LogWorldGenConsole, Log,
            TEXT("Performance validation: Samples=%d p50=%.2fms (<=%.2f) p95=%.2fms (<=%.2f) MaxSpike=%.2fms (<=%.2f)"),
            Result.SampleCount,
            Result.ObservedP50Ms, Result.TargetP50Ms,
            Result.ObservedP95Ms, Result.TargetP95Ms,
            Result.ObservedMaxSpikeMs, Result.SpikeThresholdMs);

        if (!Result.bWithinTargets)
        {
            for (const FString& Msg : Result.Messages)
            {
                UE_LOG(LogWorldGenConsole, Warning, TEXT("%s"), *Msg);
            }
            UE_LOG(LogWorldGenConsole, Error, TEXT("Performance validation FAILED."));
        }
        else
        {
            UE_LOG(LogWorldGenConsole, Log, TEXT("Performance validation passed."));
        }
    })
);

static FAutoConsoleCommand CmdMemoryReport(
    TEXT("wg.memory.report"),
    TEXT("Report heightfield texture memory usage and validate against budget. Usage: wg.memory.report [budgetMB=512]"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        if (!IsEngineReady())
        {
            UE_LOG(LogWorldGenConsole, Warning, TEXT("Engine not ready - try after PIE starts"));
            return;
        }

        UWorld* World = GetAnyWorld();
        AWorldGenManager* Mgr = FindWorldGenManager(World);
        if (!Mgr)
        {
            UE_LOG(LogWorldGenConsole, Warning, TEXT("No AWorldGenManager found in world"));
            return;
        }

        float BudgetMB = 512.0f;
        if (Args.Num() >= 1)
        {
            BudgetMB = FMath::Max(1.0f, static_cast<float>(FCString::Atof(*Args[0])));
        }

        UVHMTerrainRenderer* Renderer = Mgr->GetVHMTerrainRenderer();
        if (!Renderer)
        {
            UE_LOG(LogWorldGenConsole, Error, TEXT("VHMTerrainRenderer unavailable"));
            return;
        }

        const FVHMPerformanceStats Stats = Renderer->GetPerformanceStats();
        const bool bWithinBudget = Stats.TextureMemoryUsageMB <= BudgetMB + KINDA_SMALL_NUMBER;

        UTileStreamingService* Streaming = Mgr->GetTileStreamingService();
        const FTileStreamingMetrics StreamingMetrics = Streaming ? Streaming->GetPerformanceMetrics() : FTileStreamingMetrics();

        UE_LOG(LogWorldGenConsole, Log,
            TEXT("Texture memory usage: %.2f MB (budget %.2f MB) ActiveVHM=%d LoadedTiles=%d ActiveTiles=%d"),
            Stats.TextureMemoryUsageMB,
            BudgetMB,
            Stats.ActiveVHMComponents,
            StreamingMetrics.LoadedTiles,
            StreamingMetrics.ActiveTiles);

        if (!bWithinBudget)
        {
            UE_LOG(LogWorldGenConsole, Error, TEXT("Texture memory validation FAILED (usage exceeds budget)."));
        }
        else
        {
            UE_LOG(LogWorldGenConsole, Log, TEXT("Texture memory validation passed."));
        }
    })
);
