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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPCGInstanceDataInitializationTest, 
	"Vibeheim.WorldGen.StructInitialization.FPCGInstanceData", 
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGInstanceDataInitializationTest::RunTest(const FString& Parameters)
{
	// Test 1: Default construction should create valid InstanceId (Persistent ID pattern)
	{
		FPCGInstanceData InstanceData;
		
		TestTrue("InstanceId must be valid after default construction", InstanceData.InstanceId.IsValid());
		TestNotEqual("InstanceId should not be zero GUID", InstanceData.InstanceId, FGuid());
		
		// Verify other members have expected defaults
		TestEqual("Location should be zero vector", InstanceData.Location, FVector::ZeroVector);
		TestEqual("Rotation should be zero rotator", InstanceData.Rotation, FRotator::ZeroRotator);
		TestEqual("Scale should be one vector", InstanceData.Scale, FVector::OneVector);
		TestEqual("bIsActive should be true", InstanceData.bIsActive, true);
	}
	
	// Test 2: Multiple constructions should create unique IDs
	{
		FPCGInstanceData Instance1;
		FPCGInstanceData Instance2;
		
		TestTrue("First instance should have valid ID", Instance1.InstanceId.IsValid());
		TestTrue("Second instance should have valid ID", Instance2.InstanceId.IsValid());
		TestNotEqual("Each instance should have unique ID", Instance1.InstanceId, Instance2.InstanceId);
	}
	
	// Test 3: Copy construction should preserve ID
	{
		FPCGInstanceData Original;
		FGuid OriginalId = Original.InstanceId;
		
		FPCGInstanceData Copy = Original;
		TestEqual("Copy should preserve InstanceId", Copy.InstanceId, OriginalId);
		TestTrue("Copy InstanceId should still be valid", Copy.InstanceId.IsValid());
	}
	
	// Test 4: Serialization roundtrip should preserve ID
	{
		FPCGInstanceData Original;
		Original.Location = FVector(100.0f, 200.0f, 50.0f);
		Original.Rotation = FRotator(10.0f, 20.0f, 30.0f);
		Original.Scale = FVector(2.0f, 2.0f, 2.0f);
		Original.bIsActive = false;
		
		FGuid OriginalId = Original.InstanceId;
		
		// Serialize to memory
		TArray<uint8> SerializedData;
		FMemoryWriter Writer(SerializedData);
		Original.Serialize(Writer);
		
		// Deserialize from memory
		FPCGInstanceData Deserialized;
		FMemoryReader Reader(SerializedData);
		Deserialized.Serialize(Reader);
		
		TestEqual("Deserialized InstanceId should match original", Deserialized.InstanceId, OriginalId);
		TestEqual("Deserialized Location should match", Deserialized.Location, Original.Location);
		TestEqual("Deserialized Rotation should match", Deserialized.Rotation, Original.Rotation);
		TestEqual("Deserialized Scale should match", Deserialized.Scale, Original.Scale);
		TestEqual("Deserialized bIsActive should match", Deserialized.bIsActive, Original.bIsActive);
	}
	
	return true;
}