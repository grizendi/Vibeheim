#include "VHMTerrainRendering/VHMTerrainRenderer.h"
#include "VHMTerrainRendering/HeightfieldTextureManager.h"
#include "VHMTerrainRendering/TerrainMaterialSystem.h"
#include "VHMTerrainRendering/TerrainLODManager.h"
#include "VHMTerrainRendering/TileBoundaryManager.h"
#include "Services/HeightfieldService.h"
#include "Services/TileStreamingService.h"
#include "Services/BiomeService.h"
#include "WorldGenSettings.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Engine/Engine.h"
#include "Misc/App.h"
#include "Components/PrimitiveComponent.h"

// Include VHM component - try the most common UE5 path first
#include "VirtualHeightfieldMeshComponent.h"
// Fallback procedural mesh for visualization when VHM is not fully wired
#include "ProceduralMeshComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogVHMTerrainRenderer, Log, All);

UVHMTerrainRenderer::UVHMTerrainRenderer()
{
    // Initialize default VHM settings
    VHMSettings = FVHMSettings();
    
    // Initialize service references
    WorldGenSettings = nullptr;
    HeightfieldService = nullptr;
    TileStreamingService = nullptr;
    BiomeService = nullptr;
    HeightfieldTextureManager.SetInterface(nullptr);
    TerrainMaterialSystem.SetInterface(nullptr);
    TerrainLODManager.SetInterface(nullptr);
    TileBoundaryManager.SetInterface(nullptr);
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

    // Initialize terrain LOD manager
    if (!InitializeTerrainLODManager())
    {
        UE_LOG(LogVHMTerrainRenderer, Error, TEXT("VHMTerrainRenderer::Initialize - Failed to initialize TerrainLODManager"));
        return false;
    }

    // Initialize tile boundary manager
    if (!InitializeTileBoundaryManager())
    {
        UE_LOG(LogVHMTerrainRenderer, Error, TEXT("VHMTerrainRenderer::Initialize - Failed to initialize TileBoundaryManager"));
        return false;
    }

    UE_LOG(LogVHMTerrainRenderer, Log, TEXT("VHMTerrainRenderer initialized successfully"));
    return true;
}

bool UVHMTerrainRenderer::InitializeWithBiomeService(UWorldGenSettings* Settings, 
                                                   UHeightfieldService* InHeightfieldService,
                                                   UTileStreamingService* InTileStreamingService,
                                                   UBiomeService* InBiomeService)
{
    // First initialize the base system
    if (!Initialize(Settings, InHeightfieldService, InTileStreamingService))
    {
        return false;
    }

    if (!InBiomeService)
    {
        UE_LOG(LogVHMTerrainRenderer, Error, TEXT("InitializeWithBiomeService - BiomeService is null"));
        return false;
    }

    // Store biome service reference
    BiomeService = InBiomeService;

    // Initialize terrain material system
    if (!InitializeTerrainMaterialSystem())
    {
        UE_LOG(LogVHMTerrainRenderer, Error, TEXT("InitializeWithBiomeService - Failed to initialize TerrainMaterialSystem"));
        return false;
    }

    UE_LOG(LogVHMTerrainRenderer, Log, TEXT("VHMTerrainRenderer initialized with BiomeService successfully"));
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
        UE_LOG(LogVHMTerrainRenderer, Warning, TEXT("CreateTerrainMeshForTile - No heightfield data available for tile (%d, %d), using fallback"), TileCoord.X, TileCoord.Y);
        return CreateFlatMeadowFallback(TileCoord);
    }

    // Apply boundary stitching to height data for seamless transitions
    TArray<float> ProcessedHeightData = HeightfieldData.HeightData;
    if (VHMSettings.bEnableBoundaryStitching && TileBoundaryManager.GetInterface())
    {
        // Extract boundary data for this tile
        ITileBoundaryManager* BoundaryManagerInterface = Cast<ITileBoundaryManager>(TileBoundaryManager.GetObject());
        if (BoundaryManagerInterface)
        {
            FTileBoundaryData BoundaryData;
            if (BoundaryManagerInterface->ExtractTileBoundaryData(TileCoord, HeightfieldData.HeightData, VHMSettings.HeightTextureResolution, BoundaryData))
            {
                UE_LOG(LogVHMTerrainRenderer, Verbose, TEXT("Extracted boundary data for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
            }

            // Apply boundary stitching with adjacent tiles
            ApplyBoundaryStitching(TileCoord, ProcessedHeightData);
        }
    }

    // Create height texture with processed (potentially stitched) height data
    IHeightfieldTextureManager* TextureManagerInterface = Cast<IHeightfieldTextureManager>(HeightfieldTextureManager.GetObject());
    UTexture2D* HeightTexture = TextureManagerInterface ? TextureManagerInterface->CreateHeightTexture(TileCoord, ProcessedHeightData) : nullptr;
    if (!HeightTexture)
    {
        UE_LOG(LogVHMTerrainRenderer, Error, TEXT("VHM001: Failed to create height texture for tile (%d, %d) - using flat meadow fallback"), TileCoord.X, TileCoord.Y);
        return CreateFlatMeadowFallback(TileCoord);
    }

    // Create VHM component (placeholder implementation)
    UVirtualHeightfieldMeshComponent* VHMComponent = CreateVHMComponent(TileCoord);
    if (!VHMComponent)
    {
        UE_LOG(LogVHMTerrainRenderer, Error, TEXT("VHM001: Failed to create VHM component for tile (%d, %d) - using flat meadow fallback"), TileCoord.X, TileCoord.Y);
        return CreateFlatMeadowFallback(TileCoord);
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

    // Create material for the tile if material system is available
    UMaterialInstanceDynamic* TileMaterial = nullptr;
    if (BiomeService && TerrainMaterialSystem.GetInterface())
    {
        // Get biome data for this tile
        EBiomeType TileBiome = BiomeService->DetermineTileBiome(TileCoord, HeightfieldData.HeightData);
        FBiomeDefinition BiomeDefinition;
        if (BiomeService->GetBiomeDefinition(TileBiome, BiomeDefinition))
        {
            // Create material for this tile
            TileMaterial = TerrainMaterialSystem.GetInterface()->CreateTileMaterial(TileCoord, BiomeDefinition);
            if (TileMaterial)
            {
                // Apply material to VHM component using public base class method
                // Use explicit base class call to avoid protected SetMaterial() in UE5.6
                VHMComponent->UPrimitiveComponent::SetMaterial(0, TileMaterial);
                
                // Verify material was set successfully using base class method
                if (VHMComponent->UPrimitiveComponent::GetMaterial(0) == TileMaterial)
                {
                    UE_LOG(LogVHMTerrainRenderer, Verbose, TEXT("Applied material for biome %s to tile (%d, %d)"), 
                           *BiomeDefinition.BiomeName, TileCoord.X, TileCoord.Y);
                }
                else
                {
                    UE_LOG(LogVHMTerrainRenderer, Warning, TEXT("Failed to apply material for biome %s to tile (%d, %d) - material verification failed"), 
                           *BiomeDefinition.BiomeName, TileCoord.X, TileCoord.Y);
                }

                // Also apply to any procedural mesh fallback under the same actor
                if (AActor* Owner = VHMComponent->GetOwner())
                {
                    // Use templated GetComponents which exists across UE versions
                    TArray<UProceduralMeshComponent*> ProcMeshes;
                    Owner->GetComponents<UProceduralMeshComponent>(ProcMeshes);
                    for (UProceduralMeshComponent* PMC : ProcMeshes)
                    {
                        if (PMC)
                        {
                            PMC->SetMaterial(0, TileMaterial);
                        }
                    }
                }
            }
        }
    }

    // Create terrain mesh data
    FTerrainMeshData MeshData;
    MeshData.TileCoord = TileCoord;
    MeshData.VHMComponent = VHMComponent;
    MeshData.HeightTexture = HeightTexture;
    MeshData.MaterialInstance = TileMaterial;
    MeshData.CurrentLODLevel = 0;
    MeshData.LastUpdateTime = FPlatformTime::Seconds();
    MeshData.WorldBounds = CalculateTileWorldBounds(TileCoord);
    MeshData.bIsVisible = true;

    // Store mesh data
    TerrainMeshes.Add(TileCoord, MeshData);

    // Register tile with LOD manager
    if (ITerrainLODManager* LODManagerInterface = Cast<ITerrainLODManager>(TerrainLODManager.GetObject()))
    {
        if (UVHMTerrainLODManager* ConcreteLODManager = Cast<UVHMTerrainLODManager>(TerrainLODManager.GetObject()))
        {
            ConcreteLODManager->RegisterTile(TileCoord);
        }
    }

    // Update performance stats
    float GenerationTime = (FPlatformTime::Seconds() - StartTime) * 1000.0f; // Convert to milliseconds
    UpdatePerformanceStats(GenerationTime);
    RecordMeshGenerationTime(GenerationTime);
    LastMeshGenTimePerTile.Add(TileCoord, GenerationTime);

    // Update boundary stitching stats if boundary stitching was applied
    if (VHMSettings.bEnableBoundaryStitching && TileBoundaryManager.GetInterface())
    {
        PerformanceStats.BoundaryStitchingOperations++;
    }

    UE_LOG(LogVHMTerrainRenderer, Log, TEXT("Created terrain mesh for tile (%d, %d) in %.2fms"), TileCoord.X, TileCoord.Y, GenerationTime);
    return true;
}

bool UVHMTerrainRenderer::UpdateTerrainMesh(const FTileCoord& TileCoord, const TArray<FHeightfieldModification>& Modifications)
{
    if (!TerrainMeshes.Contains(TileCoord))
    {
        // Create on demand for edited tiles to ensure visual + collision update within target
        if (!CreateTerrainMeshForTile(TileCoord))
        {
            UE_LOG(LogVHMTerrainRenderer, Warning, TEXT("UpdateTerrainMesh - No mesh exists and creation failed for tile: (%d, %d)"), TileCoord.X, TileCoord.Y);
            return false;
        }
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

    // Update material parameters if material system is available
    if (BiomeService && TerrainMaterialSystem.GetInterface())
    {
        // Get updated heightfield data
        FHeightfieldData UpdatedHeightfieldData;
        if (HeightfieldService->GetCachedHeightfield(TileCoord, UpdatedHeightfieldData))
        {
            // Determine biome for updated terrain
            EBiomeType TileBiome = BiomeService->DetermineTileBiome(TileCoord, UpdatedHeightfieldData.HeightData);
            FBiomeDefinition BiomeDefinition;
            if (BiomeService->GetBiomeDefinition(TileBiome, BiomeDefinition))
            {
                // Update material parameters
                TerrainMaterialSystem.GetInterface()->UpdateMaterialParameters(TileCoord, BiomeDefinition);
                UE_LOG(LogVHMTerrainRenderer, Verbose, TEXT("Updated material parameters for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
            }
        }
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
    LastMeshGenTimePerTile.Remove(TileCoord);

    // Unregister tile from LOD manager
    if (UVHMTerrainLODManager* ConcreteLODManager = Cast<UVHMTerrainLODManager>(TerrainLODManager.GetObject()))
    {
        ConcreteLODManager->UnregisterTile(TileCoord);
    }

    // Remove texture from texture manager
    if (IHeightfieldTextureManager* TextureManagerInterface = Cast<IHeightfieldTextureManager>(HeightfieldTextureManager.GetObject()))
    {
        TextureManagerInterface->RemoveTexture(TileCoord);
    }

    // Remove material from material system
    if (TerrainMaterialSystem.GetInterface())
    {
        TerrainMaterialSystem.GetInterface()->RemoveTileMaterial(TileCoord);
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
    // Use TerrainLODManager for comprehensive LOD management
    UpdateFrameBasedLOD(ViewerPosition);
    
    // Handle visibility culling for mesh creation/destruction
    HandleVisibilityCulling(ViewerPosition);
    
    // Apply performance-based adaptive quality adjustment
    ApplyAdaptiveQualityAdjustment();
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

    // Get LOD transitions from LOD manager
    if (ITerrainLODManager* LODManagerInterface = Cast<ITerrainLODManager>(TerrainLODManager.GetObject()))
    {
        PerformanceStats.LODTransitionsThisFrame = LODManagerInterface->GetLODTransitionsThisFrame();
    }

    return PerformanceStats;
}

void UVHMTerrainRenderer::OnTileStreamingEvent(const FTileCoord& TileCoord, bool bTileLoaded)
{
    if (bTileLoaded)
    {
        // Tile was loaded - check if it should be visible before creating mesh
        if (!TerrainMeshes.Contains(TileCoord))
        {
            // Use LOD manager to determine if tile should be created based on visibility
            bool bShouldCreateMesh = true;
            if (ITerrainLODManager* LODManagerInterface = Cast<ITerrainLODManager>(TerrainLODManager.GetObject()))
            {
                // Get current viewer position (use world origin if no camera available)
                FVector ViewerPosition = FVector::ZeroVector;
                if (CachedWorld && CachedWorld->GetFirstPlayerController())
                {
                    if (APawn* PlayerPawn = CachedWorld->GetFirstPlayerController()->GetPawn())
                    {
                        ViewerPosition = PlayerPawn->GetActorLocation();
                    }
                }
                
                bShouldCreateMesh = LODManagerInterface->IsTileVisible(TileCoord, ViewerPosition);
            }
            
            if (bShouldCreateMesh)
            {
                CreateTerrainMeshForTile(TileCoord);
            }
            else
            {
                UE_LOG(LogVHMTerrainRenderer, VeryVerbose, TEXT("Skipping mesh creation for tile (%d, %d) - not visible"), 
                       TileCoord.X, TileCoord.Y);
            }
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
    BiomeService = nullptr;
    
    if (IHeightfieldTextureManager* TextureManagerInterface = Cast<IHeightfieldTextureManager>(HeightfieldTextureManager.GetObject()))
    {
        TextureManagerInterface->OptimizeTextureMemory();
        HeightfieldTextureManager.SetInterface(nullptr);
    }

    // Cleanup terrain material system
    if (TerrainMaterialSystem.GetInterface())
    {
        TerrainMaterialSystem.SetInterface(nullptr);
    }

    // Cleanup terrain LOD manager
    if (TerrainLODManager.GetInterface())
    {
        TerrainLODManager.SetInterface(nullptr);
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

    // Check if we already have a mesh for this tile
    if (TerrainMeshes.Contains(TileCoord))
    {
        UE_LOG(LogVHMTerrainRenderer, Warning, TEXT("CreateVHMComponent - Tile (%d, %d) already has a mesh, cleaning up first"), TileCoord.X, TileCoord.Y);
        CleanupMeshData(TileCoord);
        TerrainMeshes.Remove(TileCoord);
    }

    // Create an actor to hold the VHM component with unique naming
    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = NAME_None; // Let UE generate unique name automatically
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    
    AActor* VHMActor = CachedWorld->SpawnActor<AActor>(AActor::StaticClass(), SpawnParams);
    if (!VHMActor)
    {
        UE_LOG(LogVHMTerrainRenderer, Error, TEXT("CreateVHMComponent - Failed to spawn VHM actor for tile: (%d, %d)"), TileCoord.X, TileCoord.Y);
        return nullptr;
    }

    // Tag this actor so gameplay/blueprints can easily find terrain tiles
    VHMActor->Tags.AddUnique(FName(TEXT("VHMTerrainTile")));
    
#if WITH_EDITOR
    // Help identify in the editor outliner
    VHMActor->SetActorLabel(FString::Printf(TEXT("VHMTerrain_Tile_%d_%d"), TileCoord.X, TileCoord.Y));
#endif

    // Create and attach VHM component
    UVirtualHeightfieldMeshComponent* VHMComponent = NewObject<UVirtualHeightfieldMeshComponent>(VHMActor);
    if (!VHMComponent)
    {
        UE_LOG(LogVHMTerrainRenderer, Error, TEXT("CreateVHMComponent - Failed to create VHM component for tile: (%d, %d)"), TileCoord.X, TileCoord.Y);
        VHMActor->Destroy();
        return nullptr;
    }

    // Tag component for easy lookup
    VHMComponent->ComponentTags.AddUnique(FName(TEXT("VHMRoot")));

    // Set as root component
    VHMActor->SetRootComponent(VHMComponent);
    
    // Position the actor at the tile corner (Unreal world units are cm)
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
    // Enable collision so edited terrain affects gameplay collision promptly (Gate B)
    VHMComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    VHMComponent->SetCollisionResponseToAllChannels(ECR_Block);
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

    // Attempt to fetch the cached heightfield so we can build a simple procedural mesh fallback
    FHeightfieldData CachedHF;
    if (!HeightfieldService || !HeightfieldService->GetCachedHeightfield(TileCoord, CachedHF))
    {
        UE_LOG(LogVHMTerrainRenderer, Warning, TEXT("GenerateMeshFromHeightfield - Cached heightfield not found for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
        return false;
    }

    // Create a procedural mesh component as a fallback visualization
    AActor* Owner = VHMComponent->GetOwner();
    if (!Owner)
    {
        UE_LOG(LogVHMTerrainRenderer, Error, TEXT("GenerateMeshFromHeightfield - VHM component has no owner for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
        return false;
    }

    UProceduralMeshComponent* Proc = NewObject<UProceduralMeshComponent>(Owner);
    if (!Proc)
    {
        UE_LOG(LogVHMTerrainRenderer, Error, TEXT("GenerateMeshFromHeightfield - Failed to create ProceduralMeshComponent for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
        return false;
    }

    Proc->SetupAttachment(VHMComponent); // attach under the VHM component/actor
    Proc->RegisterComponent();
    // Enable collision on the generated mesh
    Proc->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Proc->SetCollisionResponseToAllChannels(ECR_Block);
    Proc->bUseAsyncCooking = true;
    Proc->ComponentTags.AddUnique(FName(TEXT("VHMProcMesh")));

    const int32 Resolution = CachedHF.Resolution; // expected 64
    if (Resolution <= 1)
    {
        UE_LOG(LogVHMTerrainRenderer, Error, TEXT("GenerateMeshFromHeightfield - Invalid resolution %d for tile (%d, %d)"), Resolution, TileCoord.X, TileCoord.Y);
        return false;
    }

    const FWorldGenConfig& Config = WorldGenSettings->GetWorldGenConfig();
    const float TileSizeCm = Config.TileSizeMeters * 100.0f; // meters -> centimeters
    const float HeightScale = Config.HeightfieldScale;       // meters -> centimeters (default 100)

    // Build vertex data
    TArray<FVector> Vertices;
    TArray<int32> Indices;
    TArray<FVector> Normals;
    TArray<FVector2D> UV0;
    TArray<FLinearColor> Colors;
    TArray<FProcMeshTangent> Tangents;

    Vertices.Reserve(Resolution * Resolution);
    Normals.Reserve(Resolution * Resolution);
    UV0.Reserve(Resolution * Resolution);
    Colors.Reserve(Resolution * Resolution);

    // Height scale: convert meters to centimeters
    for (int32 y = 0; y < Resolution; ++y)
    {
        for (int32 x = 0; x < Resolution; ++x)
        {
            const int32 idx = y * Resolution + x;
            const float fx = (float)x / (Resolution - 1);
            const float fy = (float)y / (Resolution - 1);
            const float height = CachedHF.HeightData.IsValidIndex(idx) ? CachedHF.HeightData[idx] : 0.0f;

            Vertices.Add(FVector(fx * TileSizeCm, fy * TileSizeCm, height * HeightScale));
            Normals.Add(FVector(0, 0, 1)); // simple up normals as a placeholder
            UV0.Add(FVector2D(fx, fy));
            Colors.Add(FLinearColor::White);
            Tangents.Add(FProcMeshTangent(1, 0, 0));
        }
    }

    // Build index buffer (two triangles per quad)
    Indices.Reserve((Resolution - 1) * (Resolution - 1) * 6);
    for (int32 y = 0; y < Resolution - 1; ++y)
    {
        for (int32 x = 0; x < Resolution - 1; ++x)
        {
            const int32 v00 = y * Resolution + x;
            const int32 v10 = v00 + 1;
            const int32 v01 = v00 + Resolution;
            const int32 v11 = v01 + 1;

            // Winding order (counter-clockwise)
            Indices.Add(v00); Indices.Add(v11); Indices.Add(v10);
            Indices.Add(v00); Indices.Add(v01); Indices.Add(v11);
        }
    }

    Proc->CreateMeshSection_LinearColor(0, Vertices, Indices, Normals, UV0, Colors, Tangents, /*bCreateCollision*/ true);
    Proc->SetMaterial(0, VHMComponent->UPrimitiveComponent::GetMaterial(0)); // apply whatever material was set on VHM comp later

    // Keep VHM component alive and registered for consistency/debug flows
    if (!VHMComponent->IsRegistered())
    {
        VHMComponent->RegisterComponent();
    }
    VHMComponent->SetHiddenInGame(true); // hide placeholder VHM component to avoid confusion

    UE_LOG(LogVHMTerrainRenderer, Log, TEXT("Procedural terrain mesh created for tile (%d, %d) with %d verts (TileSize=%.0fcm, HeightScale=%.1f)"), TileCoord.X, TileCoord.Y, Vertices.Num(), TileSizeCm, HeightScale);
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
    const float TileSizeCm = Config.TileSizeMeters * 100.0f;
    const float HalfZ = Config.MaxTerrainHeight * Config.HeightfieldScale; // approx top extent in cm
    
    FVector TileCenter = GetTileCenterWorldPosition(TileCoord);
    FVector HalfExtent(TileSizeCm * 0.5f, TileSizeCm * 0.5f, HalfZ);
    
    return FBox(TileCenter - HalfExtent, TileCenter + HalfExtent);
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
    float TileSizeCm = Config.TileSizeMeters * 100.0f;
    
    return FVector(
        TileCoord.X * TileSizeCm + TileSizeCm * 0.5f,
        TileCoord.Y * TileSizeCm + TileSizeCm * 0.5f,
        0.0f
    );
}

float UVHMTerrainRenderer::GetLastMeshGenerationTimeMs(const FTileCoord& TileCoord) const
{
    if (const float* Time = LastMeshGenTimePerTile.Find(TileCoord))
    {
        return *Time;
    }
    return 0.0f;
}

FVector UVHMTerrainRenderer::GetTileCornerWorldPosition(const FTileCoord& TileCoord) const
{
    if (!WorldGenSettings)
    {
        return FVector::ZeroVector;
    }

    const FWorldGenConfig& Config = WorldGenSettings->GetWorldGenConfig();
    float TileSizeCm = Config.TileSizeMeters * 100.0f;
    
    return FVector(
        TileCoord.X * TileSizeCm,
        TileCoord.Y * TileSizeCm,
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

bool UVHMTerrainRenderer::InitializeTerrainLODManager()
{
    UVHMTerrainLODManager* LODManager = NewObject<UVHMTerrainLODManager>(this);
    if (!LODManager)
    {
        UE_LOG(LogVHMTerrainRenderer, Error, TEXT("InitializeTerrainLODManager - Failed to create TerrainLODManager"));
        return false;
    }

    TerrainLODManager.SetObject(LODManager);

    // Initialize LOD manager with VHM settings
    LODManager->Initialize(VHMSettings);

    UE_LOG(LogVHMTerrainRenderer, Log, TEXT("TerrainLODManager initialized successfully"));
    return true;
}

void UVHMTerrainRenderer::UpdateFrameBasedLOD(const FVector& ViewerPosition)
{
    if (!TerrainLODManager.GetInterface())
    {
        return;
    }

    ITerrainLODManager* LODManagerInterface = Cast<ITerrainLODManager>(TerrainLODManager.GetObject());
    if (!LODManagerInterface)
    {
        return;
    }

    // Update LOD levels for all tiles through the LOD manager
    LODManagerInterface->UpdateLODLevels(ViewerPosition);

    // Update mesh data with new LOD levels from LOD manager
    for (auto& MeshPair : TerrainMeshes)
    {
        const FTileCoord& TileCoord = MeshPair.Key;
        FTerrainMeshData& MeshData = MeshPair.Value;

        int32 NewLODLevel = LODManagerInterface->GetCurrentLODLevel(TileCoord);
        if (NewLODLevel >= 0 && NewLODLevel != MeshData.CurrentLODLevel)
        {
            MeshData.CurrentLODLevel = NewLODLevel;
            
            // Update VHM component LOD if available
            if (MeshData.VHMComponent)
            {
                // VHM components handle LOD internally based on distance
                // We track the level here for performance monitoring
                UE_LOG(LogVHMTerrainRenderer, VeryVerbose, TEXT("Updated LOD for tile (%d, %d) to level %d"), 
                       TileCoord.X, TileCoord.Y, NewLODLevel);
            }
        }
    }

    // Reset frame stats for next frame
    LODManagerInterface->ResetFrameStats();
}

void UVHMTerrainRenderer::HandleVisibilityCulling(const FVector& ViewerPosition)
{
    if (!TerrainLODManager.GetInterface())
    {
        return;
    }

    ITerrainLODManager* LODManagerInterface = Cast<ITerrainLODManager>(TerrainLODManager.GetObject());
    if (!LODManagerInterface)
    {
        return;
    }

    // Check visibility for existing meshes and update visibility state
    TArray<FTileCoord> TilesToRemove;
    for (auto& MeshPair : TerrainMeshes)
    {
        const FTileCoord& TileCoord = MeshPair.Key;
        FTerrainMeshData& MeshData = MeshPair.Value;

        bool bShouldBeVisible = LODManagerInterface->IsTileVisible(TileCoord, ViewerPosition);
        
        if (MeshData.bIsVisible != bShouldBeVisible)
        {
            MeshData.bIsVisible = bShouldBeVisible;
            
            if (MeshData.VHMComponent)
            {
                // Update component visibility
                MeshData.VHMComponent->SetVisibility(bShouldBeVisible);
                MeshData.VHMComponent->SetHiddenInGame(!bShouldBeVisible);
                
                UE_LOG(LogVHMTerrainRenderer, VeryVerbose, TEXT("Updated visibility for tile (%d, %d): %s"), 
                       TileCoord.X, TileCoord.Y, bShouldBeVisible ? TEXT("Visible") : TEXT("Hidden"));
            }
        }

        // Mark tiles for removal if they've been invisible for too long
        if (!bShouldBeVisible)
        {
            // Could add time-based culling here if needed
            // For now, we keep invisible meshes in memory for quick re-activation
        }
    }

    // Remove tiles that are no longer needed
    for (const FTileCoord& TileCoord : TilesToRemove)
    {
        RemoveTerrainMesh(TileCoord);
    }
}

void UVHMTerrainRenderer::ApplyAdaptiveQualityAdjustment()
{
    if (!TerrainLODManager.GetInterface())
    {
        return;
    }

    ITerrainLODManager* LODManagerInterface = Cast<ITerrainLODManager>(TerrainLODManager.GetObject());
    if (!LODManagerInterface)
    {
        return;
    }

    // Get current frame time
    float CurrentFrameTime = FApp::GetDeltaTime() * 1000.0f; // Convert to milliseconds
    
    // Update performance stats in LOD manager
    if (UVHMTerrainLODManager* ConcreteLODManager = Cast<UVHMTerrainLODManager>(TerrainLODManager.GetObject()))
    {
        ConcreteLODManager->UpdatePerformanceStats(CurrentFrameTime);
    }

    // Apply adaptive quality based on target frame time
    float TargetFrameTime = 1000.0f / 60.0f; // 60 FPS target
    LODManagerInterface->OptimizeMeshDetail(TargetFrameTime);

    // Log performance adjustments if significant changes occur
    int32 LODTransitions = LODManagerInterface->GetLODTransitionsThisFrame();
    if (LODTransitions > 5) // Threshold for logging
    {
        UE_LOG(LogVHMTerrainRenderer, Verbose, TEXT("Adaptive quality adjustment: %d LOD transitions, Frame time: %.2fms"), 
               LODTransitions, CurrentFrameTime);
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

bool UVHMTerrainRenderer::InitializeTerrainMaterialSystem()
{
    if (!BiomeService)
    {
        UE_LOG(LogVHMTerrainRenderer, Error, TEXT("InitializeTerrainMaterialSystem - BiomeService is null"));
        return false;
    }

    UVHMTerrainMaterialSystem* MaterialSystem = NewObject<UVHMTerrainMaterialSystem>(this);
    if (!MaterialSystem)
    {
        UE_LOG(LogVHMTerrainRenderer, Error, TEXT("InitializeTerrainMaterialSystem - Failed to create TerrainMaterialSystem"));
        return false;
    }

    TerrainMaterialSystem.SetObject(MaterialSystem);

    // Initialize material system with biome service and VHM settings
    if (!MaterialSystem->Initialize(BiomeService, VHMSettings))
    {
        UE_LOG(LogVHMTerrainRenderer, Error, TEXT("InitializeTerrainMaterialSystem - Failed to initialize TerrainMaterialSystem"));
        return false;
    }

    UE_LOG(LogVHMTerrainRenderer, Log, TEXT("TerrainMaterialSystem initialized successfully"));
    return true;
}

bool UVHMTerrainRenderer::InitializeTileBoundaryManager()
{
    UVHMTileBoundaryManager* BoundaryManager = NewObject<UVHMTileBoundaryManager>(this);
    if (!BoundaryManager)
    {
        UE_LOG(LogVHMTerrainRenderer, Error, TEXT("InitializeTileBoundaryManager - Failed to create TileBoundaryManager"));
        return false;
    }

    TileBoundaryManager.SetObject(BoundaryManager);

    // Initialize boundary manager with heightfield service and VHM settings
    if (!BoundaryManager->Initialize(HeightfieldService, VHMSettings))
    {
        UE_LOG(LogVHMTerrainRenderer, Error, TEXT("InitializeTileBoundaryManager - Failed to initialize TileBoundaryManager"));
        return false;
    }

    UE_LOG(LogVHMTerrainRenderer, Log, TEXT("TileBoundaryManager initialized successfully"));
    return true;
}

bool UVHMTerrainRenderer::UpdateTerrainMeshWithBoundaryStitching(const FTileCoord& TileCoord, 
                                                               const TArray<FHeightfieldModification>& Modifications,
                                                               bool bStitchBoundaries)
{
    // First update the terrain mesh normally
    if (!UpdateTerrainMesh(TileCoord, Modifications))
    {
        return false;
    }

    // Apply boundary stitching if requested and enabled
    if (bStitchBoundaries && VHMSettings.bEnableBoundaryStitching && TileBoundaryManager.GetInterface())
    {
        // Update boundary data for the modified tile
        ITileBoundaryManager* BoundaryManagerInterface = Cast<ITileBoundaryManager>(TileBoundaryManager.GetObject());
        if (BoundaryManagerInterface)
        {
            BoundaryManagerInterface->UpdateTileBoundaryData(TileCoord, Modifications, VHMSettings.HeightTextureResolution);
            
            // Update adjacent tile boundaries
            UpdateAdjacentTileBoundaries(TileCoord);
        }
    }

    return true;
}

TScriptInterface<ITileBoundaryManager> UVHMTerrainRenderer::GetTileBoundaryManager() const
{
    return TileBoundaryManager;
}

TScriptInterface<ITerrainMaterialSystem> UVHMTerrainRenderer::GetTerrainMaterialSystem() const
{
    return TerrainMaterialSystem;
}

bool UVHMTerrainRenderer::ApplyBoundaryStitching(const FTileCoord& TileCoord, TArray<float>& InOutHeightData)
{
    if (!TileBoundaryManager.GetInterface())
    {
        return false; // No boundary manager available
    }

    ITileBoundaryManager* BoundaryManagerInterface = Cast<ITileBoundaryManager>(TileBoundaryManager.GetObject());
    if (!BoundaryManagerInterface)
    {
        return false;
    }

    // Get adjacent tiles
    TArray<FTileCoord> AdjacentTiles;
    BoundaryManagerInterface->GetAdjacentTiles(TileCoord, AdjacentTiles);

    bool bAnyStitchingApplied = false;

    // Apply stitching with each adjacent tile that exists
    for (const FTileCoord& AdjacentTile : AdjacentTiles)
    {
        // Check if adjacent tile has mesh data (is loaded)
        if (TerrainMeshes.Contains(AdjacentTile))
        {
            TArray<float> StitchedHeightData;
            if (BoundaryManagerInterface->StitchTileBoundaries(TileCoord, AdjacentTile, StitchedHeightData, VHMSettings.HeightTextureResolution))
            {
                InOutHeightData = MoveTemp(StitchedHeightData);
                bAnyStitchingApplied = true;
                
                UE_LOG(LogVHMTerrainRenderer, Verbose, TEXT("Applied boundary stitching between tiles (%d, %d) and (%d, %d)"), 
                       TileCoord.X, TileCoord.Y, AdjacentTile.X, AdjacentTile.Y);
            }
        }
    }

    return bAnyStitchingApplied;
}

void UVHMTerrainRenderer::UpdateAdjacentTileBoundaries(const FTileCoord& ModifiedTileCoord)
{
    if (!TileBoundaryManager.GetInterface())
    {
        return;
    }

    ITileBoundaryManager* BoundaryManagerInterface = Cast<ITileBoundaryManager>(TileBoundaryManager.GetObject());
    if (!BoundaryManagerInterface)
    {
        return;
    }

    // Get adjacent tiles
    TArray<FTileCoord> AdjacentTiles;
    BoundaryManagerInterface->GetAdjacentTiles(ModifiedTileCoord, AdjacentTiles);

    // Notify boundary manager about the modification
    TArray<FTileCoord> ModifiedTiles;
    ModifiedTiles.Add(ModifiedTileCoord);
    BoundaryManagerInterface->NotifyAdjacentTilesModified(ModifiedTiles);

    // Update texture data for adjacent tiles that are loaded
    for (const FTileCoord& AdjacentTile : AdjacentTiles)
    {
        if (FTerrainMeshData* AdjacentMeshData = TerrainMeshes.Find(AdjacentTile))
        {
            // Get current height data for adjacent tile
            FHeightfieldData AdjacentHeightfieldData;
            if (HeightfieldService && HeightfieldService->GetCachedHeightfield(AdjacentTile, AdjacentHeightfieldData))
            {
                // Apply boundary stitching to adjacent tile
                TArray<float> StitchedHeightData = AdjacentHeightfieldData.HeightData;
                if (ApplyBoundaryStitching(AdjacentTile, StitchedHeightData))
                {
                    // Update height texture for adjacent tile
                    IHeightfieldTextureManager* TextureManagerInterface = Cast<IHeightfieldTextureManager>(HeightfieldTextureManager.GetObject());
                    if (TextureManagerInterface)
                    {
                        // Create new height texture with stitched data
                        UTexture2D* NewHeightTexture = TextureManagerInterface->CreateHeightTexture(AdjacentTile, StitchedHeightData);
                        if (NewHeightTexture)
                        {
                            AdjacentMeshData->HeightTexture = NewHeightTexture;
                            
                            // Update VHM component with new texture
                            if (AdjacentMeshData->VHMComponent)
                            {
                                GenerateMeshFromHeightfield(AdjacentMeshData->VHMComponent, AdjacentTile, NewHeightTexture);
                            }
                            
                            UE_LOG(LogVHMTerrainRenderer, Verbose, TEXT("Updated adjacent tile (%d, %d) with boundary stitching"), 
                                   AdjacentTile.X, AdjacentTile.Y);
                        }
                    }
                }
            }
        }
    }
}
bool UVHMTerrainRenderer::CreateFlatMeadowFallback(const FTileCoord& TileCoord)
{
    UE_LOG(LogVHMTerrainRenderer, Warning, TEXT("Creating flat meadow fallback for tile (%d, %d)"), TileCoord.X, TileCoord.Y);

    // Create a simple flat heightfield with meadow biome
    TArray<float> FlatHeightData;
    int32 Resolution = VHMSettings.HeightTextureResolution;
    FlatHeightData.SetNumZeroed(Resolution * Resolution);
    
    // Fill with flat height at sea level
    float FlatHeight = WorldGenSettings ? WorldGenSettings->Settings.SeaLevel : 0.0f;
    for (int32 i = 0; i < FlatHeightData.Num(); i++)
    {
        FlatHeightData[i] = FlatHeight;
    }

    // Try to create height texture for flat terrain
    IHeightfieldTextureManager* TextureManagerInterface = Cast<IHeightfieldTextureManager>(HeightfieldTextureManager.GetObject());
    UTexture2D* FallbackHeightTexture = TextureManagerInterface ? TextureManagerInterface->CreateHeightTexture(TileCoord, FlatHeightData) : nullptr;
    
    if (!FallbackHeightTexture)
    {
        UE_LOG(LogVHMTerrainRenderer, Warning, TEXT("Could not create fallback height texture for tile (%d, %d) - creating minimal fallback"), TileCoord.X, TileCoord.Y);
        // Continue anyway - we'll create a fallback without texture
    }

    // Create fallback terrain mesh data without VHM component
    FTerrainMeshData FallbackMeshData;
    FallbackMeshData.TileCoord = TileCoord;
    FallbackMeshData.VHMComponent = nullptr; // No VHM component for fallback
    FallbackMeshData.HeightTexture = FallbackHeightTexture;
    FallbackMeshData.MaterialInstance = nullptr; // Will be set below if material system available
    FallbackMeshData.CurrentLODLevel = 0;
    FallbackMeshData.LastUpdateTime = FPlatformTime::Seconds();
    FallbackMeshData.WorldBounds = CalculateTileWorldBounds(TileCoord);
    FallbackMeshData.bIsVisible = true;

    // Create meadow material if material system is available
    if (BiomeService && TerrainMaterialSystem.GetInterface())
    {
        FBiomeDefinition MeadowBiome;
        if (BiomeService->GetBiomeDefinition(EBiomeType::Meadows, MeadowBiome))
        {
            UMaterialInstanceDynamic* FallbackMaterial = TerrainMaterialSystem.GetInterface()->CreateTileMaterial(TileCoord, MeadowBiome);
            FallbackMeshData.MaterialInstance = FallbackMaterial;
            UE_LOG(LogVHMTerrainRenderer, Log, TEXT("Applied meadow material to fallback tile (%d, %d)"), TileCoord.X, TileCoord.Y);
        }
    }

    // Store fallback mesh data
    TerrainMeshes.Add(TileCoord, FallbackMeshData);

    UE_LOG(LogVHMTerrainRenderer, Log, TEXT("Created flat meadow fallback for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
    return true;
}
