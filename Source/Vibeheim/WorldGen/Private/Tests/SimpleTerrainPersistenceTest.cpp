#if 0 // DISABLED FOR TEST DIET - Duplicate persistence test
#include "Services/HeightfieldService.h"
#include "Data/WorldGenTypes.h"
#include "Utils/WorldGenLogging.h"
#include "Misc/CRC.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"

// Simple console command to test terrain persistence with clean state
static FAutoConsoleCommand SimpleTerrainPersistenceTestCommand(
    TEXT("wg.SimpleTerrainTest"),
    TEXT("Simple terrain persistence test with clean state"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        UE_LOG(LogTemp, Warning, TEXT("=== Simple Terrain Persistence Test ==="));
        
        // Create HeightfieldService
        UHeightfieldService* HeightfieldService = NewObject<UHeightfieldService>();
        
        // Initialize with default config
        FWorldGenConfig Config;
        Config.MaxTerrainHeight = 1000.0f;
        HeightfieldService->Initialize(Config);
        
        // Use a different tile to avoid conflicts with existing data
        FTileCoord TestTile(5, 5);
        int32 TestSeed = 54321;
        float EditRadius = 5.0f;  // Smaller radius to avoid cross-tile effects
        float EditStrength = 2.0f;  // Smaller strength for more predictable results
        
        UE_LOG(LogTemp, Warning, TEXT("Test parameters: Tile(%d,%d), Seed=%d, Radius=%.1f, Strength=%.1f"), 
            TestTile.X, TestTile.Y, TestSeed, EditRadius, EditStrength);
        
        UE_LOG(LogTemp, Warning, TEXT("Step 1: Generate initial heightfield"));
        
        // Generate initial heightfield
        FHeightfieldData InitialHeightfield = HeightfieldService->GenerateHeightfield(TestSeed, TestTile);
        uint32 InitialChecksum = FCrc::MemCrc32(InitialHeightfield.HeightData.GetData(), 
            InitialHeightfield.HeightData.Num() * sizeof(float));
        
        UE_LOG(LogTemp, Warning, TEXT("Initial heightfield checksum: 0x%08X"), InitialChecksum);
        
        UE_LOG(LogTemp, Warning, TEXT("Step 2: Apply single terrain modification"));
        
        // Apply just one modification to keep it simple
        FVector TestLocation = TestTile.ToWorldPosition();
        FVector ModLocation = TestLocation; // Center of tile
        
        bool bModSuccess = HeightfieldService->ModifyHeightfield(ModLocation, EditRadius, EditStrength, EHeightfieldOperation::Add);
        UE_LOG(LogTemp, Warning, TEXT("Modification success: %s"), bModSuccess ? TEXT("Yes") : TEXT("No"));
        
        // Check how many modifications we have
        TArray<FHeightfieldModification> InitialMods = HeightfieldService->GetTileModifications(TestTile);
        UE_LOG(LogTemp, Warning, TEXT("Modifications after editing: %d"), InitialMods.Num());
        
        UE_LOG(LogTemp, Warning, TEXT("Step 3: Get modified heightfield checksum"));
        
        // Get modified heightfield
        FHeightfieldData ModifiedHeightfield;
        if (!HeightfieldService->GetCachedHeightfield(TestTile, ModifiedHeightfield))
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to get cached heightfield after modification"));
            return;
        }
        
        uint32 ModifiedChecksum = FCrc::MemCrc32(ModifiedHeightfield.HeightData.GetData(), 
            ModifiedHeightfield.HeightData.Num() * sizeof(float));
        
        UE_LOG(LogTemp, Warning, TEXT("Modified heightfield checksum: 0x%08X"), ModifiedChecksum);
        
        // Verify modification was applied
        if (InitialChecksum == ModifiedChecksum)
        {
            UE_LOG(LogTemp, Error, TEXT("ERROR: Modified checksum matches initial - modification not applied!"));
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
            UE_LOG(LogTemp, Warning, TEXT("✓ SUCCESS: Checksums match! Terrain persistence is working."));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("✗ FAILURE: Checksums don't match."));
            
            // Quick difference analysis
            int32 DifferenceCount = 0;
            for (int32 i = 0; i < ModifiedHeightfield.HeightData.Num() && i < ReloadedHeightfield.HeightData.Num(); i++)
            {
                if (FMath::Abs(ModifiedHeightfield.HeightData[i] - ReloadedHeightfield.HeightData[i]) > KINDA_SMALL_NUMBER)
                {
                    DifferenceCount++;
                }
            }
            UE_LOG(LogTemp, Warning, TEXT("Height differences found: %d"), DifferenceCount);
        }
        
        UE_LOG(LogTemp, Warning, TEXT("=== Simple Terrain Persistence Test Complete ==="));
    })
);#endif // 
DISABLED FOR TEST DIET