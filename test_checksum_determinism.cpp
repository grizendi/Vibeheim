// Test to verify terrain persistence checksum determinism fix
#include "Services/HeightfieldService.h"
#include "Data/WorldGenTypes.h"
#include "Utils/WorldGenLogging.h"
#include "Misc/CRC.h"
#include "Engine/Engine.h"

void TestChecksumDeterminism()
{
    UE_LOG(LogTemp, Warning, TEXT("=== Testing Checksum Determinism Fix ==="));
    
    // Create HeightfieldService
    UHeightfieldService* HeightfieldService = NewObject<UHeightfieldService>();
    
    // Initialize with default config
    FWorldGenConfig Config;
    Config.MaxTerrainHeight = 1000.0f;
    HeightfieldService->Initialize(Config);
    
    // Use a test tile
    FTileCoord TestTile(50, 50);
    int32 TestSeed = 12345;
    float EditRadius = 5.0f;
    float EditStrength = 2.0f;
    
    UE_LOG(LogTemp, Warning, TEXT("Test parameters: Tile(%d,%d), Seed=%d, Radius=%.1f, Strength=%.1f"), 
        TestTile.X, TestTile.Y, TestSeed, EditRadius, EditStrength);
    
    // Clear any existing modifications
    HeightfieldService->ClearTileModifications(TestTile);
    
    UE_LOG(LogTemp, Warning, TEXT("Step 1: Generate initial heightfield"));
    FHeightfieldData InitialHeightfield = HeightfieldService->GenerateHeightfield(TestSeed, TestTile);
    uint32 InitialChecksum = FCrc::MemCrc32(InitialHeightfield.HeightData.GetData(), 
        InitialHeightfield.HeightData.Num() * sizeof(float));
    
    UE_LOG(LogTemp, Warning, TEXT("Initial checksum: 0x%08X"), InitialChecksum);
    
    UE_LOG(LogTemp, Warning, TEXT("Step 2: Apply modifications"));
    FVector TestLocation = TestTile.ToWorldPosition();
    
    // Apply multiple modifications to test deterministic ordering
    FVector AddLocation = TestLocation + FVector(3.0f, 3.0f, 0.0f);
    HeightfieldService->ModifyHeightfield(AddLocation, EditRadius, EditStrength, EHeightfieldOperation::Add);
    
    FVector SubtractLocation = TestLocation + FVector(-3.0f, 3.0f, 0.0f);
    HeightfieldService->ModifyHeightfield(SubtractLocation, EditRadius, EditStrength, EHeightfieldOperation::Subtract);
    
    FVector FlattenLocation = TestLocation + FVector(3.0f, -3.0f, 0.0f);
    HeightfieldService->ModifyHeightfield(FlattenLocation, EditRadius, EditStrength, EHeightfieldOperation::Flatten);
    
    FVector SmoothLocation = TestLocation + FVector(-3.0f, -3.0f, 0.0f);
    HeightfieldService->ModifyHeightfield(SmoothLocation, EditRadius, EditStrength, EHeightfieldOperation::Smooth);
    
    UE_LOG(LogTemp, Warning, TEXT("Step 3: Get modified checksum (using regeneration path)"));
    // Use regeneration path to match what the reload path will do
    FHeightfieldData ModifiedHeightfield = HeightfieldService->GenerateHeightfield(TestSeed, TestTile);
    uint32 ModifiedChecksum = FCrc::MemCrc32(ModifiedHeightfield.HeightData.GetData(), 
        ModifiedHeightfield.HeightData.Num() * sizeof(float));
    
    UE_LOG(LogTemp, Warning, TEXT("Modified checksum: 0x%08X"), ModifiedChecksum);
    
    UE_LOG(LogTemp, Warning, TEXT("Step 4: Save and reload"));
    HeightfieldService->SaveTileTerrainDeltas(TestTile);
    HeightfieldService->ClearHeightfieldCache();
    HeightfieldService->LoadTileTerrainDeltas(TestTile);
    
    UE_LOG(LogTemp, Warning, TEXT("Step 5: Get reloaded checksum"));
    FHeightfieldData ReloadedHeightfield = HeightfieldService->GenerateHeightfield(TestSeed, TestTile);
    uint32 ReloadedChecksum = FCrc::MemCrc32(ReloadedHeightfield.HeightData.GetData(), 
        ReloadedHeightfield.HeightData.Num() * sizeof(float));
    
    UE_LOG(LogTemp, Warning, TEXT("Reloaded checksum: 0x%08X"), ReloadedChecksum);
    
    UE_LOG(LogTemp, Warning, TEXT("Results:"));
    UE_LOG(LogTemp, Warning, TEXT("  Initial:  0x%08X"), InitialChecksum);
    UE_LOG(LogTemp, Warning, TEXT("  Modified: 0x%08X"), ModifiedChecksum);
    UE_LOG(LogTemp, Warning, TEXT("  Reloaded: 0x%08X"), ReloadedChecksum);
    
    if (ModifiedChecksum == ReloadedChecksum)
    {
        UE_LOG(LogTemp, Warning, TEXT("✅ SUCCESS: Checksum determinism fix is working!"));
        UE_LOG(LogTemp, Warning, TEXT("✅ Modified and Reloaded checksums match perfectly"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ FAILURE: Checksums still don't match"));
        
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
        
        UE_LOG(LogTemp, Warning, TEXT("Total differences: %d"), DifferenceCount);
        UE_LOG(LogTemp, Warning, TEXT("Max difference: %.6f"), MaxDifference);
    }
    
    UE_LOG(LogTemp, Warning, TEXT("=== Checksum Determinism Test Complete ==="));
}

// Console command to run the test
static FAutoConsoleCommand ChecksumDeterminismTestCommand(
    TEXT("wg.TestChecksumDeterminism"),
    TEXT("Test terrain persistence checksum determinism fix"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        TestChecksumDeterminism();
    })
);