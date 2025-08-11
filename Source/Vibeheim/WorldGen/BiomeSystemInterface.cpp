#include "BiomeSystemInterface.h"

void FBiomeSystemWrapper::Initialize(const FWorldGenSettings& InSettings)
{
    BiomeSystem.Initialize(InSettings);
}

FBiomeEvaluation FBiomeSystemWrapper::EvaluateBiome(float WorldX, float WorldY) const
{
    return BiomeSystem.EvaluateBiome(WorldX, WorldY);
}

const FBiomeData& FBiomeSystemWrapper::GetBiomeData(EBiomeType BiomeType) const
{
    return BiomeSystem.GetBiomeData(BiomeType);
}

const TArray<FBiomeData>& FBiomeSystemWrapper::GetAllBiomeData() const
{
    return BiomeSystem.GetAllBiomeData();
}