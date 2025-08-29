#include "Services/HeightfieldService.h"
#include "Data/WorldGenTypes.h"
#include "Utils/WorldGenLogging.h"
#include "Misc/CRC.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"

// Clean test with fresh tile to isolate the issue
static FAutoConsoleCommand CleanTerrainPersistenceTestCommand(
    TEXT("wg.CleanTerrainTest"),
    TEXT("Clean terrain persistence test with fresh tile"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        UE_LOG(LogTemp, Warning, TEXT("=== Clean Terrain Persistence Test ==="));
        
        // Create HeightfieldService
        UHeightfieldService* HeightfieldService = NewObject<UHeightfieldService>();
        
        // Initialize with default config
        FWorldGenConfig Config;
        Config.MaxTerrainHeight = 1000.0f;
        HeightfieldService->Initialize(Config);
        
        // Use a completely fresh tile
        FTileCoord TestTile(20, 20);
        int32 TestSeed = 11111;
        float EditRadius = 8.0f;  // Smaller radius to minimize cross-tile effects
        float EditStrength = 2.0f;
        
        UE_LOG(LogTemp, Warning, TEXT("Test parameters: Tile(%d,%d), Seed=%d, Radius=%.1f, Strength=%.1f"), 
            TestTile.X, TestTile.Y, TestSeed, EditRadius, EditStrength);
        
        UE_LOG(LogTemp, Warning, TEXT("Step 1: Generate initial heightfield"));
        
        // Generate initial heightfield
        FHeightfieldData InitialHeightfield = HeightfieldService->GenerateHeightfield(TestSeed, TestTile);
        uint32 InitialChecksum = FCrc::MemCrc32(InitialHeightfield.HeightData.GetData(), 
            InitialHeightfield.HeightData.Num() * sizeof(float));
        
        UE_LOG(LogTemp, Warning, TEXT("Initial heightfield checksum: 0x%08X"), InitialChecksum);
        
        UE_LOG(LogTemp, Warning, TEXT("Step 2: Apply 4 terrain modifications"));
        
        FVector TestLocation = TestTile.ToWorldPosition();
        
        // Apply 4 modifications (same as integration test but with smaller radius)
        FVector AddLocation = TestLocation + FVector(5.0f, 5.0f, 0.0f);
        bool bAdd = HeightfieldService->ModifyHeightfield(AddLocation, EditRadius, EditStrength, EHeightfieldOperation::Add);
        UE_LOG(LogTemp, Warning, TEXT("Add modification success: %s"), bAdd ? TEXT("Yes") : TEXT("No"));
        
        FVector SubtractLocation = TestLocation + FVector(-8.0f, 8.0f, 0.0f);
        bool bSubtract = HeightfieldService->ModifyHeightfield(SubtractLocation, EditRadius, EditStrength, EHeightfieldOperation::Subtract);
        UE_LOG(LogTemp, Warning, TEXT("Subtract modification success: %s"), bSubtract ? TEXT("Yes") : TEXT("No"));
        
        FVector FlattenLocation = TestLocation + FVector(8.0f, -8.0f, 0.0f);
        bool bFlatten = HeightfieldService->ModifyHeightfield(FlattenLocation, EditRadius, EditStrength, EHeightfieldOperation::Flatten);
        UE_LOG(LogTemp, Warning, TEXT("Flatten modification success: %s"), bFlatten ? TEXT("Yes") : TEXT("No"));
        
        FVector SmoothLocation = TestLocation + FVector(-8.0f, -8.0f, 0.0f);
        bool bSmooth = HeightfieldService->ModifyHeightfield(SmoothLocation, EditRadius, EditStrength, EHeightfieldOperation::Smooth);
        UE_LOG(LogTemp, Warning, TEXT("Smooth modification success: %s"), bSmooth ? TEXT("Yes") : TEXT("No"));
        
        // Check how many modifications we have for this tile
        TArray<FHeightfieldModification> InitialMods = HeightfieldService->GetTileModifications(TestTile);
        UE_LOG(LogTemp, Warning, TEXT("Modifications after editing: %d"), InitialMods.Num());
        
        // Log modification IDs for debugging
        for (int32 i = 0; i < InitialMods.Num(); i++)
        {
            UE_LOG(LogTemp, Warning, TEXT("  Mod %d: ID=%s, Op=%d"), 
                i, *InitialMods[i].ModificationId.ToString(), (int32)InitialMods[i].Operation);
        }
        
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
        
        UE_LOG(LogTemp, Warning, TEXT("Step 4: Save and reload"));
        
        // Save terrain deltas
        bool bSaveSuccess = HeightfieldService->SaveTileTerrainDeltas(TestTile);
        UE_LOG(LogTemp, Warning, TEXT("Save success: %s"), bSaveSuccess ? TEXT("Yes") : TEXT("No"));
        
        // Clear cache
        HeightfieldService->ClearHeightfieldCache();
        
        // Load terrain deltas
        bool bLoadSuccess = HeightfieldService->LoadTileTerrainDeltas(TestTile);
        UE_LOG(LogTemp, Warning, TEXT("Load success: %s"), bLoadSuccess ? TEXT("Yes") : TEXT("No"));
        
        // Check loaded modifications
        TArray<FHeightfieldModification> LoadedMods = HeightfieldService->GetTileModifications(TestTile);
        UE_LOG(LogTemp, Warning, TEXT("Modifications after reload: %d"), LoadedMods.Num());
        
        // Log loaded modification IDs for debugging
        for (int32 i = 0; i < LoadedMods.Num(); i++)
        {
            UE_LOG(LogTemp, Warning, TEXT("  Loaded Mod %d: ID=%s, Op=%d"), 
                i, *LoadedMods[i].ModificationId.ToString(), (int32)LoadedMods[i].Operation);
        }
        
        UE_LOG(LogTemp, Warning, TEXT("Step 5: Regenerate and compare"));
        
        // Regenerate heightfield
        FHeightfieldData ReloadedHeightfield = HeightfieldService->GenerateHeightfield(TestSeed, TestTile);
        uint32 ReloadedChecksum = FCrc::MemCrc32(ReloadedHeightfield.HeightData.GetData(), 
            ReloadedHeightfield.HeightData.Num() * sizeof(float));
        
        UE_LOG(LogTemp, Warning, TEXT("Reloaded heightfield checksum: 0x%08X"), ReloadedChecksum);
        
        UE_LOG(LogTemp, Warning, TEXT("Results:"));
        UE_LOG(LogTemp, Warning, TEXT("  Initial:  0x%08X"), InitialChecksum);
        UE_LOG(LogTemp, Warning, TEXT("  Modified: 0x%08X"), ModifiedChecksum);
        UE_LOG(LogTemp, Warning, TEXT("  Reloaded: 0x%08X"), ReloadedChecksum);
        
        if (ModifiedChecksum == ReloadedChecksum)
        {
            UE_LOG(LogTemp, Warning, TEXT("✓ SUCCESS: Clean terrain persistence test passed!"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("✗ FAILURE: Checksums don't match."));
            
            // Analyze differences
            int32 DifferenceCount = 0;
            float MaxDifference = 0.0f;
            
            for (int32 i = 0; i < ModifiedHeightfield.HeightData.Num() && i < ReloadedHeightfield.HeightData.Num(); i++)
            {
                float Difference = FMath::Abs(ModifiedHeightfield.HeightData[i] - ReloadedHeightfield.HeightData[i]);
                if (Difference > KINDA_SMALL_NUMBER)
                {
                    DifferenceCount++;
                    MaxDifference = FMath::Max(MaxDifference, Difference);
                    
                    if (DifferenceCount <= 3)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("Difference at %d: Modified=%.6f, Reloaded=%.6f, Diff=%.6f"), 
                            i, ModifiedHeightfield.HeightData[i], ReloadedHeightfield.HeightData[i], Difference);
                    }
                }
            }
            
            UE_LOG(LogTemp, Warning, TEXT("Total differences: %d, Max difference: %.6f"), 
                DifferenceCount, MaxDifference);
        }
        
        UE_LOG(LogTemp, Warning, TEXT("=== Clean Terrain Persistence Test Complete ==="));
    })
);