#include "Data/WorldGenAssets.h"

bool UWorldGenSettingsAsset::ValidateAsset(TArray<FString>& OutErrors) const
{
    bool bOk = true;

    if (MacroWorld.WorldRadiusMeters <= 0.0f)
    {
        OutErrors.Add(TEXT("MacroWorld.WorldRadiusMeters must be > 0"));
        bOk = false;
    }

    if (StreamingBudgets.MaxActiveTiles <= 0)
    {
        OutErrors.Add(TEXT("StreamingBudgets.MaxActiveTiles must be > 0"));
        bOk = false;
    }

    if (CoreSettings.GenerateRadius < 1 || CoreSettings.LoadRadius < 1 || CoreSettings.ActiveRadius < 1)
    {
        OutErrors.Add(TEXT("Streaming radii must be >= 1 tile"));
        bOk = false;
    }

    if (CoreSettings.LoadRadius > CoreSettings.GenerateRadius)
    {
        OutErrors.Add(TEXT("LoadRadius must be <= GenerateRadius"));
        bOk = false;
    }

    if (CoreSettings.ActiveRadius > CoreSettings.LoadRadius)
    {
        OutErrors.Add(TEXT("ActiveRadius must be <= LoadRadius"));
        bOk = false;
    }

    // Basic sanity for rivers
    if (RiverSystem.MinRiverWidth > RiverSystem.MaxRiverWidth)
    {
        OutErrors.Add(TEXT("RiverSystem.MinRiverWidth must be <= MaxRiverWidth"));
        bOk = false;
    }

    return bOk;
}

bool UBiomeDefinitionsAsset::ValidateAsset(TArray<FString>& OutErrors) const
{
    bool bOk = true;
    // Validate ring radii and blending
    for (int32 i = 0; i < BiomeRings.Num(); ++i)
    {
        const FBiomeRingDefinition& R = BiomeRings[i];
        if (R.InnerRadius < 0.0f || R.OuterRadius <= R.InnerRadius)
        {
            OutErrors.Add(FString::Printf(TEXT("BiomeRing[%d] radius invalid: inner=%.1f outer=%.1f"), i, R.InnerRadius, R.OuterRadius));
            bOk = false;
        }
        if (R.BlendWidth < 0.0f)
        {
            OutErrors.Add(FString::Printf(TEXT("BiomeRing[%d] BlendWidth must be >= 0"), i));
            bOk = false;
        }
    }
    return bOk;
}
