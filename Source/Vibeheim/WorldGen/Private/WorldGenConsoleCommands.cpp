#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "WorldGenSettings.h"
#include "Data/WorldGenAssets.h"
#include "WorldGenManager.h"
#include "Services/TileStreamingService.h"

DEFINE_LOG_CATEGORY_STATIC(LogWorldGenConsole, Log, All);

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
