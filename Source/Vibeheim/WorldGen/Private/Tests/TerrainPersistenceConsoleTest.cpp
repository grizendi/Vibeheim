#include "Services/HeightfieldService.h"
#include "Data/WorldGenTypes.h"
#include "Utils/WorldGenLogging.h"
#include "Misc/CRC.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"

// Console command to test terrain persistence checksum fix
static FAutoConsoleCommand TestTerrainPersistenceCommand(
    TEXT("wg.TestTerrainPersistence"),
    TEXT("Test terrain persistence checksum matching"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        UE_LOG(LogTemp, Warning, TEXT("=== Testing Terrain Persistence Checksum Fix ==="));
        
        // Create HeightfieldService
        UHeightfieldService* HeightfieldService = NewObject<UHeightfieldService>();
        
        // Initialize with default config
        FWorldGenConfig Config;
        Config.MaxTerrainHeight = 1000.0f;
        HeightfieldService->Initialize(Config);
        
        // Test parameters (same as integration test)
        FTileCoord TestTile(0, 0);
        int32 TestSeed = 12345;
        float EditRadius = 10.0f;
        float EditStrength = 5.0f;
        
        UE_LOG(LogTemp, Warning, TEXT("Step 1: Generate initial heightfield"));
        
        // Generate initial heightfield
        FHeightfieldData InitialHeightfield = HeightfieldService->GenerateHeightfield(TestSeed, TestTile);
        uint32 InitialChecksum = FCrc::MemCrc32(InitialHeightfield.HeightData.GetData(), 
            InitialHeightfield.HeightData.Num() * sizeof(float));
        
        UE_LOG(LogTemp, Warning, TEXT("Initial heightfield checksum: 0x%08X"), InitialChecksum);
        
        UE_LOG(LogTemp, Warning, TEXT("Step 2: Apply terrain modifications"));
        
        // Apply modifications (same sequence as integration test)
        FVector TestLocation = TestTile.ToWorldPosition();
        
        // Operation 1: Add terrain
        FVector AddLocation = TestLocation + FVector(5.0f, 5.0f, 0.0f);
        bool bAddSuccess = HeightfieldService->ModifyHeightfield(AddLocation, EditRadius, EditStrength, EHeightfieldOperation::Add);
        UE_LOG(LogTemp, Warning, TEXT("Add modification success: %s"), bAddSuccess ? TEXT("Yes") : TEXT("No"));
        
        // Operation 2: Subtract terrain
        FVector SubtractLocation = TestLocation + FVector(-10.0f, 10.0f, 0.0f);
        bool bSubtractSuccess = HeightfieldService->ModifyHeightfield(SubtractLocation, EditRadius, EditStrength, EHeightfieldOperation::Subtract);
        UE_LOG(LogTemp, Warning, TEXT("Subtract modification success: %s"), bSubtractSuccess ? TEXT("Yes") : TEXT("No"));
        
        // Operation 3: Flatten terrain
        FVector FlattenLocation = TestLocation + FVector(10.0f, -10.0f, 0.0f);
        bool bFlattenSuccess = HeightfieldService->ModifyHeightfield(FlattenLocation, EditRadius, EditStrength, EHeightfieldOperation::Flatten);
        UE_LOG(LogTemp, Warning, TEXT("Flatten modification success: %s"), bFlattenSuccess ? TEXT("Yes") : TEXT("No"));
        
        // Operation 4: Smooth terrain
        FVector SmoothLocation = TestLocation + FVector(-10.0f, -10.0f, 0.0f);
        bool bSmoothSuccess = HeightfieldService->ModifyHeightfield(SmoothLocation, EditRadius, EditStrength, EHeightfieldOperation::Smooth);
        UE_LOG(LogTemp, Warning, TEXT("Smooth modification success: %s"), bSmoothSuccess ? TEXT("Yes") : TEXT("No"));
        
        UE_LOG(LogTemp, Warning, TEXT("Step 3: Get modified heightfield checksum"));
        
        // Get modified heightfield
        FHeightfieldData ModifiedHeightfield;
        if (!HeightfieldService->GetCachedHeightfield(TestTile, ModifiedHeightfield))
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to get cached heightfield after modifications"));
            return;
        }
        
        uint32 ModifiedChecksum = FCrc::MemCrc32(ModifiedHeightfield.HeightData.GetData(), 
            ModifiedHeightfield.HeightData.Num() * sizeof(float));
        
        UE_LOG(LogTemp, Warning, TEXT("Modified heightfield checksum: 0x%08X"), ModifiedChecksum);
        
        // Verify modifications were applied
        if (InitialChecksum == ModifiedChecksum)
        {
            UE_LOG(LogTemp, Error, TEXT("ERROR: Modified checksum matches initial - modifications not applied!"));
            return;
        }
        
        UE_LOG(LogTemp, Warning, TEXT("Step 4: Save terrain deltas"));
        
        // Save terrain deltas
        bool bSaveSuccess = HeightfieldService->SaveTileTerrainDeltas(TestTile);
        UE_LOG(LogTemp, Warning, TEXT("Save deltas success: %s"), bSaveSuccess ? TEXT("Yes") : TEXT("No"));
        
        // Check modification count
        TArray<FHeightfieldModification> SavedMods = HeightfieldService->GetTileModifications(TestTile);
        UE_LOG(LogTemp, Warning, TEXT("Saved modifications count: %d"), SavedMods.Num());
        
        UE_LOG(LogTemp, Warning, TEXT("Step 5: Clear cache and reload"));
        
        // Clear cache to simulate fresh load
        HeightfieldService->ClearHeightfieldCache();
        
        // Load terrain deltas
        bool bLoadSuccess = HeightfieldService->LoadTileTerrainDeltas(TestTile);
        UE_LOG(LogTemp, Warning, TEXT("Load deltas success: %s"), bLoadSuccess ? TEXT("Yes") : TEXT("No"));
        
        // Check loaded modification count
        TArray<FHeightfieldModification> LoadedMods = HeightfieldService->GetTileModifications(TestTile);
        UE_LOG(LogTemp, Warning, TEXT("Loaded modifications count: %d"), LoadedMods.Num());
        
        if (LoadedMods.Num() != SavedMods.Num())
        {
            UE_LOG(LogTemp, Error, TEXT("ERROR: Loaded modifications count (%d) != Saved count (%d)"), 
                LoadedMods.Num(), SavedMods.Num());
            return;
        }
        
        UE_LOG(LogTemp, Warning, TEXT("Step 6: Regenerate heightfield with loaded modifications"));
        
        // Regenerate heightfield (should apply loaded modifications in sorted order)
        FHeightfieldData ReloadedHeightfield = HeightfieldService->GenerateHeightfield(TestSeed, TestTile);
        uint32 ReloadedChecksum = FCrc::MemCrc32(ReloadedHeightfield.HeightData.GetData(), 
            ReloadedHeightfield.HeightData.Num() * sizeof(float));
        
        UE_LOG(LogTemp, Warning, TEXT("Reloaded heightfield checksum: 0x%08X"), ReloadedChecksum);
        
        UE_LOG(LogTemp, Warning, TEXT("Step 7: Compare checksums"));
        UE_LOG(LogTemp, Warning, TEXT("Initial:  0x%08X"), InitialChecksum);
        UE_LOG(LogTemp, Warning, TEXT("Modified: 0x%08X"), ModifiedChecksum);
        UE_LOG(LogTemp, Warning, TEXT("Reloaded: 0x%08X"), ReloadedChecksum);
        
        if (ModifiedChecksum == ReloadedChecksum)
        {
            UE_LOG(LogTemp, Warning, TEXT("✓ SUCCESS: Modified and Reloaded checksums match!"));
            UE_LOG(LogTemp, Warning, TEXT("✓ Terrain persistence checksum fix is working correctly"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("✗ FAILURE: Modified and Reloaded checksums do NOT match!"));
            UE_LOG(LogTemp, Error, TEXT("✗ Terrain persistence checksum fix needs more work"));
            
            // Debug: Compare some height values
            UE_LOG(LogTemp, Warning, TEXT("Debugging height differences..."));
            int32 DifferenceCount = 0;
            float MaxDifference = 0.0f;
            
            for (int32 i = 0; i < FMath::Min(ModifiedHeightfield.HeightData.Num(), ReloadedHeightfield.HeightData.Num()); i++)
            {
                float ModifiedHeight = ModifiedHeightfield.HeightData[i];
                float ReloadedHeight = ReloadedHeightfield.HeightData[i];
                float Difference = FMath::Abs(ModifiedHeight - ReloadedHeight);
                
                if (Difference > KINDA_SMALL_NUMBER)
                {
                    DifferenceCount++;
                    MaxDifference = FMath::Max(MaxDifference, Difference);
                    
                    if (DifferenceCount <= 5) // Log first 5 differences
                    {
                        UE_LOG(LogTemp, Warning, TEXT("Height difference at index %d: Modified=%.6f, Reloaded=%.6f, Diff=%.6f"), 
                            i, ModifiedHeight, ReloadedHeight, Difference);
                    }
                }
            }
            
            UE_LOG(LogTemp, Warning, TEXT("Total height differences: %d, Max difference: %.6f"), 
                DifferenceCount, MaxDifference);
        }
        
        UE_LOG(LogTemp, Warning, TEXT("=== Terrain Persistence Test Complete ==="));
    })
);