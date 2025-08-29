// Test to verify terrain persistence checksum fix
#include "Services/HeightfieldService.h"
#include "Data/WorldGenTypes.h"
#include "Utils/WorldGenLogging.h"
#include "Misc/CRC.h"

void TestChecksumFix()
{
    UE_LOG(LogTemp, Warning, TEXT("=== Testing Checksum Fix ==="));
    
    // Create HeightfieldService
    UHeightfieldService* HeightfieldService = NewObject<UHeightfieldService>();
    
    // Initialize with default config
    FWorldGenConfig Config;
    Config.MaxTerrainHeight = 1000.0f;
    HeightfieldService->Initialize(Config);
    
    // Test tile and seed
    FTileCoord TestTile(0, 0);
    int32 TestSeed = 12345;
    
    // Step 1: Generate initial heightfield
    FHeightfieldData InitialHeightfield = HeightfieldService->GenerateHeightfield(TestSeed, TestTile);
    uint32 InitialChecksum = FCrc::MemCrc32(InitialHeightfield.HeightData.GetData(), 
        InitialHeightfield.HeightData.Num() * sizeof(float));
    
    UE_LOG(LogTemp, Warning, TEXT("Initial checksum: 0x%08X"), InitialChecksum);
    
    // Step 2: Apply modifications (same as integration test)
    FVector TestLocation = TestTile.ToWorldPosition();
    float EditRadius = 10.0f;
    float EditStrength = 5.0f;
    
    // Apply Add operation
    FVector AddLocation = TestLocation + FVector(5.0f, 5.0f, 0.0f);
    HeightfieldService->ModifyHeightfield(AddLocation, EditRadius, EditStrength, EHeightfieldOperation::Add);
    
    // Apply Subtract operation
    FVector SubtractLocation = TestLocation + FVector(-10.0f, 10.0f, 0.0f);
    HeightfieldService->ModifyHeightfield(SubtractLocation, EditRadius, EditStrength, EHeightfieldOperation::Subtract);
    
    // Apply Flatten operation
    FVector FlattenLocation = TestLocation + FVector(10.0f, -10.0f, 0.0f);
    HeightfieldService->ModifyHeightfield(FlattenLocation, EditRadius, EditStrength, EHeightfieldOperation::Flatten);
    
    // Apply Smooth operation
    FVector SmoothLocation = TestLocation + FVector(-10.0f, -10.0f, 0.0f);
    HeightfieldService->ModifyHeightfield(SmoothLocation, EditRadius, EditStrength, EHeightfieldOperation::Smooth);
    
    // Step 3: Get modified heightfield checksum
    FHeightfieldData ModifiedHeightfield;
    HeightfieldService->GetCachedHeightfield(TestTile, ModifiedHeightfield);
    uint32 ModifiedChecksum = FCrc::MemCrc32(ModifiedHeightfield.HeightData.GetData(), 
        ModifiedHeightfield.HeightData.Num() * sizeof(float));
    
    UE_LOG(LogTemp, Warning, TEXT("Modified checksum: 0x%08X"), ModifiedChecksum);
    
    // Step 4: Save and reload
    HeightfieldService->SaveTileTerrainDeltas(TestTile);
    HeightfieldService->ClearHeightfieldCache();
    HeightfieldService->LoadTileTerrainDeltas(TestTile);
    
    // Step 5: Regenerate and check checksum
    FHeightfieldData ReloadedHeightfield = HeightfieldService->GenerateHeightfield(TestSeed, TestTile);
    uint32 ReloadedChecksum = FCrc::MemCrc32(ReloadedHeightfield.HeightData.GetData(), 
        ReloadedHeightfield.HeightData.Num() * sizeof(float));
    
    UE_LOG(LogTemp, Warning, TEXT("Reloaded checksum: 0x%08X"), ReloadedChecksum);
    
    // Step 6: Compare results
    if (ModifiedChecksum == ReloadedChecksum)
    {
        UE_LOG(LogTemp, Warning, TEXT("✓ SUCCESS: Checksums match after fix!"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("✗ FAILURE: Checksums still don't match"));
        
        // Debug differences
        int32 DifferenceCount = 0;
        for (int32 i = 0; i < ModifiedHeightfield.HeightData.Num() && i < ReloadedHeightfield.HeightData.Num(); i++)
        {
            float Diff = FMath::Abs(ModifiedHeightfield.HeightData[i] - ReloadedHeightfield.HeightData[i]);
            if (Diff > KINDA_SMALL_NUMBER)
            {
                DifferenceCount++;
                if (DifferenceCount <= 3)
                {
                    UE_LOG(LogTemp, Warning, TEXT("Difference at %d: %.6f vs %.6f (diff: %.6f)"), 
                        i, ModifiedHeightfield.HeightData[i], ReloadedHeightfield.HeightData[i], Diff);
                }
            }
        }
        UE_LOG(LogTemp, Warning, TEXT("Total differences: %d"), DifferenceCount);
    }
}