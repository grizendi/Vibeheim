#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Data/WorldGenTypes.h"
#include "Data/InstancePersistence.h"

/**
 * Test to document and validate behavior changes from struct initialization fixes
 * This test documents the new behavior: all structs now use NewGuid() for immediate unique identification
 */
class FStructBehaviorChangeTest : public FAutomationTestBase
{
public:
    FStructBehaviorChangeTest(const FString& InName, const bool bInComplexTask)
        : FAutomationTestBase(InName, bInComplexTask)
    {
    }

    virtual uint32 GetTestFlags() const override
    {
        return EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter;
    }

    virtual bool SuppressLogWarnings() override
    {
        return true;
    }

    virtual FString GetBeautifiedTestName() const override
    {
        return "Vibeheim.WorldGen.StructBehaviorChange";
    }

protected:
    virtual bool RunTest(const FString& Parameters) override;

private:
    void TestNewGuidBehavior();
    void TestContainerKeyStability();
    void TestBlueprintBehaviorChange();
    void TestConstructorValidation();
    void TestSerializationBehavior();
    void DocumentBehaviorChanges();
};

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FStructBehaviorChangeTest, FStructBehaviorChangeTest, 
    "Vibeheim.WorldGen.StructBehaviorChange", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FStructBehaviorChangeTest::RunTest(const FString& Parameters)
{
    // Document the behavior changes
    DocumentBehaviorChanges();
    
    // Test the new behavior
    TestNewGuidBehavior();
    TestContainerKeyStability();
    TestBlueprintBehaviorChange();
    TestConstructorValidation();
    TestSerializationBehavior();
    
    return true;
}

void FStructBehaviorChangeTest::DocumentBehaviorChanges()
{
    UE_LOG(LogTemp, Warning, TEXT("=== STRUCT INITIALIZATION BEHAVIOR CHANGES ==="));
    UE_LOG(LogTemp, Warning, TEXT(""));
    UE_LOG(LogTemp, Warning, TEXT("BEFORE FIXES:"));
    UE_LOG(LogTemp, Warning, TEXT("- Structs had uninitialized or zero GUID members"));
    UE_LOG(LogTemp, Warning, TEXT("- UE5.6 reflection system reported initialization errors"));
    UE_LOG(LogTemp, Warning, TEXT("- Some structs used constructor assignment instead of member initialization"));
    UE_LOG(LogTemp, Warning, TEXT(""));
    UE_LOG(LogTemp, Warning, TEXT("AFTER FIXES:"));
    UE_LOG(LogTemp, Warning, TEXT("- All ID-type structs use FGuid::NewGuid() in-class initializers"));
    UE_LOG(LogTemp, Warning, TEXT("- Every struct instance gets a unique GUID immediately upon construction"));
    UE_LOG(LogTemp, Warning, TEXT("- UE5.6 reflection system validation passes without errors"));
    UE_LOG(LogTemp, Warning, TEXT("- TStructOpsTypeTraits properly configured (WithZeroConstructor = false)"));
    UE_LOG(LogTemp, Warning, TEXT(""));
    UE_LOG(LogTemp, Warning, TEXT("BEHAVIOR CHANGE IMPLICATIONS:"));
    UE_LOG(LogTemp, Warning, TEXT("- Blueprint nodes that create these structs will now produce valid unique IDs"));
    UE_LOG(LogTemp, Warning, TEXT("- TMap/TSet containers using these structs as keys will have stable lookups"));
    UE_LOG(LogTemp, Warning, TEXT("- Serialization preserves the assigned GUIDs (no change in save file format)"));
    UE_LOG(LogTemp, Warning, TEXT("- Constructor validation ensures runtime safety with ensureMsgf"));
    UE_LOG(LogTemp, Warning, TEXT(""));
}

void FStructBehaviorChangeTest::TestNewGuidBehavior()
{
    // Test that all structs now generate unique GUIDs immediately
    
    // Test FHeightfieldModification
    FHeightfieldModification Mod1;
    FHeightfieldModification Mod2;
    
    TestTrue("FHeightfieldModification should have valid GUID", Mod1.ModificationId.IsValid());
    TestTrue("Second FHeightfieldModification should have valid GUID", Mod2.ModificationId.IsValid());
    TestNotEqual("FHeightfieldModification GUIDs should be unique", Mod1.ModificationId, Mod2.ModificationId);
    
    // Test FInstanceJournalEntry
    FInstanceJournalEntry Entry1;
    FInstanceJournalEntry Entry2;
    
    TestTrue("FInstanceJournalEntry should have valid GUID", Entry1.InstanceId.IsValid());
    TestTrue("Second FInstanceJournalEntry should have valid GUID", Entry2.InstanceId.IsValid());
    TestNotEqual("FInstanceJournalEntry GUIDs should be unique", Entry1.InstanceId, Entry2.InstanceId);
    
    // Test FPOIData
    FPOIData POI1;
    FPOIData POI2;
    
    TestTrue("FPOIData should have valid GUID", POI1.POIId.IsValid());
    TestTrue("Second FPOIData should have valid GUID", POI2.POIId.IsValid());
    TestNotEqual("FPOIData GUIDs should be unique", POI1.POIId, POI2.POIId);
    
    // Test FPCGInstanceData
    FPCGInstanceData Instance1;
    FPCGInstanceData Instance2;
    
    TestTrue("FPCGInstanceData should have valid GUID", Instance1.InstanceId.IsValid());
    TestTrue("Second FPCGInstanceData should have valid GUID", Instance2.InstanceId.IsValid());
    TestNotEqual("FPCGInstanceData GUIDs should be unique", Instance1.InstanceId, Instance2.InstanceId);
    
    UE_LOG(LogTemp, Log, TEXT("NewGuid behavior test passed - all structs generate unique GUIDs"));
}

void FStructBehaviorChangeTest::TestContainerKeyStability()
{
    // Test that GUIDs remain stable for container lookups after save/load cycles
    
    // Create a map with POI data
    TMap<FGuid, FPOIData> POIMap;
    
    FPOIData TestPOI;
    TestPOI.POIName = TEXT("StabilityTestPOI");
    TestPOI.Location = FVector(100.0f, 200.0f, 300.0f);
    FGuid OriginalPOIId = TestPOI.POIId;
    
    POIMap.Add(OriginalPOIId, TestPOI);
    
    // Verify initial lookup works
    TestTrue("Initial POI lookup should work", POIMap.Contains(OriginalPOIId));
    
    // Simulate save/load by copying the struct
    FPOIData CopiedPOI = TestPOI;
    TestEqual("Copied POI should have same GUID", CopiedPOI.POIId, OriginalPOIId);
    
    // Test that the copied struct can still be used for lookups
    TestTrue("Lookup with copied POI GUID should work", POIMap.Contains(CopiedPOI.POIId));
    
    // Test TSet stability
    TSet<FGuid> InstanceIdSet;
    
    FPCGInstanceData TestInstance;
    TestInstance.Location = FVector(50.0f, 100.0f, 150.0f);
    FGuid OriginalInstanceId = TestInstance.InstanceId;
    
    InstanceIdSet.Add(OriginalInstanceId);
    TestTrue("Initial instance ID lookup should work", InstanceIdSet.Contains(OriginalInstanceId));
    
    // Test stability after copy
    FPCGInstanceData CopiedInstance = TestInstance;
    TestEqual("Copied instance should have same GUID", CopiedInstance.InstanceId, OriginalInstanceId);
    TestTrue("Lookup with copied instance GUID should work", InstanceIdSet.Contains(CopiedInstance.InstanceId));
    
    UE_LOG(LogTemp, Log, TEXT("Container key stability test passed"));
}

void FStructBehaviorChangeTest::TestBlueprintBehaviorChange()
{
    // Document Blueprint behavior change: structs now create valid IDs automatically
    
    // Before: Blueprint-created structs might have had zero/invalid GUIDs
    // After: Blueprint-created structs automatically get valid unique GUIDs
    
    // Simulate Blueprint struct creation (default construction)
    FPOIData BlueprintPOI; // This is what happens when Blueprint creates the struct
    
    TestTrue("Blueprint-created POI should have valid GUID", BlueprintPOI.POIId.IsValid());
    TestNotEqual("Blueprint-created POI should not have zero GUID", BlueprintPOI.POIId, FGuid());
    
    // Test multiple Blueprint creations produce unique IDs
    FPOIData BlueprintPOI2;
    TestNotEqual("Multiple Blueprint POIs should have unique GUIDs", BlueprintPOI.POIId, BlueprintPOI2.POIId);
    
    // This is a behavior change: previously Blueprint might have created structs with zero GUIDs
    // Now they automatically get unique valid GUIDs
    
    UE_LOG(LogTemp, Warning, TEXT("BLUEPRINT BEHAVIOR CHANGE: Structs now automatically generate unique GUIDs"));
    UE_LOG(LogTemp, Warning, TEXT("- Before: Blueprint-created structs might have zero/invalid GUIDs"));
    UE_LOG(LogTemp, Warning, TEXT("- After: Blueprint-created structs automatically get valid unique GUIDs"));
    UE_LOG(LogTemp, Warning, TEXT("- Impact: Blueprint logic that relied on zero GUIDs may need updates"));
    
    UE_LOG(LogTemp, Log, TEXT("Blueprint behavior change test passed"));
}

void FStructBehaviorChangeTest::TestConstructorValidation()
{
    // Test that constructor validation works correctly
    
    // Test FInstanceJournalEntry constructors
    FPCGInstanceData TestInstanceData;
    TestInstanceData.Location = FVector(100.0f, 200.0f, 300.0f);
    
    FInstanceJournalEntry JournalFromInstance(TestInstanceData, EInstanceOperation::Add);
    TestEqual("Journal entry should use instance GUID", JournalFromInstance.InstanceId, TestInstanceData.InstanceId);
    TestTrue("Journal entry GUID should be valid", JournalFromInstance.InstanceId.IsValid());
    
    FPOIData TestPOIData;
    TestPOIData.POIName = TEXT("TestPOI");
    
    FInstanceJournalEntry JournalFromPOI(TestPOIData, EInstanceOperation::Add);
    TestEqual("Journal entry should use POI GUID", JournalFromPOI.InstanceId, TestPOIData.POIId);
    TestTrue("Journal entry GUID should be valid", JournalFromPOI.InstanceId.IsValid());
    
    // Test that all constructors produce valid GUIDs
    TestTrue("Default constructor should produce valid GUID", FInstanceJournalEntry().InstanceId.IsValid());
    
    UE_LOG(LogTemp, Log, TEXT("Constructor validation test passed"));
}

void FStructBehaviorChangeTest::TestSerializationBehavior()
{
    // Test that serialization behavior is unchanged (GUIDs are preserved)
    
    FHeightfieldModification Original;
    Original.Center = FVector2D(123.0f, 456.0f);
    Original.Radius = 78.9f;
    FGuid OriginalGuid = Original.ModificationId;
    
    // Serialize
    TArray<uint8> SerializedData;
    FMemoryWriter Writer(SerializedData);
    Writer << Original;
    
    // Deserialize
    FHeightfieldModification Deserialized;
    FMemoryReader Reader(SerializedData);
    Reader << Deserialized;
    
    // Verify GUID is preserved (no change in serialization behavior)
    TestEqual("Serialization should preserve GUID", Deserialized.ModificationId, OriginalGuid);
    TestEqual("Serialization should preserve other data", Deserialized.Center, Original.Center);
    
    // Test custom serialization methods
    FPOIData OriginalPOI;
    OriginalPOI.POIName = TEXT("SerializationTest");
    FGuid OriginalPOIGuid = OriginalPOI.POIId;
    
    TArray<uint8> POISerializedData;
    FMemoryWriter POIWriter(POISerializedData);
    OriginalPOI.Serialize(POIWriter);
    
    FPOIData DeserializedPOI;
    FMemoryReader POIReader(POISerializedData);
    DeserializedPOI.Serialize(POIReader);
    
    TestEqual("Custom serialization should preserve GUID", DeserializedPOI.POIId, OriginalPOIGuid);
    TestEqual("Custom serialization should preserve other data", DeserializedPOI.POIName, OriginalPOI.POIName);
    
    UE_LOG(LogTemp, Log, TEXT("Serialization behavior test passed - no changes to save file format"));
}