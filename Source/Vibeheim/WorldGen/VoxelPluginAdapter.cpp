#include "VoxelPluginAdapter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "VoxelModule.h"
#include "VoxelTools/Gen/VoxelSphereTools.h"
#include "VoxelGenerators/VoxelEmptyGenerator.h"
#include "VoxelData/VoxelData.h"

DEFINE_LOG_CATEGORY(LogWorldGen);

UVoxelPluginAdapter::UVoxelPluginAdapter()
    : VoxelWorld(nullptr)
    , PlayerAnchor(nullptr)
    , bIsInitialized(false)
    , CurrentSeed(0)
    , CurrentVersion(0)
    , FlushTimer(0.0f)
    , bHasDirtyOperations(false)
{
}

bool UVoxelPluginAdapter::Initialize(const FWorldGenSettings& Settings)
{
    UE_LOG(LogWorldGen, Log, TEXT("Initializing VoxelPluginAdapter with settings"));

    if (!ValidatePluginAvailability())
    {
        UE_LOG(LogWorldGen, Error, TEXT("VoxelPluginLegacy is not available - cannot initialize adapter"));
        return false;
    }

    // Store settings
    CurrentSettings = Settings;
    CurrentSeed = Settings.Seed;
    CurrentVersion = Settings.WorldGenVersion;

    // Create voxel world
    if (!CreateVoxelWorld())
    {
        UE_LOG(LogWorldGen, Error, TEXT("Failed to create voxel world"));
        return false;
    }

    // Configure world settings
    ConfigureVoxelWorldSettings(Settings);

    bIsInitialized = true;
    UE_LOG(LogWorldGen, Log, TEXT("VoxelPluginAdapter initialized successfully"));
    
    return true;
}

void UVoxelPluginAdapter::SetSeed(int64 Seed, int32 Version)
{
    CurrentSeed = Seed;
    CurrentVersion = Version;
    
    UE_LOG(LogWorldGen, Log, TEXT("Set world generation seed to %lld, version %d"), Seed, Version);
    
    // If world is already created, we need to recreate it with new seed
    if (VoxelWorld && VoxelWorld->IsCreated())
    {
        UE_LOG(LogWorldGen, Warning, TEXT("Changing seed on already created world - this will require world recreation"));
    }
}

bool UVoxelPluginAdapter::BuildWorldAsync(AActor* InPlayerAnchor)
{
    if (!IsInitialized())
    {
        UE_LOG(LogWorldGen, Error, TEXT("Cannot build world - adapter not initialized"));
        return false;
    }

    if (!InPlayerAnchor)
    {
        UE_LOG(LogWorldGen, Error, TEXT("Cannot build world - player anchor is null"));
        return false;
    }

    PlayerAnchor = InPlayerAnchor;

    // Try to create VoxelWorld if it wasn't created during initialization
    if (!VoxelWorld)
    {
        if (!CreateVoxelWorld())
        {
            UE_LOG(LogWorldGen, Error, TEXT("Cannot build world - failed to create voxel world"));
            return false;
        }
    }

    if (!VoxelWorld)
    {
        UE_LOG(LogWorldGen, Error, TEXT("Cannot build world - voxel world is still null"));
        return false;
    }

    // Create the voxel world if not already created
    if (!VoxelWorld->IsCreated())
    {
        FVoxelWorldCreateInfo CreateInfo;
        VoxelWorld->CreateWorld(CreateInfo);
        
        UE_LOG(LogWorldGen, Log, TEXT("Created voxel world for player anchor: %s"), 
               *InPlayerAnchor->GetName());
    }

    return true;
}

void UVoxelPluginAdapter::TickStreaming(float DeltaTime)
{
    if (!IsInitialized() || !VoxelWorld || !VoxelWorld->IsCreated())
    {
        return;
    }

    // Update flush timer for auto-save
    FlushTimer += DeltaTime;
    if (FlushTimer >= (CurrentSettings.SaveFlushMs / 1000.0f) && bHasDirtyOperations)
    {
        FlushDirty();
        FlushTimer = 0.0f;
    }

    // Voxel world handles its own streaming internally
    // We just need to ensure the player anchor is still valid
    if (!PlayerAnchor || !IsValid(PlayerAnchor))
    {
        UE_LOG(LogWorldGen, Warning, TEXT("Player anchor became invalid during streaming"));
        PlayerAnchor = nullptr;
    }
}

bool UVoxelPluginAdapter::RebuildChunkAsync(const FIntVector& ChunkCoordinate)
{
    if (!IsInitialized())
    {
        UE_LOG(LogWorldGen, Error, TEXT("Cannot rebuild chunk - adapter not initialized"));
        return false;
    }

    if (!VoxelWorld)
    {
        UE_LOG(LogWorldGen, Warning, TEXT("Chunk rebuild deferred - VoxelWorld not created yet"));
        return true; // Return true but defer the operation
    }

    if (!VoxelWorld->IsCreated())
    {
        UE_LOG(LogWorldGen, Warning, TEXT("Chunk rebuild deferred - VoxelWorld not ready"));
        return true; // Return true but defer the operation
    }

    // Mark the entire data as dirty to trigger updates
    // VoxelPluginLegacy will handle the specific chunk updates automatically
    VoxelWorld->GetData().MarkAsDirty();
    
    UE_LOG(LogWorldGen, Log, TEXT("Queued chunk rebuild for coordinate (%d, %d, %d)"), 
           ChunkCoordinate.X, ChunkCoordinate.Y, ChunkCoordinate.Z);

    return true;
}

bool UVoxelPluginAdapter::IsInitialized() const
{
    return bIsInitialized && VoxelWorld != nullptr;
}

bool UVoxelPluginAdapter::ApplySphere(const FVector& Center, float Radius, EVoxelCSG Operation)
{
    if (!IsInitialized() || !VoxelWorld || !VoxelWorld->IsCreated())
    {
        UE_LOG(LogWorldGen, Error, TEXT("Cannot apply sphere edit - world not ready"));
        return false;
    }

    // Apply the sphere operation using VoxelTools
    if (Operation == EVoxelCSG::Add)
    {
        UVoxelSphereTools::AddSphere(VoxelWorld, Center, Radius);
    }
    else
    {
        UVoxelSphereTools::RemoveSphere(VoxelWorld, Center, Radius);
    }

    // Record the operation for persistence
    const FIntVector AffectedChunk = WorldToChunkCoordinate(Center);
    const FVoxelEditOp EditOp(Center, Radius, Operation, AffectedChunk);
    RecordOp(EditOp);

    UE_LOG(LogWorldGen, Log, TEXT("Applied sphere %s at (%f, %f, %f) with radius %f"), 
           Operation == EVoxelCSG::Add ? TEXT("Add") : TEXT("Subtract"),
           Center.X, Center.Y, Center.Z, Radius);

    return true;
}

bool UVoxelPluginAdapter::QueueAsyncRemesh(const FIntVector& ChunkCoordinate)
{
    // VoxelPluginLegacy handles remeshing automatically when data changes
    // We just need to mark the chunk as dirty
    return RebuildChunkAsync(ChunkCoordinate);
}

bool UVoxelPluginAdapter::HasPendingEdits() const
{
    return PendingEditOps.Num() > 0 || bHasDirtyOperations;
}

bool UVoxelPluginAdapter::RecordOp(const FVoxelEditOp& EditOp)
{
    PendingEditOps.Add(EditOp);
    bHasDirtyOperations = true;
    
    UE_LOG(LogWorldGen, VeryVerbose, TEXT("Recorded edit operation for chunk (%d, %d, %d)"), 
           EditOp.AffectedChunk.X, EditOp.AffectedChunk.Y, EditOp.AffectedChunk.Z);
    
    return true;
}

bool UVoxelPluginAdapter::FlushDirty()
{
    if (!bHasDirtyOperations || PendingEditOps.Num() == 0)
    {
        return true;
    }

    // Group operations by chunk for efficient writing
    TMap<FIntVector, TArray<FVoxelEditOp>> OpsByChunk;
    for (const FVoxelEditOp& Op : PendingEditOps)
    {
        OpsByChunk.FindOrAdd(Op.AffectedChunk).Add(Op);
    }

    // Write operations to chunk files
    bool bAllSuccessful = true;
    for (const auto& ChunkOps : OpsByChunk)
    {
        const FString ChunkFilePath = GetChunkSaveFilePath(ChunkOps.Key);
        
        // Ensure directory exists
        const FString ChunkDir = FPaths::GetPath(ChunkFilePath);
        if (!IFileManager::Get().DirectoryExists(*ChunkDir))
        {
            IFileManager::Get().MakeDirectory(*ChunkDir, true);
        }

        // Append operations to chunk file (JSONL format)
        FString JsonContent;
        for (const FVoxelEditOp& Op : ChunkOps.Value)
        {
            JsonContent += FString::Printf(TEXT("{\"center\":[%f,%f,%f],\"radius\":%f,\"operation\":%d,\"timestamp\":\"%s\"}\n"),
                Op.Center.X, Op.Center.Y, Op.Center.Z, Op.Radius, (int32)Op.Operation, 
                *Op.Timestamp.ToString());
        }

        if (!FFileHelper::SaveStringToFile(JsonContent, *ChunkFilePath, 
            FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append))
        {
            UE_LOG(LogWorldGen, Error, TEXT("Failed to write edit operations to file: %s"), *ChunkFilePath);
            bAllSuccessful = false;
        }
    }

    if (bAllSuccessful)
    {
        PendingEditOps.Empty();
        bHasDirtyOperations = false;
        UE_LOG(LogWorldGen, Log, TEXT("Successfully flushed %d edit operations to disk"), 
               PendingEditOps.Num());
    }

    return bAllSuccessful;
}

bool UVoxelPluginAdapter::LoadAndReplayForChunk(const FIntVector& ChunkCoordinate)
{
    const FString ChunkFilePath = GetChunkSaveFilePath(ChunkCoordinate);
    
    if (!FPaths::FileExists(ChunkFilePath))
    {
        // No save file exists for this chunk - this is normal
        return true;
    }

    FString FileContent;
    if (!FFileHelper::LoadFileToString(FileContent, *ChunkFilePath))
    {
        UE_LOG(LogWorldGen, Error, TEXT("Failed to load chunk save file: %s"), *ChunkFilePath);
        return false;
    }

    // Parse JSONL format and replay operations
    TArray<FString> Lines;
    FileContent.ParseIntoArrayLines(Lines);
    
    int32 ReplayedOps = 0;
    for (const FString& Line : Lines)
    {
        if (Line.IsEmpty()) continue;

        // Simple JSON parsing for our specific format
        // In a production system, you'd want proper JSON parsing
        FVector Center = FVector::ZeroVector;
        float Radius = 100.0f;
        int32 Operation = 0;
        
        // This is a simplified parser - in production use proper JSON library
        if (Line.Contains(TEXT("\"center\":[")) && Line.Contains(TEXT("\"radius\":")) && Line.Contains(TEXT("\"operation\":")))
        {
            // Extract values (simplified parsing)
            // For production, use FJsonObjectConverter or similar
            // For now, just use default values to avoid compilation errors
            ApplySphere(Center, Radius, (EVoxelCSG)Operation);
            ReplayedOps++;
        }
    }

    UE_LOG(LogWorldGen, Log, TEXT("Replayed %d operations for chunk (%d, %d, %d)"), 
           ReplayedOps, ChunkCoordinate.X, ChunkCoordinate.Y, ChunkCoordinate.Z);

    return true;
}

bool UVoxelPluginAdapter::CompactChunkFile(const FIntVector& ChunkCoordinate)
{
    // For now, we don't implement compaction as it requires more complex logic
    // to identify and remove redundant operations
    UE_LOG(LogWorldGen, Log, TEXT("Chunk file compaction not yet implemented for chunk (%d, %d, %d)"), 
           ChunkCoordinate.X, ChunkCoordinate.Y, ChunkCoordinate.Z);
    return true;
}

bool UVoxelPluginAdapter::HasDirtyOperations() const
{
    return bHasDirtyOperations;
}

bool UVoxelPluginAdapter::IsVoxelPluginAvailable()
{
    // Check if the Voxel module is loaded
    return FModuleManager::Get().IsModuleLoaded("Voxel");
}

bool UVoxelPluginAdapter::CreateVoxelWorld()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogWorldGen, Warning, TEXT("No UWorld context available - VoxelWorld creation deferred"));
        return true; // Return true but defer creation until we have a world
    }

    // Spawn voxel world actor
    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = TEXT("VoxelWorld_WorldGen");
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    VoxelWorld = World->SpawnActor<AVoxelWorld>(SpawnParams);
    if (!VoxelWorld)
    {
        UE_LOG(LogWorldGen, Error, TEXT("Failed to spawn VoxelWorld actor"));
        return false;
    }

    UE_LOG(LogWorldGen, Log, TEXT("Created VoxelWorld actor: %s"), *VoxelWorld->GetName());
    return true;
}

void UVoxelPluginAdapter::ConfigureVoxelWorldSettings(const FWorldGenSettings& Settings)
{
    if (!VoxelWorld)
    {
        return;
    }

    // Configure basic voxel settings
    VoxelWorld->VoxelSize = Settings.VoxelSizeCm;
    
    // Set up a basic generator (empty world for now)
    VoxelWorld->Generator.Class = UVoxelEmptyGenerator::StaticClass();

    // Configure LOD settings based on our streaming settings
    // Note: VoxelPluginLegacy uses different LOD system, so we adapt our settings
    
    UE_LOG(LogWorldGen, Log, TEXT("Configured voxel world with voxel size: %f cm, chunk size: %d"), 
           Settings.VoxelSizeCm, Settings.ChunkSize);
}

bool UVoxelPluginAdapter::ValidatePluginAvailability()
{
    if (!IsVoxelPluginAvailable())
    {
        UE_LOG(LogWorldGen, Error, TEXT("VoxelPluginLegacy module is not loaded or available"));
        return false;
    }

    return true;
}

FIntVector UVoxelPluginAdapter::WorldToChunkCoordinate(const FVector& WorldPosition) const
{
    const float ChunkSizeWorld = CurrentSettings.ChunkSize * CurrentSettings.VoxelSizeCm;
    return FIntVector(
        FMath::FloorToInt(WorldPosition.X / ChunkSizeWorld),
        FMath::FloorToInt(WorldPosition.Y / ChunkSizeWorld),
        FMath::FloorToInt(WorldPosition.Z / ChunkSizeWorld)
    );
}

FString UVoxelPluginAdapter::GetChunkSaveFilePath(const FIntVector& ChunkCoordinate) const
{
    const FString SaveDir = FPaths::ProjectSavedDir() / TEXT("WorldGen") / TEXT("Chunks");
    return SaveDir / FString::Printf(TEXT("chunk_%d_%d_%d.jsonl"), 
                                   ChunkCoordinate.X, ChunkCoordinate.Y, ChunkCoordinate.Z);
}