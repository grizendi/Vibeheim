#if WITH_AUTOMATION_TESTS

#include "Editor/WorldGenBuildUtility.h"
#include "Misc/AutomationTest.h"
#include "Services/HeightfieldService.h"
#include "Utils/HashUtils.h"

namespace
{
int32 HashHeights(const FHeightfieldData& Data)
{
    return UHashUtils::CalculateFloatArrayHash(Data.HeightData, 1337);
}
} // namespace

/**
 * Property 11: Build Context Validation
 * Ensures build requests fail outside an editor, non-PIE context.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBuildContextValidationPropertyTest,
    "Vibeheim.WorldGen.BuildUtility.Property.BuildContextValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBuildContextValidationPropertyTest::RunTest(const FString& Parameters)
{
    TestFalse(TEXT("Rejects non-editor contexts"),
              UWorldGenBuildUtility::EvaluateContextForTest(
                  false, false, EWorldType::Editor));

    TestFalse(TEXT("Rejects running game worlds"),
              UWorldGenBuildUtility::EvaluateContextForTest(
                  true, true, EWorldType::Editor));

    TestFalse(TEXT("Rejects PIE worlds"),
              UWorldGenBuildUtility::EvaluateContextForTest(
                  true, false, EWorldType::PIE));

    TestTrue(TEXT("Accepts editor worlds"),
             UWorldGenBuildUtility::EvaluateContextForTest(
                 true, false, EWorldType::Editor));

    TestTrue(TEXT("Accepts editor preview worlds"),
             UWorldGenBuildUtility::EvaluateContextForTest(
                 true, false, EWorldType::EditorPreview));

    return true;
}

/**
 * Property 4: Terrain Build Determinism
 * Verifies identical heightfields are produced for the same seed and tile.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTerrainBuildDeterminismPropertyTest,
    "Vibeheim.WorldGen.BuildUtility.Property.TerrainBuildDeterminism",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTerrainBuildDeterminismPropertyTest::RunTest(const FString& Parameters)
{
    FWorldGenConfig Config;
    UHeightfieldService* HeightService = NewObject<UHeightfieldService>();
    if (!HeightService || !HeightService->Initialize(Config))
    {
        AddError(TEXT("Failed to initialize HeightfieldService for determinism test."));
        return false;
    }

    FRandomStream Random(1729);
    constexpr int32 Iterations = 8;

    for (int32 Index = 0; Index < Iterations; ++Index)
    {
        const int32 Seed = Random.RandRange(1, INT32_MAX / 2);
        const FTileCoord Tile(Random.RandRange(-10, 10), Random.RandRange(-10, 10));

        const FHeightfieldData First = HeightService->GenerateHeightfield(Seed, Tile);
        const FHeightfieldData Second = HeightService->GenerateHeightfield(Seed, Tile);

        TestEqual(TEXT("Resolutions match across builds"), First.Resolution, Second.Resolution);
        TestEqual(TEXT("Sample counts match across builds"),
                  First.HeightData.Num(), Second.HeightData.Num());

        if (First.HeightData.Num() == Second.HeightData.Num())
        {
            const int32 HashA = HashHeights(First);
            const int32 HashB = HashHeights(Second);
            TestEqual(TEXT("Height hash is deterministic"), HashA, HashB);
        }

        TestTrue(TEXT("MinHeight deterministic"),
                 FMath::IsNearlyEqual(First.MinHeight, Second.MinHeight));
        TestTrue(TEXT("MaxHeight deterministic"),
                 FMath::IsNearlyEqual(First.MaxHeight, Second.MaxHeight));
    }

    return true;
}

#endif // WITH_AUTOMATION_TESTS
