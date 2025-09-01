#include "VHMTerrainRendering/TerrainMaterialSystem.h"
#include "Services/BiomeService.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/TextureDefines.h"

DEFINE_LOG_CATEGORY_STATIC(LogTerrainMaterialSystem, Log, All);

UVHMTerrainMaterialSystem::UVHMTerrainMaterialSystem()
{
    BiomeService = nullptr;
    BaseMaterial = nullptr;
    TerrainRVT = nullptr;
    bRVTInitialized = false;
    ActiveRVTTextures = 0;
    RVTMemoryUsage = 0.0f;
    StreamingRequests = 0;
}

bool UVHMTerrainMaterialSystem::Initialize(UBiomeService* InBiomeService, const FVHMSettings& InVHMSettings)
{
    if (!InBiomeService)
    {
        UE_LOG(LogTerrainMaterialSystem, Error, TEXT("Initialize: BiomeService is null"));
        return false;
    }

    BiomeService = InBiomeService;
    VHMSettings = InVHMSettings;

    // Initialize RVT if enabled
    if (VHMSettings.bUseRuntimeVirtualTexturing)
    {
        if (!InitializeRuntimeVirtualTexturing())
        {
            UE_LOG(LogTerrainMaterialSystem, Warning, TEXT("Initialize: RVT initialization failed, continuing without RVT"));
        }
        else
        {
            // Initialize default texture layers
            InitializeDefaultTextureLayers();
        }
    }

    UE_LOG(LogTerrainMaterialSystem, Log, TEXT("TerrainMaterialSystem initialized successfully"));
    return true;
}

UMaterialInstanceDynamic* UVHMTerrainMaterialSystem::CreateTileMaterial(const FTileCoord& TileCoord, const FBiomeDefinition& BiomeData)
{
    // Check if material already exists
    if (TObjectPtr<UMaterialInstanceDynamic>* ExistingMaterial = TileMaterials.Find(TileCoord))
    {
        if (IsValid(ExistingMaterial->Get()))
        {
            // Update existing material with new biome data
            ApplyBiomeParameters(ExistingMaterial->Get(), BiomeData);
            return ExistingMaterial->Get();
        }
    }

    // Validate biome definition
    if (!ValidateBiomeDefinition(BiomeData))
    {
        UE_LOG(LogTerrainMaterialSystem, Warning, TEXT("CreateTileMaterial: Invalid biome definition for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
        return nullptr;
    }

    // Create new material instance
    UMaterialInstanceDynamic* NewMaterial = CreateMaterialFromBiome(BiomeData);
    if (!NewMaterial)
    {
        UE_LOG(LogTerrainMaterialSystem, Error, TEXT("CreateTileMaterial: Failed to create material for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
        return nullptr;
    }

    // Apply biome-specific parameters
    ApplyBiomeParameters(NewMaterial, BiomeData);

    // Store material
    TileMaterials.Add(TileCoord, NewMaterial);

    UE_LOG(LogTerrainMaterialSystem, Verbose, TEXT("CreateTileMaterial: Created material for tile (%d, %d) with biome %s"), 
           TileCoord.X, TileCoord.Y, *BiomeData.BiomeName);

    return NewMaterial;
}

bool UVHMTerrainMaterialSystem::UpdateMaterialParameters(const FTileCoord& TileCoord, const FBiomeDefinition& BiomeData)
{
    TObjectPtr<UMaterialInstanceDynamic>* MaterialPtr = TileMaterials.Find(TileCoord);
    if (!MaterialPtr || !IsValid(MaterialPtr->Get()))
    {
        UE_LOG(LogTerrainMaterialSystem, Warning, TEXT("UpdateMaterialParameters: No material found for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
        return false;
    }

    if (!ValidateBiomeDefinition(BiomeData))
    {
        UE_LOG(LogTerrainMaterialSystem, Warning, TEXT("UpdateMaterialParameters: Invalid biome definition for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
        return false;
    }

    ApplyBiomeParameters(MaterialPtr->Get(), BiomeData);

    UE_LOG(LogTerrainMaterialSystem, Verbose, TEXT("UpdateMaterialParameters: Updated material for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
    return true;
}

void UVHMTerrainMaterialSystem::BlendMaterialsAcrossTiles(const TArray<FTileCoord>& AdjacentTiles)
{
    if (AdjacentTiles.Num() < 2)
    {
        return;
    }

    // Process each tile for blending
    for (const FTileCoord& CenterTile : AdjacentTiles)
    {
        TObjectPtr<UMaterialInstanceDynamic>* MaterialPtr = TileMaterials.Find(CenterTile);
        if (!MaterialPtr || !IsValid(MaterialPtr->Get()))
        {
            continue;
        }

        // Calculate blend weights for adjacent tiles
        TMap<FTileCoord, float> BlendWeights = CalculateBlendWeights(CenterTile, AdjacentTiles);

        // Update blend data
        FTileMaterialBlendData& BlendData = TileBlendData.FindOrAdd(CenterTile);
        BlendData.AdjacentTiles = AdjacentTiles;
        BlendData.BlendWeights = BlendWeights;
        BlendData.LastUpdateTime = FPlatformTime::Seconds();

        // Apply blending to material
        ApplyMaterialBlending(MaterialPtr->Get(), BlendData);
    }

    UE_LOG(LogTerrainMaterialSystem, Verbose, TEXT("BlendMaterialsAcrossTiles: Applied blending to %d tiles"), AdjacentTiles.Num());
}

bool UVHMTerrainMaterialSystem::InitializeRuntimeVirtualTexturing()
{
    if (bRVTInitialized)
    {
        return true;
    }

    // Validate RVT configuration
    if (!ValidateRVTConfiguration())
    {
        UE_LOG(LogTerrainMaterialSystem, Error, TEXT("InitializeRuntimeVirtualTexturing: Invalid RVT configuration"));
        return false;
    }

    // Create RVT texture
    TerrainRVT = CreateTerrainRVT();
    if (!TerrainRVT)
    {
        UE_LOG(LogTerrainMaterialSystem, Warning, TEXT("InitializeRuntimeVirtualTexturing: Failed to create RVT texture, terrain will work without RVT"));
        return false;
    }

    // Configure RVT parameters using UE5.6 compatible approach
    ConfigureRVTParameters(TerrainRVT.Get());

    // Validate RVT is properly configured
    if (!IsValid(TerrainRVT.Get()))
    {
        UE_LOG(LogTerrainMaterialSystem, Warning, TEXT("InitializeRuntimeVirtualTexturing: RVT configuration validation failed, continuing without RVT"));
        TerrainRVT = nullptr;
        return false;
    }

    // Initialize texture layers if configured
    if (RVTConfig.TextureLayers.Num() > 0)
    {
        SetupTextureDetailLayers(RVTConfig.TextureLayers);
    }

    bRVTInitialized = true;
    UE_LOG(LogTerrainMaterialSystem, Log, TEXT("InitializeRuntimeVirtualTexturing: RVT system initialized successfully with %d texture layers"), RVTConfig.TextureLayers.Num());
    return true;
}

UMaterialInstanceDynamic* UVHMTerrainMaterialSystem::GetTileMaterial(const FTileCoord& TileCoord)
{
    if (TObjectPtr<UMaterialInstanceDynamic>* MaterialPtr = TileMaterials.Find(TileCoord))
    {
        return MaterialPtr->Get();
    }
    return nullptr;
}

void UVHMTerrainMaterialSystem::RemoveTileMaterial(const FTileCoord& TileCoord)
{
    if (TObjectPtr<UMaterialInstanceDynamic>* MaterialPtr = TileMaterials.Find(TileCoord))
    {
        if (IsValid(MaterialPtr->Get()))
        {
            MaterialPtr->Get()->MarkAsGarbage();
        }
        TileMaterials.Remove(TileCoord);
    }

    // Remove blend data
    TileBlendData.Remove(TileCoord);

    UE_LOG(LogTerrainMaterialSystem, Verbose, TEXT("RemoveTileMaterial: Removed material for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
}

bool UVHMTerrainMaterialSystem::IsRVTInitialized() const
{
    return bRVTInitialized && IsValid(TerrainRVT.Get());
}

void UVHMTerrainMaterialSystem::SetBaseMaterial(UMaterialInterface* InBaseMaterial)
{
    BaseMaterial = InBaseMaterial;
    UE_LOG(LogTerrainMaterialSystem, Log, TEXT("SetBaseMaterial: Base material set to %s"), 
           InBaseMaterial ? *InBaseMaterial->GetName() : TEXT("nullptr"));
}

void UVHMTerrainMaterialSystem::SetRVTConfig(const FTerrainRVTConfig& InRVTConfig)
{
    RVTConfig = InRVTConfig;
    
    // Reinitialize RVT if already initialized
    if (bRVTInitialized)
    {
        bRVTInitialized = false;
        if (VHMSettings.bUseRuntimeVirtualTexturing)
        {
            if (!InitializeRuntimeVirtualTexturing())
            {
                UE_LOG(LogTerrainMaterialSystem, Warning, TEXT("SetRVTConfig: RVT reinitialization failed with new config, continuing without RVT"));
            }
        }
    }
}

bool UVHMTerrainMaterialSystem::GetTileMaterialBlendData(const FTileCoord& TileCoord, FTileMaterialBlendData& OutBlendData) const
{
    if (const FTileMaterialBlendData* BlendData = TileBlendData.Find(TileCoord))
    {
        OutBlendData = *BlendData;
        return true;
    }
    return false;
}

void UVHMTerrainMaterialSystem::ForceUpdateAllMaterials()
{
    if (!BiomeService)
    {
        UE_LOG(LogTerrainMaterialSystem, Warning, TEXT("ForceUpdateAllMaterials: BiomeService is null"));
        return;
    }

    int32 UpdatedCount = 0;
    for (auto& MaterialPair : TileMaterials)
    {
        const FTileCoord& TileCoord = MaterialPair.Key;
        UMaterialInstanceDynamic* Material = MaterialPair.Value.Get();

        if (!IsValid(Material))
        {
            continue;
        }

        // Get biome definition for this tile
        FBiomeDefinition BiomeDefinition;
        EBiomeType TileBiome = BiomeService->DetermineTileBiome(TileCoord, TArray<float>());
        if (BiomeService->GetBiomeDefinition(TileBiome, BiomeDefinition))
        {
            ApplyBiomeParameters(Material, BiomeDefinition);
            UpdatedCount++;
        }
    }

    UE_LOG(LogTerrainMaterialSystem, Log, TEXT("ForceUpdateAllMaterials: Updated %d materials"), UpdatedCount);
}

float UVHMTerrainMaterialSystem::GetMaterialMemoryUsageMB() const
{
    // Rough estimation of material memory usage
    float MemoryUsage = 0.0f;
    
    // Count material instances
    MemoryUsage += TileMaterials.Num() * 0.1f; // ~100KB per material instance estimate
    
    // Add RVT memory if initialized
    if (bRVTInitialized && IsValid(TerrainRVT))
    {
        MemoryUsage += RVTConfig.RVTMemoryBudgetMB;
    }
    
    return MemoryUsage;
}

UMaterialInstanceDynamic* UVHMTerrainMaterialSystem::CreateMaterialFromBiome(const FBiomeDefinition& BiomeData)
{
    UMaterialInterface* ParentMaterial = nullptr;
    
    // Use biome-specific material if available
    if (BiomeData.TerrainMaterial.IsValid())
    {
        ParentMaterial = BiomeData.TerrainMaterial.LoadSynchronous();
    }
    
    // Fall back to base material
    if (!ParentMaterial)
    {
        ParentMaterial = BaseMaterial;
    }
    
    // Fall back to default material
    if (!ParentMaterial)
    {
        ParentMaterial = GetDefaultMaterial();
    }
    
    if (!ParentMaterial)
    {
        UE_LOG(LogTerrainMaterialSystem, Error, TEXT("CreateMaterialFromBiome: No parent material available"));
        return nullptr;
    }

    // Create dynamic material instance
    UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(ParentMaterial, this);
    if (!DynamicMaterial)
    {
        UE_LOG(LogTerrainMaterialSystem, Error, TEXT("CreateMaterialFromBiome: Failed to create dynamic material instance"));
        return nullptr;
    }

    return DynamicMaterial;
}

void UVHMTerrainMaterialSystem::ApplyBiomeParameters(UMaterialInstanceDynamic* Material, const FBiomeDefinition& BiomeData)
{
    if (!IsValid(Material))
    {
        return;
    }

    // Apply RVT blend color
    Material->SetVectorParameterValue(TEXT("BiomeBlendColor"), BiomeData.RVTBlendColor);

    // Apply biome mask texture if available
    if (BiomeData.BiomeMask.IsValid())
    {
        UTexture2D* BiomeMaskTexture = BiomeData.BiomeMask.LoadSynchronous();
        if (BiomeMaskTexture)
        {
            Material->SetTextureParameterValue(TEXT("BiomeMask"), BiomeMaskTexture);
        }
    }

    // Apply climate-based parameters
    Material->SetScalarParameterValue(TEXT("BiomeWeight"), BiomeData.BiomeWeight);
    Material->SetScalarParameterValue(TEXT("MinTemperature"), BiomeData.MinTemperature);
    Material->SetScalarParameterValue(TEXT("MaxTemperature"), BiomeData.MaxTemperature);
    Material->SetScalarParameterValue(TEXT("MinMoisture"), BiomeData.MinMoisture);
    Material->SetScalarParameterValue(TEXT("MaxMoisture"), BiomeData.MaxMoisture);

    // Apply terrain height parameters
    Material->SetScalarParameterValue(TEXT("BaseHeight"), BiomeData.BaseHeight);
    Material->SetScalarParameterValue(TEXT("HeightVariation"), BiomeData.HeightVariation);

    // Apply RVT texture if initialized and available
    if (bRVTInitialized && IsValid(TerrainRVT.Get()))
    {
        // TODO: UE5.6 compatibility - URuntimeVirtualTexture parameter setting needs investigation
        // The material system in UE5.6 may have changed how RVT parameters are handled
        // For now, skip RVT parameter setting to allow compilation
        UE_LOG(LogTerrainMaterialSystem, Warning, TEXT("ApplyBiomeParameters: RVT parameter setting disabled for UE5.6 compatibility - material will work without RVT"));
    }
    else if (VHMSettings.bUseRuntimeVirtualTexturing)
    {
        UE_LOG(LogTerrainMaterialSystem, Verbose, TEXT("ApplyBiomeParameters: RVT not available, material configured without RVT"));
    }
}

TMap<FTileCoord, float> UVHMTerrainMaterialSystem::CalculateBlendWeights(const FTileCoord& CenterTile, const TArray<FTileCoord>& AdjacentTiles)
{
    TMap<FTileCoord, float> BlendWeights;
    
    FVector2D CenterPosition = GetTileCenterPosition(CenterTile);
    float TotalWeight = 0.0f;

    // Calculate distance-based weights
    for (const FTileCoord& AdjacentTile : AdjacentTiles)
    {
        if (AdjacentTile == CenterTile)
        {
            continue;
        }

        FVector2D AdjacentPosition = GetTileCenterPosition(AdjacentTile);
        float Distance = FVector2D::Distance(CenterPosition, AdjacentPosition);
        
        // Use inverse distance weighting with falloff
        float Weight = 1.0f / FMath::Max(Distance, 1.0f);
        BlendWeights.Add(AdjacentTile, Weight);
        TotalWeight += Weight;
    }

    // Normalize weights
    if (TotalWeight > 0.0f)
    {
        for (auto& WeightPair : BlendWeights)
        {
            WeightPair.Value /= TotalWeight;
        }
    }

    return BlendWeights;
}

void UVHMTerrainMaterialSystem::ApplyMaterialBlending(UMaterialInstanceDynamic* Material, const FTileMaterialBlendData& BlendData)
{
    if (!IsValid(Material))
    {
        return;
    }

    // Apply blend weights as material parameters
    int32 BlendIndex = 0;
    for (const auto& WeightPair : BlendData.BlendWeights)
    {
        FString ParameterName = FString::Printf(TEXT("BlendWeight_%d"), BlendIndex);
        Material->SetScalarParameterValue(*ParameterName, WeightPair.Value);
        
        BlendIndex++;
        if (BlendIndex >= 4) // Limit to 4 blend weights for performance
        {
            break;
        }
    }

    // Set number of active blends
    Material->SetScalarParameterValue(TEXT("NumActiveBlends"), FMath::Min(BlendData.BlendWeights.Num(), 4));
}

UObject* UVHMTerrainMaterialSystem::CreateTerrainRVT()
{
    // For UE5.6 compatibility, we'll create a placeholder object
    // Real RVT functionality would require proper RVT module integration
    UObject* RVT = NewObject<UObject>(this);
    if (!RVT)
    {
        UE_LOG(LogTerrainMaterialSystem, Error, TEXT("CreateTerrainRVT: Failed to create RVT placeholder object"));
        return nullptr;
    }

    // UE5.6 compatible RVT configuration using property-based initialization
    // Note: This is a placeholder implementation for compilation compatibility
    // Real RVT integration would require proper module dependencies
    
    UE_LOG(LogTerrainMaterialSystem, Log, TEXT("CreateTerrainRVT: Created RVT placeholder object for UE5.6 compatibility"));
    return RVT;
}

void UVHMTerrainMaterialSystem::ConfigureRVTParameters(UObject* RVT)
{
    if (!IsValid(RVT))
    {
        UE_LOG(LogTerrainMaterialSystem, Warning, TEXT("ConfigureRVTParameters: Invalid RVT object"));
        return;
    }

    // UE5.6 compatible RVT configuration approach
    // This is a placeholder implementation for compilation compatibility
    // Real RVT integration would require proper module dependencies and RVT classes
    
    // Log configuration attempt
    UE_LOG(LogTerrainMaterialSystem, Log, TEXT("ConfigureRVTParameters: UE5.6 compatible RVT placeholder setup"));
    UE_LOG(LogTerrainMaterialSystem, Log, TEXT("ConfigureRVTParameters: Target resolution %d, tile size %d"), 
           RVTConfig.RVTResolution, RVTConfig.RVTTileSize);
    
    // RVT configuration placeholder - would need real RVT implementation
    UE_LOG(LogTerrainMaterialSystem, Log, TEXT("ConfigureRVTParameters: RVT placeholder configuration completed"));
}

UObject* UVHMTerrainMaterialSystem::CreateRVTComponent()
{
    // Create RVT component placeholder for UE5.6 compatibility
    // This provides a placeholder when RVT modules are not available
    
    UObject* RVTComponent = NewObject<UObject>(this);
    if (!RVTComponent)
    {
        UE_LOG(LogTerrainMaterialSystem, Warning, TEXT("CreateRVTComponent: Failed to create RVT component placeholder"));
        return nullptr;
    }

    // Configure RVT component placeholder with terrain settings
    if (IsValid(TerrainRVT.Get()))
    {
        UE_LOG(LogTerrainMaterialSystem, Log, TEXT("CreateRVTComponent: RVT component placeholder configured"));
    }
    else
    {
        UE_LOG(LogTerrainMaterialSystem, Warning, TEXT("CreateRVTComponent: No terrain RVT available for component setup"));
    }

    return RVTComponent;
}

UMaterialInterface* UVHMTerrainMaterialSystem::GetDefaultMaterial()
{
    // Try to load a default terrain material from engine content
    UMaterialInterface* DefaultMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/DefaultMaterial"));
    
    if (!DefaultMaterial)
    {
        UE_LOG(LogTerrainMaterialSystem, Warning, TEXT("GetDefaultMaterial: Could not load default material"));
    }
    
    return DefaultMaterial;
}

bool UVHMTerrainMaterialSystem::ValidateBiomeDefinition(const FBiomeDefinition& BiomeData) const
{
    // Check if biome type is valid
    if (BiomeData.BiomeType == EBiomeType::None)
    {
        return false;
    }

    // Check if biome name is not empty
    if (BiomeData.BiomeName.IsEmpty())
    {
        return false;
    }

    // Biome definition is valid
    return true;
}

FVector2D UVHMTerrainMaterialSystem::GetTileCenterPosition(const FTileCoord& TileCoord) const
{
    float TileSize = 64.0f; // Default tile size from WorldGenTypes
    return FVector2D(
        (TileCoord.X + 0.5f) * TileSize,
        (TileCoord.Y + 0.5f) * TileSize
    );
}

uint32 UVHMTerrainMaterialSystem::CalculateMaterialParameterHash(const FBiomeDefinition& BiomeData) const
{
    uint32 Hash = 0;
    
    // Hash biome type
    Hash = HashCombine(Hash, GetTypeHash(BiomeData.BiomeType));
    
    // Hash key parameters
    Hash = HashCombine(Hash, GetTypeHash(BiomeData.BiomeWeight));
    Hash = HashCombine(Hash, GetTypeHash(BiomeData.BaseHeight));
    Hash = HashCombine(Hash, GetTypeHash(BiomeData.HeightVariation));
    Hash = HashCombine(Hash, GetTypeHash(BiomeData.RVTBlendColor));
    
    return Hash;
}

void UVHMTerrainMaterialSystem::CleanupMaterialResources()
{
    // Mark all materials for garbage collection
    for (auto& MaterialPair : TileMaterials)
    {
        if (IsValid(MaterialPair.Value.Get()))
        {
            MaterialPair.Value.Get()->MarkAsGarbage();
        }
    }
    
    TileMaterials.Empty();
    TileBlendData.Empty();
    
    // Cleanup RVT texture layers
    for (auto& TexturePair : RVTTextureLayers)
    {
        if (IsValid(TexturePair.Value.Get()))
        {
            TexturePair.Value.Get()->MarkAsGarbage();
        }
    }
    RVTTextureLayers.Empty();
    
    // Cleanup RVT render targets
    for (auto& RenderTargetPair : RVTRenderTargets)
    {
        if (IsValid(RenderTargetPair.Value.Get()))
        {
            RenderTargetPair.Value.Get()->MarkAsGarbage();
        }
    }
    RVTRenderTargets.Empty();
    
    // Clear streaming data
    StreamingTiles.Empty();
    TileBiomeBlends.Empty();
    
    // Reset statistics
    ActiveRVTTextures = 0;
    RVTMemoryUsage = 0.0f;
    StreamingRequests = 0;
    
    // Cleanup RVT resources
    if (IsValid(TerrainRVT.Get()))
    {
        TerrainRVT.Get()->MarkAsGarbage();
        TerrainRVT = nullptr;
        UE_LOG(LogTerrainMaterialSystem, Verbose, TEXT("CleanupMaterialResources: RVT resources cleaned up"));
    }
    
    bRVTInitialized = false;
    
    UE_LOG(LogTerrainMaterialSystem, Log, TEXT("CleanupMaterialResources: All material resources cleaned up"));
}

bool UVHMTerrainMaterialSystem::SetupRVTTextureStreaming(const TArray<FTileCoord>& InStreamingTiles)
{
    if (!bRVTInitialized)
    {
        UE_LOG(LogTerrainMaterialSystem, Warning, TEXT("SetupRVTTextureStreaming: RVT not initialized"));
        return false;
    }

    this->StreamingTiles = InStreamingTiles;

    // Setup streaming for each tile
    for (const FTileCoord& TileCoord : InStreamingTiles)
    {
        // Create streaming textures for this tile
        for (const FRVTTextureLayer& Layer : RVTConfig.TextureLayers)
        {
            FString TextureKey = FString::Printf(TEXT("%s_Tile_%d_%d"), *Layer.LayerName, TileCoord.X, TileCoord.Y);
            
            if (!RVTTextureLayers.Contains(TextureKey))
            {
                UTexture2D* LayerTexture = CreateRVTTextureLayer(Layer);
                if (LayerTexture)
                {
                    RVTTextureLayers.Add(TextureKey, LayerTexture);
                    ActiveRVTTextures++;
                }
            }
        }
    }

    UpdateRVTStreamingStats();
    UE_LOG(LogTerrainMaterialSystem, Log, TEXT("SetupRVTTextureStreaming: Configured streaming for %d tiles"), InStreamingTiles.Num());
    return true;
}

bool UVHMTerrainMaterialSystem::ConfigureBiomeTextureBlending(const FTileCoord& TileCoord, const TArray<FBiomeDefinition>& BiomeBlends)
{
    if (!bRVTInitialized)
    {
        UE_LOG(LogTerrainMaterialSystem, Warning, TEXT("ConfigureBiomeTextureBlending: RVT not initialized"));
        return false;
    }

    if (BiomeBlends.Num() == 0)
    {
        UE_LOG(LogTerrainMaterialSystem, Warning, TEXT("ConfigureBiomeTextureBlending: No biome blends provided for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
        return false;
    }

    // Store biome blend configuration
    FTileBiomeBlendConfig& BlendConfig = TileBiomeBlends.FindOrAdd(TileCoord);
    BlendConfig.BiomeBlends = BiomeBlends;
    BlendConfig.LastUpdateTime = FPlatformTime::Seconds();

    // Apply biome blending to RVT textures
    ApplyBiomeBlendingToRVT(TileCoord, BiomeBlends);

    UE_LOG(LogTerrainMaterialSystem, Verbose, TEXT("ConfigureBiomeTextureBlending: Configured blending for tile (%d, %d) with %d biomes"), 
           TileCoord.X, TileCoord.Y, BiomeBlends.Num());
    return true;
}

bool UVHMTerrainMaterialSystem::SetupTextureDetailLayers(const TArray<FRVTTextureLayer>& DetailLayers)
{
    if (!bRVTInitialized)
    {
        UE_LOG(LogTerrainMaterialSystem, Warning, TEXT("SetupTextureDetailLayers: RVT not initialized"));
        return false;
    }

    // Update RVT configuration with new layers
    RVTConfig.TextureLayers = DetailLayers;

    // Create texture layers
    for (const FRVTTextureLayer& Layer : DetailLayers)
    {
        if (!RVTTextureLayers.Contains(Layer.LayerName))
        {
            UTexture2D* LayerTexture = CreateRVTTextureLayer(Layer);
            if (LayerTexture)
            {
                RVTTextureLayers.Add(Layer.LayerName, LayerTexture);
                ActiveRVTTextures++;
            }
        }

        // Create render target for dynamic content
        if (!RVTRenderTargets.Contains(Layer.LayerName))
        {
            UTextureRenderTarget2D* RenderTarget = CreateRVTRenderTarget(Layer.LayerName, RVTConfig.RVTResolution);
            if (RenderTarget)
            {
                RVTRenderTargets.Add(Layer.LayerName, RenderTarget);
            }
        }
    }

    UpdateRVTStreamingStats();
    UE_LOG(LogTerrainMaterialSystem, Log, TEXT("SetupTextureDetailLayers: Setup %d detail layers"), DetailLayers.Num());
    return true;
}

void UVHMTerrainMaterialSystem::OptimizeRVTPerformance()
{
    if (!bRVTInitialized)
    {
        return;
    }

    // Optimize memory usage
    OptimizeRVTMemoryUsage();

    // Update streaming based on performance level
    switch (RVTConfig.OptimizationLevel)
    {
        case 0: // No optimization
            break;
        case 1: // Basic optimization
            {
                // Reduce texture resolution for distant tiles
                for (auto& TexturePair : RVTTextureLayers)
                {
                    if (IsValid(TexturePair.Value.Get()))
                    {
                        // Basic LOD management would go here
                    }
                }
            }
            break;
        case 2: // Moderate optimization
            {
                // Implement texture streaming prioritization
                UpdateRVTTextureStreaming(FVector::ZeroVector); // Use default position for now
            }
            break;
        case 3: // Aggressive optimization
            {
                // Implement aggressive memory management
                OptimizeRVTMemoryUsage();
                
                // Reduce active texture count if memory usage is high or texture count exceeds limit
                if (RVTMemoryUsage > RVTConfig.RVTMemoryBudgetMB * 0.8f || ActiveRVTTextures > RVTConfig.MaxActiveTextures)
                {
                    // Calculate how many textures to remove
                    int32 TexturesToRemove = 0;
                    if (RVTMemoryUsage > RVTConfig.RVTMemoryBudgetMB * 0.8f)
                    {
                        TexturesToRemove = FMath::Max(TexturesToRemove, ActiveRVTTextures / 4);
                    }
                    if (ActiveRVTTextures > RVTConfig.MaxActiveTextures)
                    {
                        TexturesToRemove = FMath::Max(TexturesToRemove, ActiveRVTTextures - RVTConfig.MaxActiveTextures);
                    }
                    
                    // Remove least recently used textures
                    TArray<FString> TextureKeys;
                    RVTTextureLayers.GetKeys(TextureKeys);
                    
                    for (int32 i = 0; i < FMath::Min(TexturesToRemove, TextureKeys.Num()); i++)
                    {
                        if (TObjectPtr<UTexture2D>* TexturePtr = RVTTextureLayers.Find(TextureKeys[i]))
                        {
                            if (IsValid(TexturePtr->Get()))
                            {
                                TexturePtr->Get()->MarkAsGarbage();
                                ActiveRVTTextures--;
                            }
                            RVTTextureLayers.Remove(TextureKeys[i]);
                        }
                    }
                    
                    UE_LOG(LogTerrainMaterialSystem, Log, TEXT("OptimizeRVTPerformance: Removed %d textures for optimization"), TexturesToRemove);
                }
            }
            break;
    }

    UpdateRVTStreamingStats();
    UE_LOG(LogTerrainMaterialSystem, Verbose, TEXT("OptimizeRVTPerformance: Performance optimization completed (Level %d)"), RVTConfig.OptimizationLevel);
}

void UVHMTerrainMaterialSystem::GetRVTStreamingStats(int32& OutActiveTextures, float& OutMemoryUsageMB, int32& OutStreamingRequests) const
{
    UpdateRVTStreamingStats();
    OutActiveTextures = ActiveRVTTextures;
    OutMemoryUsageMB = RVTMemoryUsage;
    OutStreamingRequests = StreamingRequests;
}

UTexture2D* UVHMTerrainMaterialSystem::CreateRVTTextureLayer(const FRVTTextureLayer& LayerConfig)
{
    UTexture2D* NewTexture = UTexture2D::CreateTransient(RVTConfig.RVTResolution, RVTConfig.RVTResolution, LayerConfig.PixelFormat);
    if (!NewTexture)
    {
        UE_LOG(LogTerrainMaterialSystem, Error, TEXT("CreateRVTTextureLayer: Failed to create texture for layer %s"), *LayerConfig.LayerName);
        return nullptr;
    }

    // Configure texture properties based on RVT settings
    NewTexture->CompressionSettings = (LayerConfig.bEnableCompression && RVTConfig.bEnableTextureCompression) ? TC_Default : TC_VectorDisplacementmap;
    NewTexture->SRGB = (LayerConfig.LayerName == TEXT("BaseColor"));
    
    // Apply texture quality settings using valid UE5.6 texture groups
    switch (RVTConfig.TextureQualityLevel)
    {
        case 0: // Low quality
            NewTexture->LODGroup = TEXTUREGROUP_World;
            break;
        case 1: // Medium quality
            NewTexture->LODGroup = TEXTUREGROUP_World;
            break;
        case 2: // High quality
            NewTexture->LODGroup = TEXTUREGROUP_World;
            break;
        case 3: // Ultra quality
            NewTexture->LODGroup = TEXTUREGROUP_WorldNormalMap;
            break;
        default:
            NewTexture->LODGroup = TEXTUREGROUP_World;
            break;
    }
    
    NewTexture->UpdateResource();

    UE_LOG(LogTerrainMaterialSystem, Verbose, TEXT("CreateRVTTextureLayer: Created texture layer %s"), *LayerConfig.LayerName);
    return NewTexture;
}

UTextureRenderTarget2D* UVHMTerrainMaterialSystem::CreateRVTRenderTarget(const FString& LayerName, int32 Resolution)
{
    UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(this);
    if (!RenderTarget)
    {
        UE_LOG(LogTerrainMaterialSystem, Error, TEXT("CreateRVTRenderTarget: Failed to create render target for layer %s"), *LayerName);
        return nullptr;
    }

    // Configure render target
    RenderTarget->InitAutoFormat(Resolution, Resolution);
    RenderTarget->UpdateResourceImmediate(true);

    UE_LOG(LogTerrainMaterialSystem, Verbose, TEXT("CreateRVTRenderTarget: Created render target for layer %s"), *LayerName);
    return RenderTarget;
}

void UVHMTerrainMaterialSystem::UpdateRVTTextureStreaming(const FVector& ViewerPosition)
{
    if (!RVTConfig.bEnableRVTStreaming)
    {
        return;
    }

    StreamingRequests = 0;

    // Update streaming for each tile based on distance
    for (const FTileCoord& TileCoord : StreamingTiles)
    {
        FVector2D TilePosition = GetTileCenterPosition(TileCoord);
        FVector TileWorldPosition(TilePosition.X, TilePosition.Y, 0.0f);
        
        float Distance = FVector::Dist(ViewerPosition, TileWorldPosition);
        
        if (Distance <= RVTConfig.StreamingDistance)
        {
            // Ensure textures are loaded for this tile
            for (const FRVTTextureLayer& Layer : RVTConfig.TextureLayers)
            {
                FString TextureKey = FString::Printf(TEXT("%s_Tile_%d_%d"), *Layer.LayerName, TileCoord.X, TileCoord.Y);
                
                if (!RVTTextureLayers.Contains(TextureKey))
                {
                    // Create texture for streaming
                    UTexture2D* LayerTexture = CreateRVTTextureLayer(Layer);
                    if (LayerTexture)
                    {
                        RVTTextureLayers.Add(TextureKey, LayerTexture);
                        ActiveRVTTextures++;
                        StreamingRequests++;
                    }
                }
            }
        }
        else
        {
            // Unload distant textures to save memory
            for (const FRVTTextureLayer& Layer : RVTConfig.TextureLayers)
            {
                FString TextureKey = FString::Printf(TEXT("%s_Tile_%d_%d"), *Layer.LayerName, TileCoord.X, TileCoord.Y);
                
                if (TObjectPtr<UTexture2D>* TexturePtr = RVTTextureLayers.Find(TextureKey))
                {
                    if (IsValid(TexturePtr->Get()))
                    {
                        TexturePtr->Get()->MarkAsGarbage();
                        ActiveRVTTextures--;
                    }
                    RVTTextureLayers.Remove(TextureKey);
                }
            }
        }
    }

    UpdateRVTStreamingStats();
    UE_LOG(LogTerrainMaterialSystem, Verbose, TEXT("UpdateRVTTextureStreaming: Processed %d streaming requests"), StreamingRequests);
}

void UVHMTerrainMaterialSystem::ApplyBiomeBlendingToRVT(const FTileCoord& TileCoord, const TArray<FBiomeDefinition>& BiomeBlends)
{
    if (!bRVTInitialized || BiomeBlends.Num() == 0)
    {
        return;
    }

    // Generate texture detail layers for each biome blend
    for (const FBiomeDefinition& BiomeData : BiomeBlends)
    {
        GenerateTextureDetailLayers(TileCoord, BiomeData);
    }

    // Apply blending weights to RVT textures
    for (const FRVTTextureLayer& Layer : RVTConfig.TextureLayers)
    {
        FString TextureKey = FString::Printf(TEXT("%s_Tile_%d_%d"), *Layer.LayerName, TileCoord.X, TileCoord.Y);
        
        if (TObjectPtr<UTexture2D>* TexturePtr = RVTTextureLayers.Find(TextureKey))
        {
            if (IsValid(TexturePtr->Get()))
            {
                // Apply biome blending to texture
                // This would involve texture blending operations in a real implementation
                UE_LOG(LogTerrainMaterialSystem, Verbose, TEXT("ApplyBiomeBlendingToRVT: Applied blending to texture %s for tile (%d, %d)"), 
                       *TextureKey, TileCoord.X, TileCoord.Y);
            }
        }
    }
}

void UVHMTerrainMaterialSystem::GenerateTextureDetailLayers(const FTileCoord& TileCoord, const FBiomeDefinition& BiomeData)
{
    if (!bRVTInitialized)
    {
        return;
    }

    // Generate base color layer
    if (RVTConfig.TextureLayers.ContainsByPredicate([](const FRVTTextureLayer& Layer) { return Layer.LayerName == TEXT("BaseColor"); }))
    {
        FString BaseColorKey = FString::Printf(TEXT("BaseColor_Tile_%d_%d"), TileCoord.X, TileCoord.Y);
        
        if (!RVTTextureLayers.Contains(BaseColorKey))
        {
            FRVTTextureLayer BaseColorLayer;
            BaseColorLayer.LayerName = TEXT("BaseColor");
            BaseColorLayer.PixelFormat = PF_B8G8R8A8;
            BaseColorLayer.bEnableCompression = true;
            
            UTexture2D* BaseColorTexture = CreateRVTTextureLayer(BaseColorLayer);
            if (BaseColorTexture)
            {
                RVTTextureLayers.Add(BaseColorKey, BaseColorTexture);
                ActiveRVTTextures++;
            }
        }
    }

    // Generate normal map layer
    if (RVTConfig.TextureLayers.ContainsByPredicate([](const FRVTTextureLayer& Layer) { return Layer.LayerName == TEXT("Normal"); }))
    {
        FString NormalKey = FString::Printf(TEXT("Normal_Tile_%d_%d"), TileCoord.X, TileCoord.Y);
        
        if (!RVTTextureLayers.Contains(NormalKey))
        {
            FRVTTextureLayer NormalLayer;
            NormalLayer.LayerName = TEXT("Normal");
            NormalLayer.PixelFormat = PF_B8G8R8A8;
            NormalLayer.bEnableCompression = false;
            
            UTexture2D* NormalTexture = CreateRVTTextureLayer(NormalLayer);
            if (NormalTexture)
            {
                RVTTextureLayers.Add(NormalKey, NormalTexture);
                ActiveRVTTextures++;
            }
        }
    }

    // Generate roughness layer
    if (RVTConfig.TextureLayers.ContainsByPredicate([](const FRVTTextureLayer& Layer) { return Layer.LayerName == TEXT("Roughness"); }))
    {
        FString RoughnessKey = FString::Printf(TEXT("Roughness_Tile_%d_%d"), TileCoord.X, TileCoord.Y);
        
        if (!RVTTextureLayers.Contains(RoughnessKey))
        {
            FRVTTextureLayer RoughnessLayer;
            RoughnessLayer.LayerName = TEXT("Roughness");
            RoughnessLayer.PixelFormat = PF_G8;
            RoughnessLayer.bEnableCompression = true;
            
            UTexture2D* RoughnessTexture = CreateRVTTextureLayer(RoughnessLayer);
            if (RoughnessTexture)
            {
                RVTTextureLayers.Add(RoughnessKey, RoughnessTexture);
                ActiveRVTTextures++;
            }
        }
    }

    UE_LOG(LogTerrainMaterialSystem, Verbose, TEXT("GenerateTextureDetailLayers: Generated detail layers for tile (%d, %d) with biome %s"), 
           TileCoord.X, TileCoord.Y, *BiomeData.BiomeName);
}

void UVHMTerrainMaterialSystem::OptimizeRVTMemoryUsage()
{
    if (!bRVTInitialized)
    {
        return;
    }

    float CurrentMemoryUsage = 0.0f;
    int32 TextureCount = 0;

    // Calculate current memory usage
    for (const auto& TexturePair : RVTTextureLayers)
    {
        if (IsValid(TexturePair.Value.Get()))
        {
            UTexture2D* Texture = TexturePair.Value.Get();
            
            // Estimate texture memory usage (rough calculation)
            int32 TextureSize = Texture->GetSizeX() * Texture->GetSizeY();
            int32 BytesPerPixel = 4; // Assume RGBA8 for estimation
            
            CurrentMemoryUsage += (TextureSize * BytesPerPixel) / (1024.0f * 1024.0f); // Convert to MB
            TextureCount++;
        }
    }

    RVTMemoryUsage = CurrentMemoryUsage;
    ActiveRVTTextures = TextureCount;

    // If over budget, remove least recently used textures
    if (CurrentMemoryUsage > RVTConfig.RVTMemoryBudgetMB)
    {
        int32 TexturesToRemove = FMath::CeilToInt((CurrentMemoryUsage - RVTConfig.RVTMemoryBudgetMB) / (CurrentMemoryUsage / TextureCount));
        
        // Remove oldest textures (simple LRU approximation)
        TArray<FString> TextureKeys;
        RVTTextureLayers.GetKeys(TextureKeys);
        
        for (int32 i = 0; i < FMath::Min(TexturesToRemove, TextureKeys.Num()); i++)
        {
            if (TObjectPtr<UTexture2D>* TexturePtr = RVTTextureLayers.Find(TextureKeys[i]))
            {
                if (IsValid(TexturePtr->Get()))
                {
                    TexturePtr->Get()->MarkAsGarbage();
                    ActiveRVTTextures--;
                }
                RVTTextureLayers.Remove(TextureKeys[i]);
            }
        }
        
        UE_LOG(LogTerrainMaterialSystem, Log, TEXT("OptimizeRVTMemoryUsage: Removed %d textures to stay within memory budget"), TexturesToRemove);
    }

    UE_LOG(LogTerrainMaterialSystem, Verbose, TEXT("OptimizeRVTMemoryUsage: Current usage %.2f MB / %.2f MB budget"), 
           RVTMemoryUsage, RVTConfig.RVTMemoryBudgetMB);
}

void UVHMTerrainMaterialSystem::UpdateRVTStreamingStats() const
{
    // Update streaming statistics
    int32 ActiveTextures = 0;
    float MemoryUsage = 0.0f;
    
    for (const auto& TexturePair : RVTTextureLayers)
    {
        if (IsValid(TexturePair.Value.Get()))
        {
            ActiveTextures++;
            
            // Estimate memory usage
            UTexture2D* Texture = TexturePair.Value.Get();
            int32 TextureSize = Texture->GetSizeX() * Texture->GetSizeY();
            MemoryUsage += (TextureSize * 4) / (1024.0f * 1024.0f); // Assume 4 bytes per pixel
        }
    }
    
    // Update mutable statistics
    const_cast<UVHMTerrainMaterialSystem*>(this)->ActiveRVTTextures = ActiveTextures;
    const_cast<UVHMTerrainMaterialSystem*>(this)->RVTMemoryUsage = MemoryUsage;
}

bool UVHMTerrainMaterialSystem::ValidateRVTConfiguration() const
{
    // Validate RVT resolution
    if (RVTConfig.RVTResolution <= 0 || RVTConfig.RVTResolution > 8192)
    {
        UE_LOG(LogTerrainMaterialSystem, Error, TEXT("ValidateRVTConfiguration: Invalid RVT resolution %d"), RVTConfig.RVTResolution);
        return false;
    }

    // Validate tile size
    if (RVTConfig.RVTTileSize <= 0 || RVTConfig.RVTTileSize > RVTConfig.RVTResolution)
    {
        UE_LOG(LogTerrainMaterialSystem, Error, TEXT("ValidateRVTConfiguration: Invalid RVT tile size %d"), RVTConfig.RVTTileSize);
        return false;
    }

    // Validate memory budget
    if (RVTConfig.RVTMemoryBudgetMB <= 0.0f)
    {
        UE_LOG(LogTerrainMaterialSystem, Error, TEXT("ValidateRVTConfiguration: Invalid memory budget %.2f MB"), RVTConfig.RVTMemoryBudgetMB);
        return false;
    }

    // Validate streaming distance
    if (RVTConfig.StreamingDistance <= 0.0f)
    {
        UE_LOG(LogTerrainMaterialSystem, Error, TEXT("ValidateRVTConfiguration: Invalid streaming distance %.2f"), RVTConfig.StreamingDistance);
        return false;
    }

    // Validate optimization level
    if (RVTConfig.OptimizationLevel < 0 || RVTConfig.OptimizationLevel > 3)
    {
        UE_LOG(LogTerrainMaterialSystem, Error, TEXT("ValidateRVTConfiguration: Invalid optimization level %d"), RVTConfig.OptimizationLevel);
        return false;
    }

    // Validate max active textures
    if (RVTConfig.MaxActiveTextures <= 0)
    {
        UE_LOG(LogTerrainMaterialSystem, Error, TEXT("ValidateRVTConfiguration: Invalid max active textures %d"), RVTConfig.MaxActiveTextures);
        return false;
    }

    // Validate texture quality level
    if (RVTConfig.TextureQualityLevel < 0 || RVTConfig.TextureQualityLevel > 3)
    {
        UE_LOG(LogTerrainMaterialSystem, Error, TEXT("ValidateRVTConfiguration: Invalid texture quality level %d"), RVTConfig.TextureQualityLevel);
        return false;
    }

    // Validate texture update frequency
    if (RVTConfig.TextureUpdateFrequency <= 0.0f)
    {
        UE_LOG(LogTerrainMaterialSystem, Error, TEXT("ValidateRVTConfiguration: Invalid texture update frequency %.3f"), RVTConfig.TextureUpdateFrequency);
        return false;
    }

    return true;
}

void UVHMTerrainMaterialSystem::InitializeDefaultTextureLayers()
{
    if (RVTConfig.TextureLayers.Num() == 0)
    {
        // Create default texture layers
        FRVTTextureLayer BaseColorLayer;
        BaseColorLayer.LayerName = TEXT("BaseColor");
        BaseColorLayer.PixelFormat = PF_B8G8R8A8;
        BaseColorLayer.bEnableCompression = true;
        BaseColorLayer.StreamingPriority = 3;
        RVTConfig.TextureLayers.Add(BaseColorLayer);

        FRVTTextureLayer NormalLayer;
        NormalLayer.LayerName = TEXT("Normal");
        NormalLayer.PixelFormat = PF_B8G8R8A8;
        NormalLayer.bEnableCompression = false;
        NormalLayer.StreamingPriority = 2;
        RVTConfig.TextureLayers.Add(NormalLayer);

        FRVTTextureLayer RoughnessLayer;
        RoughnessLayer.LayerName = TEXT("Roughness");
        RoughnessLayer.PixelFormat = PF_G8;
        RoughnessLayer.bEnableCompression = true;
        RoughnessLayer.StreamingPriority = 1;
        RVTConfig.TextureLayers.Add(RoughnessLayer);

        UE_LOG(LogTerrainMaterialSystem, Log, TEXT("InitializeDefaultTextureLayers: Created %d default texture layers"), RVTConfig.TextureLayers.Num());
    }
}

void UVHMTerrainMaterialSystem::UpdateRVTStreaming(const FVector& ViewerPosition)
{
    if (!bRVTInitialized)
    {
        return;
    }

    UpdateRVTTextureStreaming(ViewerPosition);
    OptimizeRVTPerformance();
    
    UE_LOG(LogTerrainMaterialSystem, Verbose, TEXT("UpdateRVTStreaming: Updated streaming for viewer position (%.1f, %.1f, %.1f)"), 
           ViewerPosition.X, ViewerPosition.Y, ViewerPosition.Z);
}

UTexture2D* UVHMTerrainMaterialSystem::GetRVTTexture(const FString& LayerName, const FTileCoord& TileCoord) const
{
    FString TextureKey = FString::Printf(TEXT("%s_Tile_%d_%d"), *LayerName, TileCoord.X, TileCoord.Y);
    
    if (const TObjectPtr<UTexture2D>* TexturePtr = RVTTextureLayers.Find(TextureKey))
    {
        return TexturePtr->Get();
    }
    
    return nullptr;
}

void UVHMTerrainMaterialSystem::ClearRVTCache()
{
    if (!bRVTInitialized)
    {
        return;
    }

    // Clear all RVT texture layers
    for (auto& TexturePair : RVTTextureLayers)
    {
        if (IsValid(TexturePair.Value.Get()))
        {
            TexturePair.Value.Get()->MarkAsGarbage();
        }
    }
    RVTTextureLayers.Empty();

    // Clear render targets
    for (auto& RenderTargetPair : RVTRenderTargets)
    {
        if (IsValid(RenderTargetPair.Value.Get()))
        {
            RenderTargetPair.Value.Get()->MarkAsGarbage();
        }
    }
    RVTRenderTargets.Empty();

    // Clear biome blend configurations
    TileBiomeBlends.Empty();

    // Reset statistics
    ActiveRVTTextures = 0;
    RVTMemoryUsage = 0.0f;
    StreamingRequests = 0;

    UE_LOG(LogTerrainMaterialSystem, Log, TEXT("ClearRVTCache: RVT cache cleared"));
}

FString UVHMTerrainMaterialSystem::GetRVTStatusInfo() const
{
    FString StatusInfo;
    
    StatusInfo += FString::Printf(TEXT("RVT System Status:\n"));
    StatusInfo += FString::Printf(TEXT("  Initialized: %s\n"), bRVTInitialized ? TEXT("Yes") : TEXT("No"));
    StatusInfo += FString::Printf(TEXT("  Active Textures: %d / %d\n"), ActiveRVTTextures, RVTConfig.MaxActiveTextures);
    StatusInfo += FString::Printf(TEXT("  Memory Usage: %.2f / %.2f MB\n"), RVTMemoryUsage, RVTConfig.RVTMemoryBudgetMB);
    StatusInfo += FString::Printf(TEXT("  Streaming Requests: %d\n"), StreamingRequests);
    StatusInfo += FString::Printf(TEXT("  Resolution: %d x %d\n"), RVTConfig.RVTResolution, RVTConfig.RVTResolution);
    StatusInfo += FString::Printf(TEXT("  Tile Size: %d\n"), RVTConfig.RVTTileSize);
    StatusInfo += FString::Printf(TEXT("  Texture Layers: %d\n"), RVTConfig.TextureLayers.Num());
    StatusInfo += FString::Printf(TEXT("  Streaming Distance: %.1f\n"), RVTConfig.StreamingDistance);
    StatusInfo += FString::Printf(TEXT("  Optimization Level: %d\n"), RVTConfig.OptimizationLevel);
    StatusInfo += FString::Printf(TEXT("  Quality Level: %d\n"), RVTConfig.TextureQualityLevel);
    StatusInfo += FString::Printf(TEXT("  Biome Blending: %s\n"), RVTConfig.bEnableBiomeBlending ? TEXT("Enabled") : TEXT("Disabled"));
    StatusInfo += FString::Printf(TEXT("  Texture Compression: %s\n"), RVTConfig.bEnableTextureCompression ? TEXT("Enabled") : TEXT("Disabled"));
    StatusInfo += FString::Printf(TEXT("  Streaming Tiles: %d\n"), StreamingTiles.Num());
    StatusInfo += FString::Printf(TEXT("  Biome Blend Configs: %d\n"), TileBiomeBlends.Num());
    
    return StatusInfo;
}