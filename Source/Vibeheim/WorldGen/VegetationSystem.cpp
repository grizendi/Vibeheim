#include "VegetationSystem.h"
#include "Engine/Engine.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Math/UnrealMathUtility.h"

DEFINE_LOG_CATEGORY_STATIC(LogVegetationSystem, Log, All);

void FVegetationSystem::Initialize(const FWorldGenSettings& InSettings, const FBiomeSystem* InBiomeSystem)
{
    Settings = InSettings;
    BiomeSystem = InBiomeSystem;
    
    if (!BiomeSystem)
    {
        UE_LOG(LogVegetationSystem, Error, TEXT("BiomeSystem is null during VegetationSystem initialization"));
        return;
    }
    
    // Initialize noise generator with vegetation-specific feature tag
    NoiseGenerator.Initialize(InSettings);
    
    // Initialize default vegetation profiles for each biome
    InitializeDefaultVegetationProfiles();
    
    // Ensure vegetation data directory exists
    EnsureVegetationDataDirectory();
    
    UE_LOG(LogVegetationSystem, Log, TEXT("VegetationSystem initialized with seed %llu"), InSettings.Seed);
}

const FVegetationProfile& FVegetationSystem::GetVegetationDataForBiome(EBiomeType BiomeType) const
{
    if (const FVegetationProfile* Profile = BiomeVegetationProfiles.Find(BiomeType))
    {
        return *Profile;
    }
    
    // Return empty profile if biome not found
    static FVegetationProfile EmptyProfile;
    UE_LOG(LogVegetationSystem, Warning, TEXT("No vegetation profile found for biome type %d"), static_cast<int32>(BiomeType));
    return EmptyProfile;
}

FVegetationDensity FVegetationSystem::CalculateVegetationDensity(float WorldX, float WorldY, float TerrainHeight, const FIntVector& ChunkCoord) const
{
    FVegetationDensity Result;
    
    if (!BiomeSystem)
    {
        UE_LOG(LogVegetationSystem, Warning, TEXT("BiomeSystem is null, returning zero vegetation density"));
        return Result;
    }
    
    // Evaluate biome at this location
    FBiomeEvaluation BiomeEval = BiomeSystem->EvaluateBiome(WorldX, WorldY, TerrainHeight, ChunkCoord);
    Result.DominantBiome = BiomeEval.BiomeWeights.DominantBiome;
    
    // Calculate base vegetation density using noise
    float BaseDensity = CalculateBaseVegetationDensity(WorldX, WorldY, ChunkCoord);
    
    // Apply height modifiers
    BaseDensity = ApplyHeightModifiers(BaseDensity, TerrainHeight, Result.DominantBiome);
    
    // Get vegetation profile for dominant biome
    const FVegetationProfile& VegProfile = GetVegetationDataForBiome(Result.DominantBiome);
    
    // Calculate specific density types
    Result.TreeDensity = BaseDensity * VegProfile.TreeDensity;
    Result.FoliageDensity = BaseDensity * VegProfile.FoliageDensity;
    Result.ResourceDensity = BaseDensity * VegProfile.ResourceDensity;
    Result.OverallDensity = (Result.TreeDensity + Result.FoliageDensity + Result.ResourceDensity) / 3.0f;
    
    // Clamp all values to valid range
    Result.OverallDensity = FMath::Clamp(Result.OverallDensity, 0.0f, 1.0f);
    Result.TreeDensity = FMath::Clamp(Result.TreeDensity, 0.0f, 1.0f);
    Result.FoliageDensity = FMath::Clamp(Result.FoliageDensity, 0.0f, 1.0f);
    Result.ResourceDensity = FMath::Clamp(Result.ResourceDensity, 0.0f, 1.0f);
    
    return Result;
}

FVegetationAvailability FVegetationSystem::GetVegetationAvailability(float WorldX, float WorldY, float TerrainHeight, const FIntVector& ChunkCoord) const
{
    FVegetationAvailability Result;
    
    if (!BiomeSystem)
    {
        UE_LOG(LogVegetationSystem, Warning, TEXT("BiomeSystem is null, returning empty vegetation availability"));
        return Result;
    }
    
    // Evaluate biome at this location
    FBiomeEvaluation BiomeEval = BiomeSystem->EvaluateBiome(WorldX, WorldY, TerrainHeight, ChunkCoord);
    
    // Blend vegetation profiles based on biome weights
    Result.BlendedProfile = BlendVegetationProfiles(BiomeEval.BiomeWeights);
    
    // Copy blended data to availability arrays
    Result.AvailableTreeSpecies = Result.BlendedProfile.TreeSpecies;
    Result.AvailableFoliageTypes = Result.BlendedProfile.FoliageTypes;
    Result.AvailableResources = Result.BlendedProfile.HarvestableResources;
    
    return Result;
}

FChunkVegetationData FVegetationSystem::GenerateChunkVegetationData(const FIntVector& ChunkCoord, int32 SampleResolution) const
{
    FChunkVegetationData Result;
    Result.ChunkCoord = ChunkCoord;
    Result.GenerationTimestamp = FDateTime::Now();
    Result.WorldGenVersion = Settings.WorldGenVersion;
    Result.GenerationSeed = Settings.Seed;
    
    // Sample vegetation density at regular intervals within the chunk
    Result.DensityMap = SampleChunkVegetationDensity(ChunkCoord, SampleResolution);
    
    // Get vegetation availability for the center of the chunk
    FVector2D ChunkCenter = ChunkCoordToWorldPosition(ChunkCoord);
    Result.VegetationAvailability = GetVegetationAvailability(ChunkCenter.X, ChunkCenter.Y, 0.0f, ChunkCoord);
    
    UE_LOG(LogVegetationSystem, Verbose, TEXT("Generated vegetation data for chunk (%d, %d, %d) with %d density samples"), 
        ChunkCoord.X, ChunkCoord.Y, ChunkCoord.Z, Result.DensityMap.Num());
    
    return Result;
}

bool FVegetationSystem::SaveChunkVegetationData(const FChunkVegetationData& ChunkVegetationData) const
{
    FString FilePath = GetChunkVegetationDataFilePath(ChunkVegetationData.ChunkCoord);
    
    // Create JSON object for serialization
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
    
    // Basic chunk info
    JsonObject->SetNumberField(TEXT("ChunkX"), ChunkVegetationData.ChunkCoord.X);
    JsonObject->SetNumberField(TEXT("ChunkY"), ChunkVegetationData.ChunkCoord.Y);
    JsonObject->SetNumberField(TEXT("ChunkZ"), ChunkVegetationData.ChunkCoord.Z);
    JsonObject->SetStringField(TEXT("GenerationTimestamp"), ChunkVegetationData.GenerationTimestamp.ToString());
    JsonObject->SetNumberField(TEXT("WorldGenVersion"), ChunkVegetationData.WorldGenVersion);
    JsonObject->SetNumberField(TEXT("GenerationSeed"), static_cast<double>(ChunkVegetationData.GenerationSeed));
    
    // Serialize density map
    TArray<TSharedPtr<FJsonValue>> DensityArray;
    for (const FVegetationDensity& Density : ChunkVegetationData.DensityMap)
    {
        TSharedPtr<FJsonObject> DensityObj = MakeShareable(new FJsonObject);
        DensityObj->SetNumberField(TEXT("OverallDensity"), Density.OverallDensity);
        DensityObj->SetNumberField(TEXT("TreeDensity"), Density.TreeDensity);
        DensityObj->SetNumberField(TEXT("FoliageDensity"), Density.FoliageDensity);
        DensityObj->SetNumberField(TEXT("ResourceDensity"), Density.ResourceDensity);
        DensityObj->SetNumberField(TEXT("DominantBiome"), static_cast<int32>(Density.DominantBiome));
        
        DensityArray.Add(MakeShareable(new FJsonValueObject(DensityObj)));
    }
    JsonObject->SetArrayField(TEXT("DensityMap"), DensityArray);
    
    // Serialize vegetation availability (simplified - just store the blended profile)
    TSharedPtr<FJsonObject> VegAvailObj = MakeShareable(new FJsonObject);
    VegAvailObj->SetNumberField(TEXT("TreeDensity"), ChunkVegetationData.VegetationAvailability.BlendedProfile.TreeDensity);
    VegAvailObj->SetNumberField(TEXT("FoliageDensity"), ChunkVegetationData.VegetationAvailability.BlendedProfile.FoliageDensity);
    VegAvailObj->SetNumberField(TEXT("ResourceDensity"), ChunkVegetationData.VegetationAvailability.BlendedProfile.ResourceDensity);
    JsonObject->SetObjectField(TEXT("VegetationAvailability"), VegAvailObj);
    
    // Write to file
    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
    
    bool bSuccess = FFileHelper::SaveStringToFile(OutputString, *FilePath);
    if (!bSuccess)
    {
        UE_LOG(LogVegetationSystem, Error, TEXT("Failed to save vegetation data to file: %s"), *FilePath);
    }
    
    return bSuccess;
}

bool FVegetationSystem::LoadChunkVegetationData(const FIntVector& ChunkCoord, FChunkVegetationData& OutChunkVegetationData) const
{
    FString FilePath = GetChunkVegetationDataFilePath(ChunkCoord);
    
    if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*FilePath))
    {
        return false;
    }
    
    FString FileContent;
    if (!FFileHelper::LoadFileToString(FileContent, *FilePath))
    {
        UE_LOG(LogVegetationSystem, Error, TEXT("Failed to load vegetation data from file: %s"), *FilePath);
        return false;
    }
    
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FileContent);
    
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        UE_LOG(LogVegetationSystem, Error, TEXT("Failed to parse vegetation data JSON from file: %s"), *FilePath);
        return false;
    }
    
    // Parse basic chunk info
    OutChunkVegetationData.ChunkCoord.X = JsonObject->GetIntegerField(TEXT("ChunkX"));
    OutChunkVegetationData.ChunkCoord.Y = JsonObject->GetIntegerField(TEXT("ChunkY"));
    OutChunkVegetationData.ChunkCoord.Z = JsonObject->GetIntegerField(TEXT("ChunkZ"));
    
    FString TimestampStr = JsonObject->GetStringField(TEXT("GenerationTimestamp"));
    FDateTime::Parse(TimestampStr, OutChunkVegetationData.GenerationTimestamp);
    
    OutChunkVegetationData.WorldGenVersion = JsonObject->GetIntegerField(TEXT("WorldGenVersion"));
    OutChunkVegetationData.GenerationSeed = static_cast<uint64>(JsonObject->GetNumberField(TEXT("GenerationSeed")));
    
    // Parse density map
    const TArray<TSharedPtr<FJsonValue>>* DensityArray;
    if (JsonObject->TryGetArrayField(TEXT("DensityMap"), DensityArray))
    {
        OutChunkVegetationData.DensityMap.Empty();
        for (const TSharedPtr<FJsonValue>& DensityValue : *DensityArray)
        {
            const TSharedPtr<FJsonObject>* DensityObj;
            if (DensityValue->TryGetObject(DensityObj))
            {
                FVegetationDensity Density;
                Density.OverallDensity = (*DensityObj)->GetNumberField(TEXT("OverallDensity"));
                Density.TreeDensity = (*DensityObj)->GetNumberField(TEXT("TreeDensity"));
                Density.FoliageDensity = (*DensityObj)->GetNumberField(TEXT("FoliageDensity"));
                Density.ResourceDensity = (*DensityObj)->GetNumberField(TEXT("ResourceDensity"));
                Density.DominantBiome = static_cast<EBiomeType>((*DensityObj)->GetIntegerField(TEXT("DominantBiome")));
                
                OutChunkVegetationData.DensityMap.Add(Density);
            }
        }
    }
    
    // Parse vegetation availability (simplified)
    const TSharedPtr<FJsonObject>* VegAvailObj;
    if (JsonObject->TryGetObjectField(TEXT("VegetationAvailability"), VegAvailObj))
    {
        OutChunkVegetationData.VegetationAvailability.BlendedProfile.TreeDensity = (*VegAvailObj)->GetNumberField(TEXT("TreeDensity"));
        OutChunkVegetationData.VegetationAvailability.BlendedProfile.FoliageDensity = (*VegAvailObj)->GetNumberField(TEXT("FoliageDensity"));
        OutChunkVegetationData.VegetationAvailability.BlendedProfile.ResourceDensity = (*VegAvailObj)->GetNumberField(TEXT("ResourceDensity"));
    }
    
    return true;
}

bool FVegetationSystem::HasChunkVegetationData(const FIntVector& ChunkCoord) const
{
    FString FilePath = GetChunkVegetationDataFilePath(ChunkCoord);
    return FPlatformFileManager::Get().GetPlatformFile().FileExists(*FilePath);
}

bool FVegetationSystem::ClearChunkVegetationData(const FIntVector& ChunkCoord) const
{
    FString FilePath = GetChunkVegetationDataFilePath(ChunkCoord);
    
    if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*FilePath))
    {
        return true; // Already cleared
    }
    
    bool bSuccess = FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*FilePath);
    if (!bSuccess)
    {
        UE_LOG(LogVegetationSystem, Error, TEXT("Failed to delete vegetation data file: %s"), *FilePath);
    }
    
    return bSuccess;
}

float FVegetationSystem::GetResourceAvailability(const FString& ResourceName, float WorldX, float WorldY, float TerrainHeight, const FIntVector& ChunkCoord) const
{
    FVegetationAvailability VegAvail = GetVegetationAvailability(WorldX, WorldY, TerrainHeight, ChunkCoord);
    
    // Search for the resource in available resources
    for (const FResourceData& Resource : VegAvail.AvailableResources)
    {
        if (Resource.ResourceName.Equals(ResourceName, ESearchCase::IgnoreCase))
        {
            return Resource.SpawnRate;
        }
    }
    
    return 0.0f; // Resource not available at this location
}

TArray<FResourceData> FVegetationSystem::GetAvailableResources(float WorldX, float WorldY, float TerrainHeight, const FIntVector& ChunkCoord) const
{
    FVegetationAvailability VegAvail = GetVegetationAvailability(WorldX, WorldY, TerrainHeight, ChunkCoord);
    return VegAvail.AvailableResources;
}

void FVegetationSystem::InitializeDefaultVegetationProfiles()
{
    // Initialize vegetation profiles for each biome type
    BiomeVegetationProfiles.Empty();
    
    BiomeVegetationProfiles.Add(EBiomeType::Meadows, CreateMeadowsVegetationProfile());
    BiomeVegetationProfiles.Add(EBiomeType::BlackForest, CreateBlackForestVegetationProfile());
    BiomeVegetationProfiles.Add(EBiomeType::Swamp, CreateSwampVegetationProfile());
    BiomeVegetationProfiles.Add(EBiomeType::Mountains, CreateMountainsVegetationProfile());
    BiomeVegetationProfiles.Add(EBiomeType::Ocean, CreateOceanVegetationProfile());
    
    UE_LOG(LogVegetationSystem, Log, TEXT("Initialized %d vegetation profiles"), BiomeVegetationProfiles.Num());
}

FVegetationProfile FVegetationSystem::CreateMeadowsVegetationProfile() const
{
    FVegetationProfile Profile;
    Profile.TreeDensity = 0.6f;
    Profile.FoliageDensity = 0.8f;
    Profile.ResourceDensity = 0.7f;
    
    // Tree species for Meadows
    FTreeSpeciesData OakTree;
    OakTree.SpeciesName = TEXT("Oak");
    OakTree.SpawnProbability = 0.7f;
    OakTree.SizeRange = FVector2D(0.8f, 1.2f);
    OakTree.HarvestableResources = {TEXT("Wood"), TEXT("Oak Wood")};
    Profile.TreeSpecies.Add(OakTree);
    
    FTreeSpeciesData BirchTree;
    BirchTree.SpeciesName = TEXT("Birch");
    BirchTree.SpawnProbability = 0.3f;
    BirchTree.SizeRange = FVector2D(0.9f, 1.1f);
    BirchTree.HarvestableResources = {TEXT("Wood"), TEXT("Birch Wood")};
    Profile.TreeSpecies.Add(BirchTree);
    
    // Foliage types for Meadows
    FFoliageTypeData Grass;
    Grass.FoliageTypeName = TEXT("Meadow Grass");
    Grass.SpawnProbability = 0.9f;
    Grass.DensityMultiplier = 1.2f;
    Profile.FoliageTypes.Add(Grass);
    
    FFoliageTypeData Wildflowers;
    Wildflowers.FoliageTypeName = TEXT("Wildflowers");
    Wildflowers.SpawnProbability = 0.4f;
    Wildflowers.DensityMultiplier = 0.8f;
    Profile.FoliageTypes.Add(Wildflowers);
    
    // Harvestable resources for Meadows
    FResourceData Berries;
    Berries.ResourceName = TEXT("Berries");
    Berries.ResourceType = TEXT("Food");
    Berries.SpawnRate = 0.6f;
    Berries.MinQuantity = 1;
    Berries.MaxQuantity = 3;
    Profile.HarvestableResources.Add(Berries);
    
    FResourceData Herbs;
    Herbs.ResourceName = TEXT("Herbs");
    Herbs.ResourceType = TEXT("Crafting");
    Herbs.SpawnRate = 0.5f;
    Herbs.MinQuantity = 1;
    Herbs.MaxQuantity = 2;
    Profile.HarvestableResources.Add(Herbs);
    
    return Profile;
}

FVegetationProfile FVegetationSystem::CreateBlackForestVegetationProfile() const
{
    FVegetationProfile Profile;
    Profile.TreeDensity = 1.2f;
    Profile.FoliageDensity = 0.9f;
    Profile.ResourceDensity = 0.8f;
    
    // Tree species for BlackForest
    FTreeSpeciesData PineTree;
    PineTree.SpeciesName = TEXT("Pine");
    PineTree.SpawnProbability = 0.6f;
    PineTree.SizeRange = FVector2D(1.0f, 1.4f);
    PineTree.HarvestableResources = {TEXT("Wood"), TEXT("Pine Wood"), TEXT("Resin")};
    Profile.TreeSpecies.Add(PineTree);
    
    FTreeSpeciesData SpruceTree;
    SpruceTree.SpeciesName = TEXT("Spruce");
    SpruceTree.SpawnProbability = 0.4f;
    SpruceTree.SizeRange = FVector2D(1.1f, 1.3f);
    SpruceTree.HarvestableResources = {TEXT("Wood"), TEXT("Spruce Wood")};
    Profile.TreeSpecies.Add(SpruceTree);
    
    // Foliage types for BlackForest
    FFoliageTypeData Ferns;
    Ferns.FoliageTypeName = TEXT("Forest Ferns");
    Ferns.SpawnProbability = 0.8f;
    Ferns.DensityMultiplier = 1.1f;
    Profile.FoliageTypes.Add(Ferns);
    
    FFoliageTypeData Mushrooms;
    Mushrooms.FoliageTypeName = TEXT("Forest Mushrooms");
    Mushrooms.SpawnProbability = 0.3f;
    Mushrooms.DensityMultiplier = 0.6f;
    Profile.FoliageTypes.Add(Mushrooms);
    
    // Harvestable resources for BlackForest
    FResourceData MushroomResource;
    MushroomResource.ResourceName = TEXT("Mushrooms");
    MushroomResource.ResourceType = TEXT("Food");
    MushroomResource.SpawnRate = 0.4f;
    MushroomResource.MinQuantity = 1;
    MushroomResource.MaxQuantity = 2;
    Profile.HarvestableResources.Add(MushroomResource);
    
    FResourceData DarkWood;
    DarkWood.ResourceName = TEXT("Dark Wood");
    DarkWood.ResourceType = TEXT("Crafting");
    DarkWood.SpawnRate = 0.7f;
    DarkWood.MinQuantity = 2;
    DarkWood.MaxQuantity = 4;
    Profile.HarvestableResources.Add(DarkWood);
    
    return Profile;
}

FVegetationProfile FVegetationSystem::CreateSwampVegetationProfile() const
{
    FVegetationProfile Profile;
    Profile.TreeDensity = 0.4f;
    Profile.FoliageDensity = 1.0f;
    Profile.ResourceDensity = 0.6f;
    
    // Tree species for Swamp
    FTreeSpeciesData WillowTree;
    WillowTree.SpeciesName = TEXT("Willow");
    WillowTree.SpawnProbability = 0.8f;
    WillowTree.SizeRange = FVector2D(0.7f, 1.0f);
    WillowTree.HarvestableResources = {TEXT("Wood"), TEXT("Willow Wood")};
    Profile.TreeSpecies.Add(WillowTree);
    
    // Foliage types for Swamp
    FFoliageTypeData Reeds;
    Reeds.FoliageTypeName = TEXT("Swamp Reeds");
    Reeds.SpawnProbability = 0.9f;
    Reeds.DensityMultiplier = 1.3f;
    Profile.FoliageTypes.Add(Reeds);
    
    FFoliageTypeData Moss;
    Moss.FoliageTypeName = TEXT("Swamp Moss");
    Moss.SpawnProbability = 0.7f;
    Moss.DensityMultiplier = 1.0f;
    Profile.FoliageTypes.Add(Moss);
    
    // Harvestable resources for Swamp
    FResourceData SwampHerbs;
    SwampHerbs.ResourceName = TEXT("Swamp Herbs");
    SwampHerbs.ResourceType = TEXT("Alchemy");
    SwampHerbs.SpawnRate = 0.5f;
    SwampHerbs.MinQuantity = 1;
    SwampHerbs.MaxQuantity = 2;
    Profile.HarvestableResources.Add(SwampHerbs);
    
    FResourceData Peat;
    Peat.ResourceName = TEXT("Peat");
    Peat.ResourceType = TEXT("Fuel");
    Peat.SpawnRate = 0.8f;
    Peat.MinQuantity = 2;
    Peat.MaxQuantity = 5;
    Profile.HarvestableResources.Add(Peat);
    
    return Profile;
}

FVegetationProfile FVegetationSystem::CreateMountainsVegetationProfile() const
{
    FVegetationProfile Profile;
    Profile.TreeDensity = 0.2f;
    Profile.FoliageDensity = 0.3f;
    Profile.ResourceDensity = 0.4f;
    
    // Tree species for Mountains (sparse)
    FTreeSpeciesData AlpineFir;
    AlpineFir.SpeciesName = TEXT("Alpine Fir");
    AlpineFir.SpawnProbability = 0.5f;
    AlpineFir.SizeRange = FVector2D(0.6f, 0.9f);
    AlpineFir.HarvestableResources = {TEXT("Wood"), TEXT("Alpine Wood")};
    Profile.TreeSpecies.Add(AlpineFir);
    
    // Foliage types for Mountains
    FFoliageTypeData AlpineGrass;
    AlpineGrass.FoliageTypeName = TEXT("Alpine Grass");
    AlpineGrass.SpawnProbability = 0.6f;
    AlpineGrass.DensityMultiplier = 0.5f;
    Profile.FoliageTypes.Add(AlpineGrass);
    
    FFoliageTypeData HardyShrubs;
    HardyShrubs.FoliageTypeName = TEXT("Hardy Shrubs");
    HardyShrubs.SpawnProbability = 0.4f;
    HardyShrubs.DensityMultiplier = 0.4f;
    Profile.FoliageTypes.Add(HardyShrubs);
    
    // Harvestable resources for Mountains
    FResourceData Stone;
    Stone.ResourceName = TEXT("Stone");
    Stone.ResourceType = TEXT("Building");
    Stone.SpawnRate = 0.9f;
    Stone.MinQuantity = 3;
    Stone.MaxQuantity = 6;
    Profile.HarvestableResources.Add(Stone);
    
    FResourceData RareHerbs;
    RareHerbs.ResourceName = TEXT("Rare Mountain Herbs");
    RareHerbs.ResourceType = TEXT("Alchemy");
    RareHerbs.SpawnRate = 0.2f;
    RareHerbs.MinQuantity = 1;
    RareHerbs.MaxQuantity = 1;
    Profile.HarvestableResources.Add(RareHerbs);
    
    return Profile;
}

FVegetationProfile FVegetationSystem::CreateOceanVegetationProfile() const
{
    FVegetationProfile Profile;
    Profile.TreeDensity = 0.0f;
    Profile.FoliageDensity = 0.5f;
    Profile.ResourceDensity = 0.3f;
    
    // No trees in ocean
    
    // Foliage types for Ocean (aquatic vegetation)
    FFoliageTypeData Kelp;
    Kelp.FoliageTypeName = TEXT("Kelp");
    Kelp.SpawnProbability = 0.6f;
    Kelp.DensityMultiplier = 0.8f;
    Profile.FoliageTypes.Add(Kelp);
    
    FFoliageTypeData Seaweed;
    Seaweed.FoliageTypeName = TEXT("Seaweed");
    Seaweed.SpawnProbability = 0.4f;
    Seaweed.DensityMultiplier = 0.6f;
    Profile.FoliageTypes.Add(Seaweed);
    
    // Harvestable resources for Ocean
    FResourceData Driftwood;
    Driftwood.ResourceName = TEXT("Driftwood");
    Driftwood.ResourceType = TEXT("Wood");
    Driftwood.SpawnRate = 0.3f;
    Driftwood.MinQuantity = 1;
    Driftwood.MaxQuantity = 2;
    Profile.HarvestableResources.Add(Driftwood);
    
    FResourceData Shells;
    Shells.ResourceName = TEXT("Shells");
    Shells.ResourceType = TEXT("Crafting");
    Shells.SpawnRate = 0.5f;
    Shells.MinQuantity = 1;
    Shells.MaxQuantity = 3;
    Profile.HarvestableResources.Add(Shells);
    
    return Profile;
}

FVegetationProfile FVegetationSystem::BlendVegetationProfiles(const FBiomeWeights& BiomeWeights) const
{
    FVegetationProfile BlendedProfile;
    
    // Initialize blended values
    BlendedProfile.TreeDensity = 0.0f;
    BlendedProfile.FoliageDensity = 0.0f;
    BlendedProfile.ResourceDensity = 0.0f;
    
    // Blend density values based on biome weights
    for (int32 i = 0; i < static_cast<int32>(EBiomeType::Count); ++i)
    {
        EBiomeType BiomeType = static_cast<EBiomeType>(i);
        float Weight = BiomeWeights.NormalizedWeights[i];
        
        if (Weight > 0.0f)
        {
            const FVegetationProfile& BiomeProfile = GetVegetationDataForBiome(BiomeType);
            BlendedProfile.TreeDensity += BiomeProfile.TreeDensity * Weight;
            BlendedProfile.FoliageDensity += BiomeProfile.FoliageDensity * Weight;
            BlendedProfile.ResourceDensity += BiomeProfile.ResourceDensity * Weight;
        }
    }
    
    // For vegetation types, use the dominant biome's profile
    const FVegetationProfile& DominantProfile = GetVegetationDataForBiome(BiomeWeights.DominantBiome);
    BlendedProfile.TreeSpecies = DominantProfile.TreeSpecies;
    BlendedProfile.FoliageTypes = DominantProfile.FoliageTypes;
    BlendedProfile.HarvestableResources = DominantProfile.HarvestableResources;
    
    return BlendedProfile;
}

float FVegetationSystem::CalculateBaseVegetationDensity(float WorldX, float WorldY, const FIntVector& ChunkCoord) const
{
    // Use vegetation-specific noise for base density
    float VegetationNoise = NoiseGenerator.GeneratePerlinNoise(WorldX, WorldY, 0.005f, ENoiseFeatureTag::Vegetation, ChunkCoord);
    
    // Apply some variation with multiple octaves
    float DetailNoise = NoiseGenerator.GeneratePerlinNoise(WorldX, WorldY, 0.01f, ENoiseFeatureTag::Vegetation, ChunkCoord);
    
    // Combine base and detail noise
    float BaseDensity = (VegetationNoise * 0.7f) + (DetailNoise * 0.3f);
    
    return FMath::Clamp(BaseDensity, 0.0f, 1.0f);
}

float FVegetationSystem::ApplyHeightModifiers(float BaseDensity, float TerrainHeight, EBiomeType BiomeType) const
{
    float ModifiedDensity = BaseDensity;
    
    // Apply height-based modifiers based on biome type
    switch (BiomeType)
    {
        case EBiomeType::Mountains:
            // Reduce vegetation density at very high altitudes
            if (TerrainHeight > 300.0f)
            {
                float HeightFactor = FMath::Clamp((500.0f - TerrainHeight) / 200.0f, 0.1f, 1.0f);
                ModifiedDensity *= HeightFactor;
            }
            break;
            
        case EBiomeType::Ocean:
            // Very low vegetation density in ocean
            ModifiedDensity *= 0.2f;
            break;
            
        case EBiomeType::Swamp:
            // Slightly higher density in low-lying areas
            if (TerrainHeight < 50.0f)
            {
                ModifiedDensity *= 1.2f;
            }
            break;
            
        case EBiomeType::Meadows:
            // Optimal vegetation growth in moderate elevations
            if (TerrainHeight > 50.0f && TerrainHeight < 200.0f)
            {
                ModifiedDensity *= 1.1f;
            }
            break;
            
        case EBiomeType::BlackForest:
            // Dense forest growth, less affected by height
            break;
            
        default:
            break;
    }
    
    return FMath::Clamp(ModifiedDensity, 0.0f, 1.0f);
}

FString FVegetationSystem::GetChunkVegetationDataFilePath(const FIntVector& ChunkCoord) const
{
    FString SaveDir = FPaths::ProjectSavedDir() / TEXT("WorldGen") / TEXT("VegetationData");
    FString FileName = FString::Printf(TEXT("vegetation_chunk_%d_%d_%d.json"), ChunkCoord.X, ChunkCoord.Y, ChunkCoord.Z);
    return SaveDir / FileName;
}

bool FVegetationSystem::EnsureVegetationDataDirectory() const
{
    FString SaveDir = FPaths::ProjectSavedDir() / TEXT("WorldGen") / TEXT("VegetationData");
    return FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*SaveDir);
}

FVector2D FVegetationSystem::ChunkCoordToWorldPosition(const FIntVector& ChunkCoord) const
{
    float ChunkSizeInCm = Settings.ChunkSize * Settings.VoxelSizeCm;
    return FVector2D(
        ChunkCoord.X * ChunkSizeInCm + (ChunkSizeInCm * 0.5f),
        ChunkCoord.Y * ChunkSizeInCm + (ChunkSizeInCm * 0.5f)
    );
}

TArray<FVegetationDensity> FVegetationSystem::SampleChunkVegetationDensity(const FIntVector& ChunkCoord, int32 SampleResolution) const
{
    TArray<FVegetationDensity> DensityMap;
    DensityMap.Reserve(SampleResolution * SampleResolution);
    
    float ChunkSizeInCm = Settings.ChunkSize * Settings.VoxelSizeCm;
    FVector2D ChunkWorldPos = ChunkCoordToWorldPosition(ChunkCoord);
    FVector2D ChunkStartPos = ChunkWorldPos - FVector2D(ChunkSizeInCm * 0.5f, ChunkSizeInCm * 0.5f);
    
    float SampleSpacing = ChunkSizeInCm / static_cast<float>(SampleResolution);
    
    for (int32 Y = 0; Y < SampleResolution; ++Y)
    {
        for (int32 X = 0; X < SampleResolution; ++X)
        {
            FVector2D SamplePos = ChunkStartPos + FVector2D(
                X * SampleSpacing + (SampleSpacing * 0.5f),
                Y * SampleSpacing + (SampleSpacing * 0.5f)
            );
            
            // For now, assume terrain height of 0 - this could be improved by sampling actual terrain
            FVegetationDensity Density = CalculateVegetationDensity(SamplePos.X, SamplePos.Y, 0.0f, ChunkCoord);
            DensityMap.Add(Density);
        }
    }
    
    return DensityMap;
}