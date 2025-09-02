#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Data/WorldGenTypes.h"
#include "WorldGenTestSubsystem.generated.h"

// Forward declarations
class AWorldGenManager;
class UWorldGenSeedSubsystem;

DECLARE_LOG_CATEGORY_EXTERN(LogWorldGenTest, Log, All);

/**
 * Test subsystem for the Simple Test World feature
 * Provides console commands and orchestration for testing world generation systems
 * Only operates on /Game/Maps/WG_TestMap for safety
 */
UCLASS()
class VIBEHEIM_API UWorldGenTestSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	// Test world management
	UFUNCTION(BlueprintCallable, CallInEditor)
	bool LaunchTestWorld();
	
	UFUNCTION(BlueprintCallable, CallInEditor)
	void ResetTestWorld();
	
	// Configuration management
	UFUNCTION(BlueprintCallable)
	void SetWorldSeed(int32 NewSeed);
	
	UFUNCTION(BlueprintCallable)
	void SetStreamingRadii(int32 GenerateRadius, int32 LoadRadius, int32 ActiveRadius);

	// Get the authoritative seed source
	UFUNCTION(BlueprintCallable)
	int32 GetAuthoritativeSeed() const;

private:
	// Console command management
	void RegisterConsoleCommands();
	void UnregisterConsoleCommands();
	TArray<IConsoleObject*> RegisteredCommands;

	// Map validation
	bool IsValidTestMap() const;
	void LogMapError(const FString& CommandName) const;

	// Console command implementations
	void ExecuteLaunchCommand(const TArray<FString>& Args);
	void ExecuteSeedCommand(const TArray<FString>& Args);
	void ExecuteRadiiCommand(const TArray<FString>& Args);
	void ExecuteResetCommand(const TArray<FString>& Args);

	// Get the seed subsystem
	UWorldGenSeedSubsystem* GetSeedSubsystem() const;

	// Test map path constant
	static const FString TestMapPath;
};
