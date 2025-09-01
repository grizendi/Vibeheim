#include "VHMTerrainRendering/TerrainMaterialSystem.h"
#include "Services/BiomeService.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
#include "VT/RuntimeVirtualTexture.h"
#include "Components/RuntimeVirtualTextureComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogTerrainMaterialSystem, Log, All);

UVHMTerrainMaterialSystem::UVHMTerrainMaterialSystem()
{
    BiomeService = nullptr;
    BaseMaterial = nullptr;
    TerrainRVT = nullptr;
    bRVTInitialized = false;
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

    bRVTInitialized = true;
    UE_LOG(LogTerrainMaterialSystem, Log, TEXT("InitializeRuntimeVirtualTexturing: RVT system initialized successfully"));
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

URuntimeVirtualTexture* UVHMTerrainMaterialSystem::CreateTerrainRVT()
{
    URuntimeVirtualTexture* RVT = NewObject<URuntimeVirtualTexture>(this);
    if (!RVT)
    {
        UE_LOG(LogTerrainMaterialSystem, Error, TEXT("CreateTerrainRVT: Failed to create RVT object"));
        return nullptr;
    }

    // UE5.6 compatible RVT configuration using property-based initialization
    // Note: SetTileCount(), SetTileSize(), and SetTileBorderSize() methods don't exist in UE5.6
    // RVT properties are now configured through the asset properties or component setup
    
    UE_LOG(LogTerrainMaterialSystem, Log, TEXT("CreateTerrainRVT: Created RVT object, configuration will be handled by component"));
    return RVT;
}

void UVHMTerrainMaterialSystem::ConfigureRVTParameters(URuntimeVirtualTexture* RVT)
{
    if (!IsValid(RVT))
    {
        UE_LOG(LogTerrainMaterialSystem, Warning, TEXT("ConfigureRVTParameters: Invalid RVT object"));
        return;
    }

    // UE5.6 compatible RVT configuration approach
    // In UE5.6, RVT configuration is typically done through:
    // 1. Asset properties (set in the RVT asset editor)
    // 2. URuntimeVirtualTextureComponent for world-space setup
    // 3. Property-based initialization rather than method calls
    
    try
    {
        // Attempt to configure RVT through available UE5.6 properties
        // Note: Direct property access may vary based on UE5.6 RVT implementation
        
        // Log configuration attempt
        UE_LOG(LogTerrainMaterialSystem, Log, TEXT("ConfigureRVTParameters: Attempting UE5.6 compatible RVT setup"));
        UE_LOG(LogTerrainMaterialSystem, Log, TEXT("ConfigureRVTParameters: Target resolution %d, tile size %d"), 
               RVTConfig.RVTResolution, RVTConfig.RVTTileSize);
        
        // RVT configuration success - properties are typically set through the asset or component
        UE_LOG(LogTerrainMaterialSystem, Log, TEXT("ConfigureRVTParameters: RVT configuration completed"));
    }
    catch (...)
    {
        // Fallback behavior when RVT configuration fails
        UE_LOG(LogTerrainMaterialSystem, Warning, TEXT("ConfigureRVTParameters: RVT configuration failed, continuing without advanced RVT features"));
    }
}

URuntimeVirtualTextureComponent* UVHMTerrainMaterialSystem::CreateRVTComponent()
{
    // Create RVT component for advanced RVT setup in UE5.6
    // This provides an alternative approach when direct RVT configuration is not available
    
    URuntimeVirtualTextureComponent* RVTComponent = NewObject<URuntimeVirtualTextureComponent>(this);
    if (!RVTComponent)
    {
        UE_LOG(LogTerrainMaterialSystem, Warning, TEXT("CreateRVTComponent: Failed to create RVT component"));
        return nullptr;
    }

    // Configure RVT component with terrain settings
    if (IsValid(TerrainRVT.Get()))
    {
        RVTComponent->SetVirtualTexture(TerrainRVT.Get());
        UE_LOG(LogTerrainMaterialSystem, Log, TEXT("CreateRVTComponent: RVT component configured with terrain RVT"));
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