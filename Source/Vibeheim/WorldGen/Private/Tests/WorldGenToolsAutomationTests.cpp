#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"

#include "Editor.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/IConsoleManager.h"
#include "ToolMenus.h"
#include "UObject/Package.h"

#include "Data/WorldGenBuildState.h"
#include "Data/WorldGenTerrainResource.h"
#include "Editor/WorldGenBuildUtility.h"
#include "WorldGenSettings.h"

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

FString MakeAssetPath(const FString& MapIdentifier, const FString& Suffix)
{
    return FString::Printf(TEXT("/Game/WorldGen/Baked/%s_%s.%s_%s"),
                           *MapIdentifier, *Suffix, *MapIdentifier, *Suffix);
}

struct FScopedWorldGenConfig
{
    UWorldGenSettings* Settings = nullptr;
    FWorldGenConfig Original;

    explicit FScopedWorldGenConfig(UWorldGenSettings* InSettings)
        : Settings(InSettings),
          Original(InSettings ? InSettings->Settings : FWorldGenConfig())
    {
    }

    ~FScopedWorldGenConfig()
    {
        if (Settings)
        {
            Settings->Settings = Original;
        }
    }
};

bool RunConsoleCommandChecks(FAutomationTestBase& Test)
{
    IConsoleManager& Console = IConsoleManager::Get();
    const TCHAR* Commands[] = {
        TEXT("wg.build.world"),
        TEXT("wg.build.terrain"),
        TEXT("wg.build.pcg"),
        TEXT("wg.build.status"),
        TEXT("wg.build.validate"),
    };

    bool bOk = true;
    for (const TCHAR* Command : Commands)
    {
        if (!Console.FindConsoleObject(Command))
        {
            Test.AddError(FString::Printf(TEXT("Missing console command: %s"), Command));
            bOk = false;
        }
    }

    return bOk;
}

bool RunMenuChecks(FAutomationTestBase& Test)
{
    if (!UToolMenus::IsToolMenuUIEnabled())
    {
        Test.AddWarning(TEXT("ToolMenus UI is disabled; skipping menu entry validation."));
        return true;
    }

    UToolMenus* ToolMenus = UToolMenus::Get();
    if (!ToolMenus)
    {
        Test.AddError(TEXT("ToolMenus subsystem unavailable."));
        return false;
    }

    bool bOk = true;
    UToolMenu* BuildMenu = ToolMenus->FindMenu(TEXT("LevelEditor.MainMenu.Build"));
    if (!BuildMenu)
    {
        Test.AddError(TEXT("LevelEditor.MainMenu.Build menu not found."));
        bOk = false;
    }
    else if (!BuildMenu->ContainsEntry(TEXT("RebuildVibeheimWorld")))
    {
        Test.AddError(TEXT("Menu entry RebuildVibeheimWorld not registered."));
        bOk = false;
    }

    UToolMenu* WindowMenu = ToolMenus->FindMenu(TEXT("LevelEditor.MainMenu.Window"));
    if (!WindowMenu)
    {
        Test.AddError(TEXT("LevelEditor.MainMenu.Window menu not found."));
        bOk = false;
    }
    else if (!WindowMenu->ContainsEntry(TEXT("WorldBuildPipeline")))
    {
        Test.AddError(TEXT("Menu entry WorldBuildPipeline not registered."));
        bOk = false;
    }

    if (!FGlobalTabmanager::Get()->HasTabSpawner(TEXT("VibeheimWorldBuildPipeline")))
    {
        Test.AddError(TEXT("World Build Pipeline tab spawner not registered."));
        bOk = false;
    }

    return bOk;
}

bool RunBuildPipelineChecks(FAutomationTestBase& Test)
{
    if (!GEditor)
    {
        Test.AddError(TEXT("GEditor is unavailable; cannot run build pipeline test."));
        return false;
    }

    UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
    if (!EditorWorld)
    {
        Test.AddError(TEXT("No editor world loaded; open a map before running this test."));
        return false;
    }

    UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
    if (!Settings)
    {
        Test.AddError(TEXT("WorldGenSettings unavailable; cannot run build pipeline test."));
        return false;
    }

    FScopedWorldGenConfig ScopedSettings(Settings);

    Settings->Settings.GenerateRadius = 1;
    Settings->Settings.LoadRadius = 1;
    Settings->Settings.ActiveRadius = 1;
    Settings->Settings.HeightfieldResolution = 64;
    Settings->Settings.WorldGenVersion = FMath::Max(Settings->Settings.WorldGenVersion, 1);

    const int32 SeedOverride = 4242;
    const int32 ExpectedSeed = SeedOverride != 0 ? SeedOverride : Settings->Settings.Seed;
    const int32 ExpectedVersion = Settings->Settings.WorldGenVersion;

    const bool bSuccess = UWorldGenBuildUtility::BuildWorldFromSeed(SeedOverride);
    Test.TestTrue(TEXT("Build pipeline should succeed"), bSuccess);

    const FString MapIdentifier = SanitizeIdentifier(EditorWorld->GetMapName());
    const FString TerrainPath = MakeAssetPath(MapIdentifier, TEXT("TerrainData"));
    const FString BuildStatePath = MakeAssetPath(MapIdentifier, TEXT("BuildState"));

    UWorldGenTerrainResource* TerrainResource =
        LoadObject<UWorldGenTerrainResource>(nullptr, *TerrainPath);
    Test.TestNotNull(TEXT("Terrain resource asset should exist"), TerrainResource);
    if (TerrainResource)
    {
        Test.TestTrue(TEXT("Terrain resource has prebaked height textures"),
                      TerrainResource->HeightTextures.Num() > 0);
        if (UPackage* Package = TerrainResource->GetOutermost())
        {
            Test.TestTrue(TEXT("Terrain resource package is dirty"), Package->IsDirty());
        }
    }

    UWorldGenBuildStateAsset* BuildStateAsset =
        LoadObject<UWorldGenBuildStateAsset>(nullptr, *BuildStatePath);
    Test.TestNotNull(TEXT("Build state asset should exist"), BuildStateAsset);
    if (BuildStateAsset)
    {
        const FWorldBuildState& BuildState = BuildStateAsset->BuildState;
        Test.TestTrue(TEXT("Build state is valid"), BuildState.IsValid());
        Test.TestEqual(TEXT("Build state seed matches"), BuildState.BuiltSeed, ExpectedSeed);
        Test.TestEqual(TEXT("Build state version matches"), BuildState.WorldGenVersion, ExpectedVersion);
        Test.TestTrue(TEXT("Build state PCG hash is set"), !BuildState.PCGBuildHash.IsEmpty());
        Test.TestNotEqual(TEXT("PCG hash should not be PCG_FAILED"),
                          BuildState.PCGBuildHash, FString(TEXT("PCG_FAILED")));

        if (UPackage* Package = BuildStateAsset->GetOutermost())
        {
            Test.TestTrue(TEXT("Build state package is dirty"), Package->IsDirty());
        }
    }

    return bSuccess;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWorldGenToolsConsoleCommandTest,
    "Vibeheim.WorldGen.Tools.ConsoleCommands.Registered",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWorldGenToolsConsoleCommandTest::RunTest(const FString& Parameters)
{
    return RunConsoleCommandChecks(*this);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWorldGenToolsMenuEntriesTest,
    "Vibeheim.WorldGen.Tools.MenuEntries.Registered",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWorldGenToolsMenuEntriesTest::RunTest(const FString& Parameters)
{
    return RunMenuChecks(*this);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWorldGenToolsBuildPipelineTest,
    "Vibeheim.WorldGen.Tools.BuildPipeline.FullBuild",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWorldGenToolsBuildPipelineTest::RunTest(const FString& Parameters)
{
    return RunBuildPipelineChecks(*this);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWorldGenToolsCheckpointTest,
    "Vibeheim.WorldGen.Tools.Checkpoint",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWorldGenToolsCheckpointTest::RunTest(const FString& Parameters)
{
    bool bOk = true;
    bOk &= RunConsoleCommandChecks(*this);
    bOk &= RunMenuChecks(*this);
    bOk &= RunBuildPipelineChecks(*this);
    return bOk;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
