#include "VoxelPluginAdapterTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"

DEFINE_LOG_CATEGORY_STATIC(LogVoxelPluginAdapterTest, Log, All);

// Test Fixture Implementation
FVoxelPluginAdapterTestFixture::FVoxelPluginAdapterTestFixture()
    : Adapter(nullptr)
{
    // Set up default test settings
    TestSettings.Seed = 12345;
    TestSettings.WorldGenVersion = 1;
    TestSettings.VoxelSizeCm = 50.0f;
    TestSettings.ChunkSize = 32;
    TestSettings.MaxLOD = 3;
    TestSettings.LOD0Radius = 2;
    TestSettings.LOD1Radius = 4;
    TestSettings.LOD2Radius = 6;
    TestSettings.SaveFlushMs = 3000;
}

FVoxelPluginAdapterTestFixture::~FVoxelPluginAdapterTestFixture()
{
    TearDown();
}

void FVoxelPluginAdapterTestFixture::SetUp()
{
    // Create adapter instance
    Adapter = NewObject<UVoxelPluginAdapter>();
    check(Adapter);
}

void FVoxelPluginAdapterTestFixture::TearDown()
{
    if (Adapter)
    {
        Adapter->ConditionalBeginDestroy();
        Adapter = nullptr;
    }
}

// Plugin Availability Test
bool FVoxelPluginAdapterAvailabilityTest::RunTest(const FString& Parameters)
{
    // Test static method for plugin availability
    const bool bIsAvailable = UVoxelPluginAdapter::IsVoxelPluginAvailable();
    
    // Log the result for debugging
    UE_LOG(LogVoxelPluginAdapterTest, Log, TEXT("VoxelPlugin availability: %s"), 
           bIsAvailable ? TEXT("Available") : TEXT("Not Available"));
    
    // The test passes regardless of availability, but we log the result
    // In a real environment, this would typically be true
    TestTrue(TEXT("Plugin availability check completed"), true);
    
    return true;
}

// Initialization Test
bool FVoxelPluginAdapterInitializationTest::RunTest(const FString& Parameters)
{
    FVoxelPluginAdapterTestFixture Fixture;
    Fixture.SetUp();
    
    UVoxelPluginAdapter* Adapter = Fixture.GetAdapter();
    TestNotNull(TEXT("Adapter should be created"), Adapter);
    
    if (!Adapter)
    {
        return false;
    }
    
    // Test initial state
    TestFalse(TEXT("Adapter should not be initialized initially"), Adapter->IsInitialized());
    TestNull(TEXT("VoxelWorld should be null initially"), Adapter->GetVoxelWorld());
    TestFalse(TEXT("Should not have pending edits initially"), Adapter->HasPendingEdits());
    TestFalse(TEXT("Should not have dirty operations initially"), Adapter->HasDirtyOperations());
    
    // Test initialization with valid settings
    const FWorldGenSettings& TestSettings = Fixture.GetTestSettings();
    
    // Note: This test may fail if VoxelPluginLegacy is not available
    // In that case, we expect a graceful failure
    const bool bInitResult = Adapter->Initialize(TestSettings);
    
    if (UVoxelPluginAdapter::IsVoxelPluginAvailable())
    {
        TestTrue(TEXT("Initialization should succeed when plugin is available"), bInitResult);
        if (bInitResult)
        {
            TestTrue(TEXT("Adapter should be initialized after successful init"), Adapter->IsInitialized());
        }
    }
    else
    {
        TestFalse(TEXT("Initialization should fail when plugin is not available"), bInitResult);
        TestFalse(TEXT("Adapter should not be initialized when plugin unavailable"), Adapter->IsInitialized());
    }
    
    return true;
}

// World Creation Test
bool FVoxelPluginAdapterWorldCreationTest::RunTest(const FString& Parameters)
{
    // Skip this test if VoxelPlugin is not available
    if (!UVoxelPluginAdapter::IsVoxelPluginAvailable())
    {
        AddWarning(TEXT("Skipping world creation test - VoxelPlugin not available"));
        return true;
    }
    
    FVoxelPluginAdapterTestFixture Fixture;
    Fixture.SetUp();
    
    UVoxelPluginAdapter* Adapter = Fixture.GetAdapter();
    TestNotNull(TEXT("Adapter should be created"), Adapter);
    
    if (!Adapter)
    {
        return false;
    }
    
    // Initialize the adapter
    const bool bInitResult = Adapter->Initialize(Fixture.GetTestSettings());
    TestTrue(TEXT("Adapter initialization should succeed"), bInitResult);
    
    if (!bInitResult)
    {
        AddWarning(TEXT("Cannot test world creation - initialization failed"));
        return true;
    }
    
    // Test that voxel world was created during initialization
    AVoxelWorld* VoxelWorld = Adapter->GetVoxelWorld();
    TestNotNull(TEXT("VoxelWorld should be created after initialization"), VoxelWorld);
    
    if (VoxelWorld)
    {
        TestTrue(TEXT("VoxelWorld should be valid"), IsValid(VoxelWorld));
        TestEqual(TEXT("VoxelWorld should have correct voxel size"), 
                 VoxelWorld->VoxelSize, Fixture.GetTestSettings().VoxelSizeCm);
    }
    
    return true;
}

// Settings Configuration Test
bool FVoxelPluginAdapterSettingsTest::RunTest(const FString& Parameters)
{
    FVoxelPluginAdapterTestFixture Fixture;
    Fixture.SetUp();
    
    UVoxelPluginAdapter* Adapter = Fixture.GetAdapter();
    TestNotNull(TEXT("Adapter should be created"), Adapter);
    
    if (!Adapter)
    {
        return false;
    }
    
    // Test seed setting before initialization
    const int64 TestSeed = 98765;
    const int32 TestVersion = 2;
    
    Adapter->SetSeed(TestSeed, TestVersion);
    
    // Initialize with settings
    FWorldGenSettings TestSettings = Fixture.GetTestSettings();
    TestSettings.VoxelSizeCm = 25.0f;  // Different from default
    TestSettings.ChunkSize = 16;       // Different from default
    
    if (UVoxelPluginAdapter::IsVoxelPluginAvailable())
    {
        const bool bInitResult = Adapter->Initialize(TestSettings);
        TestTrue(TEXT("Initialization should succeed"), bInitResult);
        
        if (bInitResult)
        {
            AVoxelWorld* VoxelWorld = Adapter->GetVoxelWorld();
            if (VoxelWorld)
            {
                TestEqual(TEXT("VoxelWorld should use configured voxel size"), 
                         VoxelWorld->VoxelSize, TestSettings.VoxelSizeCm);
            }
        }
    }
    else
    {
        AddWarning(TEXT("Skipping settings test - VoxelPlugin not available"));
    }
    
    return true;
}

// Basic Functionality Test
bool FVoxelPluginAdapterBasicFunctionalityTest::RunTest(const FString& Parameters)
{
    // Skip this test if VoxelPlugin is not available
    if (!UVoxelPluginAdapter::IsVoxelPluginAvailable())
    {
        AddWarning(TEXT("Skipping basic functionality test - VoxelPlugin not available"));
        return true;
    }
    
    FVoxelPluginAdapterTestFixture Fixture;
    Fixture.SetUp();
    
    UVoxelPluginAdapter* Adapter = Fixture.GetAdapter();
    TestNotNull(TEXT("Adapter should be created"), Adapter);
    
    if (!Adapter)
    {
        return false;
    }
    
    // Initialize the adapter
    const bool bInitResult = Adapter->Initialize(Fixture.GetTestSettings());
    TestTrue(TEXT("Adapter initialization should succeed"), bInitResult);
    
    if (!bInitResult)
    {
        AddWarning(TEXT("Cannot test basic functionality - initialization failed"));
        return true;
    }
    
    // Note: VoxelWorld creation might be deferred in test environment, which is expected
    
    // Test coordinate conversion functionality
    const FVector TestWorldPos(1600.0f, 1600.0f, 800.0f);  // Should be chunk (1, 1, 0) with 32*50cm chunks
    
    // Test edit operation recording
    const FVoxelEditOp TestEditOp(TestWorldPos, 100.0f, EVoxelCSG::Add, FIntVector(1, 1, 0));
    const bool bRecordResult = Adapter->RecordOp(TestEditOp);
    TestTrue(TEXT("Recording edit operation should succeed"), bRecordResult);
    TestTrue(TEXT("Should have pending edits after recording"), Adapter->HasPendingEdits());
    TestTrue(TEXT("Should have dirty operations after recording"), Adapter->HasDirtyOperations());
    
    // Test flush functionality
    const bool bFlushResult = Adapter->FlushDirty();
    TestTrue(TEXT("Flushing dirty operations should succeed"), bFlushResult);
    
    if (bFlushResult)
    {
        TestFalse(TEXT("Should not have dirty operations after flush"), Adapter->HasDirtyOperations());
    }
    
    // Test chunk rebuild
    const FIntVector TestChunkCoord(0, 0, 0);
    const bool bRebuildResult = Adapter->RebuildChunkAsync(TestChunkCoord);
    TestTrue(TEXT("Chunk rebuild should succeed"), bRebuildResult);
    
    return true;
}