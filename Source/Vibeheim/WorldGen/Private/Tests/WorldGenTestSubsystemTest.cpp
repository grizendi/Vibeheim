#include "CoreMinimal.h"
#include "WorldGenTestSubsystem.h"
#include "WorldGenSeedSubsystem.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

// Only include automation test if available
#if WITH_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldGenTestSubsystemTest, "Vibeheim.WorldGen.TestSubsystem.Basic", EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FWorldGenTestSubsystemTest::RunTest(const FString& Parameters)
{
	// Test that subsystems can be created (basic smoke test)
	// Note: Full functionality testing requires the actual test map
	
	// Test seed subsystem creation
	UWorldGenSeedSubsystem* SeedSubsystem = NewObject<UWorldGenSeedSubsystem>();
	TestNotNull("WorldGenSeedSubsystem should be created", SeedSubsystem);
	
	if (SeedSubsystem)
	{
		// Test default seed
		TestEqual("Default seed should be 1337", SeedSubsystem->GetAuthoritativeSeed(), 1337);
		
		// Test seed setting
		SeedSubsystem->SetAuthoritativeSeed(42);
		TestEqual("Seed should be updated to 42", SeedSubsystem->GetAuthoritativeSeed(), 42);
		
		// Test seed reset
		SeedSubsystem->SetAuthoritativeSeed(1337);
		TestEqual("Seed should be reset to 1337", SeedSubsystem->GetAuthoritativeSeed(), 1337);
	}
	
	// Test console command registration (basic check)
	UWorldGenTestSubsystem* TestSubsystem = NewObject<UWorldGenTestSubsystem>();
	TestNotNull("WorldGenTestSubsystem should be created", TestSubsystem);
	
	// Note: Console command functionality requires proper world context and test map
	// This is tested through the gate check requirements in the actual test map
	
	return true;
}

#endif // WITH_AUTOMATION_TESTS
