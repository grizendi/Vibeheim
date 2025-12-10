#if WITH_AUTOMATION_TESTS

#include "WorldGenManager.h"
#include "Misc/AutomationTest.h"

namespace
{
FWorldBuildState MakeValidBuildStateMatching(const FWorldGenConfig& Config)
{
    FWorldBuildState State;
    State.BuiltSeed = Config.Seed;
    State.WorldGenVersion = Config.WorldGenVersion;
    State.PCGBuildHash = TEXT("TestHash");
    State.LastBuildTime = FDateTime::UtcNow();
    State.bIsBaked = true;
    return State;
}
} // namespace

/**
 * Property 1: Build Mode Determines Streaming Behavior
 * Validates runtime streaming enablement across build modes.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBuildModeStreamingDecisionPropertyTest,
    "Vibeheim.WorldGen.BuildMode.Property.StreamingDecision",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBuildModeStreamingDecisionPropertyTest::RunTest(const FString& Parameters)
{
    FWorldGenConfig Config;
    Config.StaleBuildPolicy = EWorldBuildStatePolicy::FallbackToRuntime;

    const FWorldBuildState ValidState = MakeValidBuildStateMatching(Config);

    // RuntimeStreaming: legacy behavior stays enabled
    Config.BuildMode = EWorldGenBuildMode::RuntimeStreaming;
    {
        const FWorldGenRuntimeDecision Decision =
            EvaluateWorldGenRuntimeDecision(Config, ValidState, true, Config.StaleBuildPolicy);
        TestTrue(TEXT("RuntimeStreaming enables runtime streaming"), Decision.bUseRuntimeStreaming);
        TestFalse(TEXT("RuntimeStreaming does not treat world as baked"), Decision.bTreatWorldAsBaked);
    }

    // EditorBuildOnce with valid build state disables runtime streaming
    Config.BuildMode = EWorldGenBuildMode::EditorBuildOnce;
    {
        const FWorldGenRuntimeDecision Decision =
            EvaluateWorldGenRuntimeDecision(Config, ValidState, true, Config.StaleBuildPolicy);
        TestFalse(TEXT("Baked EditorBuildOnce disables runtime streaming"), Decision.bUseRuntimeStreaming);
        TestTrue(TEXT("Baked EditorBuildOnce treats world as baked"), Decision.bTreatWorldAsBaked);
    }

    // EditorBuildOnce without build state falls back when policy allows
    {
        const FWorldGenRuntimeDecision Decision =
            EvaluateWorldGenRuntimeDecision(Config, FWorldBuildState(), false, EWorldBuildStatePolicy::FallbackToRuntime);
        TestTrue(TEXT("Missing build state falls back to runtime streaming"), Decision.bUseRuntimeStreaming);
        TestFalse(TEXT("Missing build state is not treated as baked"), Decision.bTreatWorldAsBaked);
    }

    // Hybrid initializes runtime streaming but still acknowledges baked content when available
    Config.BuildMode = EWorldGenBuildMode::Hybrid;
    {
        const FWorldGenRuntimeDecision Decision =
            EvaluateWorldGenRuntimeDecision(Config, ValidState, true, EWorldBuildStatePolicy::FallbackToRuntime);
        TestTrue(TEXT("Hybrid keeps runtime streaming active"), Decision.bUseRuntimeStreaming);
        TestTrue(TEXT("Hybrid can use baked content when valid"), Decision.bTreatWorldAsBaked);
    }

    return true;
}

/**
 * Property 3: Seed Mismatch Detection
 * Ensures mismatched seeds trigger policy-driven responses.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSeedMismatchDetectionPropertyTest,
    "Vibeheim.WorldGen.BuildMode.Property.SeedMismatchDetection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSeedMismatchDetectionPropertyTest::RunTest(const FString& Parameters)
{
    FWorldGenConfig Config;
    Config.BuildMode = EWorldGenBuildMode::EditorBuildOnce;

    FWorldBuildState MismatchedState = MakeValidBuildStateMatching(Config);
    MismatchedState.BuiltSeed = Config.Seed + 42;

    // Fallback policy switches to runtime streaming on mismatch
    Config.StaleBuildPolicy = EWorldBuildStatePolicy::FallbackToRuntime;
    {
        const FWorldGenRuntimeDecision Decision =
            EvaluateWorldGenRuntimeDecision(Config, MismatchedState, true, Config.StaleBuildPolicy);
        TestTrue(TEXT("Seed mismatch enables runtime fallback"), Decision.bUseRuntimeStreaming);
        TestFalse(TEXT("Seed mismatch disables baked path"), Decision.bTreatWorldAsBaked);
        TestFalse(TEXT("Fallback policy does not force rebuild"), Decision.bRequireRebuild);
    }

    // RequireRebuild policy blocks runtime generation and marks stale
    Config.StaleBuildPolicy = EWorldBuildStatePolicy::RequireRebuild;
    {
        const FWorldGenRuntimeDecision Decision =
            EvaluateWorldGenRuntimeDecision(Config, MismatchedState, true, Config.StaleBuildPolicy);
        TestFalse(TEXT("RequireRebuild disables runtime streaming on mismatch"), Decision.bUseRuntimeStreaming);
        TestTrue(TEXT("RequireRebuild flags rebuild requirement"), Decision.bRequireRebuild);
        TestFalse(TEXT("RequireRebuild does not treat mismatched state as baked"), Decision.bTreatWorldAsBaked);
    }

    return true;
}

#endif // WITH_AUTOMATION_TESTS
