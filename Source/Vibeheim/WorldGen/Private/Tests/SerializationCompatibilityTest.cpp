#if WITH_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Data/WorldGenTypes.h"
#include "Data/InstancePersistence.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

/**
 * Test serialization compatibility and behavior changes for struct initialization fixes
 * Validates that save/load cycles maintain data integrity and GUID stability
 */
class FSerializationCompatibilityBase : public FAutomationTestBase
{
public:
    FSerializationCompatibilityBase(const FString& InName, bool bInComplex)
        : FAutomationTestBase(InName, bInComplex)
    {
    }

protected:
    void TestHeightfieldModificationSerialization();
    void TestInstanceJournalEntrySerialization();
    void TestPOIDataSerialization();
    void TestPCGInstanceDataSerialization();
    void TestContainerLookupStability();
    void TestGetTypeHashConsistency();
    void TestBinaryVsCustomSerialization();
};

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
    FSerializationCompatibilityTest,
    FSerializationCompatibilityBase,
    "Vibeheim.WorldGen.SerializationCompatibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSerializationCompatibilityTest::RunTest(const FString& /*Parameters*/)
{
    TestHeightfieldModificationSerialization();
    TestInstanceJournalEntrySerialization();
    TestPOIDataSerialization();
    TestPCGInstanceDataSerialization();
    TestContainerLookupStability();
    TestGetTypeHashConsistency();
    TestBinaryVsCustomSerialization();
    return true;
}

void FSerializationCompatibilityBase::TestHeightfieldModificationSerialization()
{
    // Create original struct with current initialization pattern
    FHeightfieldModification Original;
    Original.Center = FVector2D(100.0f, 200.0f);
    Original.Radius = 50.0f;
    Original.Strength = 0.8f;
    Original.Operation = EHeightfieldOperation::Add;
    Original.Timestamp = FDateTime::Now();
    Original.AffectedTile = FTileCoord(5, 10);
    
    // Store original GUID for comparison
    FGuid OriginalGuid = Original.ModificationId;
    TestTrue("Original ModificationId should be valid", OriginalGuid.IsValid());
    
    // Serialize to memory using reflection
    TArray<uint8> SerializedData;
    FMemoryWriter Writer(SerializedData);
    FHeightfieldModification::StaticStruct()->SerializeItem(Writer, &Original, nullptr);
    
    // Deserialize from memory using reflection
    FHeightfieldModification Deserialized;
    FMemoryReader Reader(SerializedData);
    FHeightfieldModification::StaticStruct()->SerializeItem(Reader, &Deserialized, nullptr);
    
    // Validate data integrity
    TestEqual("Center should match", Deserialized.Center, Original.Center);
    TestEqual("Radius should match", Deserialized.Radius, Original.Radius);
    TestEqual("Strength should match", Deserialized.Strength, Original.Strength);
    TestEqual("Operation should match", Deserialized.Operation, Original.Operation);
    TestEqual("AffectedTile should match", Deserialized.AffectedTile, Original.AffectedTile);
    TestEqual("ModificationId should be preserved", Deserialized.ModificationId, OriginalGuid);
    
    UE_LOG(LogTemp, Log, TEXT("FHeightfieldModification serialization test passed"));
}

void FSerializationCompatibilityBase::TestInstanceJournalEntrySerialization()
{
    // Create original struct
    FInstanceJournalEntry Original;
    Original.Operation = EInstanceOperation::Add;
    Original.Timestamp = FDateTime::Now().ToUnixTimestamp();
    Original.bIsPOI = false;
    Original.Version = 1;
    
    // Store original GUID
    FGuid OriginalGuid = Original.InstanceId;
    TestTrue("Original InstanceId should be valid", OriginalGuid.IsValid());
    
    // Serialize to memory using reflection
    TArray<uint8> SerializedData;
    FMemoryWriter Writer(SerializedData);
    FInstanceJournalEntry::StaticStruct()->SerializeItem(Writer, &Original, nullptr);
    
    // Deserialize from memory using reflection
    FInstanceJournalEntry Deserialized;
    FMemoryReader Reader(SerializedData);
    FInstanceJournalEntry::StaticStruct()->SerializeItem(Reader, &Deserialized, nullptr);
    
    // Validate data integrity
    TestEqual("Operation should match", Deserialized.Operation, Original.Operation);
    TestEqual("Timestamp should match", Deserialized.Timestamp, Original.Timestamp);
    TestEqual("bIsPOI should match", Deserialized.bIsPOI, Original.bIsPOI);
    TestEqual("Version should match", Deserialized.Version, Original.Version);
    TestEqual("InstanceId should be preserved", Deserialized.InstanceId, OriginalGuid);
    
    UE_LOG(LogTemp, Log, TEXT("FInstanceJournalEntry serialization test passed"));
}

void FSerializationCompatibilityBase::TestPOIDataSerialization()
{
    // Create original struct
    FPOIData Original;
    Original.POIName = TEXT("TestPOI");
    Original.Location = FVector(100.0f, 200.0f, 300.0f);
    Original.Rotation = FRotator(10.0f, 20.0f, 30.0f);
    Original.Scale = FVector(1.5f, 1.5f, 1.5f);
    Original.OriginBiome = EBiomeType::Forest;
    Original.bIsSpawned = true;
    
    // Store original GUID
    FGuid OriginalGuid = Original.POIId;
    TestTrue("Original POIId should be valid", OriginalGuid.IsValid());
    
    // Test custom serialization
    TArray<uint8> SerializedData;
    FMemoryWriter Writer(SerializedData);
    Original.Serialize(Writer);
    
    // Deserialize using custom method
    FPOIData Deserialized;
    FMemoryReader Reader(SerializedData);
    Deserialized.Serialize(Reader);
    
    // Validate data integrity
    TestEqual("POIName should match", Deserialized.POIName, Original.POIName);
    TestEqual("Location should match", Deserialized.Location, Original.Location);
    TestEqual("Rotation should match", Deserialized.Rotation, Original.Rotation);
    TestEqual("Scale should match", Deserialized.Scale, Original.Scale);
    TestEqual("OriginBiome should match", Deserialized.OriginBiome, Original.OriginBiome);
    TestEqual("bIsSpawned should match", Deserialized.bIsSpawned, Original.bIsSpawned);
    TestEqual("POIId should be preserved", Deserialized.POIId, OriginalGuid);
    
    UE_LOG(LogTemp, Log, TEXT("FPOIData serialization test passed"));
}

void FSerializationCompatibilityBase::TestPCGInstanceDataSerialization()
{
    // Create original struct
    FPCGInstanceData Original;
    Original.Location = FVector(50.0f, 100.0f, 150.0f);
    Original.Rotation = FRotator(5.0f, 10.0f, 15.0f);
    Original.Scale = FVector(0.8f, 0.8f, 0.8f);
    Original.bIsActive = true;
    Original.OwningTile = FTileCoord(3, 7);
    
    // Store original GUID
    FGuid OriginalGuid = Original.InstanceId;
    TestTrue("Original InstanceId should be valid", OriginalGuid.IsValid());
    
    // Test custom serialization
    TArray<uint8> SerializedData;
    FMemoryWriter Writer(SerializedData);
    Original.Serialize(Writer);
    
    // Deserialize using custom method
    FPCGInstanceData Deserialized;
    FMemoryReader Reader(SerializedData);
    Deserialized.Serialize(Reader);
    
    // Validate data integrity
    TestEqual("Location should match", Deserialized.Location, Original.Location);
    TestEqual("Rotation should match", Deserialized.Rotation, Original.Rotation);
    TestEqual("Scale should match", Deserialized.Scale, Original.Scale);
    TestEqual("bIsActive should match", Deserialized.bIsActive, Original.bIsActive);
    TestEqual("OwningTile should match", Deserialized.OwningTile, Original.OwningTile);
    TestEqual("InstanceId should be preserved", Deserialized.InstanceId, OriginalGuid);
    
    UE_LOG(LogTemp, Log, TEXT("FPCGInstanceData serialization test passed"));
}

void FSerializationCompatibilityBase::TestContainerLookupStability()
{
    // Test TMap lookup stability with FPOIData
    TMap<FGuid, FPOIData> POIMap;
    
    // Create test POI data
    FPOIData TestPOI;
    TestPOI.POIName = TEXT("TestPOI");
    TestPOI.Location = FVector(100.0f, 200.0f, 300.0f);
    FGuid TestPOIId = TestPOI.POIId;
    
    // Add to map
    POIMap.Add(TestPOIId, TestPOI);
    TestTrue("POI should be found in map before serialization", POIMap.Contains(TestPOIId));
    
    // Serialize the map
    TArray<uint8> SerializedMapData;
    FMemoryWriter MapWriter(SerializedMapData);
    MapWriter << POIMap;
    
    // Deserialize the map
    TMap<FGuid, FPOIData> DeserializedPOIMap;
    FMemoryReader MapReader(SerializedMapData);
    MapReader << DeserializedPOIMap;
    
    // Test lookup stability
    TestTrue("POI should be found in deserialized map", DeserializedPOIMap.Contains(TestPOIId));
    TestEqual("POI data should match after deserialization", 
        DeserializedPOIMap[TestPOIId].POIName, TestPOI.POIName);
    
    // Test TSet lookup stability with FPCGInstanceData
    TSet<FGuid> InstanceIdSet;
    
    FPCGInstanceData TestInstance;
    TestInstance.Location = FVector(50.0f, 100.0f, 150.0f);
    FGuid TestInstanceId = TestInstance.InstanceId;
    
    InstanceIdSet.Add(TestInstanceId);
    TestTrue("Instance ID should be found in set before serialization", InstanceIdSet.Contains(TestInstanceId));
    
    // Serialize the set
    TArray<uint8> SerializedSetData;
    FMemoryWriter SetWriter(SerializedSetData);
    SetWriter << InstanceIdSet;
    
    // Deserialize the set
    TSet<FGuid> DeserializedInstanceIdSet;
    FMemoryReader SetReader(SerializedSetData);
    SetReader << DeserializedInstanceIdSet;
    
    // Test lookup stability
    TestTrue("Instance ID should be found in deserialized set", DeserializedInstanceIdSet.Contains(TestInstanceId));
    
    UE_LOG(LogTemp, Log, TEXT("Container lookup stability test passed"));
}

void FSerializationCompatibilityBase::TestGetTypeHashConsistency()
{
    // Test GetTypeHash consistency for structs with GUID members
    
    // Create identical structs
    FHeightfieldModification Mod1;
    Mod1.Center = FVector2D(100.0f, 200.0f);
    Mod1.Radius = 50.0f;
    Mod1.ModificationId = FGuid::NewGuid();
    
    FHeightfieldModification Mod2 = Mod1; // Copy constructor
    
    // Hash should be identical for identical structs
    uint32 Hash1 = GetTypeHash(Mod1);
    uint32 Hash2 = GetTypeHash(Mod2);
    TestEqual("Hash should be identical for identical structs", Hash1, Hash2);
    
    // Test after serialization round-trip
    TArray<uint8> SerializedData;
    FMemoryWriter Writer(SerializedData);
    FHeightfieldModification::StaticStruct()->SerializeItem(Writer, &Mod1, nullptr);
    
    FHeightfieldModification Mod3;
    FMemoryReader Reader(SerializedData);
    FHeightfieldModification::StaticStruct()->SerializeItem(Reader, &Mod3, nullptr);
    
    uint32 Hash3 = GetTypeHash(Mod3);
    TestEqual("Hash should be identical after serialization round-trip", Hash1, Hash3);
    
    // Test POI data hash consistency
    FPOIData POI1;
    POI1.POIName = TEXT("TestPOI");
    POI1.Location = FVector(100.0f, 200.0f, 300.0f);
    
    FPOIData POI2 = POI1;
    
    // Note: FPOIData doesn't have a custom GetTypeHash, so it uses default struct hashing
    // This test ensures the GUID is consistently included in the hash
    uint32 POIHash1 = GetTypeHash(POI1.POIId);
    uint32 POIHash2 = GetTypeHash(POI2.POIId);
    TestEqual("POI GUID hash should be identical for copied structs", POIHash1, POIHash2);
    
    UE_LOG(LogTemp, Log, TEXT("GetTypeHash consistency test passed"));
}

void FSerializationCompatibilityBase::TestBinaryVsCustomSerialization()
{
    // Test FPOIData which has custom serialization
    FPOIData OriginalPOI;
    OriginalPOI.POIName = TEXT("TestPOI");
    OriginalPOI.Location = FVector(100.0f, 200.0f, 300.0f);
    OriginalPOI.OriginBiome = EBiomeType::Forest;
    
    // Test custom serialization
    TArray<uint8> CustomSerializedData;
    FMemoryWriter CustomWriter(CustomSerializedData);
    OriginalPOI.Serialize(CustomWriter);
    
    FPOIData CustomDeserialized;
    FMemoryReader CustomReader(CustomSerializedData);
    CustomDeserialized.Serialize(CustomReader);
    
    // Test binary serialization (using reflection)
    TArray<uint8> BinarySerializedData;
    FMemoryWriter BinaryWriter(BinarySerializedData);
    FPOIData::StaticStruct()->SerializeItem(BinaryWriter, &OriginalPOI, nullptr);
    
    FPOIData BinaryDeserialized;
    FMemoryReader BinaryReader(BinarySerializedData);
    FPOIData::StaticStruct()->SerializeItem(BinaryReader, &BinaryDeserialized, nullptr);
    
    // Both methods should preserve the GUID
    TestEqual("Custom serialization should preserve POIId", 
        CustomDeserialized.POIId, OriginalPOI.POIId);
    TestEqual("Binary serialization should preserve POIId", 
        BinaryDeserialized.POIId, OriginalPOI.POIId);
    
    // Both methods should preserve other data
    TestEqual("Custom serialization should preserve POIName", 
        CustomDeserialized.POIName, OriginalPOI.POIName);
    TestEqual("Binary serialization should preserve POIName", 
        BinaryDeserialized.POIName, OriginalPOI.POIName);
    
    // Test FPCGInstanceData which also has custom serialization
    FPCGInstanceData OriginalInstance;
    OriginalInstance.Location = FVector(50.0f, 100.0f, 150.0f);
    OriginalInstance.bIsActive = true;
    
    // Test custom serialization
    TArray<uint8> InstanceCustomData;
    FMemoryWriter InstanceCustomWriter(InstanceCustomData);
    OriginalInstance.Serialize(InstanceCustomWriter);
    
    FPCGInstanceData InstanceCustomDeserialized;
    FMemoryReader InstanceCustomReader(InstanceCustomData);
    InstanceCustomDeserialized.Serialize(InstanceCustomReader);
    
    TestEqual("Custom serialization should preserve InstanceId", 
        InstanceCustomDeserialized.InstanceId, OriginalInstance.InstanceId);
    TestEqual("Custom serialization should preserve Location", 
        InstanceCustomDeserialized.Location, OriginalInstance.Location);
    
    UE_LOG(LogTemp, Log, TEXT("Binary vs custom serialization test passed"));
}

#endif // WITH_AUTOMATION_TESTS