#if WITH_AUTOMATION_TESTS

#include "Data/WorldGenBuildState.h"
#include "Data/WorldGenTypes.h"
#include "Misc/AutomationTest.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

/**
 * Property 2: Build State Persistence Round-Trip
 * Verifies that FWorldBuildState survives serialization with all fields intact.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWorldBuildStateRoundTripPropertyTest,
    "Vibeheim.WorldGen.BuildState.Property.RoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWorldBuildStateRoundTripPropertyTest::RunTest(const FString& Parameters)
{
    constexpr int32 Iterations = 10;
    FRandomStream RandomStream(1337);

    for (int32 Index = 0; Index < Iterations; ++Index)
    {
        FWorldBuildState Original;
        Original.BuiltSeed = RandomStream.RandRange(1, INT32_MAX / 2);
        Original.WorldGenVersion = RandomStream.RandRange(1, 10000);
        Original.LastBuildTime = FDateTime(2024, 1, 1, Index % 24, 0, 0);
        Original.PCGBuildHash = FString::Printf(TEXT("PCG_%08x"), RandomStream.RandHelper(INT32_MAX));
        Original.bIsBaked = true;

        TestTrue(TEXT("Build state should be valid before serialization"), Original.IsValid());

        TArray<uint8> Buffer;
        {
            FMemoryWriter Writer(Buffer, true);
            Writer << Original;
        }

        FWorldBuildState RoundTripped;
        {
            FMemoryReader Reader(Buffer, true);
            Reader << RoundTripped;
        }

        TestTrue(TEXT("Round-tripped state should be valid"), RoundTripped.IsValid());
        TestEqual(TEXT("BuiltSeed should round-trip"), RoundTripped.BuiltSeed, Original.BuiltSeed);
        TestEqual(TEXT("WorldGenVersion should round-trip"), RoundTripped.WorldGenVersion, Original.WorldGenVersion);
        TestEqual(TEXT("LastBuildTime should round-trip"), RoundTripped.LastBuildTime, Original.LastBuildTime);
        TestEqual(TEXT("PCGBuildHash should round-trip"), RoundTripped.PCGBuildHash, Original.PCGBuildHash);
        TestEqual(TEXT("bIsBaked should round-trip"), RoundTripped.bIsBaked, Original.bIsBaked);

        FWorldGenConfig MatchingConfig;
        MatchingConfig.Seed = Original.BuiltSeed;
        MatchingConfig.WorldGenVersion = Original.WorldGenVersion;
        TestTrue(TEXT("Round-tripped state should be compatible with matching config"),
                 RoundTripped.IsCompatibleWith(MatchingConfig));

        FWorldGenConfig MismatchConfig = MatchingConfig;
        MismatchConfig.Seed += 1;
        TestFalse(TEXT("Seed mismatch should be incompatible"), RoundTripped.IsCompatibleWith(MismatchConfig));
    }

    return true;
}

#endif // WITH_AUTOMATION_TESTS
