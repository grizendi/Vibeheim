#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Data/WorldGenTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeightfieldModificationInitializationTest, 
	"Vibeheim.WorldGen.StructInitialization.FHeightfieldModification", 
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHeightfieldModificationInitializationTest::RunTest(const FString& Parameters)
{
	// Test 1: Default construction should create valid ModificationId (Persistent ID pattern)
	{
		FHeightfieldModification Modification;
		
		TestTrue("ModificationId must be valid after default construction", Modification.ModificationId.IsValid());
		TestNotEqual("ModificationId should not be zero GUID", Modification.ModificationId, FGuid());
		
		// Verify other members have expected defaults
		TestEqual("Center should be zero vector", Modification.Center, FVector2D::ZeroVector);
		TestEqual("Radius should be 100.0f", Modification.Radius, 100.0f);
		TestEqual("Strength should be 1.0f", Modification.Strength, 1.0f);
		TestEqual("Operation should be Add", Modification.Operation, EHeightfieldOperation::Add);
	}
	
	// Test 2: Multiple constructions should create unique IDs
	{
		FHeightfieldModification Mod1;
		FHeightfieldModification Mod2;
		
		TestTrue("First modification should have valid ID", Mod1.ModificationId.IsValid());
		TestTrue("Second modification should have valid ID", Mod2.ModificationId.IsValid());
		TestNotEqual("Each modification should have unique ID", Mod1.ModificationId, Mod2.ModificationId);
	}
	
	// Test 3: Copy construction should preserve ID
	{
		FHeightfieldModification Original;
		FGuid OriginalId = Original.ModificationId;
		
		FHeightfieldModification Copy = Original;
		TestEqual("Copy should preserve ModificationId", Copy.ModificationId, OriginalId);
		TestTrue("Copy ModificationId should still be valid", Copy.ModificationId.IsValid());
	}
	
	return true;
}