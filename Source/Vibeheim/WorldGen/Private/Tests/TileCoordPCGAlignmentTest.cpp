#if WITH_AUTOMATION_TESTS

#include "Data/WorldGenTypes.h"
#include "Misc/AutomationTest.h"

/**
 * Property 6: PCG Grid Alignment Round-Trip
 * Verifies that FTileCoord converts to and from PCG grid cells when sizes share an integer ratio.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTileCoordPCGGridRoundTripTest,
    "Vibeheim.WorldGen.TileCoord.Property.PCGGridRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTileCoordPCGGridRoundTripTest::RunTest(const FString& Parameters)
{
    constexpr float TileSize = 64.0f;
    constexpr float EqualGridSize = 64.0f;
    constexpr float SubdividedGridSize = 32.0f; // Tile subdivides into a 2x2 PCG grid

    TestTrue(TEXT("Equal grid size should align with tile size"),
             FTileCoord::IsAlignedWithPCGGrid(TileSize, EqualGridSize));
    TestTrue(TEXT("Subdivided grid should align when TileSize / PCGGridSize is integer"),
             FTileCoord::IsAlignedWithPCGGrid(TileSize, SubdividedGridSize));
    TestFalse(TEXT("Coarser grids collapse tiles and are treated as misaligned"),
              FTileCoord::IsAlignedWithPCGGrid(TileSize, 128.0f));
    TestFalse(TEXT("Non-integer ratios should report misalignment"),
              FTileCoord::IsAlignedWithPCGGrid(TileSize, 48.0f));

    FRandomStream RandomStream(2025);
    constexpr int32 Iterations = 25;

    for (int32 Index = 0; Index < Iterations; ++Index)
    {
        const int32 TileX = RandomStream.RandRange(-50, 50);
        const int32 TileY = RandomStream.RandRange(-50, 50);
        const FTileCoord Tile(TileX, TileY);

        // 1:1 mapping
        {
            const FIntPoint Cell = Tile.ToPCGGridCell(EqualGridSize, TileSize);
            const FTileCoord RoundTrip = FTileCoord::FromPCGGridCell(Cell, EqualGridSize, TileSize);
            TestEqual(TEXT("Round-trip should preserve tile when grid equals tile size"), RoundTrip, Tile);
        }

        // Tile subdivides the PCG grid
        {
            const FIntPoint Cell = Tile.ToPCGGridCell(SubdividedGridSize, TileSize);
            const FTileCoord RoundTrip = FTileCoord::FromPCGGridCell(Cell, SubdividedGridSize, TileSize);
            TestEqual(TEXT("Round-trip should preserve tile when tile spans integer PCG cells"), RoundTrip, Tile);
        }
    }

    return true;
}

#endif // WITH_AUTOMATION_TESTS
