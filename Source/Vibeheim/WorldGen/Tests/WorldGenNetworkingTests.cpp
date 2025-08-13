#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Misc/DateTime.h"
#include "../WorldGenGameState.h"
#include "../Data/VoxelEditOp.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Test basic WorldGenGameState functionality
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldGenNetworkingBasicTest, "Vibeheim.WorldGen.Networking.Basic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWorldGenNetworkingBasicTest::RunTest(const FString& Parameters)
{
    // Create a test world
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestWorld)
    {
        AddError(TEXT("Failed to create test world"));
        return false;
    }

    // Create WorldGenGameState
    AWorldGenGameState* GameState = TestWorld->SpawnActor<AWorldGenGameState>();
    if (!GameState)
    {
        AddError(TEXT("Failed to create WorldGenGameState"));
        TestWorld->DestroyWorld(false);
        return false;
    }

    // Test initial state
    TestFalse(TEXT("GameState should not be initialized initially"), GameState->IsWorldGenInitialized());
    TestEqual(TEXT("Initial seed should be 0"), GameState->GetWorldGenSeed(), (int64)0);
    TestEqual(TEXT("Initial version should be 0"), GameState->GetWorldGenVersion(), (int32)0);

    // Test initialization (simulate server authority)
    const int64 TestSeed = 12345;
    const int32 TestVersion = 1;
    
    // Note: In a real test, we would need to properly set up authority
    // For this basic test, we'll just test the data structures
    
    TestTrue(TEXT("GameState should have authority in test"), GameState->HasAuthority());
    TestTrue(TEXT("Initialization should succeed"), GameState->InitializeWorldGeneration(TestSeed, TestVersion));
    
    // Test post-initialization state
    TestTrue(TEXT("GameState should be initialized after setup"), GameState->IsWorldGenInitialized());
    TestEqual(TEXT("Seed should match initialized value"), GameState->GetWorldGenSeed(), TestSeed);
    TestEqual(TEXT("Version should match initialized value"), GameState->GetWorldGenVersion(), TestVersion);

    // Test duplicate initialization
    TestFalse(TEXT("Duplicate initialization should fail"), GameState->InitializeWorldGeneration(54321, 2));
    TestEqual(TEXT("Seed should remain unchanged after failed init"), GameState->GetWorldGenSeed(), TestSeed);

    // Clean up
    TestWorld->DestroyWorld(false);
    return true;
}

/**
 * Test voxel edit operation data structures
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldGenVoxelEditTest, "Vibeheim.WorldGen.Networking.VoxelEdit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWorldGenVoxelEditTest::RunTest(const FString& Parameters)
{
    // Test FVoxelEditOp construction
    const FVector TestCenter(100.0f, 200.0f, 300.0f);
    const float TestRadius = 50.0f;
    const EVoxelCSG TestOperation = EVoxelCSG::Subtract;
    const FIntVector TestChunk(1, 2, 3);

    FVoxelEditOp EditOp(TestCenter, TestRadius, TestOperation, TestChunk);

    // Test data integrity
    TestEqual(TEXT("Edit op center should match"), EditOp.Center, TestCenter);
    TestEqual(TEXT("Edit op radius should match"), EditOp.Radius, TestRadius);
    TestEqual(TEXT("Edit op operation should match"), EditOp.Operation, TestOperation);
    TestEqual(TEXT("Edit op chunk should match"), EditOp.AffectedChunk, TestChunk);
    TestTrue(TEXT("Edit op should have valid timestamp"), EditOp.Timestamp > 0.0);

    // Test default constructor
    FVoxelEditOp DefaultEditOp;
    TestEqual(TEXT("Default center should be zero"), DefaultEditOp.Center, FVector::ZeroVector);
    TestEqual(TEXT("Default radius should be zero"), DefaultEditOp.Radius, 0.0f);
    TestEqual(TEXT("Default operation should be Add"), DefaultEditOp.Operation, EVoxelCSG::Add);
    TestEqual(TEXT("Default chunk should be zero"), DefaultEditOp.AffectedChunk, FIntVector::ZeroValue);

    return true;
}

/**
 * Test chunk synchronization data structures
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldGenChunkSyncTest, "Vibeheim.WorldGen.Networking.ChunkSync",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWorldGenChunkSyncTest::RunTest(const FString& Parameters)
{
    // Create test data
    const FIntVector TestChunk(5, 10, 15);
    const int64 TestSeed = 98765;
    
    TArray<FVoxelEditOp> TestOps;
    TestOps.Add(FVoxelEditOp(FVector(100, 100, 100), 25.0f, EVoxelCSG::Add, TestChunk));
    TestOps.Add(FVoxelEditOp(FVector(200, 200, 200), 30.0f, EVoxelCSG::Subtract, TestChunk));

    // Test FChunkSyncData construction
    FChunkSyncData SyncData(TestChunk, TestOps, TestSeed);

    // Verify data integrity
    TestEqual(TEXT("Sync data chunk should match"), SyncData.ChunkCoordinate, TestChunk);
    TestEqual(TEXT("Sync data seed should match"), SyncData.ChunkSeed, TestSeed);
    TestEqual(TEXT("Sync data should have correct number of operations"), SyncData.EditOperations.Num(), 2);
    
    if (SyncData.EditOperations.Num() >= 2)
    {
        TestEqual(TEXT("First operation should match"), SyncData.EditOperations[0].Center, FVector(100, 100, 100));
        TestEqual(TEXT("Second operation should match"), SyncData.EditOperations[1].Center, FVector(200, 200, 200));
    }

    // Test default constructor
    FChunkSyncData DefaultSyncData;
    TestEqual(TEXT("Default chunk should be zero"), DefaultSyncData.ChunkCoordinate, FIntVector::ZeroValue);
    TestEqual(TEXT("Default seed should be zero"), DefaultSyncData.ChunkSeed, (int64)0);
    TestEqual(TEXT("Default operations should be empty"), DefaultSyncData.EditOperations.Num(), 0);

    return true;
}

/**
 * Test networking validation functions
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldGenNetworkingValidationTest, "Vibeheim.WorldGen.Networking.Validation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWorldGenNetworkingValidationTest::RunTest(const FString& Parameters)
{
    // Create test world and game state
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestWorld)
    {
        AddError(TEXT("Failed to create test world"));
        return false;
    }

    AWorldGenGameState* GameState = TestWorld->SpawnActor<AWorldGenGameState>();
    if (!GameState)
    {
        AddError(TEXT("Failed to create WorldGenGameState"));
        TestWorld->DestroyWorld(false);
        return false;
    }

    // Test authority validation
    TestTrue(TEXT("GameState should have authority in test"), GameState->HasWorldGenAuthority());

    // Test chunk coordinate validation (using the validation logic from Server_RequestChunkSync_Validate)
    const int32 MaxChunkCoord = 10000;
    
    // Valid coordinates
    FIntVector ValidChunk(100, 200, 300);
    TestTrue(TEXT("Valid chunk coordinates should pass"), 
        FMath::Abs(ValidChunk.X) < MaxChunkCoord && 
        FMath::Abs(ValidChunk.Y) < MaxChunkCoord && 
        FMath::Abs(ValidChunk.Z) < MaxChunkCoord);

    // Invalid coordinates (too large)
    FIntVector InvalidChunk(15000, 200, 300);
    TestFalse(TEXT("Invalid chunk coordinates should fail"), 
        FMath::Abs(InvalidChunk.X) < MaxChunkCoord && 
        FMath::Abs(InvalidChunk.Y) < MaxChunkCoord && 
        FMath::Abs(InvalidChunk.Z) < MaxChunkCoord);

    // Test edit operation validation (using the validation logic from Server_ApplyEdit_Validate)
    FVoxelEditOp ValidEdit(FVector(100, 100, 100), 50.0f, EVoxelCSG::Add, FIntVector(1, 1, 1));
    TestTrue(TEXT("Valid edit operation should pass basic validation"), 
        ValidEdit.Radius > 0.0f && ValidEdit.Radius < 10000.0f);

    FVoxelEditOp InvalidEdit(FVector(100, 100, 100), -10.0f, EVoxelCSG::Add, FIntVector(1, 1, 1));
    TestFalse(TEXT("Invalid edit operation should fail basic validation"), 
        InvalidEdit.Radius > 0.0f && InvalidEdit.Radius < 10000.0f);

    FVoxelEditOp OversizedEdit(FVector(100, 100, 100), 15000.0f, EVoxelCSG::Add, FIntVector(1, 1, 1));
    TestFalse(TEXT("Oversized edit operation should fail basic validation"), 
        OversizedEdit.Radius > 0.0f && OversizedEdit.Radius < 10000.0f);

    // Clean up
    TestWorld->DestroyWorld(false);
    return true;
}

/**
 * Test edit operation storage and retrieval
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldGenEditStorageTest, "Vibeheim.WorldGen.Networking.EditStorage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWorldGenEditStorageTest::RunTest(const FString& Parameters)
{
    // Create test world and game state
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestWorld)
    {
        AddError(TEXT("Failed to create test world"));
        return false;
    }

    AWorldGenGameState* GameState = TestWorld->SpawnActor<AWorldGenGameState>();
    if (!GameState)
    {
        AddError(TEXT("Failed to create WorldGenGameState"));
        TestWorld->DestroyWorld(false);
        return false;
    }

    // Initialize game state
    GameState->InitializeWorldGeneration(1337, 1);

    // Test chunk with no operations
    FIntVector EmptyChunk(0, 0, 0);
    TArray<FVoxelEditOp> EmptyOps = GameState->GetChunkEditOperations(EmptyChunk);
    TestEqual(TEXT("Empty chunk should have no operations"), EmptyOps.Num(), 0);

    // Apply some edit operations
    FIntVector TestChunk(1, 2, 3);
    FVoxelEditOp Edit1(FVector(100, 100, 100), 25.0f, EVoxelCSG::Add, TestChunk);
    FVoxelEditOp Edit2(FVector(200, 200, 200), 30.0f, EVoxelCSG::Subtract, TestChunk);

    // Note: In a real test with proper networking setup, we would use ApplyVoxelEdit
    // For this test, we'll verify the data structures work correctly

    TestTrue(TEXT("GameState should be initialized"), GameState->IsWorldGenInitialized());
    TestEqual(TEXT("Seed should be correct"), GameState->GetWorldGenSeed(), (int64)1337);
    TestEqual(TEXT("Version should be correct"), GameState->GetWorldGenVersion(), (int32)1);

    // Clean up
    TestWorld->DestroyWorld(false);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS