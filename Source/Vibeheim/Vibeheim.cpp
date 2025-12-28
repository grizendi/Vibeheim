// Copyright Epic Games, Inc. All Rights Reserved.

#include "Vibeheim.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "Blueprint/UserWidget.h"
#include "Editor.h"
#include "Editor/WorldGenBuildUtility.h"
#include "Editor/WorldGenBuilderWidget.h"
#include "Framework/Docking/TabManager.h"
#include "Styling/AppStyle.h"
#include "Templates/SharedPointer.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SBoxPanel.h"
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

    TStrongObjectPtr<UWorldGenBuilderWidget> BuilderWidget;
    FDelegateHandle MenuStartupHandle;
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
    UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;

    UWorldGenBuilderWidget* WidgetInstance = nullptr;
    if (EditorWorld)
    {
        WidgetInstance = CreateWidget<UWorldGenBuilderWidget>(EditorWorld, UWorldGenBuilderWidget::StaticClass());
    }
    if (!WidgetInstance)
    {
        WidgetInstance = NewObject<UWorldGenBuilderWidget>();
    }

    BuilderWidget.Reset();
    BuilderWidget = TStrongObjectPtr<UWorldGenBuilderWidget>(WidgetInstance);

    const TSharedRef<SWidget> Content = WidgetInstance
                                            ? WidgetInstance->TakeWidget()
                                            : SNew(STextBlock)
                                            .Text(LOCTEXT("WidgetCreateFailed", "Failed to create World Build Pipeline widget."));

    TSharedRef<SDockTab> NewTab = SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            Content
        ];

    NewTab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateRaw(this, &FVibeheimModule::HandleTabClosed));
    return NewTab;
}

void FVibeheimModule::HandleTabClosed(TSharedRef<SDockTab> ClosedTab)
{
    BuilderWidget.Reset();
}
#endif // WITH_EDITOR

#if WITH_EDITOR
#undef LOCTEXT_NAMESPACE
#endif
