#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "PCGElement.h"
#include "Services/PCGWorldServiceTypes.h"
#include "VibeheimPCGGetTerrainHeight.generated.h"

class FPCGVibeheimGetTerrainHeightElement;

/**
 * PCG element settings that annotate points with terrain height and slope sampled from the heightfield service.
 * Updated for UE 5.7 compatibility.
 */
UCLASS(BlueprintType, ClassGroup = (Vibeheim, PCG))
class VIBEHEIM_API UVibeheimPCGGetTerrainHeightSettings : public UPCGSettings
{
        GENERATED_BODY()

public:
        UVibeheimPCGGetTerrainHeightSettings();

        //~ Begin UPCGSettings interface
        virtual TArray<FPCGPinProperties> InputPinProperties() const override;
        virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
        virtual FName GetDefaultNodeName() const override;
        virtual FText GetDefaultNodeTitle() const override;
        virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
        virtual bool IsCacheable() const override { return false; }
#if WITH_EDITOR
        virtual FText GetNodeTooltipText() const override;
#endif
        virtual FPCGElementPtr CreateElement() const override;
        //~ End UPCGSettings interface

        /** Attribute name that will store the sampled terrain height. */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
        FName HeightAttribute = VHMPCGAttr::TerrainHeight;

        /** Attribute name that will store the sampled terrain slope in degrees. */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
        FName SlopeAttribute = VHMPCGAttr::TerrainSlope;

        /** When enabled, the point's Z location will be projected to the sampled height. */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
        bool bProjectPointZ = false;
};

/**
 * Runtime element that evaluates terrain height for incoming points.
 */
class FPCGVibeheimGetTerrainHeightElement : public IPCGElement
{
public:
        virtual bool ExecuteInternal(FPCGContext* Context) const override;
        virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
};
