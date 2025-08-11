#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "../VoxelPluginAdapter.h"
#include "../Data/WorldGenSettings.h"

/**
 * Test fixture for VoxelPluginAdapter tests
 */
class FVoxelPluginAdapterTestFixture
{
public:
    FVoxelPluginAdapterTestFixture();
    ~FVoxelPluginAdapterTestFixture();

    void SetUp();
    void TearDown();

    UVoxelPluginAdapter* GetAdapter() const { return Adapter; }
    const FWorldGenSettings& GetTestSettings() const { return TestSettings; }

private:
    UVoxelPluginAdapter* Adapter;
    FWorldGenSettings TestSettings;
};

/**
 * Test class for VoxelPluginAdapter initialization and basic functionality
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelPluginAdapterInitializationTest, 
    "Vibeheim.WorldGen.VoxelPluginAdapter.Initialization",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

/**
 * Test class for VoxelPluginAdapter plugin availability validation
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelPluginAdapterAvailabilityTest,
    "Vibeheim.WorldGen.VoxelPluginAdapter.PluginAvailability", 
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

/**
 * Test class for VoxelPluginAdapter world creation
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelPluginAdapterWorldCreationTest,
    "Vibeheim.WorldGen.VoxelPluginAdapter.WorldCreation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

/**
 * Test class for VoxelPluginAdapter settings configuration
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelPluginAdapterSettingsTest,
    "Vibeheim.WorldGen.VoxelPluginAdapter.Settings",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

/**
 * Test class for VoxelPluginAdapter basic functionality
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelPluginAdapterBasicFunctionalityTest,
    "Vibeheim.WorldGen.VoxelPluginAdapter.BasicFunctionality",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)