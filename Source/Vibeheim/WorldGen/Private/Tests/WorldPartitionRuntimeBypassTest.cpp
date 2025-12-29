#if WITH_AUTOMATION_TESTS

#include "WorldGenManager.h"
#include "WorldGenSettings.h"
#include "Services/TileStreamingService.h"
#include "Misc/AutomationTest.h"

/**
 * Property 10: Runtime Generation Bypass in Baked Mode
 * Ensures TileStreamingService::UpdateStreaming is not invoked for baked worlds.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRuntimeGenerationBypassPropertyTest,
    "Vibeheim.WorldGen.WorldPartition.Property.RuntimeBypass",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRuntimeGenerationBypassPropertyTest::RunTest(const FString& Parameters)
{
    AWorldGenManager* Manager =
        NewObject<AWorldGenManager>(GetTransientPackage());
    UWorldGenSettings* Settings =
        NewObject<UWorldGenSettings>(GetTransientPackage());
    Settings->Settings.BuildMode = EWorldGenBuildMode::EditorBuildOnce;

    UTileStreamingService* StreamingService =
        NewObject<UTileStreamingService>(GetTransientPackage());
    FTileStreamingServiceTestAccessor::ResetUpdateStreamingCalls(StreamingService);

    FWorldGenRuntimeDecision Decision;
    Decision.bUseRuntimeStreaming = false;
    Decision.bTreatWorldAsBaked = true;

    FWorldGenManagerTestAccessor::SetRuntimeDecision(Manager, Decision);
    FWorldGenManagerTestAccessor::SetWorldPartitionSuppression(Manager, true);
    FWorldGenManagerTestAccessor::SetTileStreamingService(Manager, StreamingService);
    FWorldGenManagerTestAccessor::SetWorldGenSettings(Manager, Settings);

    Manager->UpdateWorldStreaming();

    const int32 Calls =
        FTileStreamingServiceTestAccessor::GetUpdateStreamingCalls(StreamingService);
    TestEqual(TEXT("UpdateStreaming should not run in baked EditorBuildOnce mode"),
              Calls, 0);

    return true;
}

#endif // WITH_AUTOMATION_TESTS
