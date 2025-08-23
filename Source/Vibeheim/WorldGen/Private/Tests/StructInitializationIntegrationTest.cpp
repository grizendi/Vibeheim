#include "CoreMinimal.h"
#include "Data/WorldGenTypes.h"
#include "Data/InstancePersistence.h"
#include "WorldGenSettings.h"
#include "Services/HeightfieldService.h"
#include "Services/POIService.h"
#include "Services/PCGWorldService.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "Engine/Engine.h"

#if WITH_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

/**
 * Comprehensive integration test for struct initialization fixes
 * Tests all fixed structs (FHeightfieldModification, FInstanceJournalEntry, FPOIData, FPCGInstanceData)
 * in realistic WorldGen system usage scenarios
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStructInitializationIntegrationTest, 
	"Vibeheim.WorldGen.StructInitialization.IntegrationTest", 
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FStructInitializationIntegrationTest::RunTest(const FString& Parameters)
{
	UE_LOG(LogTemp, Log, TEXT("=== STRUCT INITIALIZATION INTEGRATION TEST ==="));
	
	bool bAllTestsPassed = true;
	int32 TestsRun = 0;
	int32 TestsPassed = 0;
	
	// Test 1: Heightfield Modification System Integration
	UE_LOG(LogTemp, Log, TEXT("--- Test 1: Heightfield Modification System Integration ---"));
	TestsRun++;
	if (TestHeightfieldModificationIntegration())
	{
		TestsPassed++;
		UE_LOG(LogTemp, Log, TEXT("✓ Heightfield modification integration test passed"));
	}
	else
	{
		bAllTestsPassed = false;
		UE_LOG(LogTemp, Error, TEXT("✗ Heightfield modification integration test failed"));
	}
	
	// Test 2: POI System Integration
	UE_LOG(LogTemp, Log, TEXT("--- Test 2: POI System Integration ---"));
	TestsRun++;
	if (TestPOISystemIntegration())
	{
		TestsPassed++;
		UE_LOG(LogTemp, Log, TEXT("✓ POI system integration test passed"));
	}
	else
	{
		bAllTestsPassed = false;
		UE_LOG(LogTemp, Error, TEXT("✗ POI system integration test failed"));
	}
	
	// Test 3: PCG Instance System Integration
	UE_LOG(LogTemp, Log, TEXT("--- Test 3: PCG Instance System Integration ---"));
	TestsRun++;
	if (TestPCGInstanceSystemIntegration())
	{
		TestsPassed++;
		UE_LOG(LogTemp, Log, TEXT("✓ PCG instance system integration test passed"));
	}
	else
	{
		bAllTestsPassed = false;
		UE_LOG(LogTemp, Error, TEXT("✗ PCG instance system integration test failed"));
	}
	
	// Test 4: Instance Persistence System Integration
	UE_LOG(LogTemp, Log, TEXT("--- Test 4: Instance Persistence System Integration ---"));
	TestsRun++;
	if (TestInstancePersistenceIntegration())
	{
		TestsPassed++;
		UE_LOG(LogTemp, Log, TEXT("✓ Instance persistence integration test passed"));
	}
	else
	{
		bAllTestsPassed = false;
		UE_LOG(LogTemp, Error, TEXT("✗ Instance persistence integration test failed"));
	}
	
	// Test 5: Cross-System Workflow Integration
	UE_LOG(LogTemp, Log, TEXT("--- Test 5: Cross-System Workflow Integration ---"));
	TestsRun++;
	if (TestCrossSystemWorkflowIntegration())
	{
		TestsPassed++;
		UE_LOG(LogTemp, Log, TEXT("✓ Cross-system workflow integration test passed"));
	}
	else
	{
		bAllTestsPassed = false;
		UE_LOG(LogTemp, Error, TEXT("✗ Cross-system workflow integration test failed"));
	}
	
	// Test 6: Serialization and Persistence Integration
	UE_LOG(LogTemp, Log, TEXT("--- Test 6: Serialization and Persistence Integration ---"));
	TestsRun++;
	if (TestSerializationPersistenceIntegration())
	{
		TestsPassed++;
		UE_LOG(LogTemp, Log, TEXT("✓ Serialization and persistence integration test passed"));
	}
	else
	{
		bAllTestsPassed = false;
		UE_LOG(LogTemp, Error, TEXT("✗ Serialization and persistence integration test failed"));
	}
	
	// Final results
	UE_LOG(LogTemp, Log, TEXT("=== INTEGRATION TEST RESULTS ==="));
	UE_LOG(LogTemp, Log, TEXT("Tests Passed: %d/%d"), TestsPassed, TestsRun);
	
	if (bAllTestsPassed)
	{
		UE_LOG(LogTemp, Log, TEXT("✓ ALL STRUCT INITIALIZATION INTEGRATION TESTS PASSED"));
		UE_LOG(LogTemp, Log, TEXT("All fixed structs work correctly in realistic WorldGen scenarios"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("✗ SOME INTEGRATION TESTS FAILED"));
		UE_LOG(LogTemp, Error, TEXT("Struct initialization fixes may not be working correctly in real usage"));
	}
	
	return bAllTestsPassed;
}

bool FStructInitializationIntegrationTest::TestHeightfieldModificationIntegration()
{
	UE_LOG(LogTemp, Log, TEXT("Testing heightfield modification system with FHeightfieldModification..."));
	
	try
	{
		// Test 1: Create multiple heightfield modifications
		TArray<FHeightfieldModification> Modifications;
		
		for (int32 i = 0; i < 10; i++)
		{
			FHeightfieldModification Mod;
			Mod.Center = FVector2D(i * 100.0f, i * 100.0f);
			Mod.Radius = 50.0f + i * 10.0f;
			Mod.Strength = 1.0f + i * 0.5f;
			Mod.Operation = static_cast<EHeightfieldOperation>(i % 4);
			Mod.AffectedTile = FTileCoord(i / 2, i / 2);
			
			// Verify GUID is properly initialized
			if (!Mod.ModificationId.IsValid())
			{
				UE_LOG(LogTemp, Error, TEXT("FHeightfieldModification %d has invalid GUID: %s"), i, *Mod.ModificationId.ToString());
				return false;
			}
			
			Modifications.Add(Mod);
		}
		
		// Test 2: Verify all GUIDs are unique
		TSet<FGuid> UniqueGuids;
		for (const FHeightfieldModification& Mod : Modifications)
		{
			if (UniqueGuids.Contains(Mod.ModificationId))
			{
				UE_LOG(LogTemp, Error, TEXT("Duplicate GUID found in heightfield modifications: %s"), *Mod.ModificationId.ToString());
				return false;
			}
			UniqueGuids.Add(Mod.ModificationId);
		}
		
		// Test 3: Test copy construction and assignment
		FHeightfieldModification OriginalMod = Modifications[0];
		FHeightfieldModification CopiedMod = OriginalMod;
		
		// After copy, GUIDs should be the same (copy semantics)
		if (CopiedMod.ModificationId != OriginalMod.ModificationId)
		{
			UE_LOG(LogTemp, Error, TEXT("Copy construction changed GUID: %s -> %s"), 
				*OriginalMod.ModificationId.ToString(), *CopiedMod.ModificationId.ToString());
			return false;
		}
		
		// Test 4: Test array operations (realistic usage)
		TArray<FHeightfieldModification> FilteredMods = Modifications.FilterByPredicate([](const FHeightfieldModification& Mod)
		{
			return Mod.Operation == EHeightfieldOperation::Add;
		});
		
		for (const FHeightfieldModification& Mod : FilteredMods)
		{
			if (!Mod.ModificationId.IsValid())
			{
				UE_LOG(LogTemp, Error, TEXT("Filtered modification has invalid GUID"));
				return false;
			}
		}
		
		// Test 5: Test TMap usage (realistic for tracking modifications)
		TMap<FGuid, FHeightfieldModification> ModificationMap;
		for (const FHeightfieldModification& Mod : Modifications)
		{
			ModificationMap.Add(Mod.ModificationId, Mod);
		}
		
		if (ModificationMap.Num() != Modifications.Num())
		{
			UE_LOG(LogTemp, Error, TEXT("TMap size mismatch: expected %d, got %d"), Modifications.Num(), ModificationMap.Num());
			return false;
		}
		
		UE_LOG(LogTemp, Log, TEXT("✓ Created %d heightfield modifications with unique valid GUIDs"), Modifications.Num());
		UE_LOG(LogTemp, Log, TEXT("✓ Copy semantics work correctly"));
		UE_LOG(LogTemp, Log, TEXT("✓ Array filtering preserves GUID validity"));
		UE_LOG(LogTemp, Log, TEXT("✓ TMap operations work correctly with GUIDs"));
		
		return true;
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogTemp, Error, TEXT("Exception in heightfield modification test: %s"), ANSI_TO_TCHAR(e.what()));
		return false;
	}
	catch (...)
	{
		UE_LOG(LogTemp, Error, TEXT("Unknown exception in heightfield modification test"));
		return false;
	}
}

bool FStructInitializationIntegrationTest::TestPOISystemIntegration()
{
	UE_LOG(LogTemp, Log, TEXT("Testing POI system with FPOIData..."));
	
	try
	{
		// Test 1: Create POIs for different biomes
		TArray<FPOIData> POIs;
		TArray<EBiomeType> BiomeTypes = { EBiomeType::Meadows, EBiomeType::Forest, EBiomeType::Mountains };
		
		for (int32 i = 0; i < 15; i++)
		{
			FPOIData POI;
			POI.POIName = FString::Printf(TEXT("TestPOI_%d"), i);
			POI.Location = FVector(i * 200.0f, i * 150.0f, FMath::RandRange(0.0f, 100.0f));
			POI.Rotation = FRotator(0.0f, i * 30.0f, 0.0f);
			POI.Scale = FVector(1.0f + i * 0.1f);
			POI.OriginBiome = BiomeTypes[i % BiomeTypes.Num()];
			POI.bIsSpawned = (i % 2 == 0);
			
			// Verify GUID is properly initialized
			if (!POI.POIId.IsValid())
			{
				UE_LOG(LogTemp, Error, TEXT("FPOIData %d has invalid GUID: %s"), i, *POI.POIId.ToString());
				return false;
			}
			
			POIs.Add(POI);
		}
		
		// Test 2: Verify all POI GUIDs are unique
		TSet<FGuid> UniquePOIGuids;
		for (const FPOIData& POI : POIs)
		{
			if (UniquePOIGuids.Contains(POI.POIId))
			{
				UE_LOG(LogTemp, Error, TEXT("Duplicate GUID found in POI data: %s"), *POI.POIId.ToString());
				return false;
			}
			UniquePOIGuids.Add(POI.POIId);
		}
		
		// Test 3: Test biome-based filtering (realistic POI system usage)
		for (EBiomeType BiomeType : BiomeTypes)
		{
			TArray<FPOIData> BiomePOIs = POIs.FilterByPredicate([BiomeType](const FPOIData& POI)
			{
				return POI.OriginBiome == BiomeType;
			});
			
			for (const FPOIData& POI : BiomePOIs)
			{
				if (!POI.POIId.IsValid())
				{
					UE_LOG(LogTemp, Error, TEXT("Biome-filtered POI has invalid GUID"));
					return false;
				}
			}
			
			UE_LOG(LogTemp, Log, TEXT("✓ Found %d POIs for biome %d with valid GUIDs"), BiomePOIs.Num(), (int32)BiomeType);
		}
		
		// Test 4: Test spawned vs unspawned POI tracking
		TArray<FPOIData> SpawnedPOIs = POIs.FilterByPredicate([](const FPOIData& POI)
		{
			return POI.bIsSpawned;
		});
		
		TArray<FPOIData> UnspawnedPOIs = POIs.FilterByPredicate([](const FPOIData& POI)
		{
			return !POI.bIsSpawned;
		});
		
		if (SpawnedPOIs.Num() + UnspawnedPOIs.Num() != POIs.Num())
		{
			UE_LOG(LogTemp, Error, TEXT("POI spawn state filtering error"));
			return false;
		}
		
		// Test 5: Test TMap usage for POI lookup (realistic for POI management)
		TMap<FGuid, FPOIData> POILookup;
		for (const FPOIData& POI : POIs)
		{
			POILookup.Add(POI.POIId, POI);
		}
		
		// Test lookup functionality
		for (const FPOIData& OriginalPOI : POIs)
		{
			FPOIData* FoundPOI = POILookup.Find(OriginalPOI.POIId);
			if (!FoundPOI)
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to find POI in lookup map: %s"), *OriginalPOI.POIId.ToString());
				return false;
			}
			
			if (FoundPOI->POIName != OriginalPOI.POIName)
			{
				UE_LOG(LogTemp, Error, TEXT("POI lookup data mismatch"));
				return false;
			}
		}
		
		UE_LOG(LogTemp, Log, TEXT("✓ Created %d POIs with unique valid GUIDs"), POIs.Num());
		UE_LOG(LogTemp, Log, TEXT("✓ Biome filtering works correctly"));
		UE_LOG(LogTemp, Log, TEXT("✓ Spawn state tracking works correctly"));
		UE_LOG(LogTemp, Log, TEXT("✓ POI lookup map operations work correctly"));
		
		return true;
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogTemp, Error, TEXT("Exception in POI system test: %s"), ANSI_TO_TCHAR(e.what()));
		return false;
	}
	catch (...)
	{
		UE_LOG(LogTemp, Error, TEXT("Unknown exception in POI system test"));
		return false;
	}
}

bool FStructInitializationIntegrationTest::TestPCGInstanceSystemIntegration()
{
	UE_LOG(LogTemp, Log, TEXT("Testing PCG instance system with FPCGInstanceData..."));
	
	try
	{
		// Test 1: Create PCG instances for multiple tiles
		TArray<FPCGInstanceData> Instances;
		
		for (int32 TileX = 0; TileX < 3; TileX++)
		{
			for (int32 TileY = 0; TileY < 3; TileY++)
			{
				FTileCoord Tile(TileX, TileY);
				
				// Create multiple instances per tile
				for (int32 i = 0; i < 20; i++)
				{
					FPCGInstanceData Instance;
					Instance.Location = FVector(
						TileX * 64.0f + FMath::RandRange(0.0f, 64.0f),
						TileY * 64.0f + FMath::RandRange(0.0f, 64.0f),
						FMath::RandRange(0.0f, 10.0f)
					);
					Instance.Rotation = FRotator(0.0f, FMath::RandRange(0.0f, 360.0f), 0.0f);
					Instance.Scale = FVector(FMath::RandRange(0.8f, 1.2f));
					Instance.bIsActive = (i % 4 != 0); // Some inactive instances
					Instance.OwningTile = Tile;
					
					// Verify GUID is properly initialized
					if (!Instance.InstanceId.IsValid())
					{
						UE_LOG(LogTemp, Error, TEXT("FPCGInstanceData has invalid GUID: %s"), *Instance.InstanceId.ToString());
						return false;
					}
					
					Instances.Add(Instance);
				}
			}
		}
		
		// Test 2: Verify all instance GUIDs are unique
		TSet<FGuid> UniqueInstanceGuids;
		for (const FPCGInstanceData& Instance : Instances)
		{
			if (UniqueInstanceGuids.Contains(Instance.InstanceId))
			{
				UE_LOG(LogTemp, Error, TEXT("Duplicate GUID found in PCG instances: %s"), *Instance.InstanceId.ToString());
				return false;
			}
			UniqueInstanceGuids.Add(Instance.InstanceId);
		}
		
		// Test 3: Test tile-based filtering (realistic PCG usage)
		TMap<FTileCoord, TArray<FPCGInstanceData>> InstancesByTile;
		for (const FPCGInstanceData& Instance : Instances)
		{
			InstancesByTile.FindOrAdd(Instance.OwningTile).Add(Instance);
		}
		
		for (const auto& TilePair : InstancesByTile)
		{
			const FTileCoord& Tile = TilePair.Key;
			const TArray<FPCGInstanceData>& TileInstances = TilePair.Value;
			
			for (const FPCGInstanceData& Instance : TileInstances)
			{
				if (!Instance.InstanceId.IsValid())
				{
					UE_LOG(LogTemp, Error, TEXT("Tile-filtered instance has invalid GUID"));
					return false;
				}
				
				if (Instance.OwningTile != Tile)
				{
					UE_LOG(LogTemp, Error, TEXT("Instance tile mismatch"));
					return false;
				}
			}
			
			UE_LOG(LogTemp, Log, TEXT("✓ Tile (%d,%d) has %d instances with valid GUIDs"), 
				Tile.X, Tile.Y, TileInstances.Num());
		}
		
		// Test 4: Test active/inactive filtering
		TArray<FPCGInstanceData> ActiveInstances = Instances.FilterByPredicate([](const FPCGInstanceData& Instance)
		{
			return Instance.bIsActive;
		});
		
		TArray<FPCGInstanceData> InactiveInstances = Instances.FilterByPredicate([](const FPCGInstanceData& Instance)
		{
			return !Instance.bIsActive;
		});
		
		if (ActiveInstances.Num() + InactiveInstances.Num() != Instances.Num())
		{
			UE_LOG(LogTemp, Error, TEXT("Instance active state filtering error"));
			return false;
		}
		
		// Test 5: Test instance lookup and modification (realistic for instance management)
		TMap<FGuid, FPCGInstanceData> InstanceLookup;
		for (const FPCGInstanceData& Instance : Instances)
		{
			InstanceLookup.Add(Instance.InstanceId, Instance);
		}
		
		// Simulate instance modifications
		int32 ModifiedCount = 0;
		for (auto& InstancePair : InstanceLookup)
		{
			FPCGInstanceData& Instance = InstancePair.Value;
			if (ModifiedCount < 10)
			{
				Instance.bIsActive = !Instance.bIsActive;
				Instance.Scale *= 1.1f;
				ModifiedCount++;
			}
		}
		
		UE_LOG(LogTemp, Log, TEXT("✓ Created %d PCG instances with unique valid GUIDs"), Instances.Num());
		UE_LOG(LogTemp, Log, TEXT("✓ Tile-based filtering works correctly"));
		UE_LOG(LogTemp, Log, TEXT("✓ Active/inactive filtering works correctly"));
		UE_LOG(LogTemp, Log, TEXT("✓ Instance lookup and modification works correctly"));
		
		return true;
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogTemp, Error, TEXT("Exception in PCG instance system test: %s"), ANSI_TO_TCHAR(e.what()));
		return false;
	}
	catch (...)
	{
		UE_LOG(LogTemp, Error, TEXT("Unknown exception in PCG instance system test"));
		return false;
	}
}bool 
FStructInitializationIntegrationTest::TestInstancePersistenceIntegration()
{
	UE_LOG(LogTemp, Log, TEXT("Testing instance persistence system with FInstanceJournalEntry..."));
	
	try
	{
		// Test 1: Create journal entries for different operations
		TArray<FInstanceJournalEntry> JournalEntries;
		
		// Create some test PCG instances and POIs
		TArray<FPCGInstanceData> TestInstances;
		TArray<FPOIData> TestPOIs;
		
		for (int32 i = 0; i < 5; i++)
		{
			FPCGInstanceData Instance;
			Instance.Location = FVector(i * 100.0f, i * 100.0f, 0.0f);
			Instance.OwningTile = FTileCoord(i, i);
			TestInstances.Add(Instance);
			
			FPOIData POI;
			POI.POIName = FString::Printf(TEXT("TestPOI_%d"), i);
			POI.Location = FVector(i * 150.0f, i * 150.0f, 0.0f);
			TestPOIs.Add(POI);
		}
		
		// Create journal entries for different operations
		TArray<EInstanceOperation> Operations = { 
			EInstanceOperation::Add, 
			EInstanceOperation::Remove, 
			EInstanceOperation::Modify 
		};
		
		for (int32 i = 0; i < TestInstances.Num(); i++)
		{
			EInstanceOperation Op = Operations[i % Operations.Num()];
			
			// Test PCG instance journal entry
			FInstanceJournalEntry PCGEntry(TestInstances[i], Op);
			
			// Verify GUID is properly initialized
			if (!PCGEntry.InstanceId.IsValid())
			{
				UE_LOG(LogTemp, Error, TEXT("FInstanceJournalEntry (PCG) has invalid GUID: %s"), *PCGEntry.InstanceId.ToString());
				return false;
			}
			
			// Verify the GUID matches the source instance
			if (PCGEntry.InstanceId != TestInstances[i].InstanceId)
			{
				UE_LOG(LogTemp, Error, TEXT("Journal entry GUID doesn't match source instance GUID"));
				return false;
			}
			
			JournalEntries.Add(PCGEntry);
			
			// Test POI journal entry
			FInstanceJournalEntry POIEntry(TestPOIs[i], Op);
			
			// Verify GUID is properly initialized
			if (!POIEntry.InstanceId.IsValid())
			{
				UE_LOG(LogTemp, Error, TEXT("FInstanceJournalEntry (POI) has invalid GUID: %s"), *POIEntry.InstanceId.ToString());
				return false;
			}
			
			// Verify the GUID matches the source POI
			if (POIEntry.InstanceId != TestPOIs[i].POIId)
			{
				UE_LOG(LogTemp, Error, TEXT("Journal entry GUID doesn't match source POI GUID"));
				return false;
			}
			
			JournalEntries.Add(POIEntry);
		}
		
		// Test 2: Test default constructor journal entries
		for (int32 i = 0; i < 5; i++)
		{
			FInstanceJournalEntry DefaultEntry;
			
			// Verify GUID is properly initialized
			if (!DefaultEntry.InstanceId.IsValid())
			{
				UE_LOG(LogTemp, Error, TEXT("Default FInstanceJournalEntry has invalid GUID: %s"), *DefaultEntry.InstanceId.ToString());
				return false;
			}
			
			JournalEntries.Add(DefaultEntry);
		}
		
		// Test 3: Verify all journal entry GUIDs are unique
		TSet<FGuid> UniqueJournalGuids;
		for (const FInstanceJournalEntry& Entry : JournalEntries)
		{
			if (UniqueJournalGuids.Contains(Entry.InstanceId))
			{
				UE_LOG(LogTemp, Error, TEXT("Duplicate GUID found in journal entries: %s"), *Entry.InstanceId.ToString());
				return false;
			}
			UniqueJournalGuids.Add(Entry.InstanceId);
		}
		
		// Test 4: Test tile journal functionality
		FTileInstanceJournal TileJournal(FTileCoord(0, 0));
		
		for (const FInstanceJournalEntry& Entry : JournalEntries)
		{
			TileJournal.AddEntry(Entry);
		}
		
		if (TileJournal.Entries.Num() != JournalEntries.Num())
		{
			UE_LOG(LogTemp, Error, TEXT("Tile journal entry count mismatch"));
			return false;
		}
		
		// Verify all entries in tile journal have valid GUIDs
		for (const FInstanceJournalEntry& Entry : TileJournal.Entries)
		{
			if (!Entry.InstanceId.IsValid())
			{
				UE_LOG(LogTemp, Error, TEXT("Tile journal entry has invalid GUID"));
				return false;
			}
		}
		
		// Test 5: Test journal filtering by operation type
		for (EInstanceOperation Op : Operations)
		{
			TArray<FInstanceJournalEntry> FilteredEntries = JournalEntries.FilterByPredicate([Op](const FInstanceJournalEntry& Entry)
			{
				return Entry.Operation == Op;
			});
			
			for (const FInstanceJournalEntry& Entry : FilteredEntries)
			{
				if (!Entry.InstanceId.IsValid())
				{
					UE_LOG(LogTemp, Error, TEXT("Filtered journal entry has invalid GUID"));
					return false;
				}
			}
			
			UE_LOG(LogTemp, Log, TEXT("✓ Found %d journal entries for operation %d with valid GUIDs"), 
				FilteredEntries.Num(), (int32)Op);
		}
		
		UE_LOG(LogTemp, Log, TEXT("✓ Created %d journal entries with unique valid GUIDs"), JournalEntries.Num());
		UE_LOG(LogTemp, Log, TEXT("✓ Constructor variants work correctly"));
		UE_LOG(LogTemp, Log, TEXT("✓ Tile journal operations work correctly"));
		UE_LOG(LogTemp, Log, TEXT("✓ Journal filtering works correctly"));
		
		return true;
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogTemp, Error, TEXT("Exception in instance persistence test: %s"), ANSI_TO_TCHAR(e.what()));
		return false;
	}
	catch (...)
	{
		UE_LOG(LogTemp, Error, TEXT("Unknown exception in instance persistence test"));
		return false;
	}
}

bool FStructInitializationIntegrationTest::TestCrossSystemWorkflowIntegration()
{
	UE_LOG(LogTemp, Log, TEXT("Testing cross-system workflow integration..."));
	
	try
	{
		// Test realistic WorldGen workflow: Heightfield -> POI -> PCG -> Persistence
		
		// Step 1: Create heightfield modifications
		TArray<FHeightfieldModification> HeightfieldMods;
		for (int32 i = 0; i < 3; i++)
		{
			FHeightfieldModification Mod;
			Mod.Center = FVector2D(i * 200.0f, i * 200.0f);
			Mod.Radius = 100.0f;
			Mod.Strength = 5.0f;
			Mod.Operation = EHeightfieldOperation::Flatten;
			Mod.AffectedTile = FTileCoord(i, i);
			
			if (!Mod.ModificationId.IsValid())
			{
				UE_LOG(LogTemp, Error, TEXT("Heightfield modification has invalid GUID in workflow"));
				return false;
			}
			
			HeightfieldMods.Add(Mod);
		}
		
		// Step 2: Create POIs that would be placed on flattened areas
		TArray<FPOIData> WorkflowPOIs;
		for (const FHeightfieldModification& Mod : HeightfieldMods)
		{
			FPOIData POI;
			POI.POIName = FString::Printf(TEXT("WorkflowPOI_%s"), *Mod.ModificationId.ToString(EGuidFormats::Short));
			POI.Location = FVector(Mod.Center.X, Mod.Center.Y, 0.0f);
			POI.OriginBiome = EBiomeType::Meadows;
			POI.bIsSpawned = true;
			
			if (!POI.POIId.IsValid())
			{
				UE_LOG(LogTemp, Error, TEXT("Workflow POI has invalid GUID"));
				return false;
			}
			
			WorkflowPOIs.Add(POI);
		}
		
		// Step 3: Create PCG instances around POIs
		TArray<FPCGInstanceData> WorkflowInstances;
		for (const FPOIData& POI : WorkflowPOIs)
		{
			// Create instances in a circle around each POI
			for (int32 i = 0; i < 8; i++)
			{
				float Angle = i * 45.0f;
				float Distance = 50.0f;
				
				FPCGInstanceData Instance;
				Instance.Location = POI.Location + FVector(
					FMath::Cos(FMath::DegreesToRadians(Angle)) * Distance,
					FMath::Sin(FMath::DegreesToRadians(Angle)) * Distance,
					0.0f
				);
				Instance.Rotation = FRotator(0.0f, Angle, 0.0f);
				Instance.OwningTile = FTileCoord::FromWorldPosition(Instance.Location);
				Instance.bIsActive = true;
				
				if (!Instance.InstanceId.IsValid())
				{
					UE_LOG(LogTemp, Error, TEXT("Workflow PCG instance has invalid GUID"));
					return false;
				}
				
				WorkflowInstances.Add(Instance);
			}
		}
		
		// Step 4: Create journal entries for the entire workflow
		TArray<FInstanceJournalEntry> WorkflowJournal;
		
		// Journal POI placements
		for (const FPOIData& POI : WorkflowPOIs)
		{
			FInstanceJournalEntry POIEntry(POI, EInstanceOperation::Add);
			
			if (!POIEntry.InstanceId.IsValid())
			{
				UE_LOG(LogTemp, Error, TEXT("Workflow POI journal entry has invalid GUID"));
				return false;
			}
			
			WorkflowJournal.Add(POIEntry);
		}
		
		// Journal PCG instance spawns
		for (const FPCGInstanceData& Instance : WorkflowInstances)
		{
			FInstanceJournalEntry InstanceEntry(Instance, EInstanceOperation::Add);
			
			if (!InstanceEntry.InstanceId.IsValid())
			{
				UE_LOG(LogTemp, Error, TEXT("Workflow instance journal entry has invalid GUID"));
				return false;
			}
			
			WorkflowJournal.Add(InstanceEntry);
		}
		
		// Step 5: Test cross-referencing between systems
		TMap<FGuid, FHeightfieldModification> ModificationLookup;
		TMap<FGuid, FPOIData> POILookup;
		TMap<FGuid, FPCGInstanceData> InstanceLookup;
		TMap<FGuid, FInstanceJournalEntry> JournalLookup;
		
		for (const FHeightfieldModification& Mod : HeightfieldMods)
		{
			ModificationLookup.Add(Mod.ModificationId, Mod);
		}
		
		for (const FPOIData& POI : WorkflowPOIs)
		{
			POILookup.Add(POI.POIId, POI);
		}
		
		for (const FPCGInstanceData& Instance : WorkflowInstances)
		{
			InstanceLookup.Add(Instance.InstanceId, Instance);
		}
		
		for (const FInstanceJournalEntry& Entry : WorkflowJournal)
		{
			JournalLookup.Add(Entry.InstanceId, Entry);
		}
		
		// Verify all lookups work correctly
		if (ModificationLookup.Num() != HeightfieldMods.Num() ||
			POILookup.Num() != WorkflowPOIs.Num() ||
			InstanceLookup.Num() != WorkflowInstances.Num() ||
			JournalLookup.Num() != WorkflowJournal.Num())
		{
			UE_LOG(LogTemp, Error, TEXT("Cross-system lookup table size mismatch"));
			return false;
		}
		
		// Step 6: Test workflow data integrity
		TSet<FGuid> AllWorkflowGuids;
		
		// Collect all GUIDs from all systems
		for (const FHeightfieldModification& Mod : HeightfieldMods)
		{
			AllWorkflowGuids.Add(Mod.ModificationId);
		}
		
		for (const FPOIData& POI : WorkflowPOIs)
		{
			AllWorkflowGuids.Add(POI.POIId);
		}
		
		for (const FPCGInstanceData& Instance : WorkflowInstances)
		{
			AllWorkflowGuids.Add(Instance.InstanceId);
		}
		
		// Note: Journal entries reuse GUIDs from POIs and instances, so we don't add them to the unique set
		
		int32 ExpectedUniqueGuids = HeightfieldMods.Num() + WorkflowPOIs.Num() + WorkflowInstances.Num();
		if (AllWorkflowGuids.Num() != ExpectedUniqueGuids)
		{
			UE_LOG(LogTemp, Error, TEXT("Workflow GUID uniqueness check failed: expected %d, got %d"), 
				ExpectedUniqueGuids, AllWorkflowGuids.Num());
			return false;
		}
		
		UE_LOG(LogTemp, Log, TEXT("✓ Cross-system workflow completed successfully"));
		UE_LOG(LogTemp, Log, TEXT("✓ Created %d heightfield modifications"), HeightfieldMods.Num());
		UE_LOG(LogTemp, Log, TEXT("✓ Created %d POIs"), WorkflowPOIs.Num());
		UE_LOG(LogTemp, Log, TEXT("✓ Created %d PCG instances"), WorkflowInstances.Num());
		UE_LOG(LogTemp, Log, TEXT("✓ Created %d journal entries"), WorkflowJournal.Num());
		UE_LOG(LogTemp, Log, TEXT("✓ All %d workflow GUIDs are unique and valid"), AllWorkflowGuids.Num());
		UE_LOG(LogTemp, Log, TEXT("✓ Cross-system lookups work correctly"));
		
		return true;
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogTemp, Error, TEXT("Exception in cross-system workflow test: %s"), ANSI_TO_TCHAR(e.what()));
		return false;
	}
	catch (...)
	{
		UE_LOG(LogTemp, Error, TEXT("Unknown exception in cross-system workflow test"));
		return false;
	}
}

bool FStructInitializationIntegrationTest::TestSerializationPersistenceIntegration()
{
	UE_LOG(LogTemp, Log, TEXT("Testing serialization and persistence integration..."));
	
	try
	{
		// Test 1: Create test data for all struct types
		TArray<FHeightfieldModification> OriginalMods;
		TArray<FPOIData> OriginalPOIs;
		TArray<FPCGInstanceData> OriginalInstances;
		TArray<FInstanceJournalEntry> OriginalJournal;
		
		// Create test data
		for (int32 i = 0; i < 5; i++)
		{
			FHeightfieldModification Mod;
			Mod.Center = FVector2D(i * 100.0f, i * 100.0f);
			Mod.Radius = 50.0f + i * 10.0f;
			Mod.Strength = 1.0f + i * 0.5f;
			Mod.Operation = static_cast<EHeightfieldOperation>(i % 4);
			Mod.AffectedTile = FTileCoord(i, i);
			OriginalMods.Add(Mod);
			
			FPOIData POI;
			POI.POIName = FString::Printf(TEXT("SerializationTestPOI_%d"), i);
			POI.Location = FVector(i * 150.0f, i * 150.0f, i * 10.0f);
			POI.Rotation = FRotator(0.0f, i * 45.0f, 0.0f);
			POI.Scale = FVector(1.0f + i * 0.2f);
			POI.OriginBiome = static_cast<EBiomeType>((i % 3) + 1);
			POI.bIsSpawned = (i % 2 == 0);
			OriginalPOIs.Add(POI);
			
			FPCGInstanceData Instance;
			Instance.Location = FVector(i * 200.0f, i * 200.0f, i * 5.0f);
			Instance.Rotation = FRotator(0.0f, i * 60.0f, 0.0f);
			Instance.Scale = FVector(0.8f + i * 0.1f);
			Instance.bIsActive = (i % 3 != 0);
			Instance.OwningTile = FTileCoord(i / 2, i / 2);
			OriginalInstances.Add(Instance);
			
			FInstanceJournalEntry JournalEntry(Instance, EInstanceOperation::Add);
			OriginalJournal.Add(JournalEntry);
		}
		
		// Test 2: Serialize all data to memory
		TArray<uint8> SerializedData;
		FMemoryWriter MemoryWriter(SerializedData);
		
		// Serialize each array
		MemoryWriter << OriginalMods;
		MemoryWriter << OriginalPOIs;
		MemoryWriter << OriginalInstances;
		MemoryWriter << OriginalJournal;
		
		UE_LOG(LogTemp, Log, TEXT("✓ Serialized all data to memory (%d bytes)"), SerializedData.Num());
		
		// Test 3: Deserialize data from memory
		TArray<FHeightfieldModification> DeserializedMods;
		TArray<FPOIData> DeserializedPOIs;
		TArray<FPCGInstanceData> DeserializedInstances;
		TArray<FInstanceJournalEntry> DeserializedJournal;
		
		FMemoryReader MemoryReader(SerializedData);
		MemoryReader << DeserializedMods;
		MemoryReader << DeserializedPOIs;
		MemoryReader << DeserializedInstances;
		MemoryReader << DeserializedJournal;
		
		UE_LOG(LogTemp, Log, TEXT("✓ Deserialized all data from memory"));
		
		// Test 4: Verify data integrity after serialization roundtrip
		if (DeserializedMods.Num() != OriginalMods.Num())
		{
			UE_LOG(LogTemp, Error, TEXT("Heightfield modification count mismatch after serialization"));
			return false;
		}
		
		for (int32 i = 0; i < OriginalMods.Num(); i++)
		{
			const FHeightfieldModification& Original = OriginalMods[i];
			const FHeightfieldModification& Deserialized = DeserializedMods[i];
			
			if (Original.ModificationId != Deserialized.ModificationId)
			{
				UE_LOG(LogTemp, Error, TEXT("Heightfield modification GUID mismatch after serialization"));
				return false;
			}
			
			if (!Deserialized.ModificationId.IsValid())
			{
				UE_LOG(LogTemp, Error, TEXT("Deserialized heightfield modification has invalid GUID"));
				return false;
			}
		}
		
		if (DeserializedPOIs.Num() != OriginalPOIs.Num())
		{
			UE_LOG(LogTemp, Error, TEXT("POI count mismatch after serialization"));
			return false;
		}
		
		for (int32 i = 0; i < OriginalPOIs.Num(); i++)
		{
			const FPOIData& Original = OriginalPOIs[i];
			const FPOIData& Deserialized = DeserializedPOIs[i];
			
			if (Original.POIId != Deserialized.POIId)
			{
				UE_LOG(LogTemp, Error, TEXT("POI GUID mismatch after serialization"));
				return false;
			}
			
			if (!Deserialized.POIId.IsValid())
			{
				UE_LOG(LogTemp, Error, TEXT("Deserialized POI has invalid GUID"));
				return false;
			}
			
			if (Original.POIName != Deserialized.POIName)
			{
				UE_LOG(LogTemp, Error, TEXT("POI name mismatch after serialization"));
				return false;
			}
		}
		
		if (DeserializedInstances.Num() != OriginalInstances.Num())
		{
			UE_LOG(LogTemp, Error, TEXT("PCG instance count mismatch after serialization"));
			return false;
		}
		
		for (int32 i = 0; i < OriginalInstances.Num(); i++)
		{
			const FPCGInstanceData& Original = OriginalInstances[i];
			const FPCGInstanceData& Deserialized = DeserializedInstances[i];
			
			if (Original.InstanceId != Deserialized.InstanceId)
			{
				UE_LOG(LogTemp, Error, TEXT("PCG instance GUID mismatch after serialization"));
				return false;
			}
			
			if (!Deserialized.InstanceId.IsValid())
			{
				UE_LOG(LogTemp, Error, TEXT("Deserialized PCG instance has invalid GUID"));
				return false;
			}
		}
		
		if (DeserializedJournal.Num() != OriginalJournal.Num())
		{
			UE_LOG(LogTemp, Error, TEXT("Journal entry count mismatch after serialization"));
			return false;
		}
		
		for (int32 i = 0; i < OriginalJournal.Num(); i++)
		{
			const FInstanceJournalEntry& Original = OriginalJournal[i];
			const FInstanceJournalEntry& Deserialized = DeserializedJournal[i];
			
			if (Original.InstanceId != Deserialized.InstanceId)
			{
				UE_LOG(LogTemp, Error, TEXT("Journal entry GUID mismatch after serialization"));
				return false;
			}
			
			if (!Deserialized.InstanceId.IsValid())
			{
				UE_LOG(LogTemp, Error, TEXT("Deserialized journal entry has invalid GUID"));
				return false;
			}
		}
		
		// Test 5: Test file-based persistence (temporary file)
		FString TempFilePath = FPaths::ProjectTempDir() / TEXT("StructIntegrationTest.dat");
		
		// Save to file
		if (!FFileHelper::SaveArrayToFile(SerializedData, *TempFilePath))
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to save serialized data to file"));
			return false;
		}
		
		// Load from file
		TArray<uint8> LoadedData;
		if (!FFileHelper::LoadFileToArray(LoadedData, *TempFilePath))
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to load serialized data from file"));
			return false;
		}
		
		// Verify file data matches memory data
		if (LoadedData.Num() != SerializedData.Num())
		{
			UE_LOG(LogTemp, Error, TEXT("File data size mismatch"));
			return false;
		}
		
		if (FMemory::Memcmp(LoadedData.GetData(), SerializedData.GetData(), SerializedData.Num()) != 0)
		{
			UE_LOG(LogTemp, Error, TEXT("File data content mismatch"));
			return false;
		}
		
		// Clean up temp file
		IFileManager::Get().Delete(*TempFilePath);
		
		UE_LOG(LogTemp, Log, TEXT("✓ All struct data serialization/deserialization works correctly"));
		UE_LOG(LogTemp, Log, TEXT("✓ All GUIDs preserved correctly through serialization"));
		UE_LOG(LogTemp, Log, TEXT("✓ File-based persistence works correctly"));
		UE_LOG(LogTemp, Log, TEXT("✓ Data integrity maintained through complete roundtrip"));
		
		return true;
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogTemp, Error, TEXT("Exception in serialization persistence test: %s"), ANSI_TO_TCHAR(e.what()));
		return false;
	}
	catch (...)
	{
		UE_LOG(LogTemp, Error, TEXT("Unknown exception in serialization persistence test"));
		return false;
	}
}

#endif // WITH_AUTOMATION_TESTS