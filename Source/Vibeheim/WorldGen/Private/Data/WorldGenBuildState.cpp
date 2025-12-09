#include "Data/WorldGenBuildState.h"
#include "Data/WorldGenTypes.h"

bool FWorldBuildState::IsValid() const
{
    return bIsBaked && BuiltSeed != 0 && WorldGenVersion != 0 && !PCGBuildHash.IsEmpty();
}

bool FWorldBuildState::IsCompatibleWith(const FWorldGenConfig& Config) const
{
    return IsValid() && BuiltSeed == Config.Seed && WorldGenVersion == Config.WorldGenVersion;
}

void UWorldGenBuildStateAsset::UpdateFromBuild(int32 Seed, int32 Version, const FString& PCGHash)
{
    Modify();
    BuildState.BuiltSeed = Seed;
    BuildState.WorldGenVersion = Version;
    BuildState.LastBuildTime = FDateTime::UtcNow();
    BuildState.PCGBuildHash = PCGHash;
    BuildState.bIsBaked = true;
}

FArchive& operator<<(FArchive& Ar, FWorldBuildState& State)
{
    Ar << State.BuiltSeed;
    Ar << State.WorldGenVersion;
    Ar << State.LastBuildTime;
    Ar << State.PCGBuildHash;
    Ar << State.bIsBaked;
    return Ar;
}

