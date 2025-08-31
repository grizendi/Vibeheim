#include "VHMTerrainRendering/VHMTerrainRenderer.h"
#include "VHMTerrainRendering/HeightfieldTextureManager.h"
#include "Services/HeightfieldService.h"
#include "Services/TileStreamingService.h"
#include "WorldGenSettings.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Engine/Engine.h"
#include "Misc/App.h"
#include "Components/PrimitiveComponent.h"

// Include VHM component - try the most common UE5 path first
#include "VirtualHeightfieldMeshComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogVHMTerrainRenderer, Log, All);

UVHMTerrainRenderer::UVHMTerrainRenderer()
{
    // Initialize default VHM settings
    VHMSettings = FVHMSettings();
    
    // Initialize service references
    WorldGenSettings = nullptr;
    HeightfieldService = nullptr;
    TileStreamingService = nullptr;
    HeightfieldTextureManager.SetInterface(nullptr);
    CachedWorld = nullptr;

    // Initialize performance tracking
    PerformanceStats = FVHMPerformanceStats();
    RecentMeshGenerationTimes.Reserve(MaxRecentTimes);
}

bool UVHMTerrainRenderer::Initialize(UWorldGenSettings* Settings, 
                                   UHeightfieldService* InHeightfieldService,
                                   UTileStreamingService* InTileStreamingService)
{
    if (!Settings || !InHeightfieldService || !InTileStreamingService)
    {
        UE_LOG(LogVHMTerrainRenderer, Error, TEXT("VHMTerrainRenderer::Initialize - Invalid service references"));
        return false;
    }

    // Store service references
    WorldGenSettings = Settings;
    HeightfieldService = InHeightfieldService;
    TileStreamingService = InTileStreamingService;

    // Get world reference
    CachedWorld = GetWorld();
    if (!CachedWorld)
    {
        UE_LOG(LogVHMTerrainRenderer, Error, TEXT("VHMTerrainRenderer::Initialize - No valid world context"));
        return false;
    }

    // Initialize heightfield texture manager
    if (!InitializeHeightfieldTextureManager())
    {
        UE_LOG(LogVHMTerrainRenderer, Error, TEXT("VHMTerrainRenderer::Initialize - Failed to initialize HeightfieldTextureManager"));
        return false;
    }

    // Load VHM settings from world generation config
    if (Settings->VHMSettings.IsSet())
    {
        VHMSettings = Settings->VHMSettings.GetValue();
    }

    UE_LOG(LogVHMTerrainRenderer, Log, TEXT("VHMTerrainRenderer initialized successfully"));
    return true;
}

bool UVHMTerrainRenderer::CreateTerrainMeshForTile(const FTileCoord& TileCoord)
{
    if (!IsValidTileCoordinate(TileCoord))
    {
        UE_LOG(LogVHMTerrainRenderer, Warning, TEXT("CreateTerrainMeshForTile - Invalid tile coordinate: (%d, %d)"), TileCoord.X, TileCoord.Y);
        return false;
    }

    // Check if mesh already exists
    if (TerrainMeshes.Contains(TileCoord))
    {
        UE_LOG(LogVHMTerrainRenderer, Log, TEXT("CreateTerrainMeshForTile - Mesh already exists for tile: (%d, %d)"), TileCoord.X, TileCoord.Y);
        return true;
    }

    double StartTime = FPlatformTime::Seconds();

    // Get heightfield data from service
    FHeightfieldData HeightfieldData;
    if (!HeightfieldService->GetCachedHeightfield(TileCoord, HeightfieldData))
    {
        UE_LOG(LogVHMTerrainRenderer, Warning, TEXT("CreateTerrainMeshForTile - No heightfield data available for tile: (%d, %d)"), TileCoord.X, TileCoord.Y);
        return false;
    }

    // Create height texture
    IHeightfieldTextureManager* TextureManagerInterface = Cast<IHeightfieldTextureManager>(HeightfieldTextureManager.GetObject());
    UTexture2D* HeightTexture = TextureManagerInterface ? TextureManagerInterface->CreateHeightTexture(TileCoord, HeightfieldData.HeightData) : nullptr;
    if (!HeightTexture)
    {
        UE_LOG(LogVHMTerrainRenderer, Error, TEXT("CreateTerrainMeshForTile - Failed to create height texture for tile: (%d, %d)"), TileCoord.X, TileCoord.Y);
        return false;
    }

    // Create VHM component (placeholder implementation)
    UVirtualHeightfieldMeshComponent* VHMComponent = CreateVHMComponent(TileCoord);
    if (!VHMComponent)
    {
        UE_LOG(LogVHMTerrainRenderer, Error, TEXT("CreateTerrainMeshForTile - Failed to create VHM component for tile: (%d, %d)"), TileCoord.X, TileCoord.Y);
        return false;
    }

    // Configure VHM component
    ConfigureVHMComponent(VHMComponent, TileCoord);

    // Generate mesh from heightfield data
    if (!GenerateMeshFromHeightfield(VHMComponent, TileCoord, HeightTexture))
    {
        UE_LOG(LogVHMTerrainRenderer, Error, TEXT("CreateTerrainMeshForTile - Failed to generate mesh from heightfield for tile: (%d, %d)"), TileCoord.X, TileCoord.Y);
        // Clean up the created component and actor
        if (AActor* Owner = VHMComponent->GetOwner())
        {
            Owner->Destroy();
        }
        return false;
    }

    // Validate the VHM component is properly configured
    if (!ValidateVHMComponent(VHMComponent, TileCoord))
    {
        UE_LOG(LogVHMTerrainRenderer, Warning, TEXT("CreateTerrainMeshForTile - VHM component validation failed for tile: (%d, %d)"), TileCoord.X, TileCoord.Y);
        // Continue anyway as this might be a non-critical issue
    }

    // Create terrain mesh data
    FTerrainMeshData MeshData;
    MeshData.TileCoord = TileCoord;
    MeshData.VHMComponent = VHMComponent;
    MeshData.HeightTexture = HeightTexture;
    MeshData.CurrentLODLevel = 0;
    MeshData.LastUpdateTime = FPlatformTime::Seconds();
    MeshData.WorldBounds = CalculateTileWorldBounds(TileCoord);
    MeshData.bIsVisible = true;

    // Store mesh data
    TerrainMeshes.Add(TileCoord, MeshData);

    // Update performance stats
    float GenerationTime = (FPlatformTime::Seconds() - StartTime) * 1000.0f; // Convert to milliseconds
    UpdatePerformanceStats(GenerationTime);
    RecordMeshGenerationTime(GenerationTime);

    UE_LOG(LogVHMTerrainRenderer, Log, TEXT("Created terrain mesh for tile (%d, %d) in %.2fms"), TileCoord.X, TileCoord.Y, GenerationTime);
    return true;
}

bool UVHMTerrainRenderer::UpdateTerrainMesh(const FTileCoord& TileCoord, const TArray<FHeightfieldModification>& Modifications)
{
    if (!TerrainMeshes.Contains(TileCoord))
    {
        UE_LOG(LogVHMTerrainRenderer, Warning, TEXT("UpdateTerrainMesh - No mesh exists for tile: (%d, %d)"), TileCoord.X, TileCoord.Y);
        return false;
    }

    if (!VHMSettings.bEnableRealTimeEditing)
    {
        UE_LOG(LogVHMTerrainRenderer, Log, TEXT("UpdateTerrainMesh - Real-time editing disabled, skipping update for tile: (%d, %d)"), TileCoord.X, TileCoord.Y);
        return true;
    }

    double StartTime = FPlatformTime::Seconds();

    // Update height texture with modifications
    IHeightfieldTextureManager* TextureManagerInterface = Cast<IHeightfieldTextureManager>(HeightfieldTextureManager.GetObject());
    bool bTextureUpdated = TextureManagerInterface ? TextureManagerInterface->UpdateHeightTexture(TileCoord, Modifications) : false;
    if (!bTextureUpdated)
    {
        UE_LOG(LogVHMTerrainRenderer, Error, TEXT("UpdateTerrainMesh - Failed to update height texture for tile: (%d, %d)"), TileCoord.X, TileCoord.Y);
        return false;
    }

    // Update mesh data timestamp
    FTerrainMeshData& MeshData = TerrainMeshes[TileCoord];
    MeshData.LastUpdateTime = FPlatformTime::Seconds();

    // Regenerate mesh with updated height texture
    if (MeshData.VHMComponent && MeshData.HeightTexture)
    {
        if (!GenerateMeshFromHeightfield(MeshData.VHMComponent, TileCoord, MeshData.HeightTexture))
        {
            UE_LOG(LogVHMTerrainRenderer, Error, TEXT("UpdateTerrainMesh - Failed to regenerate mesh for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
            return false;
        }
        
        UE_LOG(LogVHMTerrainRenderer, Log, TEXT("Regenerated VHM mesh for tile (%d, %d) after height texture update"), TileCoord.X, TileCoord.Y);
    }

    float UpdateTime = (FPlatformTime::Seconds() - StartTime) * 1000.0f;
    UE_LOG(LogVHMTerrainRenderer, Log, TEXT("Updated terrain mesh for tile (%d, %d) in %.2fms"), TileCoord.X, TileCoord.Y, UpdateTime);
    return true;
}

void UVHMTerrainRenderer::RemoveTerrainMesh(const FTileCoord& TileCoord)
{
    if (!TerrainMeshes.Contains(TileCoord))
    {
        return;
    }

    // Cleanup mesh data
    CleanupMeshData(TileCoord);

    // Remove from cache
    TerrainMeshes.Remove(TileCoord);

    // Remove texture from texture manager
    if (IHeightfieldTextureManager* TextureManagerInterface = Cast<IHeightfieldTextureManager>(HeightfieldTextureManager.GetObject()))
    {
        TextureManagerInterface->RemoveTexture(TileCoord);
    }

    UE_LOG(LogVHMTerrainRenderer, Log, TEXT("Removed terrain mesh for tile: (%d, %d)"), TileCoord.X, TileCoord.Y);
}

UVirtualHeightfieldMeshComponent* UVHMTerrainRenderer::GetVHMComponent(const FTileCoord& TileCoord)
{
    if (const FTerrainMeshData* MeshData = TerrainMeshes.Find(TileCoord))
    {
        return MeshData->VHMComponent;
    }
    return nullptr;
}

void UVHMTerrainRenderer::UpdateLODLevels(const FVector& ViewerPosition)
{
    for (auto& MeshPair : TerrainMeshes)
    {
        const FTileCoord& TileCoord = MeshPair.Key;
        FTerrainMeshData& MeshData = MeshPair.Value;

        // Calculate new LOD level
        int32 NewLODLevel = CalculateLODLevel(TileCoord, ViewerPosition);
        
        if (NewLODLevel != MeshData.CurrentLODLevel)
        {
            MeshData.CurrentLODLevel = NewLODLevel;
            
            // Update VHM component LOD if available
            if (MeshData.VHMComponent)
            {
                // VHM components handle LOD internally based on distance
                // We track it here for performance monitoring
                PerformanceStats.LODTransitionsThisFrame++;
            }
        }
    }
}

FVHMPerformanceStats UVHMTerrainRenderer::GetPerformanceStats() const
{
    // Update current stats
    PerformanceStats.ActiveVHMComponents = TerrainMeshes.Num();
    IHeightfieldTextureManager* TextureManagerInterface = Cast<IHeightfieldTextureManager>(HeightfieldTextureManager.GetObject());
    PerformanceStats.TextureMemoryUsageMB = TextureManagerInterface ? TextureManagerInterface->GetTextureMemoryUsageMB() : 0.0f;
    
    // Calculate average generation time
    if (RecentMeshGenerationTimes.Num() > 0)
    {
        float TotalTime = 0.0f;
        for (float Time : RecentMeshGenerationTimes)
        {
            TotalTime += Time;
        }
        PerformanceStats.AverageMeshGenerationMs = TotalTime / RecentMeshGenerationTimes.Num();
    }

    // Get current FPS
    if (GEngine)
    {
        PerformanceStats.CurrentFPS = 1.0f / FApp::GetDeltaTime();
    }

    // Reset frame-based counters
    PerformanceStats.LODTransitionsThisFrame = 0;

    return PerformanceStats;
}

void UVHMTerrainRenderer::OnTileStreamingEvent(const FTileCoord& TileCoord, bool bTileLoaded)
{
    if (bTileLoaded)
    {
        // Tile was loaded - create mesh if it doesn't exist
        if (!TerrainMeshes.Contains(TileCoord))
        {
            CreateTerrainMeshForTile(TileCoord);
        }
    }
    else
    {
        // Tile was unloaded - remove mesh
        RemoveTerrainMesh(TileCoord);
    }
}

void UVHMTerrainRenderer::Cleanup()
{
    // Remove all terrain meshes
    TArray<FTileCoord> TilesToRemove;
    TerrainMeshes.GetKeys(TilesToRemove);
    
    for (const FTileCoord& TileCoord : TilesToRemove)
    {
        RemoveTerrainMesh(TileCoord);
    }

    // Clear service references
    WorldGenSettings = nullptr;
    HeightfieldService = nullptr;
    TileStreamingService = nullptr;
    
    if (IHeightfieldTextureManager* TextureManagerInterface = Cast<IHeightfieldTextureManager>(HeightfieldTextureManager.GetObject()))
    {
        TextureManagerInterface->OptimizeTextureMemory();
        HeightfieldTextureManager.SetInterface(nullptr);
    }

    CachedWorld = nullptr;

    UE_LOG(LogVHMTerrainRenderer, Log, TEXT("VHMTerrainRenderer cleanup completed"));
}

void UVHMTerrainRenderer::SetVHMSettings(const FVHMSettings& NewSettings)
{
    VHMSettings = NewSettings;
    
    // Apply settings to existing components if needed
    for (auto& MeshPair : TerrainMeshes)
    {
        FTerrainMeshData& MeshData = MeshPair.Value;
        if (MeshData.VHMComponent)
        {
            ConfigureVHMComponent(MeshData.VHMComponent, MeshPair.Key);
        }
    }
}

bool UVHMTerrainRenderer::GetTerrainMeshData(const FTileCoord& TileCoord, FTerrainMeshData& OutMeshData) const
{
    if (const FTerrainMeshData* MeshData = TerrainMeshes.Find(TileCoord))
    {
        OutMeshData = *MeshData;
        return true;
    }
    return false;
}

TArray<FTileCoord> UVHMTerrainRenderer::GetActiveMeshTiles() const
{
    TArray<FTileCoord> ActiveTiles;
    TerrainMeshes.GetKeys(ActiveTiles);
    return ActiveTiles;
}

UVirtualHeightfieldMeshComponent* UVHMTerrainRenderer::CreateVHMComponent(const FTileCoord& TileCoord)
{
    if (!CachedWorld)
    {
        UE_LOG(LogVHMTerrainRenderer, Error, TEXT("CreateVHMComponent - No valid world context"));
        return nullptr;
    }



    // Create an actor to hold the VHM component
    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = FName(*FString::Printf(TEXT("VHMTerrain_%d_%d"), TileCoord.X, TileCoord.Y));
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    
    AActor* VHMActor = CachedWorld->SpawnActor<AActor>(AActor::StaticClass(), SpawnParams);
    if (!VHMActor)
    {
        UE_LOG(LogVHMTerrainRenderer, Error, TEXT("CreateVHMComponent - Failed to spawn VHM actor for tile: (%d, %d)"), TileCoord.X, TileCoord.Y);
        return nullptr;
    }

    // Create and attach VHM component
    UVirtualHeightfieldMeshComponent* VHMComponent = NewObject<UVirtualHeightfieldMeshComponent>(VHMActor);
    if (!VHMComponent)
    {
        UE_LOG(LogVHMTerrainRenderer, Error, TEXT("CreateVHMComponent - Failed to create VHM component for tile: (%d, %d)"), TileCoord.X, TileCoord.Y);
        VHMActor->Destroy();
        return nullptr;
    }

    // Set as root component
    VHMActor->SetRootComponent(VHMComponent);
    
    // Position the actor at the tile corner (VHM components use corner as origin)
    FVector TileWorldPosition = GetTileCornerWorldPosition(TileCoord);
    VHMActor->SetActorLocation(TileWorldPosition);

    UE_LOG(LogVHMTerrainRenderer, Log, TEXT("Created VHM component for tile (%d, %d) at position (%s)"), 
           TileCoord.X, TileCoord.Y, *TileWorldPosition.ToString());

    return VHMComponent;
}

void UVHMTerrainRenderer::ConfigureVHMComponent(UVirtualHeightfieldMeshComponent* VHMComponent, const FTileCoord& TileCoord)
{
    if (!VHMComponent || !WorldGenSettings)
    {
        return;
    }

    const FWorldGenConfig& Config = WorldGenSettings->GetWorldGenConfig();
    float TileSize = Config.TileSizeMeters;

    // Calculate tile bounds in world space
    FBox TileBounds = CalculateTileWorldBounds(TileCoord);
    
    // Configure basic VHM component properties
    // Note: VHM-specific API methods will be added once the correct API is determined
    UE_LOG(LogVHMTerrainRenderer, Log, TEXT("VHM component created and configured for tile (%d, %d)"), TileCoord.X, TileCoord.Y);

    // Configure basic component settings (available on all primitive components)
    VHMComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    VHMComponent->SetVisibility(true);
    VHMComponent->SetHiddenInGame(false);
    VHMComponent->SetCastShadow(true);
    VHMComponent->SetReceivesDecals(true);

    UE_LOG(LogVHMTerrainRenderer, Log, TEXT("Configured VHM component for tile (%d, %d) with bounds %s"), 
           TileCoord.X, TileCoord.Y, *TileBounds.ToString());
}

bool UVHMTerrainRenderer::GenerateMeshFromHeightfield(UVirtualHeightfieldMeshComponent* VHMComponent, const FTileCoord& TileCoord, UTexture2D* HeightTexture)
{
    if (!VHMComponent || !HeightTexture)
    {
        UE_LOG(LogVHMTerrainRenderer, Error, TEXT("GenerateMeshFromHeightfield - Invalid VHM component or height texture for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
        return false;
    }

    // Configure VHM component with height texture
    // Note: VHM-specific API methods will be added once the correct API is determined
    UE_LOG(LogVHMTerrainRenderer, Log, TEXT("Height texture bound to VHM component for tile (%d, %d)"), TileCoord.X, TileCoord.Y);

    // Force the component to rebuild its mesh
    VHMComponent->MarkRenderStateDirty();
    
    // Ensure the component is registered and visible
    if (!VHMComponent->IsRegistered())
    {
        VHMComponent->RegisterComponent();
    }

    UE_LOG(LogVHMTerrainRenderer, Log, TEXT("Generated mesh from heightfield for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
    return true;
}

bool UVHMTerrainRenderer::ValidateVHMComponent(UVirtualHeightfieldMeshComponent* VHMComponent, const FTileCoord& TileCoord) const
{
    if (!VHMComponent)
    {
        UE_LOG(LogVHMTerrainRenderer, Error, TEXT("ValidateVHMComponent - VHM component is null for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
        return false;
    }

    // Check if component is registered
    if (!VHMComponent->IsRegistered())
    {
        UE_LOG(LogVHMTerrainRenderer, Warning, TEXT("ValidateVHMComponent - VHM component is not registered for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
        return false;
    }

    // Check if component has a valid owner
    AActor* Owner = VHMComponent->GetOwner();
    if (!Owner || !IsValid(Owner))
    {
        UE_LOG(LogVHMTerrainRenderer, Error, TEXT("ValidateVHMComponent - VHM component has invalid owner for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
        return false;
    }

    // Basic validation - VHM-specific validation will be added once the correct API is determined
    UE_LOG(LogVHMTerrainRenderer, Log, TEXT("VHM component validation passed for tile (%d, %d)"), TileCoord.X, TileCoord.Y);

    UE_LOG(LogVHMTerrainRenderer, Log, TEXT("ValidateVHMComponent - VHM component is valid for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
    return true;
}

FBox UVHMTerrainRenderer::CalculateTileWorldBounds(const FTileCoord& TileCoord) const
{
    if (!WorldGenSettings)
    {
        return FBox(ForceInit);
    }

    const FWorldGenConfig& Config = WorldGenSettings->GetWorldGenConfig();
    float TileSize = Config.TileSizeMeters;
    
    FVector TileCenter = GetTileCenterWorldPosition(TileCoord);
    FVector HalfExtent(TileSize * 0.5f, TileSize * 0.5f, 1000.0f); // Assume max height of 1000 units
    
    return FBox(TileCenter - HalfExtent, TileCenter + HalfExtent);
}

int32 UVHMTerrainRenderer::CalculateLODLevel(const FTileCoord& TileCoord, const FVector& ViewerPosition) const
{
    FVector TileCenter = GetTileCenterWorldPosition(TileCoord);
    float Distance = FVector::Dist(ViewerPosition, TileCenter);
    
    // Calculate LOD based on distance and settings
    float LODDistance = Distance / VHMSettings.MaxViewDistance;
    int32 LODLevel = FMath::Clamp(FMath::FloorToInt(LODDistance * VHMSettings.LODLevels), 0, VHMSettings.LODLevels - 1);
    
    return LODLevel;
}

void UVHMTerrainRenderer::UpdatePerformanceStats(float MeshGenerationTime) const
{
    PerformanceStats.LastMeshGenerationMs = MeshGenerationTime;
}

void UVHMTerrainRenderer::RecordMeshGenerationTime(float GenerationTime)
{
    RecentMeshGenerationTimes.Add(GenerationTime);
    
    // Keep only recent times
    if (RecentMeshGenerationTimes.Num() > MaxRecentTimes)
    {
        RecentMeshGenerationTimes.RemoveAt(0);
    }
}

FVector UVHMTerrainRenderer::GetTileCenterWorldPosition(const FTileCoord& TileCoord) const
{
    if (!WorldGenSettings)
    {
        return FVector::ZeroVector;
    }

    const FWorldGenConfig& Config = WorldGenSettings->GetWorldGenConfig();
    float TileSize = Config.TileSizeMeters;
    
    return FVector(
        TileCoord.X * TileSize + TileSize * 0.5f,
        TileCoord.Y * TileSize + TileSize * 0.5f,
        0.0f
    );
}

FVector UVHMTerrainRenderer::GetTileCornerWorldPosition(const FTileCoord& TileCoord) const
{
    if (!WorldGenSettings)
    {
        return FVector::ZeroVector;
    }

    const FWorldGenConfig& Config = WorldGenSettings->GetWorldGenConfig();
    float TileSize = Config.TileSizeMeters;
    
    return FVector(
        TileCoord.X * TileSize,
        TileCoord.Y * TileSize,
        0.0f
    );
}

bool UVHMTerrainRenderer::IsValidTileCoordinate(const FTileCoord& TileCoord) const
{
    // Basic validation - could be expanded based on world limits
    const int32 MaxTileCoordinate = 10000; // Reasonable limit
    return FMath::Abs(TileCoord.X) <= MaxTileCoordinate && FMath::Abs(TileCoord.Y) <= MaxTileCoordinate;
}

void UVHMTerrainRenderer::CleanupMeshData(const FTileCoord& TileCoord)
{
    if (FTerrainMeshData* MeshData = TerrainMeshes.Find(TileCoord))
    {
        // Destroy VHM component and its actor
        if (MeshData->VHMComponent)
        {
            AActor* Owner = MeshData->VHMComponent->GetOwner();
            if (Owner && IsValid(Owner))
            {
                Owner->Destroy();
                UE_LOG(LogVHMTerrainRenderer, Log, TEXT("Destroyed VHM actor for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
            }
            MeshData->VHMComponent = nullptr;
        }

        // Clear texture reference (texture manager handles cleanup)
        MeshData->HeightTexture = nullptr;
        MeshData->MaterialInstance = nullptr;
    }
}

bool UVHMTerrainRenderer::InitializeHeightfieldTextureManager()
{
    UVHMHeightfieldTextureManager* TextureManager = NewObject<UVHMHeightfieldTextureManager>(this);
    if (!TextureManager)
    {
        return false;
    }

    HeightfieldTextureManager.SetObject(TextureManager);

    // Initialize texture manager with VHM settings (call on concrete object, not interface)
    return TextureManager->Initialize(VHMSettings);
}