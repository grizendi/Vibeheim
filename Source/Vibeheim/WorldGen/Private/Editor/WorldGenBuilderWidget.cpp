#include "Editor/WorldGenBuilderWidget.h"

#if WITH_EDITOR

#include "Data/WorldGenBuildState.h"
#include "Editor.h"
#include "Editor/WorldGenBuildUtility.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "WorldGenManager.h"
#include "WorldGenSettings.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "WorldGenBuilderWidget"

namespace WorldGenBuilderWidget
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

FString MakeBuildStateObjectPath(const FString& MapIdentifier)
{
    return FString::Printf(TEXT("/Game/WorldGen/Baked/%s_BuildState.%s_BuildState"),
                           *MapIdentifier, *MapIdentifier);
}
} // namespace WorldGenBuilderWidget

TSharedRef<SWidget> UWorldGenBuilderWidget::RebuildWidget()
{
    const FText HeaderText = LOCTEXT("Header", "World Build Pipeline");
    const FText SeedLabel = LOCTEXT("SeedLabel", "Seed Override (optional)");
    const FText MapLabel = LOCTEXT("MapLabel", "Map Path (optional)");

    const TSharedRef<SWidget> Widget = SNew(SBorder)
        .Padding(8.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(STextBlock)
                .Text(HeaderText)
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 6.0f))
            [
                SAssignNew(ModeText, STextBlock)
                .Text(LOCTEXT("ModeLoading", "Mode: (loading)"))
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SAssignNew(BuildStateText, STextBlock)
                .Text(LOCTEXT("BuildStateLoading", "Build State: (loading)"))
                .AutoWrapText(true)
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 6.0f))
            [
                SNew(SSeparator)
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(FMargin(0.0f, 2.0f))
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Text(SeedLabel)
                    ]
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .Padding(FMargin(8.0f, 0.0f))
                    [
                        SAssignNew(SeedTextBox, SEditableTextBox)
                        .HintText(LOCTEXT("SeedHint", "Blank = use settings seed"))
                    ]
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(FMargin(0.0f, 2.0f))
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Text(MapLabel)
                    ]
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .Padding(FMargin(8.0f, 0.0f))
                    [
                        SAssignNew(MapPathTextBox, SEditableTextBox)
                        .HintText(LOCTEXT("MapHint", "/Game/Maps/VibeheimMain"))
                    ]
                ]
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 8.0f))
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FMargin(0.0f, 0.0f, 8.0f, 0.0f))
                [
                    SNew(SButton)
                    .Text(LOCTEXT("BuildWorld", "Build World"))
                    .OnClicked_Lambda([this]()
                    {
                        StartBuild(/*bBuildTerrain=*/true, /*bBuildPCG=*/true);
                        return FReply::Handled();
                    })
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FMargin(0.0f, 0.0f, 8.0f, 0.0f))
                [
                    SNew(SButton)
                    .Text(LOCTEXT("BuildTerrainOnly", "Build Terrain Only"))
                    .OnClicked_Lambda([this]()
                    {
                        StartBuild(/*bBuildTerrain=*/true, /*bBuildPCG=*/false);
                        return FReply::Handled();
                    })
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                    .Text(LOCTEXT("BuildPCGOnly", "Build PCG Only"))
                    .OnClicked_Lambda([this]()
                    {
                        StartBuild(/*bBuildTerrain=*/false, /*bBuildPCG=*/true);
                        return FReply::Handled();
                    })
                ]
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 6.0f))
            [
                SAssignNew(StatusText, STextBlock)
                .Text(LOCTEXT("StatusIdle", "Status: Idle"))
                .AutoWrapText(true)
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SAssignNew(ProgressBar, SProgressBar)
                .Percent(0.0f)
            ]
        ];

    RefreshMetadata();
    return Widget;
}

void UWorldGenBuilderWidget::NativeDestruct()
{
    DetachDelegates();
    BuildUtility = nullptr;
    Super::NativeDestruct();
}

void UWorldGenBuilderWidget::StartBuild(bool bBuildTerrain, bool bBuildPCG)
{
    if (bIsBuilding)
    {
        if (StatusText.IsValid())
        {
            StatusText->SetText(LOCTEXT("BuildAlreadyRunning", "Build already running."));
        }
        return;
    }

    if (!BuildUtility)
    {
        BuildUtility = NewObject<UWorldGenBuildUtility>(this);
    }

    if (!BuildUtility)
    {
        if (StatusText.IsValid())
        {
            StatusText->SetText(LOCTEXT("BuildUtilityMissing", "Unable to create build utility."));
        }
        return;
    }

    DetachDelegates();
    AttachDelegates();

    int32 SeedOverride = 0;
    if (SeedTextBox.IsValid())
    {
        const FString SeedString = SeedTextBox->GetText().ToString().TrimStartAndEnd();
        if (!SeedString.IsEmpty() && !LexTryParseString(SeedOverride, *SeedString))
        {
            if (StatusText.IsValid())
            {
                StatusText->SetText(LOCTEXT("InvalidSeed", "Invalid seed value. Enter an integer or leave blank."));
            }
            return;
        }
    }

    const FString MapPath = MapPathTextBox.IsValid()
                                ? MapPathTextBox->GetText().ToString().TrimStartAndEnd()
                                : FString();

    bIsBuilding = true;
    if (StatusText.IsValid())
    {
        StatusText->SetText(LOCTEXT("BuildStarting", "Starting world build..."));
    }
    if (ProgressBar.IsValid())
    {
        ProgressBar->SetPercent(0.0f);
    }

    const bool bLaunched = BuildUtility->BuildWorldFromSeedInstance(
        SeedOverride, MapPath, bBuildTerrain, bBuildPCG);

    if (!bLaunched)
    {
        bIsBuilding = false;
        if (StatusText.IsValid())
        {
            StatusText->SetText(LOCTEXT("BuildLaunchFailed", "Build failed to start. See log for details."));
        }
        DetachDelegates();
    }
    else
    {
        RefreshMetadata();
    }
}

void UWorldGenBuilderWidget::RefreshMetadata()
{
    FText ModeLine = LOCTEXT("ModeUnavailable", "Mode: (settings unavailable)");
    FText BuildStateLine = LOCTEXT("BuildStateUnavailable", "Build State: (not found)");

    if (UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings())
    {
        const FWorldGenConfig& Config = Settings->Settings;
        ModeLine = FText::FromString(FString::Printf(TEXT("Mode: %s"), *DescribeBuildMode(Config)));

        FWorldBuildState BuildState;
        FString AssetPath;
        if (ResolveBuildState(BuildState, AssetPath))
        {
            const bool bValid = BuildState.IsValid();
            const bool bCompatible = bValid && BuildState.IsCompatibleWith(Config);
            BuildStateLine = FText::FromString(FString::Printf(
                TEXT("BuildState: %s | Seed=%d Version=%d Hash=%s Timestamp=%s | Baked=%s Valid=%s Compatible=%s"),
                *AssetPath,
                BuildState.BuiltSeed,
                BuildState.WorldGenVersion,
                *BuildState.PCGBuildHash,
                *BuildState.LastBuildTime.ToString(),
                BuildState.bIsBaked ? TEXT("true") : TEXT("false"),
                bValid ? TEXT("true") : TEXT("false"),
                bCompatible ? TEXT("true") : TEXT("false")));
        }
        else
        {
            BuildStateLine = LOCTEXT("BuildStateMissing", "Build State: No companion asset found for this map.");
        }
    }

    if (ModeText.IsValid())
    {
        ModeText->SetText(ModeLine);
    }
    if (BuildStateText.IsValid())
    {
        BuildStateText->SetText(BuildStateLine);
    }
}

void UWorldGenBuilderWidget::AttachDelegates()
{
    if (BuildUtility)
    {
        BuildUtility->OnBuildProgress.AddDynamic(this, &UWorldGenBuilderWidget::HandleBuildProgress);
        BuildUtility->OnBuildComplete.AddDynamic(this, &UWorldGenBuilderWidget::HandleBuildComplete);
    }
}

void UWorldGenBuilderWidget::DetachDelegates()
{
    if (BuildUtility)
    {
        BuildUtility->OnBuildProgress.RemoveDynamic(this, &UWorldGenBuilderWidget::HandleBuildProgress);
        BuildUtility->OnBuildComplete.RemoveDynamic(this, &UWorldGenBuilderWidget::HandleBuildComplete);
    }
}

FString UWorldGenBuilderWidget::DescribeBuildMode(const FWorldGenConfig& Config) const
{
    if (const UEnum* ModeEnum = StaticEnum<EWorldGenBuildMode>())
    {
        const FString Name = ModeEnum->GetDisplayNameTextByValue(static_cast<int64>(Config.BuildMode)).ToString();
        return Name;
    }
    return TEXT("Unknown");
}

bool UWorldGenBuilderWidget::ResolveBuildState(FWorldBuildState& OutState, FString& OutAssetPath) const
{
    OutState = FWorldBuildState();
    OutAssetPath.Reset();

    if (!GEditor)
    {
        return false;
    }

    UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
    if (!EditorWorld)
    {
        return false;
    }

    UWorldGenBuildStateAsset* Asset = nullptr;
    for (TActorIterator<AWorldGenManager> It(EditorWorld); It; ++It)
    {
        if (It->WorldBuildStateAsset.IsValid())
        {
            Asset = It->WorldBuildStateAsset.LoadSynchronous();
            OutAssetPath = It->WorldBuildStateAsset.ToString();
            break;
        }
    }

    if (!Asset)
    {
        const FString MapIdentifier = WorldGenBuilderWidget::SanitizeIdentifier(EditorWorld->GetMapName());
        const FString ObjectPath = WorldGenBuilderWidget::MakeBuildStateObjectPath(MapIdentifier);
        Asset = LoadObject<UWorldGenBuildStateAsset>(nullptr, *ObjectPath);
        OutAssetPath = ObjectPath;
    }

    if (!Asset)
    {
        return false;
    }

    OutState = Asset->BuildState;
    return true;
}

void UWorldGenBuilderWidget::HandleBuildProgress(int32 Current, int32 Total, const FString& Status)
{
    const float Fraction = (Total > 0) ? static_cast<float>(Current) / static_cast<float>(Total) : 0.0f;
    if (ProgressBar.IsValid())
    {
        ProgressBar->SetPercent(FMath::Clamp(Fraction, 0.0f, 1.0f));
    }
    if (StatusText.IsValid())
    {
        StatusText->SetText(FText::FromString(Status));
    }
}

void UWorldGenBuilderWidget::HandleBuildComplete(bool bSuccess, const FString& Message)
{
    bIsBuilding = false;
    if (ProgressBar.IsValid())
    {
        ProgressBar->SetPercent(bSuccess ? 1.0f : 0.0f);
    }
    if (StatusText.IsValid())
    {
        StatusText->SetText(FText::FromString(Message));
    }

    RefreshMetadata();
    DetachDelegates();
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
