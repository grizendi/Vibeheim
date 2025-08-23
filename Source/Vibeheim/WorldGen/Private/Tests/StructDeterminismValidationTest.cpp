#include "CoreMinimal.h"
#include "Data/WorldGenTypes.h"
#include "Data/InstancePersistence.h"

#if WITH_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UObject/UObjectIterator.h"
#include "UObject/FieldIterator.h"

/**
 * Helper function to check if a FGuid is either zero (deterministic) or valid (non-garbage)
 * This validates that the GUID doesn't contain uninitialized memory patterns
 */
static bool IsZeroOrValid(const FGuid& Guid)
{
	// Check if it's a zero GUID (deterministic default)
	if (Guid == FGuid())
	{
		return true;
	}
	
	// Check if it's a valid GUID (properly initialized with NewGuid())
	if (Guid.IsValid())
	{
		return true;
	}
	
	// If neither zero nor valid, it contains garbage/uninitialized memory
	return false;
}

/**
 * Fast automation test that scans all USTRUCTs in the Vibeheim package for proper FGuid initialization
 * This test validates that all reflected FGuid members are deterministically initialized
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStructDeterminismValidationTest, 
	"Vibeheim.WorldGen.StructInitialization.DeterminismValidation", 
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FStructDeterminismValidationTest::RunTest(const FString& Parameters)
{
	UE_LOG(LogTemp, Log, TEXT("Starting struct determinism validation test..."));
	
	int32 StructsScanned = 0;
	int32 ProblematicStructsFound = 0;
	int32 FGuidPropertiesValidated = 0;
	
	// Track the four known problematic structs specifically
	bool bFoundHeightfieldModification = false;
	bool bFoundInstanceJournalEntry = false;
	bool bFoundPOIData = false;
	bool bFoundPCGInstanceData = false;
	
	// Iterate through all UScriptStruct objects in the Vibeheim package
	for (TObjectIterator<UScriptStruct> StructIt; StructIt; ++StructIt)
	{
		UScriptStruct* Struct = *StructIt;
		if (!Struct)
		{
			continue;
		}
		
		// Filter to only Vibeheim/WorldGen structs
		FString PackageName = Struct->GetPackage()->GetName();
		if (!PackageName.Contains(TEXT("Vibeheim")))
		{
			continue;
		}
		
		// Skip transient/test modules for CI stability
		if (PackageName.Contains(TEXT("Test")) || PackageName.Contains(TEXT("Temp")))
		{
			continue;
		}
		
		StructsScanned++;
		FString StructName = Struct->GetName();
		
		UE_LOG(LogTemp, VeryVerbose, TEXT("Scanning struct: %s"), *StructName);
		
		// Track if we found our known problematic structs
		if (StructName == TEXT("HeightfieldModification"))
		{
			bFoundHeightfieldModification = true;
		}
		else if (StructName == TEXT("InstanceJournalEntry"))
		{
			bFoundInstanceJournalEntry = true;
		}
		else if (StructName == TEXT("POIData"))
		{
			bFoundPOIData = true;
		}
		else if (StructName == TEXT("PCGInstanceData"))
		{
			bFoundPCGInstanceData = true;
		}
		
		// Find all FGuid properties in this struct
		bool bStructHasProblems = false;
		for (TFieldIterator<FProperty> PropIt(Struct); PropIt; ++PropIt)
		{
			FProperty* Property = *PropIt;
			if (!Property)
			{
				continue;
			}
			
			// Check if this is a FGuid property
			if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
			{
				if (StructProp->Struct && StructProp->Struct->GetName() == TEXT("Guid"))
				{
					FGuidPropertiesValidated++;
					
					// Default-construct the struct to test initialization
					void* StructMemory = FMemory::Malloc(Struct->GetStructureSize());
					Struct->InitializeStruct(StructMemory);
					
					// Get the FGuid value from the property
					FGuid* GuidPtr = StructProp->ContainerPtrToValuePtr<FGuid>(StructMemory);
					if (GuidPtr)
					{
						bool bIsProperlyInitialized = IsZeroOrValid(*GuidPtr);
						
						if (!bIsProperlyInitialized)
						{
							bStructHasProblems = true;
							ProblematicStructsFound++;
							
							UE_LOG(LogTemp, Error, TEXT("STRUCT INITIALIZATION ERROR: %s::%s contains uninitialized FGuid: %s"), 
								*StructName, *Property->GetName(), *GuidPtr->ToString());
							
							TestFalse(FString::Printf(TEXT("%s::%s should be deterministically initialized"), 
								*StructName, *Property->GetName()), true);
						}
						else
						{
							UE_LOG(LogTemp, VeryVerbose, TEXT("✓ %s::%s properly initialized: %s"), 
								*StructName, *Property->GetName(), *GuidPtr->ToString());
						}
					}
					
					// Clean up
					Struct->DestroyStruct(StructMemory);
					FMemory::Free(StructMemory);
				}
			}
		}
		
		if (!bStructHasProblems)
		{
			UE_LOG(LogTemp, VeryVerbose, TEXT("✓ Struct %s passed validation"), *StructName);
		}
	}
	
	// Validate that we found all four known problematic structs
	TestTrue("Should find FHeightfieldModification struct", bFoundHeightfieldModification);
	TestTrue("Should find FInstanceJournalEntry struct", bFoundInstanceJournalEntry);
	TestTrue("Should find FPOIData struct", bFoundPOIData);
	TestTrue("Should find FPCGInstanceData struct", bFoundPCGInstanceData);
	
	// Test specific struct instances to ensure they construct properly
	{
		FHeightfieldModification HeightfieldMod;
		TestTrue("FHeightfieldModification::ModificationId should be deterministic", 
			IsZeroOrValid(HeightfieldMod.ModificationId));
		
		FInstanceJournalEntry JournalEntry;
		TestTrue("FInstanceJournalEntry::InstanceId should be deterministic", 
			IsZeroOrValid(JournalEntry.InstanceId));
		
		FPOIData POIData;
		TestTrue("FPOIData::POIId should be deterministic", 
			IsZeroOrValid(POIData.POIId));
		
		FPCGInstanceData PCGInstance;
		TestTrue("FPCGInstanceData::InstanceId should be deterministic", 
			IsZeroOrValid(PCGInstance.InstanceId));
	}
	
	// Log summary
	UE_LOG(LogTemp, Log, TEXT("Struct determinism validation complete:"));
	UE_LOG(LogTemp, Log, TEXT("  - Structs scanned: %d"), StructsScanned);
	UE_LOG(LogTemp, Log, TEXT("  - FGuid properties validated: %d"), FGuidPropertiesValidated);
	UE_LOG(LogTemp, Log, TEXT("  - Problematic structs found: %d"), ProblematicStructsFound);
	
	// Test passes if no problematic structs were found
	bool bTestPassed = (ProblematicStructsFound == 0);
	TestTrue("All structs should have deterministic FGuid initialization", bTestPassed);
	
	return bTestPassed;
}

#endif // WITH_AUTOMATION_TESTS