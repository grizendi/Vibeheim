#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "DungeonPortalWidget.generated.h"

/**
 * Simple widget for dungeon portal interaction prompts
 */
UCLASS(BlueprintType, Blueprintable)
class VIBEHEIM_API UDungeonPortalWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UDungeonPortalWidget(const FObjectInitializer& ObjectInitializer);

protected:
    virtual void NativeConstruct() override;

public:
    /**
     * Set the portal information to display
     * @param PortalTypeName The name of the portal type
     * @param TargetLevelName The target level name
     */
    UFUNCTION(BlueprintCallable, Category = "Portal Widget")
    void SetPortalInfo(const FString& PortalTypeName, const FString& TargetLevelName);

    /**
     * Set whether the portal is active and can be used
     * @param bActive Whether the portal is active
     */
    UFUNCTION(BlueprintCallable, Category = "Portal Widget")
    void SetPortalActive(bool bActive);

protected:
    /** Text block for the portal name */
    UPROPERTY(BlueprintReadOnly, Category = "Components", meta = (BindWidget))
    TObjectPtr<UTextBlock> PortalNameText;

    /** Text block for the interaction prompt */
    UPROPERTY(BlueprintReadOnly, Category = "Components", meta = (BindWidget))
    TObjectPtr<UTextBlock> InteractionPromptText;

    /** Text block for the target level */
    UPROPERTY(BlueprintReadOnly, Category = "Components", meta = (BindWidget))
    TObjectPtr<UTextBlock> TargetLevelText;

private:
    /** Current portal type name */
    FString CurrentPortalTypeName;

    /** Current target level name */
    FString CurrentTargetLevelName;

    /** Whether the portal is currently active */
    bool bIsPortalActive = true;
};