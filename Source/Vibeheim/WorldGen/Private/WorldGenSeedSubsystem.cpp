#include "WorldGenSeedSubsystem.h"
#include "WorldGenSettings.h"
#include "Engine/Engine.h"

DEFINE_LOG_CATEGORY_STATIC(LogWorldGenSeed, Log, All);

void UWorldGenSeedSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	UE_LOG(LogWorldGenSeed, Log, TEXT("WorldGenSeedSubsystem initialized with seed %d"), AuthoritativeSeed);
	
	// Propagate the initial seed to all systems
	PropagateToAllSystems();
}

void UWorldGenSeedSubsystem::SetAuthoritativeSeed(int32 NewSeed)
{
	if (AuthoritativeSeed != NewSeed)
	{
		AuthoritativeSeed = NewSeed;
		UE_LOG(LogWorldGenSeed, Log, TEXT("Authoritative seed changed to %d"), AuthoritativeSeed);
		
		// Propagate to all systems
		PropagateToAllSystems();
	}
}

void UWorldGenSeedSubsystem::PropagateToAllSystems()
{
	// Update WorldGenSettings
	UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
	if (Settings)
	{
		Settings->Settings.Seed = AuthoritativeSeed;
		UE_LOG(LogWorldGenSeed, VeryVerbose, TEXT("Propagated seed %d to WorldGenSettings"), AuthoritativeSeed);
	}
	
	// Future: Add propagation to other systems as they're implemented
	// - NoiseSystem
	// - BiomeService
	// - PCGWorldService
	// etc.
}
