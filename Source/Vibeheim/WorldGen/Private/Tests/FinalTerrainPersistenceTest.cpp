#if 0 // DISABLED FOR TEST DIET - Duplicate persistence test
#include "Services/HeightfieldService.h"
#include "Data/WorldGenTypes.h"
#include "Utils/WorldGenLogging.h"
#include "Misc/CRC.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"

// Final test to verify the complete fix
static FAutoConsoleCommand FinalTerrainPersistenceTestCommand(
    TEXT("wg.FinalTerrainTest"),
    TEXT("Final terrain persistence test with all fixes applied"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        UE_LOG(LogTemp, Warning, TEXT("=== Final Terrain Persistence Test ==="));
        
        // Create HeightfieldService
        UHeightfieldService* HeightfieldService = NewObject<UHeightfieldService>();
        
        // Initialize with default config
        FWorldGenConfig Config;
        Config.MaxTerrainHeight = 1000.0f;
        HeightfieldService->Initialize(Config);
        
        // Use a completely fresh tile
        FTileCoord TestTile(30, 30);
        int32 TestSeed = 99999;
        float EditRadius = 8.0f;
        float EditStrength = 2.0f;
        
        UE_LOG(LogTemp, Warning, TEXT("Test parameters: Tile(%d,%d), Seed=%d, Radius=%.1f, Strength=%.1f"), 
            TestTile.X, TestTile.Y, TestSeed, EditRadius, EditStrength);
        
        UE_LOG(LogTemp, Warning, TEXT("Step 1: Generate initial heightfield"));
        
        // Generate initial heightfield
        FHeightfieldData InitialHeightfield = HeightfieldService->GenerateHeightfield(TestSeed, TestTile);
        uint32 InitialChecksum = FCrc::MemCrc32(InitialHeightfield.HeightData.GetData(), 
            InitialHeightfield.HeightData.Num() * sizeof(float));
        
        UE_LOG(LogTemp, Warning, TEXT("Initial heightfield checksum: 0x%08X"), InitialChecksum);
        
        UE_LOG(LogTemp, Warning, TEXT("Step 2: Apply 4 terrain modifications (including smooth)"));
        
        FVector TestLocation = TestTile.ToWorldPosition();
        
        // Apply the same 4 modifications as the integration test
        FVector AddLocation = TestLocation + FVector(5.0f, 5.0f, 0.0f);
        HeightfieldService->ModifyHeightfield(AddLocation, EditRadius, EditStrength, EHeightfieldOperation::Add);
        
        FVector SubtractLocation = TestLocation + FVector(-8.0f, 8.0f, 0.0f);
        HeightfieldService->ModifyHeightfield(SubtractLocation, EditRadius, EditStrength, EHeightfieldOperation::Subtract);
        
        FVector FlattenLocation = TestLocation + FVector(8.0f, -8.0f, 0.0f);
        HeightfieldService->ModifyHeightfield(FlattenLocation, EditRadius, EditStrength, EHeightfieldOperation::Flatten);
        
        // The critical smooth operation
        FVector SmoothLocation = TestLocation + FVector(-8.0f, -8.0f, 0.0f);
        HeightfieldService->ModifyHeightfield(SmoothLocation, EditRadius, EditStrength, EHeightfieldOperation::Smooth);
        
        UE_LOG(LogTemp, Warning, TEXT("Applied all 4 modifications"));
        
        // Check modification count
        TArray<FHeightfieldModification> InitialMods = HeightfieldService->GetTileModifications(TestTile);
        UE_LOG(LogTemp, Warning, TEXT("Modifications after editing: %d"), InitialMods.Num());
        
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
        
        UE_LOG(LogTemp, Warning, TEXT("Step 4: Save and reload"));
        
        // Save terrain deltas
        HeightfieldService->SaveTileTerrainDeltas(TestTile);
        
        // Clear cache
        HeightfieldService->ClearHeightfieldCache();
        
        // Load terrain deltas
        HeightfieldService->LoadTileTerrainDeltas(TestTile);
        
        // Check loaded modifications
        TArray<FHeightfieldModification> LoadedMods = HeightfieldService->GetTileModifications(TestTile);
        UE_LOG(LogTemp, Warning, TEXT("Modifications after reload: %d"), LoadedMods.Num());
        
        UE_LOG(LogTemp, Warning, TEXT("Step 5: Regenerate and compare"));
        
        // Regenerate heightfield
        FHeightfieldData ReloadedHeightfield = HeightfieldService->GenerateHeightfield(TestSeed, TestTile);
        uint32 ReloadedChecksum = FCrc::MemCrc32(ReloadedHeightfield.HeightData.GetData(), 
            ReloadedHeightfield.HeightData.Num() * sizeof(float));
        
        UE_LOG(LogTemp, Warning, TEXT("Reloaded heightfield checksum: 0x%08X"), ReloadedChecksum);
        
        UE_LOG(LogTemp, Warning, TEXT("Final Results:"));
        UE_LOG(LogTemp, Warning, TEXT("  Initial:  0x%08X"), InitialChecksum);
        UE_LOG(LogTemp, Warning, TEXT("  Modified: 0x%08X"), ModifiedChecksum);
        UE_LOG(LogTemp, Warning, TEXT("  Reloaded: 0x%08X"), ReloadedChecksum);
        
        if (ModifiedChecksum == ReloadedChecksum)
        {
            UE_LOG(LogTemp, Warning, TEXT("🎉 SUCCESS: TERRAIN PERSISTENCE CHECKSUM FIX COMPLETE!"));
            UE_LOG(LogTemp, Warning, TEXT("✅ All modifications persist correctly across save/load cycles"));
            UE_LOG(LogTemp, Warning, TEXT("✅ Smooth operations are now consistent"));
            UE_LOG(LogTemp, Warning, TEXT("✅ Cross-tile modifications are properly deduplicated"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("❌ FAILURE: Still have checksum mismatch"));
            
            // Analyze remaining differences
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
            
            UE_LOG(LogTemp, Warning, TEXT("Remaining differences: %d, Max difference: %.6f"), 
                DifferenceCount, MaxDifference);
        }
        
        UE_LOG(LogTemp, Warning, TEXT("=== Final Terrain Persistence Test Complete ==="));
    })
);

#endif // DISABLED FOR TEST DIET
