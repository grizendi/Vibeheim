#include "Data/InstancePersistence.h"
#include "Services/PCGWorldService.h"
#include "WorldGenSettings.h"
#include "Engine/Engine.h"
#include "Misc/AutomationTest.h"

DEFINE_LOG_CATEGORY_STATIC(LogInstancePersistenceTest, Log, All);

#if WITH_DEV_AUTOMATION_TESTS

// Basic journal behavior
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWgPersistenceSmoke,
	"Vibeheim.WorldGen.Smoke.Persistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FWgPersistenceSmoke::RunTest(const FString& Parameters)
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
	TestEqual("Entry should be Add operation",
		static_cast<int32>(Journal.Entries[0].Operation),
		static_cast<int32>(EInstanceOperation::Add));

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
		static_cast<int32>(Journal.Entries[0].Operation),
		static_cast<int32>(EInstanceOperation::Remove));

	return true;
}

// POI journal behavior
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInstancePersistencePOITest,
	"Vibeheim.WorldGen.InstancePersistence.POI",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

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
	TestEqual("Entry should be Add operation",
		static_cast<int32>(Journal.Entries[0].Operation),
		static_cast<int32>(EInstanceOperation::Add));

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
		static_cast<int32>(Journal.Entries[0].Operation),
		static_cast<int32>(EInstanceOperation::Add));
	TestEqual("Compacted entry should have modified name",
		Journal.Entries[0].POIData.POIName, ModifiedPOI.POIName);

	return true;
}

// Manager behavior
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInstancePersistenceManagerTest,
	"Vibeheim.WorldGen.InstancePersistence.Manager",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

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

/**
 * Property 8: Delta System Isolation
 * Ensures baked/base instances are not persisted while player deltas are.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInstanceDeltaIsolationPropertyTest,
	"Vibeheim.WorldGen.InstancePersistence.Property.DeltaIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FInstanceDeltaIsolationPropertyTest::RunTest(const FString& Parameters)
{
	UInstancePersistenceManager* PersistenceManager = NewObject<UInstancePersistenceManager>();

	FWorldGenConfig TestConfig;
	TestConfig.Seed = 42;
	PersistenceManager->Initialize(TestConfig);

	const FTileCoord Tile(2, 2);

	FPCGInstanceData BaseInstance;
	BaseInstance.InstanceId = FGuid(1, 2, 3, 4);
	BaseInstance.OwningTile = Tile;

	PersistenceManager->RegisterBaseContent(Tile, { BaseInstance }, {});

	// Attempting to persist baked content should be ignored
	const bool bBaseAddLogged = PersistenceManager->AddInstanceOperation(Tile, BaseInstance, EInstanceOperation::Add);
	TestFalse(TEXT("Base PCG content is filtered from persistence"), bBaseAddLogged);

	// Player adds a new instance (should be persisted)
	FPCGInstanceData PlayerInstance;
	PlayerInstance.InstanceId = FGuid(10, 11, 12, 13);
	PlayerInstance.OwningTile = Tile;

	const bool bPlayerAdded = PersistenceManager->AddInstanceOperation(Tile, PlayerInstance, EInstanceOperation::Add);
	TestTrue(TEXT("Player-authored instance is persisted"), bPlayerAdded);

	// Player removes baked content (delta that must be persisted)
	const bool bBaseRemoved = PersistenceManager->AddInstanceOperation(Tile, BaseInstance, EInstanceOperation::Remove);
	TestTrue(TEXT("Base removal is recorded as a delta"), bBaseRemoved);

	const FTileInstanceJournal Journal = PersistenceManager->GetTileJournalData(Tile);
	TestEqual(TEXT("Only player deltas are stored"), Journal.Entries.Num(), 2);

	int32 AddCount = 0;
	int32 RemoveCount = 0;
	for (const FInstanceJournalEntry& Entry : Journal.Entries)
	{
		if (Entry.Operation == EInstanceOperation::Add)
		{
			++AddCount;
			TestEqual(TEXT("Add entry targets player instance"), Entry.InstanceId, PlayerInstance.InstanceId);
		}
		else if (Entry.Operation == EInstanceOperation::Remove)
		{
			++RemoveCount;
			TestEqual(TEXT("Remove entry targets baked instance"), Entry.InstanceId, BaseInstance.InstanceId);
		}
	}

	TestEqual(TEXT("One add delta recorded"), AddCount, 1);
	TestEqual(TEXT("One removal delta recorded"), RemoveCount, 1);

	return true;
}

/**
 * Property 9: Delta Application Determinism
 * Applying the same delta set twice yields identical final instance sets.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInstanceDeltaDeterminismPropertyTest,
	"Vibeheim.WorldGen.InstancePersistence.Property.DeltaDeterminism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FInstanceDeltaDeterminismPropertyTest::RunTest(const FString& Parameters)
{
	UInstancePersistenceManager* PersistenceManager = NewObject<UInstancePersistenceManager>();

	FWorldGenConfig TestConfig;
	TestConfig.Seed = 1337;
	PersistenceManager->Initialize(TestConfig);

	const FTileCoord Tile(3, 3);

	// Baseline baked content
	FPCGInstanceData BaseA;
	BaseA.InstanceId = FGuid(100, 200, 300, 400);
	BaseA.Location = FVector(10.0f, 10.0f, 0.0f);
	BaseA.OwningTile = Tile;

	FPCGInstanceData BaseB;
	BaseB.InstanceId = FGuid(101, 201, 301, 401);
	BaseB.Location = FVector(20.0f, 20.0f, 0.0f);
	BaseB.OwningTile = Tile;

	TArray<FPCGInstanceData> BaseInstances = { BaseA, BaseB };
	PersistenceManager->RegisterBaseContent(Tile, BaseInstances, {});

	// Player adds a new instance
	FPCGInstanceData PlayerAdd;
	PlayerAdd.InstanceId = FGuid(500, 600, 700, 800);
	PlayerAdd.Location = FVector(30.0f, 30.0f, 0.0f);
	PlayerAdd.OwningTile = Tile;
	PersistenceManager->AddInstanceOperation(Tile, PlayerAdd, EInstanceOperation::Add);

	// Player modifies base A
	FPCGInstanceData ModifiedBaseA = BaseA;
	ModifiedBaseA.Scale = FVector(2.0f, 2.0f, 2.0f);
	PersistenceManager->AddInstanceOperation(Tile, ModifiedBaseA, EInstanceOperation::Modify);

	// Player removes base B
	PersistenceManager->AddInstanceOperation(Tile, BaseB, EInstanceOperation::Remove);

	const FTileInstanceJournal Journal = PersistenceManager->GetTileJournalData(Tile);

	const auto FirstResult = PersistenceManager->ComputeDeltaApplication(Tile, Journal.Entries, BaseInstances, {});
	const auto SecondResult = PersistenceManager->ComputeDeltaApplication(Tile, Journal.Entries, BaseInstances, {});

	TestTrue(TEXT("First application succeeds"), FirstResult.bSuccess);
	TestTrue(TEXT("Second application succeeds"), SecondResult.bSuccess);
	TestEqual(TEXT("Deterministic instance counts"), FirstResult.FinalInstances.Num(), SecondResult.FinalInstances.Num());

	// Final set should contain modified base A and the player addition, but not base B
	TestEqual(TEXT("Final delta set size matches expectation"), FirstResult.FinalInstances.Num(), 2);

	// Compare IDs and ensure base removal stuck
	for (int32 Index = 0; Index < FirstResult.FinalInstances.Num(); ++Index)
	{
		const FPCGInstanceData& A = FirstResult.FinalInstances[Index];
		const FPCGInstanceData& B = SecondResult.FinalInstances[Index];
		TestEqual(TEXT("Deterministic ordering by GUID"), A.InstanceId, B.InstanceId);
		TestNotEqual(TEXT("Base B removed from final state"), A.InstanceId, BaseB.InstanceId);
	}

	// Ensure modified base data persisted
	const FPCGInstanceData* FinalBaseA = FirstResult.FinalInstances.FindByPredicate(
		[&](const FPCGInstanceData& Instance) { return Instance.InstanceId == BaseA.InstanceId; });
	TestNotNull(TEXT("Modified base A remains after delta application"), FinalBaseA);
	if (FinalBaseA)
	{
		TestEqual(TEXT("Modified scale is deterministic"), FinalBaseA->Scale, ModifiedBaseA.Scale);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

// Optional manual integration test you can call via console (e.g., with an exec function/commandlet)
void TestInstancePersistenceIntegration()
{
	UE_LOG(LogInstancePersistenceTest, Log, TEXT("Starting Instance Persistence Integration Test"));

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
	const bool bLoaded = PCGService->LoadTileWithPersistence(TestTile, EBiomeType::Meadows, TestHeightData);
	UE_LOG(LogInstancePersistenceTest, Log, TEXT("Loaded tile with persistence: %s"), bLoaded ? TEXT("Success") : TEXT("Failed"));

	// Add a new instance manually
	FPCGInstanceData NewInstance;
	NewInstance.Location = FVector(32.0f, 32.0f, 5.0f);
	NewInstance.Rotation = FRotator(0.0f, 45.0f, 0.0f);
	NewInstance.Scale = FVector(2.0f, 2.0f, 2.0f);
	NewInstance.bIsActive = true;

	const bool bInstanceAdded = PCGService->AddInstance(TestTile, NewInstance);
	UE_LOG(LogInstancePersistenceTest, Log, TEXT("Added new instance: %s"), bInstanceAdded ? TEXT("Success") : TEXT("Failed"));

	// Create and add a POI
	FPOIData NewPOI;
	NewPOI.POIName = TEXT("TestShrine");
	NewPOI.Location = FVector(16.0f, 16.0f, 1.0f);
	NewPOI.OriginBiome = EBiomeType::Meadows;

	const bool bPOIAdded = PCGService->AddPOI(NewPOI);
	UE_LOG(LogInstancePersistenceTest, Log, TEXT("Added new POI: %s"), bPOIAdded ? TEXT("Success") : TEXT("Failed"));

	// Get persistence stats
	const FString Stats = PersistenceManager->GetPersistenceStats();
	UE_LOG(LogInstancePersistenceTest, Log, TEXT("Persistence Stats:\n%s"), *Stats);

	UE_LOG(LogInstancePersistenceTest, Log, TEXT("Instance Persistence Integration Test Complete"));
}
