#include "Services/HeightfieldService.h"
#include "Data/WorldGenTypes.h"
#include "Utils/WorldGenLogging.h"
#include "Misc/CRC.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"

// Test to verify both modification methods produce identical results
static FAutoConsoleCommand ModificationConsistencyTestCommand(
    TEXT("wg.ModificationConsistencyTest"),
    TEXT("Test to verify both modification application methods produce identical results"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        UE_LOG(LogTemp, Warning, TEXT("=== Modification Consistency Test ==="));
        
        // Create HeightfieldService
        UHeightfieldService* HeightfieldService = NewObject<UHeightfieldService>();
        
        // Initialize with default config
        FWorldGenConfig Config;
        Config.MaxTerrainHeight = 1000.0f;
        HeightfieldService->Initialize(Config);
        
        FTileCoord TestTile(50, 50);
        int32 TestSeed = 12345;
        
        // Generate base heightfield twice
        FHeightfieldData HeightfieldA = HeightfieldService->GenerateHeightfield(TestSeed, TestTile);
        FHeightfieldData HeightfieldB = HeightfieldService->GenerateHeightfield(TestSeed, TestTile);
        
        // Verify they start identical
        uint32 ChecksumA = FCrc::MemCrc32(HeightfieldA.HeightData.GetData(), HeightfieldA.HeightData.Num() * sizeof(float));
        uint32 ChecksumB = FCrc::MemCrc32(HeightfieldB.HeightData.GetData(), HeightfieldB.HeightData.Num() * sizeof(float));
        
        UE_LOG(LogTemp, Warning, TEXT("Initial checksums: A=0x%08X, B=0x%08X"), ChecksumA, ChecksumB);
        
        if (ChecksumA != ChecksumB)
        {
            UE_LOG(LogTemp, Error, TEXT("ERROR: Base heightfields are not identical!"));
            return;
        }
        
        // Create a test modification
        FHeightfieldModification TestMod;
        TestMod.ModificationId = FGuid::NewGuid();
        TestMod.Center = FVector2D(TestTile.ToWorldPosition().X, TestTile.ToWorldPosition().Y);
        TestMod.Radius = 5.0f;
        TestMod.Strength = 2.0f;
        TestMod.Operation = EHeightfieldOperation::Add;
        TestMod.AffectedTile = TestTile;
        TestMod.Timestamp = FDateTime::Now();
        
        UE_LOG(LogTemp, Warning, TEXT("Applying modification at (%.1f, %.1f) with radius %.1f"), 
            TestMod.Center.X, TestMod.Center.Y, TestMod.Radius);
        
        // Method A: Apply using ApplyModificationToHeightfield (direct)
        HeightfieldService->ApplyModificationToHeightfield(HeightfieldA, TestMod);
        
        // Method B: Apply using ApplyModificationsToTile (from stored list)
        // First, store the modification
        HeightfieldService->TileModifications.FindOrAdd(TestTile).Modifications.Add(TestMod);
        HeightfieldService->ApplyModificationsToTile(TestTile, HeightfieldB.HeightData);
        
        // Compare results
        uint32 FinalChecksumA = FCrc::MemCrc32(HeightfieldA.HeightData.GetData(), HeightfieldA.HeightData.Num() * sizeof(float));
        uint32 FinalChecksumB = FCrc::MemCrc32(HeightfieldB.HeightData.GetData(), HeightfieldB.HeightData.Num() * sizeof(float));
        
        UE_LOG(LogTemp, Warning, TEXT("Final checksums: A=0x%08X, B=0x%08X"), FinalChecksumA, FinalChecksumB);
        
        if (FinalChecksumA == FinalChecksumB)
        {
            UE_LOG(LogTemp, Warning, TEXT("✅ SUCCESS: Both methods produce identical results!"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("❌ FAILURE: Methods produce different results"));
            
            // Analyze differences
            int32 DifferenceCount = 0;
            float MaxDifference = 0.0f;
            
            for (int32 i = 0; i < HeightfieldA.HeightData.Num() && i < HeightfieldB.HeightData.Num(); i++)
            {
                float Difference = FMath::Abs(HeightfieldA.HeightData[i] - HeightfieldB.HeightData[i]);
                if (Difference > KINDA_SMALL_NUMBER)
                {
                    DifferenceCount++;
                    MaxDifference = FMath::Max(MaxDifference, Difference);
                    
                    if (DifferenceCount <= 5)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("Difference at %d: A=%.6f, B=%.6f, Diff=%.6f"), 
                            i, HeightfieldA.HeightData[i], HeightfieldB.HeightData[i], Difference);
                    }
                }
            }
            
            UE_LOG(LogTemp, Warning, TEXT("Total differences: %d, Max difference: %.6f"), DifferenceCount, MaxDifference);
        }
        
        UE_LOG(LogTemp, Warning, TEXT("=== Modification Consistency Test Complete ==="));
    })
);