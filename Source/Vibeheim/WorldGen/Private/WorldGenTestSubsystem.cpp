#include "WorldGenTestSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "WorldGenManager.h"
#include "WorldGenSettings.h"
#include "WorldGenSeedSubsystem.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY(LogWorldGenTest);

// Define the test map path
const FString UWorldGenTestSubsystem::TestMapPath = TEXT("/Game/Maps/WG_TestMap");

void UWorldGenTestSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	UE_LOG(LogWorldGenTest, Log, TEXT("WorldGenTestSubsystem initialized"));
	
	// Register console commands
	RegisterConsoleCommands();
}

void UWorldGenTestSubsystem::Deinitialize()
{
	// Unregister console commands
	UnregisterConsoleCommands();
	
	Super::Deinitialize();
	
	UE_LOG(LogWorldGenTest, Log, TEXT("WorldGenTestSubsystem deinitialized"));
}

bool UWorldGenTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Only create in game worlds, not in editor preview worlds
	if (UWorld* World = Cast<UWorld>(Outer))
	{
		return World->IsGameWorld();
	}
	return false;
}

void UWorldGenTestSubsystem::RegisterConsoleCommands()
{
	// Clear any existing commands
	UnregisterConsoleCommands();

#if WITH_EDITOR
	// wg.launch command
	RegisteredCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("wg.launch"),
		TEXT("Load /Game/Maps/WG_TestMap and initialize world generation systems"),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UWorldGenTestSubsystem::ExecuteLaunchCommand),
		ECVF_Default
	));

	// wg.seed command
	RegisteredCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("wg.seed"),
		TEXT("Set world generation seed. Usage: wg.seed <value>"),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UWorldGenTestSubsystem::ExecuteSeedCommand),
		ECVF_Default
	));

	// wg.radii command
	RegisteredCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("wg.radii"),
		TEXT("Set streaming radii. Usage: wg.radii <generate> <load> <active>"),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UWorldGenTestSubsystem::ExecuteRadiiCommand),
		ECVF_Default
	));

	// wg.reset command
	RegisteredCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("wg.reset"),
		TEXT("Reset world generation and clear persistence data"),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UWorldGenTestSubsystem::ExecuteResetCommand),
		ECVF_Default
	));
#endif // WITH_EDITOR

	UE_LOG(LogWorldGenTest, Log, TEXT("Registered %d console commands"), RegisteredCommands.Num());
}

void UWorldGenTestSubsystem::UnregisterConsoleCommands()
{
	for (IConsoleObject* Command : RegisteredCommands)
	{
		if (Command)
		{
			IConsoleManager::Get().UnregisterConsoleObject(Command);
		}
	}
	RegisteredCommands.Empty();
}

bool UWorldGenTestSubsystem::IsValidTestMap() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// Check if we're in the correct map
	FString CurrentMapName = World->GetMapName();
	
	// Handle PIE prefix and package name variations
	if (CurrentMapName.Contains(TEXT("WG_TestMap")))
	{
		return true;
	}

	// Also check the package name
	if (World->GetPackage())
	{
		FString PackageName = World->GetPackage()->GetName();
		if (PackageName.Contains(TEXT("WG_TestMap")))
		{
			return true;
		}
	}

	return false;
}

void UWorldGenTestSubsystem::LogMapError(const FString& CommandName) const
{
	UWorld* World = GetWorld();
	FString CurrentMap = World ? World->GetMapName() : TEXT("Unknown");
	UE_LOG(LogWorldGenTest, Error, TEXT("%s: Command only works in %s (current: %s)"), 
		*CommandName, *TestMapPath, *CurrentMap);
}

void UWorldGenTestSubsystem::ExecuteLaunchCommand(const TArray<FString>& Args)
{
	if (!IsValidTestMap())
	{
		LogMapError(TEXT("wg.launch"));
		return;
	}

	UE_LOG(LogWorldGenTest, Log, TEXT("Launching test world systems..."));
	
	// Find or create WorldGenManager
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogWorldGenTest, Error, TEXT("No valid world found"));
		return;
	}

	AWorldGenManager* WorldGenManager = nullptr;
	for (TActorIterator<AWorldGenManager> ActorItr(World); ActorItr; ++ActorItr)
	{
		WorldGenManager = *ActorItr;
		break;
	}

	if (!WorldGenManager)
	{
		UE_LOG(LogWorldGenTest, Warning, TEXT("No WorldGenManager found in scene. Please add one to the map."));
		return;
	}

	// Initialize with authoritative seed
	UWorldGenSeedSubsystem* SeedSubsystem = GetSeedSubsystem();
	if (SeedSubsystem)
	{
		SeedSubsystem->PropagateToAllSystems();
		UE_LOG(LogWorldGenTest, Log, TEXT("Propagated authoritative seed %d to all systems"), 
			SeedSubsystem->GetAuthoritativeSeed());
	}

	UE_LOG(LogWorldGenTest, Log, TEXT("Test world launch completed"));
}

void UWorldGenTestSubsystem::ExecuteSeedCommand(const TArray<FString>& Args)
{
	if (!IsValidTestMap())
	{
		LogMapError(TEXT("wg.seed"));
		return;
	}

	if (Args.Num() < 1)
	{
		UE_LOG(LogWorldGenTest, Error, TEXT("wg.seed: Usage: wg.seed <value>"));
		return;
	}

	int32 NewSeed = FCString::Atoi(*Args[0]);
	SetWorldSeed(NewSeed);
}

void UWorldGenTestSubsystem::ExecuteRadiiCommand(const TArray<FString>& Args)
{
	if (!IsValidTestMap())
	{
		LogMapError(TEXT("wg.radii"));
		return;
	}

	if (Args.Num() < 3)
	{
		UE_LOG(LogWorldGenTest, Error, TEXT("wg.radii: Usage: wg.radii <generate> <load> <active>"));
		return;
	}

	int32 GenerateRadius = FCString::Atoi(*Args[0]);
	int32 LoadRadius = FCString::Atoi(*Args[1]);
	int32 ActiveRadius = FCString::Atoi(*Args[2]);

	SetStreamingRadii(GenerateRadius, LoadRadius, ActiveRadius);
}

void UWorldGenTestSubsystem::ExecuteResetCommand(const TArray<FString>& Args)
{
	if (!IsValidTestMap())
	{
		LogMapError(TEXT("wg.reset"));
		return;
	}

	ResetTestWorld();
}

bool UWorldGenTestSubsystem::LaunchTestWorld()
{
	if (!IsValidTestMap())
	{
		UE_LOG(LogWorldGenTest, Error, TEXT("LaunchTestWorld: Must be called from %s"), *TestMapPath);
		return false;
	}

	UE_LOG(LogWorldGenTest, Log, TEXT("Launching test world..."));
	
	// This would typically load the map, but since we're already in it,
	// we just initialize the systems
	ExecuteLaunchCommand(TArray<FString>());
	
	return true;
}

void UWorldGenTestSubsystem::ResetTestWorld()
{
	if (!IsValidTestMap())
	{
		UE_LOG(LogWorldGenTest, Error, TEXT("ResetTestWorld: Must be called from %s"), *TestMapPath);
		return;
	}

	UE_LOG(LogWorldGenTest, Log, TEXT("Resetting test world..."));
	
	// Reset to default seed
	UWorldGenSeedSubsystem* SeedSubsystem = GetSeedSubsystem();
	if (SeedSubsystem)
	{
		SeedSubsystem->SetAuthoritativeSeed(1337);
	}
	
	// Update settings
	UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
	if (Settings)
	{
		Settings->ResetToDefaults();
		UE_LOG(LogWorldGenTest, Log, TEXT("Reset settings to defaults with seed %d"), 
			SeedSubsystem ? SeedSubsystem->GetAuthoritativeSeed() : 1337);
	}

	UE_LOG(LogWorldGenTest, Log, TEXT("Test world reset completed"));
}

void UWorldGenTestSubsystem::SetWorldSeed(int32 NewSeed)
{
	if (!IsValidTestMap())
	{
		UE_LOG(LogWorldGenTest, Error, TEXT("SetWorldSeed: Must be called from %s"), *TestMapPath);
		return;
	}

	// Update the authoritative seed source
	UWorldGenSeedSubsystem* SeedSubsystem = GetSeedSubsystem();
	if (SeedSubsystem)
	{
		SeedSubsystem->SetAuthoritativeSeed(NewSeed);
		UE_LOG(LogWorldGenTest, Log, TEXT("Set authoritative seed to %d"), NewSeed);
	}
	else
	{
		UE_LOG(LogWorldGenTest, Error, TEXT("Failed to get WorldGenSeedSubsystem instance"));
	}
}

void UWorldGenTestSubsystem::SetStreamingRadii(int32 GenerateRadius, int32 LoadRadius, int32 ActiveRadius)
{
	if (!IsValidTestMap())
	{
		UE_LOG(LogWorldGenTest, Error, TEXT("SetStreamingRadii: Must be called from %s"), *TestMapPath);
		return;
	}

	// Validate radii relationships
	if (LoadRadius > GenerateRadius)
	{
		UE_LOG(LogWorldGenTest, Error, TEXT("LoadRadius (%d) cannot be greater than GenerateRadius (%d)"), 
			LoadRadius, GenerateRadius);
		return;
	}

	if (ActiveRadius > LoadRadius)
	{
		UE_LOG(LogWorldGenTest, Error, TEXT("ActiveRadius (%d) cannot be greater than LoadRadius (%d)"), 
			ActiveRadius, LoadRadius);
		return;
	}

	// Update the settings
	UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
	if (Settings)
	{
		Settings->Settings.GenerateRadius = GenerateRadius;
		Settings->Settings.LoadRadius = LoadRadius;
		Settings->Settings.ActiveRadius = ActiveRadius;
		
		UE_LOG(LogWorldGenTest, Log, TEXT("Set streaming radii: Generate=%d, Load=%d, Active=%d"), 
			GenerateRadius, LoadRadius, ActiveRadius);
	}
	else
	{
		UE_LOG(LogWorldGenTest, Error, TEXT("Failed to get WorldGenSettings instance"));
	}
}

int32 UWorldGenTestSubsystem::GetAuthoritativeSeed() const
{
	UWorldGenSeedSubsystem* SeedSubsystem = GetSeedSubsystem();
	return SeedSubsystem ? SeedSubsystem->GetAuthoritativeSeed() : 1337;
}

UWorldGenSeedSubsystem* UWorldGenTestSubsystem::GetSeedSubsystem() const
{
	if (UGameInstance* GameInstance = GetWorld()->GetGameInstance())
	{
		return GameInstance->GetSubsystem<UWorldGenSeedSubsystem>();
	}
	return nullptr;
}
