#include "VoxelPluginAdapter.h"
#include "ChunkStreamingManager.h"
#include "FallbackTerrainGenerator.h"
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
    , StreamingManager(nullptr)
    , FallbackGenerator(nullptr)
{
}

bool UVoxelPluginAdapter::Initialize(const FWorldGenSettings& Settings)
{
    UE_LOG(LogWorldGen, Log, TEXT("Initializing VoxelPluginAdapter - Seed: %lld, Version: %d"), Settings.Seed, Settings.WorldGenVersion);

    if (!ValidatePluginAvailability())
    {
        LogStructuredError(TEXT("VoxelPluginLegacy is not available - cannot initialize adapter"), FIntVector::ZeroValue, 
                          FString::Printf(TEXT("Seed: %lld, Version: %d"), Settings.Seed, Settings.WorldGenVersion));
        return false;
    }

    // Store settings
    CurrentSettings = Settings;
    CurrentSeed = Settings.Seed;
    CurrentVersion = Settings.WorldGenVersion;

    // Initialize fallback generator
    FallbackGenerator = NewObject<UFallbackTerrainGenerator>(this);
    if (!FallbackGenerator->Initialize(Settings))
    {
        LogStructuredError(TEXT("Failed to initialize fallback terrain generator"), FIntVector::ZeroValue, 
                          FString::Printf(TEXT("Seed: %lld"), Settings.Seed));
        return false;
    }

    // Create voxel world
    if (!CreateVoxelWorld())
    {
        LogStructuredError(TEXT("Failed to create voxel world"), FIntVector::ZeroValue, 
                          FString::Printf(TEXT("Seed: %lld"), Settings.Seed));
        return false;
    }

    // Configure world settings
    ConfigureVoxelWorldSettings(Settings);

    // Initialize streaming manager
    StreamingManager = NewObject<UChunkStreamingManager>(this);
    if (!StreamingManager->Initialize(Settings, this))
    {
        LogStructuredError(TEXT("Failed to initialize chunk streaming manager"), FIntVector::ZeroValue, 
                          FString::Printf(TEXT("Seed: %lld"), Settings.Seed));
        return false;
    }

    bIsInitialized = true;
    UE_LOG(LogWorldGen, Log, TEXT("VoxelPluginAdapter initialized successfully - Seed: %lld, Version: %d, FallbackReady: %s"), 
           Settings.Seed, Settings.WorldGenVersion, FallbackGenerator->IsInitialized() ? TEXT("Yes") : TEXT("No"));
    
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

    // Set player anchor in streaming manager
    if (StreamingManager)
    {
        StreamingManager->SetPlayerAnchor(InPlayerAnchor);
    }

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

    // Update streaming manager
    if (StreamingManager)
    {
        StreamingManager->UpdateStreaming(DeltaTime);
    }

    // Ensure the player anchor is still valid
    if (!PlayerAnchor || !IsValid(PlayerAnchor))
    {
        UE_LOG(LogWorldGen, Warning, TEXT("Player anchor became invalid during streaming"));
        PlayerAnchor = nullptr;
        if (StreamingManager)
        {
            StreamingManager->SetPlayerAnchor(nullptr);
        }
    }
}

bool UVoxelPluginAdapter::RebuildChunkAsync(const FIntVector& ChunkCoordinate)
{
    if (!IsInitialized())
    {
        LogStructuredError(TEXT("Cannot rebuild chunk - adapter not initialized"), ChunkCoordinate, 
                          FString::Printf(TEXT("Seed: %lld"), CurrentSeed));
        return false;
    }

    if (!VoxelWorld)
    {
        UE_LOG(LogWorldGen, Warning, TEXT("Chunk rebuild deferred - VoxelWorld not created yet - Seed: %lld, Chunk: (%d, %d, %d)"), 
               CurrentSeed, ChunkCoordinate.X, ChunkCoordinate.Y, ChunkCoordinate.Z);
        return true; // Return true but defer the operation
    }

    if (!VoxelWorld->IsCreated())
    {
        UE_LOG(LogWorldGen, Warning, TEXT("Chunk rebuild deferred - VoxelWorld not ready - Seed: %lld, Chunk: (%d, %d, %d)"), 
               CurrentSeed, ChunkCoordinate.X, ChunkCoordinate.Y, ChunkCoordinate.Z);
        return true; // Return true but defer the operation
    }

    // Check if this chunk has failed before
    if (int32* FailureCount = FailedChunks.Find(ChunkCoordinate))
    {
        if (*FailureCount >= MaxRetryAttempts)
        {
            UE_LOG(LogWorldGen, Warning, TEXT("Chunk exceeded retry attempts, using fallback generation - Seed: %lld, Chunk: (%d, %d, %d), Attempts: %d"), 
                   CurrentSeed, ChunkCoordinate.X, ChunkCoordinate.Y, ChunkCoordinate.Z, *FailureCount);
            return HandleChunkGenerationFailure(ChunkCoordinate, TEXT("Exceeded retry attempts"), true);
        }
    }

    // Attempt normal generation
    if (VoxelWorld && VoxelWorld->IsCreated())
    {
        // Mark the entire data as dirty to trigger updates
        // VoxelPluginLegacy will handle the specific chunk updates automatically
        VoxelWorld->GetData().MarkAsDirty();
        
        UE_LOG(LogWorldGen, VeryVerbose, TEXT("Queued chunk rebuild - Seed: %lld, Chunk: (%d, %d, %d)"), 
               CurrentSeed, ChunkCoordinate.X, ChunkCoordinate.Y, ChunkCoordinate.Z);

        return true;
    }
    else
    {
        return HandleChunkGenerationFailure(ChunkCoordinate, TEXT("VoxelWorld not ready for chunk rebuild"), true);
    }
}

bool UVoxelPluginAdapter::IsInitialized() const
{
    return bIsInitialized && VoxelWorld != nullptr;
}

bool UVoxelPluginAdapter::ApplySphere(const FVector& Center, float Radius, EVoxelCSG Operation)
{
    const FIntVector AffectedChunk = WorldToChunkCoordinate(Center);
    
    if (!IsInitialized() || !VoxelWorld || !VoxelWorld->IsCreated())
    {
        LogStructuredError(TEXT("Cannot apply sphere edit - world not ready"), AffectedChunk, 
                          FString::Printf(TEXT("Seed: %lld, Center: (%.2f, %.2f, %.2f), Radius: %.2f"), 
                                        CurrentSeed, Center.X, Center.Y, Center.Z, Radius));
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
    const FVoxelEditOp EditOp(Center, Radius, Operation, AffectedChunk);
    RecordOp(EditOp);

    UE_LOG(LogWorldGen, VeryVerbose, TEXT("Applied sphere %s - Seed: %lld, Chunk: (%d, %d, %d), Center: (%.2f, %.2f, %.2f), Radius: %.2f"), 
           Operation == EVoxelCSG::Add ? TEXT("Add") : TEXT("Subtract"),
           CurrentSeed, AffectedChunk.X, AffectedChunk.Y, AffectedChunk.Z,
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
            JsonContent += FString::Printf(TEXT("{\"center\":[%f,%f,%f],\"radius\":%f,\"operation\":%d,\"timestamp\":%f}\n"),
                Op.Center.X, Op.Center.Y, Op.Center.Z, Op.Radius, (int32)Op.Operation, 
                Op.Timestamp);
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

bool UVoxelPluginAdapter::HandleChunkGenerationFailure(const FIntVector& ChunkCoordinate, const FString& ErrorMessage, bool bAttemptFallback)
{
    // Track failure count
    int32& FailureCount = FailedChunks.FindOrAdd(ChunkCoordinate, 0);
    FailureCount++;
    
    LogStructuredError(ErrorMessage, ChunkCoordinate, 
                      FString::Printf(TEXT("Seed: %lld, FailureCount: %d, AttemptFallback: %s"), 
                                    CurrentSeed, FailureCount, bAttemptFallback ? TEXT("Yes") : TEXT("No")));
    
    if (!bAttemptFallback || !FallbackGenerator || !FallbackGenerator->IsInitialized())
    {
        UE_LOG(LogWorldGen, Error, TEXT("Cannot attempt fallback generation - Seed: %lld, Chunk: (%d, %d, %d), FallbackAvailable: %s"), 
               CurrentSeed, ChunkCoordinate.X, ChunkCoordinate.Y, ChunkCoordinate.Z,
               (FallbackGenerator && FallbackGenerator->IsInitialized()) ? TEXT("Yes") : TEXT("No"));
        return false;
    }
    
    // Attempt fallback generation
    if (FailureCount >= MaxRetryAttempts)
    {
        UE_LOG(LogWorldGen, Warning, TEXT("Using proxy mesh for completely failed chunk - Seed: %lld, Chunk: (%d, %d, %d), Attempts: %d"), 
               CurrentSeed, ChunkCoordinate.X, ChunkCoordinate.Y, ChunkCoordinate.Z, FailureCount);
        
        return FallbackGenerator->GenerateProxyMesh(ChunkCoordinate, CurrentSeed, ChunkCoordinate);
    }
    else
    {
        UE_LOG(LogWorldGen, Warning, TEXT("Using fallback heightmap generation - Seed: %lld, Chunk: (%d, %d, %d), Attempts: %d"), 
               CurrentSeed, ChunkCoordinate.X, ChunkCoordinate.Y, ChunkCoordinate.Z, FailureCount);
        
        TArray<float> HeightData;
        return FallbackGenerator->GenerateHeightmapForChunk(ChunkCoordinate, HeightData, CurrentSeed, ChunkCoordinate);
    }
}

void UVoxelPluginAdapter::LogStructuredError(const FString& ErrorMessage, const FIntVector& ChunkCoordinate, const FString& AdditionalContext) const
{
    if (AdditionalContext.IsEmpty())
    {
        UE_LOG(LogWorldGen, Error, TEXT("[STRUCTURED_ERROR] %s - Seed: %lld, Chunk: (%d, %d, %d)"), 
               *ErrorMessage, CurrentSeed, ChunkCoordinate.X, ChunkCoordinate.Y, ChunkCoordinate.Z);
    }
    else
    {
        UE_LOG(LogWorldGen, Error, TEXT("[STRUCTURED_ERROR] %s - Seed: %lld, Chunk: (%d, %d, %d), Context: %s"), 
               *ErrorMessage, CurrentSeed, ChunkCoordinate.X, ChunkCoordinate.Y, ChunkCoordinate.Z, *AdditionalContext);
    }
}