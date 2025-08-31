#include "VHMTerrainRendering/VHMTerrainRenderer.h"
#include "VHMTerrainRendering/HeightfieldTextureManager.h"
#include "Services/HeightfieldService.h"
#include "Services/TileStreamingService.h"
#include "WorldGenSettings.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Engine/Engine.h"
#include "Misc/App.h"

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

    // TODO: Bind height texture to VHM component when VHM API is available
    // VHMComponent->SetHeightTexture(HeightTexture);

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

    // TODO: Force VHM component to refresh when VHM API is available
    if (MeshData.VHMComponent)
    {
        // MeshData.VHMComponent->MarkRenderStateDirty();
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

    // TODO: Implement VHM component creation when VirtualHeightfieldMeshComponent is available
    // For now, return a placeholder to allow compilation
    UE_LOG(LogVHMTerrainRenderer, Warning, TEXT("CreateVHMComponent - VHM component creation not yet implemented for tile: (%d, %d)"), TileCoord.X, TileCoord.Y);
    
    // Create a placeholder component for now - this will be replaced with actual VHM when available
    // Return nullptr to indicate VHM is not yet implemented
    return nullptr;
}

void UVHMTerrainRenderer::ConfigureVHMComponent(UVirtualHeightfieldMeshComponent* VHMComponent, const FTileCoord& TileCoord)
{
    if (!VHMComponent)
    {
        return;
    }

    // TODO: Set component transform to tile position when VHM API is available
    // FVector TileWorldPosition = GetTileCenterWorldPosition(TileCoord);
    // VHMComponent->SetWorldLocation(TileWorldPosition);

    // Configure VHM properties based on settings
    // Note: Actual VHM component configuration depends on UE5.6 VHM API
    // This is a simplified version - real implementation would set:
    // - Heightfield texture
    // - Material
    // - LOD settings
    // - Bounds
    // - Resolution
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
        // TODO: Destroy VHM component and its actor when VHM API is available
        if (MeshData->VHMComponent)
        {
            // AActor* Owner = MeshData->VHMComponent->GetOwner();
            // if (Owner)
            // {
            //     Owner->Destroy();
            // }
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