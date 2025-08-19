#include "Data/InstancePersistence.h"
#include "Services/PCGWorldService.h"
#include "WorldGenSettings.h"
#include "Engine/Engine.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInstancePersistenceBasicTest, "Vibeheim.WorldGen.InstancePersistence.BasicTest",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FInstancePersistenceBasicTest::RunTest(const FString& Parameters)
{
	// Test basic journal operations
	FTileCoord TestTileCoord(0, 0);
	FTileInstanceJournal Journal(TestTileCoord);
	
	// Create test instance data
	FPCGInstanceData TestInstance;
	TestInstance.Location = FVector(100.0f, 200.0f, 50.0f);
	TestInstance.Rotation = FRotator::ZeroRotator;
	TestInstance.Scale = FVector::OneVector;
	TestInstance.bIsActive = true;
	TestInstance.OwningTile = TestTileCoord;
	
	// Test adding instance operation
	FInstanceJournalEntry AddEntry(TestInstance, EInstanceOperation::Add);
	Journal.AddEntry(AddEntry);
	
	TestEqual("Journal should have one entry after add", Journal.Entries.Num(), 1);
	TestEqual("Entry should be Add operation", static_cast<int32>(Journal.Entries[0].Operation), static_cast<int32>(EInstanceOperation::Add));
	
	// Test journal integrity
	TestTrue("Journal integrity should be valid", Journal.ValidateIntegrity());
	
	// Test getting active instances
	TArray<FPCGInstanceData> ActiveInstances = Journal.GetActiveInstances();
	TestEqual("Should have one active instance", ActiveInstances.Num(), 1);
	TestEqual("Active instance location should match", ActiveInstances[0].Location, TestInstance.Location);
	
	// Test remove operation
	FInstanceJournalEntry RemoveEntry(TestInstance, EInstanceOperation::Remove);
	Journal.AddEntry(RemoveEntry);
	
	TestEqual("Journal should have two entries", Journal.Entries.Num(), 2);
	
	// After remove, should have no active instances
	ActiveInstances = Journal.GetActiveInstances();
	TestEqual("Should have no active instances after remove", ActiveInstances.Num(), 0);
	
	// Test journal compaction
	Journal.CompactJournal();
	TestEqual("Journal should be compacted to one remove entry", Journal.Entries.Num(), 1);
	TestEqual("Remaining entry should be Remove operation", 
		static_cast<int32>(Journal.Entries[0].Operation), static_cast<int32>(EInstanceOperation::Remove));
	
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInstancePersistencePOITest, "Vibeheim.WorldGen.InstancePersistence.POITest",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FInstancePersistencePOITest::RunTest(const FString& Parameters)
{
	// Test POI journal operations
	FTileCoord TestTileCoord(1, 1);
	FTileInstanceJournal Journal(TestTileCoord);
	
	// Create test POI data
	FPOIData TestPOI;
	TestPOI.POIName = TEXT("TestShrine");
	TestPOI.Location = FVector(64.0f, 64.0f, 10.0f);
	TestPOI.Rotation = FRotator::ZeroRotator;
	TestPOI.Scale = FVector::OneVector;
	TestPOI.OriginBiome = EBiomeType::Meadows;
	TestPOI.bIsSpawned = false;
	
	// Test adding POI operation
	FInstanceJournalEntry AddEntry(TestPOI, EInstanceOperation::Add);
	Journal.AddEntry(AddEntry);
	
	TestEqual("Journal should have one entry", Journal.Entries.Num(), 1);
	TestTrue("Entry should be POI", Journal.Entries[0].bIsPOI);
	TestEqual("Entry should be Add operation", static_cast<int32>(Journal.Entries[0].Operation), static_cast<int32>(EInstanceOperation::Add));
	
	// Test getting active POIs
	TArray<FPOIData> ActivePOIs = Journal.GetActivePOIs();
	TestEqual("Should have one active POI", ActivePOIs.Num(), 1);
	TestEqual("Active POI name should match", ActivePOIs[0].POIName, TestPOI.POIName);
	TestEqual("Active POI location should match", ActivePOIs[0].Location, TestPOI.Location);
	
	// Test modify operation
	FPOIData ModifiedPOI = TestPOI;
	ModifiedPOI.POIName = TEXT("ModifiedShrine");
	ModifiedPOI.Location = FVector(128.0f, 128.0f, 20.0f);
	
	FInstanceJournalEntry ModifyEntry(ModifiedPOI, EInstanceOperation::Modify);
	Journal.AddEntry(ModifyEntry);
	
	TestEqual("Journal should have two entries", Journal.Entries.Num(), 2);
	
	// After modify, should have one POI with modified data
	ActivePOIs = Journal.GetActivePOIs();
	TestEqual("Should still have one active POI", ActivePOIs.Num(), 1);
	TestEqual("POI name should be modified", ActivePOIs[0].POIName, ModifiedPOI.POIName);
	TestEqual("POI location should be modified", ActivePOIs[0].Location, ModifiedPOI.Location);
	
	// Test journal compaction
	Journal.CompactJournal();
	TestEqual("Journal should be compacted to one add entry", Journal.Entries.Num(), 1);
	TestEqual("Remaining entry should be Add operation with final state", 
		static_cast<int32>(Journal.Entries[0].Operation), static_cast<int32>(EInstanceOperation::Add));
	TestEqual("Compacted entry should have modified name", Journal.Entries[0].POIData.POIName, ModifiedPOI.POIName);
	
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInstancePersistenceManagerTest, "Vibeheim.WorldGen.InstancePersistence.ManagerTest",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FInstancePersistenceManagerTest::RunTest(const FString& Parameters)
{
	// Test persistence manager basic functionality
	UInstancePersistenceManager* PersistenceManager = NewObject<UInstancePersistenceManager>();
	
	// Initialize with test settings
	FWorldGenConfig TestConfig;
	TestConfig.Seed = 12345;
	TestConfig.TileSizeMeters = 64.0f;
	
	bool bInitialized = PersistenceManager->Initialize(TestConfig);
	TestTrue("Persistence manager should initialize successfully", bInitialized);
	
	// Test basic operations
	FTileCoord TestTile(0, 0);
	
	// Test that tile has no modifications initially
	TestFalse("Tile should have no persistent modifications initially", 
		PersistenceManager->HasPersistentModifications(TestTile));
	
	// Create test instance
	FPCGInstanceData TestInstance;
	TestInstance.Location = FVector(32.0f, 32.0f, 0.0f);
	TestInstance.Rotation = FRotator::ZeroRotator;
	TestInstance.Scale = FVector::OneVector;
	TestInstance.bIsActive = true;
	TestInstance.OwningTile = TestTile;
	
	// Add instance operation
	bool bAdded = PersistenceManager->AddInstanceOperation(TestTile, TestInstance, EInstanceOperation::Add);
	TestTrue("Should successfully add instance operation", bAdded);
	
	// Check that tile now has modifications
	TestTrue("Tile should have persistent modifications after add", 
		PersistenceManager->HasPersistentModifications(TestTile));
	
	// Test journal retrieval
	const FTileInstanceJournal* Journal = PersistenceManager->GetTileJournal(TestTile);
	TestNotNull("Should be able to retrieve journal", Journal);
	if (Journal)
	{
		TestEqual("Journal should have one entry", Journal->Entries.Num(), 1);
		TestEqual("Journal entry should match added instance", Journal->Entries[0].InstanceData.Location, TestInstance.Location);
	}
	
	// Test compaction
	PersistenceManager->CompactAllJournals();
	
	// Journal should still exist and be valid after compaction
	Journal = PersistenceManager->GetTileJournal(TestTile);
	TestNotNull("Journal should still exist after compaction", Journal);
	if (Journal)
	{
		TestEqual("Journal should still have one entry after compaction", Journal->Entries.Num(), 1);
	}
	
	return true;
}

DEFINE_LOG_CATEGORY_STATIC(LogInstancePersistenceTest, Log, All);

void TestInstancePersistenceIntegration()
{
	UE_LOG(LogInstancePersistenceTest, Log, TEXT("Starting Instance Persistence Integration Test"));
	
	// This function can be called from console commands for manual testing
	UInstancePersistenceManager* PersistenceManager = NewObject<UInstancePersistenceManager>();
	UPCGWorldService* PCGService = NewObject<UPCGWorldService>();
	
	// Initialize systems
	FWorldGenConfig TestConfig;
	TestConfig.Seed = 1337;
	TestConfig.TileSizeMeters = 64.0f;
	TestConfig.MaxHISMInstances = 1000;
	
	PersistenceManager->Initialize(TestConfig);
	PCGService->Initialize(TestConfig);
	PCGService->SetPersistenceManager(PersistenceManager);
	
	// Create test tile
	FTileCoord TestTile(0, 0);
	TArray<float> TestHeightData;
	TestHeightData.SetNum(64 * 64);
	for (int32 i = 0; i < TestHeightData.Num(); i++)
	{
		TestHeightData[i] = 0.0f; // Flat terrain for testing
	}
	
	// Load tile with persistence (should generate base content)
	bool bLoaded = PCGService->LoadTileWithPersistence(TestTile, EBiomeType::Meadows, TestHeightData);
	UE_LOG(LogInstancePersistenceTest, Log, TEXT("Loaded tile with persistence: %s"), bLoaded ? TEXT("Success") : TEXT("Failed"));
	
	// Add a new instance manually
	FPCGInstanceData NewInstance;
	NewInstance.Location = FVector(32.0f, 32.0f, 5.0f);
	NewInstance.Rotation = FRotator(0.0f, 45.0f, 0.0f);
	NewInstance.Scale = FVector(2.0f, 2.0f, 2.0f);
	NewInstance.bIsActive = true;
	
	bool bInstanceAdded = PCGService->AddInstance(TestTile, NewInstance);
	UE_LOG(LogInstancePersistenceTest, Log, TEXT("Added new instance: %s"), bInstanceAdded ? TEXT("Success") : TEXT("Failed"));
	
	// Create and add a POI
	FPOIData NewPOI;
	NewPOI.POIName = TEXT("TestShrine");
	NewPOI.Location = FVector(16.0f, 16.0f, 1.0f);
	NewPOI.OriginBiome = EBiomeType::Meadows;
	
	bool bPOIAdded = PCGService->AddPOI(NewPOI);
	UE_LOG(LogInstancePersistenceTest, Log, TEXT("Added new POI: %s"), bPOIAdded ? TEXT("Success") : TEXT("Failed"));
	
	// Get persistence stats
	FString Stats = PersistenceManager->GetPersistenceStats();
	UE_LOG(LogInstancePersistenceTest, Log, TEXT("Persistence Stats:\n%s"), *Stats);
	
	UE_LOG(LogInstancePersistenceTest, Log, TEXT("Instance Persistence Integration Test Complete"));
}
