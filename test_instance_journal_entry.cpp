// Simple test to verify FInstanceJournalEntry initialization
#include "Data/InstancePersistence.h"
#include "Data/WorldGenTypes.h"

void TestFInstanceJournalEntryInitialization()
{
    // Test default constructor
    FInstanceJournalEntry DefaultEntry;
    check(DefaultEntry.InstanceId.IsValid());
    
    // Test parameterized constructor with PCGInstanceData
    FPCGInstanceData TestInstance;
    TestInstance.InstanceId = FGuid::NewGuid();
    FInstanceJournalEntry PCGEntry(TestInstance, EInstanceOperation::Add);
    check(PCGEntry.InstanceId.IsValid());
    check(PCGEntry.InstanceId == TestInstance.InstanceId);
    
    // Test parameterized constructor with POIData
    FPOIData TestPOI;
    TestPOI.POIId = FGuid::NewGuid();
    FInstanceJournalEntry POIEntry(TestPOI, EInstanceOperation::Add);
    check(POIEntry.InstanceId.IsValid());
    check(POIEntry.InstanceId == TestPOI.POIId);
    
    UE_LOG(LogTemp, Log, TEXT("FInstanceJournalEntry initialization test passed"));
}