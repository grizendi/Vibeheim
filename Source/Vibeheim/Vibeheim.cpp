// Copyright Epic Games, Inc. All Rights Reserved.

#include "Vibeheim.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "Blueprint/UserWidget.h"
#include "Editor.h"
#include "Editor/WorldGenBuildUtility.h"
#include "Editor/WorldGenBuilderWidget.h"
#include "Framework/Docking/TabManager.h"
#include "LevelEditor.h"
#include "Styling/AppStyle.h"
#include "Templates/SharedPointer.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"
#endif

static_assert(ENGINE_MAJOR_VERSION == 5 && (ENGINE_MINOR_VERSION == 6 || ENGINE_MINOR_VERSION == 7),
              "Vibeheim requires Unreal Engine 5.6.x or 5.7.x");

#if WITH_EDITOR
#define LOCTEXT_NAMESPACE "FVibeheimModule"
#endif

class FVibeheimModule : public FDefaultGameModuleImpl
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
#if WITH_EDITOR
    void RegisterMenus();
    void ExecuteRebuildWorld();
    void OpenWorldBuildPipeline();
    TSharedRef<SDockTab> SpawnWorldBuildTab(const FSpawnTabArgs& Args);
    void HandleTabClosed(TSharedRef<SDockTab> ClosedTab);
    void HandleMapChanged(UWorld* World, EMapChangeType MapChangeType);
    TSharedRef<SWidget> CreateWorldBuildWidget();

    TStrongObjectPtr<UWorldGenBuilderWidget> BuilderWidget;
    FDelegateHandle MenuStartupHandle;
    FDelegateHandle MapChangedHandle;
    TWeakPtr<SDockTab> WorldBuildTab;
#endif
};

IMPLEMENT_PRIMARY_GAME_MODULE(FVibeheimModule, Vibeheim, "Vibeheim");

#if WITH_EDITOR
static const FName WorldBuildTabId(TEXT("VibeheimWorldBuildPipeline"));

void FVibeheimModule::StartupModule()
{
    FDefaultGameModuleImpl::StartupModule();

    if (UToolMenus::IsToolMenuUIEnabled())
    {
        MenuStartupHandle = UToolMenus::RegisterStartupCallback(
            FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FVibeheimModule::RegisterMenus));
    }

    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        WorldBuildTabId,
        FOnSpawnTab::CreateRaw(this, &FVibeheimModule::SpawnWorldBuildTab))
        .SetDisplayName(LOCTEXT("WorldBuildTabTitle", "World Build Pipeline"))
        .SetTooltipText(LOCTEXT("WorldBuildTabTooltip", "Open the Vibeheim world build pipeline utility"))
        .SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Build"));
}

void FVibeheimModule::ShutdownModule()
{
    BuilderWidget.Reset();
    if (MapChangedHandle.IsValid())
    {
        if (FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor"))
        {
            LevelEditorModule->OnMapChanged().Remove(MapChangedHandle);
        }
        MapChangedHandle.Reset();
    }

    if (UToolMenus::IsToolMenuUIEnabled())
    {
        UToolMenus::UnRegisterStartupCallback(MenuStartupHandle);
        UToolMenus::UnregisterOwner(this);
    }

    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(WorldBuildTabId);

    FDefaultGameModuleImpl::ShutdownModule();
}

void FVibeheimModule::RegisterMenus()
{
    FToolMenuOwnerScoped OwnerScoped(this);

    if (UToolMenu* BuildMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Build"))
    {
        FToolMenuSection& Section = BuildMenu->AddSection("VibeheimBuild", LOCTEXT("VibeheimBuildSection", "Vibeheim"));
        Section.AddMenuEntry(
            "RebuildVibeheimWorld",
            LOCTEXT("RebuildWorldLabel", "Rebuild Vibeheim World"),
            LOCTEXT("RebuildWorldTooltip", "Run the Vibeheim world build pipeline (terrain + PCG) using the current settings."),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateRaw(this, &FVibeheimModule::ExecuteRebuildWorld)));
    }

    if (UToolMenu* WindowMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window"))
    {
        FToolMenuSection& Section = WindowMenu->AddSection("VibeheimWindow", LOCTEXT("VibeheimWindowSection", "Vibeheim"));
        Section.AddMenuEntry(
            "WorldBuildPipeline",
            LOCTEXT("WorldBuildPipelineLabel", "World Build Pipeline"),
            LOCTEXT("WorldBuildPipelineTooltip", "Open the Vibeheim world build pipeline utility widget."),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateRaw(this, &FVibeheimModule::OpenWorldBuildPipeline)));
    }
}

void FVibeheimModule::ExecuteRebuildWorld()
{
    const bool bLaunched = UWorldGenBuildUtility::BuildWorldFromSeed(0);
    if (!bLaunched)
    {
        UE_LOG(LogTemp, Error, TEXT("Rebuild Vibeheim World failed to launch. Check editor log for details."));
    }
}

void FVibeheimModule::OpenWorldBuildPipeline()
{
    FGlobalTabmanager::Get()->TryInvokeTab(WorldBuildTabId);
}

TSharedRef<SDockTab> FVibeheimModule::SpawnWorldBuildTab(const FSpawnTabArgs& Args)
{
    const TSharedRef<SWidget> Content = CreateWorldBuildWidget();

    TSharedRef<SDockTab> NewTab = SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            Content
        ];

    NewTab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateRaw(this, &FVibeheimModule::HandleTabClosed));
    WorldBuildTab = NewTab;

    if (!MapChangedHandle.IsValid())
    {
        FLevelEditorModule& LevelEditor = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
        MapChangedHandle = LevelEditor.OnMapChanged().AddRaw(this, &FVibeheimModule::HandleMapChanged);
    }
    return NewTab;
}

void FVibeheimModule::HandleTabClosed(TSharedRef<SDockTab> ClosedTab)
{
    BuilderWidget.Reset();
    WorldBuildTab.Reset();
}

TSharedRef<SWidget> FVibeheimModule::CreateWorldBuildWidget()
{
    UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    UWorldGenBuilderWidget* WidgetInstance = EditorWorld
                                                 ? CreateWidget<UWorldGenBuilderWidget>(EditorWorld, UWorldGenBuilderWidget::StaticClass())
                                                 : nullptr;

    if (!WidgetInstance)
    {
        BuilderWidget.Reset();
        return SNew(STextBlock)
            .Text(LOCTEXT("WidgetCreateFailed", "Failed to create World Build Pipeline widget."));
    }

    BuilderWidget.Reset();
    BuilderWidget = TStrongObjectPtr<UWorldGenBuilderWidget>(WidgetInstance);
    return WidgetInstance->TakeWidget();
}

void FVibeheimModule::HandleMapChanged(UWorld* World, EMapChangeType MapChangeType)
{
    if (MapChangeType == EMapChangeType::TearDownWorld)
    {
        if (BuilderWidget.IsValid() && BuilderWidget->GetWorld() == World)
        {
            if (TSharedPtr<SDockTab> Tab = WorldBuildTab.Pin())
            {
                Tab->SetContent(SNullWidget::NullWidget);
            }

            BuilderWidget->Rename(nullptr, GetTransientPackage(), REN_DoNotDirty);
            BuilderWidget.Reset();
        }
        return;
    }

    if (MapChangeType == EMapChangeType::SaveMap)
    {
        return;
    }

    if (TSharedPtr<SDockTab> Tab = WorldBuildTab.Pin())
    {
        Tab->SetContent(CreateWorldBuildWidget());
    }
}
#endif // WITH_EDITOR

#if WITH_EDITOR
#undef LOCTEXT_NAMESPACE
#endif
