#include "DungeonPortalWidget.h"
#include "Components/TextBlock.h"

UDungeonPortalWidget::UDungeonPortalWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bIsPortalActive = true;
}

void UDungeonPortalWidget::NativeConstruct()
{
    Super::NativeConstruct();
    
    // Set default text if widgets are bound
    if (PortalNameText)
    {
        PortalNameText->SetText(FText::FromString(TEXT("Dungeon Portal")));
    }
    
    if (InteractionPromptText)
    {
        InteractionPromptText->SetText(FText::FromString(TEXT("Press E to Enter")));
    }
    
    if (TargetLevelText)
    {
        TargetLevelText->SetText(FText::FromString(TEXT("Unknown Destination")));
    }
}

void UDungeonPortalWidget::SetPortalInfo(const FString& PortalTypeName, const FString& TargetLevelName)
{
    CurrentPortalTypeName = PortalTypeName;
    CurrentTargetLevelName = TargetLevelName;
    
    if (PortalNameText)
    {
        // Clean up the portal type name for display
        FString DisplayName = PortalTypeName;
        DisplayName = DisplayName.Replace(TEXT("Portal"), TEXT(""));
        DisplayName = DisplayName.Replace(TEXT("Dungeon"), TEXT(""));
        
        if (DisplayName.IsEmpty())
        {
            DisplayName = TEXT("Dungeon Portal");
        }
        else
        {
            DisplayName += TEXT(" Portal");
        }
        
        PortalNameText->SetText(FText::FromString(DisplayName));
    }
    
    if (TargetLevelText)
    {
        FString DisplayTarget = TargetLevelName;
        if (DisplayTarget.IsEmpty())
        {
            DisplayTarget = TEXT("Unknown Destination");
        }
        
        TargetLevelText->SetText(FText::FromString(DisplayTarget));
    }
}

void UDungeonPortalWidget::SetPortalActive(bool bActive)
{
    bIsPortalActive = bActive;
    
    if (InteractionPromptText)
    {
        if (bActive)
        {
            InteractionPromptText->SetText(FText::FromString(TEXT("Press E to Enter")));
            InteractionPromptText->SetColorAndOpacity(FLinearColor::White);
        }
        else
        {
            InteractionPromptText->SetText(FText::FromString(TEXT("Portal Inactive")));
            InteractionPromptText->SetColorAndOpacity(FLinearColor::Red);
        }
    }
    
    // Update overall widget opacity based on active state
    SetRenderOpacity(bActive ? 1.0f : 0.6f);
}