#pragma once

#include "CoreMinimal.h"

#if WITH_EDITORONLY_DATA
#include "EditorUtilityWidget.h"
#include "WorldGenBuilderWidget.generated.h"

class SEditableTextBox;
class SProgressBar;
class STextBlock;
class UWorldGenBuildUtility;
struct FWorldBuildState;
struct FWorldGenConfig;

/**
 * Editor Utility Widget that wraps UWorldGenBuildUtility for manual world builds.
 * Provides seed/map inputs, build-mode visibility, and live progress reporting.
 */
UCLASS()
class VIBEHEIM_API UWorldGenBuilderWidget : public UEditorUtilityWidget
{
    GENERATED_BODY()

public:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeDestruct() override;

private:
    void StartBuild(bool bBuildTerrain, bool bBuildPCG);
    void RefreshMetadata();
    void AttachDelegates();
    void DetachDelegates();

    FString DescribeBuildMode(const FWorldGenConfig& Config) const;
    bool ResolveBuildState(FWorldBuildState& OutState, FString& OutAssetPath) const;

    UFUNCTION()
    void HandleBuildProgress(int32 Current, int32 Total, const FString& Status);

    UFUNCTION()
    void HandleBuildComplete(bool bSuccess, const FString& Message);

private:
    /** Transient build utility instance used while the widget is open. */
    UPROPERTY()
    TObjectPtr<UWorldGenBuildUtility> BuildUtility;

    bool bIsBuilding = false;

    TSharedPtr<SEditableTextBox> SeedTextBox;
    TSharedPtr<SEditableTextBox> MapPathTextBox;
    TSharedPtr<STextBlock> ModeText;
    TSharedPtr<STextBlock> BuildStateText;
    TSharedPtr<STextBlock> StatusText;
    TSharedPtr<SProgressBar> ProgressBar;
};

#endif // WITH_EDITORONLY_DATA
