#include "POISystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Kismet/GameplayStatics.h"
#include "Math/UnrealMathUtility.h"

DEFINE_LOG_CATEGORY(LogPOISystem);

FPOISystem::FPOISystem()
    : NoiseGenerator(nullptr)
    , BiomeSystem(nullptr)
    , TotalPlacementAttempts(0)
    , SuccessfulPlacements(0)
    , FailedPlacements(0)
    , bIsInitialized(false)
{
}

FPOISystem::~FPOISystem()
{
    Reset();
}

void FPOISystem::Initialize(const FWorldGenSettings& Settings, FNoiseGenerator* InNoiseGenerator, const FBiomeSystem* InBiomeSystem)
{
    WorldGenSettings = Settings;
    NoiseGenerator = InNoiseGenerator;
    BiomeSystem = InBiomeSystem;
    
    if (!NoiseGenerator)
    {
        UE_LOG(LogPOISystem, Error, TEXT("NoiseGenerator is null - POI system cannot function"));
        return;
    }
    
    if (!BiomeSystem)
    {
        UE_LOG(LogPOISystem, Error, TEXT("BiomeSystem is null - POI system cannot function"));
        return;
    }
    
    // Initialize default POI spawn rules
    POISpawnRules.Empty();
    
    // Add default POI types for each biome
    FPOISpawnRule MeadowsRuin;
    MeadowsRuin.POITypeName = TEXT("MeadowsRuin");
    MeadowsRuin.MinSpacing = 200.0f;
    MeadowsRuin.MaxSlope = 15.0f;
    MeadowsRuin.MinAltitude = 0.0f;
    MeadowsRuin.MaxAltitude = 100.0f;
    MeadowsRuin.SpawnProbability = 0.15f;
    MeadowsRuin.AllowedBiomes = {TEXT("Meadows")};
    MeadowsRuin.FlattenRadius = 8.0f;
    POISpawnRules.Add(MeadowsRuin);
    
    FPOISpawnRule BlackForestTower;
    BlackForestTower.POITypeName = TEXT("BlackForestTower");
    BlackForestTower.MinSpacing = 300.0f;
    BlackForestTower.MaxSlope = 25.0f;
    BlackForestTower.MinAltitude = 20.0f;
    BlackForestTower.MaxAltitude = 200.0f;
    BlackForestTower.SpawnProbability = 0.08f;
    BlackForestTower.AllowedBiomes = {TEXT("BlackForest")};
    BlackForestTower.FlattenRadius = 12.0f;
    POISpawnRules.Add(BlackForestTower);
    
    FPOISpawnRule SwampHut;
    SwampHut.POITypeName = TEXT("SwampHut");
    SwampHut.MinSpacing = 150.0f;
    SwampHut.MaxSlope = 10.0f;
    SwampHut.MinAltitude = -10.0f;
    SwampHut.MaxAltitude = 20.0f;
    SwampHut.MinWaterlineClearance = 2.0f;
    SwampHut.SpawnProbability = 0.12f;
    SwampHut.AllowedBiomes = {TEXT("Swamp")};
    SwampHut.FlattenRadius = 6.0f;
    POISpawnRules.Add(SwampHut);
    
    bIsInitialized = true;
    
    UE_LOG(LogPOISystem, Log, TEXT("POI System initialized with %d spawn rules"), POISpawnRules.Num());
}

TArray<FPOIPlacementResult> FPOISystem::GeneratePOIsForChunk(const FIntVector& ChunkCoordinate, UWorld* World)
{
    TArray<FPOIPlacementResult> Results;
    
    if (!bIsInitialized)
    {
        UE_LOG(LogPOISystem, Warning, TEXT("POI System not initialized - cannot generate POIs for chunk %s"), *ChunkCoordinate.ToString());
        return Results;
    }
    
    if (!World)
    {
        UE_LOG(LogPOISystem, Error, TEXT("World is null - cannot generate POIs for chunk %s"), *ChunkCoordinate.ToString());
        return Results;
    }
    
    // Check if we already have POIs for this chunk
    if (ChunkPOIMap.Contains(ChunkCoordinate))
    {
        UE_LOG(LogPOISystem, Log, TEXT("Chunk %s already has POIs generated"), *ChunkCoordinate.ToString());
        return Results;
    }
    
    UE_LOG(LogPOISystem, Log, TEXT("Generating POIs for chunk %s"), *ChunkCoordinate.ToString());
    
    TArray<FPOIInstance> ChunkPOIs;
    
    // Attempt to place each POI type
    for (const FPOISpawnRule& SpawnRule : POISpawnRules)
    {
        FPOIPlacementResult Result = AttemptPOIPlacement(ChunkCoordinate, SpawnRule, World);
        Results.Add(Result);
        
        if (Result.bSuccess)
        {
            ChunkPOIs.Add(Result.POIInstance);
            AllPOIInstances.Add(Result.POIInstance);
            SuccessfulPlacements++;
        }
        else
        {
            FailedPlacements++;
        }
        
        TotalPlacementAttempts += Result.AttemptsUsed;
    }
    
    // Store POIs for this chunk
    ChunkPOIMap.Add(ChunkCoordinate, ChunkPOIs);
    
    UE_LOG(LogPOISystem, Log, TEXT("Generated %d POIs for chunk %s"), ChunkPOIs.Num(), *ChunkCoordinate.ToString());
    
    return Results;
}

void FPOISystem::RemovePOIsFromChunk(const FIntVector& ChunkCoordinate)
{
    if (!ChunkPOIMap.Contains(ChunkCoordinate))
    {
        return;
    }
    
    TArray<FPOIInstance>& ChunkPOIs = ChunkPOIMap[ChunkCoordinate];
    
    // Destroy spawned actors
    for (FPOIInstance& POI : ChunkPOIs)
    {
        if (POI.SpawnedActor.IsValid())
        {
            POI.SpawnedActor->Destroy();
            POI.SpawnedActor = nullptr;
            POI.bIsSpawned = false;
        }
        
        // Remove from global list
        AllPOIInstances.RemoveAll([&POI](const FPOIInstance& Instance) {
            return Instance.WorldLocation.Equals(POI.WorldLocation, 1.0f) && 
                   Instance.POITypeName == POI.POITypeName;
        });
    }
    
    ChunkPOIMap.Remove(ChunkCoordinate);
    
    UE_LOG(LogPOISystem, Log, TEXT("Removed %d POIs from chunk %s"), ChunkPOIs.Num(), *ChunkCoordinate.ToString());
}

TArray<FPOIInstance> FPOISystem::GetPOIsInChunk(const FIntVector& ChunkCoordinate) const
{
    if (ChunkPOIMap.Contains(ChunkCoordinate))
    {
        return ChunkPOIMap[ChunkCoordinate];
    }
    
    return TArray<FPOIInstance>();
}

TArray<FPOIInstance> FPOISystem::GetAllActivePOIs() const
{
    return AllPOIInstances;
}

void FPOISystem::AddPOISpawnRule(const FPOISpawnRule& SpawnRule)
{
    // Remove existing rule with same type name
    RemovePOISpawnRule(SpawnRule.POITypeName);
    
    POISpawnRules.Add(SpawnRule);
    
    UE_LOG(LogPOISystem, Log, TEXT("Added POI spawn rule: %s"), *SpawnRule.POITypeName);
}

bool FPOISystem::RemovePOISpawnRule(const FString& POITypeName)
{
    int32 RemovedCount = POISpawnRules.RemoveAll([&POITypeName](const FPOISpawnRule& Rule) {
        return Rule.POITypeName == POITypeName;
    });
    
    if (RemovedCount > 0)
    {
        UE_LOG(LogPOISystem, Log, TEXT("Removed POI spawn rule: %s"), *POITypeName);
        return true;
    }
    
    return false;
}

TArray<FPOISpawnRule> FPOISystem::GetPOISpawnRules() const
{
    return POISpawnRules;
}

bool FPOISystem::IsValidPOILocation(const FVector& Location, const FPOISpawnRule& SpawnRule, FString& OutFailureReason) const
{
    // Check spacing requirements
    if (!CheckSpacingRequirements(Location, SpawnRule))
    {
        OutFailureReason = FString::Printf(TEXT("Spacing requirement not met (min: %.1fm)"), SpawnRule.MinSpacing);
        return false;
    }
    
    // Check terrain slope
    if (!CheckTerrainSlope(Location, SpawnRule.MaxSlope))
    {
        OutFailureReason = FString::Printf(TEXT("Terrain too steep (max: %.1f degrees)"), SpawnRule.MaxSlope);
        return false;
    }
    
    // Check altitude requirements
    if (!CheckAltitudeRequirements(Location, SpawnRule.MinAltitude, SpawnRule.MaxAltitude))
    {
        OutFailureReason = FString::Printf(TEXT("Altitude out of range (%.1f - %.1f)"), SpawnRule.MinAltitude, SpawnRule.MaxAltitude);
        return false;
    }
    
    // Check waterline clearance
    if (!CheckWaterlineClearance(Location, SpawnRule.MinWaterlineClearance))
    {
        OutFailureReason = FString::Printf(TEXT("Too close to water (min clearance: %.1fm)"), SpawnRule.MinWaterlineClearance);
        return false;
    }
    
    // Check biome compatibility
    if (BiomeSystem)
    {
        FBiomeEvaluation BiomeEval = BiomeSystem->EvaluateBiome(Location.X, Location.Y);
        bool bBiomeAllowed = false;
        
        for (const FString& AllowedBiome : SpawnRule.AllowedBiomes)
        {
            if (BiomeEval.DominantBiome == AllowedBiome)
            {
                bBiomeAllowed = true;
                break;
            }
        }
        
        if (!bBiomeAllowed)
        {
            OutFailureReason = FString::Printf(TEXT("Biome not allowed (current: %s)"), *BiomeEval.DominantBiome);
            return false;
        }
    }
    
    return true;
}

void FPOISystem::GetPlacementStats(int32& OutTotalAttempts, int32& OutSuccessfulPlacements, 
                                  int32& OutFailedPlacements, float& OutAverageAttemptsPerPOI) const
{
    OutTotalAttempts = TotalPlacementAttempts;
    OutSuccessfulPlacements = SuccessfulPlacements;
    OutFailedPlacements = FailedPlacements;
    
    if (SuccessfulPlacements > 0)
    {
        OutAverageAttemptsPerPOI = static_cast<float>(TotalPlacementAttempts) / static_cast<float>(SuccessfulPlacements);
    }
    else
    {
        OutAverageAttemptsPerPOI = 0.0f;
    }
}

void FPOISystem::Reset()
{
    // Destroy all spawned actors
    for (const FPOIInstance& POI : AllPOIInstances)
    {
        if (POI.SpawnedActor.IsValid())
        {
            POI.SpawnedActor->Destroy();
        }
    }
    
    ChunkPOIMap.Empty();
    AllPOIInstances.Empty();
    
    TotalPlacementAttempts = 0;
    SuccessfulPlacements = 0;
    FailedPlacements = 0;
    
    UE_LOG(LogPOISystem, Log, TEXT("POI System reset"));
}

FPOIPlacementResult FPOISystem::AttemptPOIPlacement(const FIntVector& ChunkCoordinate, const FPOISpawnRule& SpawnRule, UWorld* World)
{
    FPOIPlacementResult Result;
    Result.bSuccess = false;
    Result.AttemptsUsed = 0;
    
    // Generate deterministic seed for this chunk and POI type
    uint64 POISeed = GeneratePOISeed(ChunkCoordinate, SpawnRule.POITypeName);
    FRandomStream RandomStream(POISeed);
    
    // Check spawn probability first
    if (RandomStream.FRand() > SpawnRule.SpawnProbability)
    {
        Result.FailureReason = TEXT("Spawn probability check failed");
        return Result;
    }
    
    // Attempt placement with retries
    for (int32 Attempt = 0; Attempt < SpawnRule.MaxRetryAttempts; ++Attempt)
    {
        Result.AttemptsUsed++;
        
        FVector Location;
        FRotator Rotation;
        
        if (FindValidPOILocation(ChunkCoordinate, SpawnRule, Location, Rotation))
        {
            // Create POI instance
            FPOIInstance POIInstance;
            POIInstance.WorldLocation = Location;
            POIInstance.Rotation = Rotation;
            POIInstance.POITypeName = SpawnRule.POITypeName;
            POIInstance.ChunkCoordinate = ChunkCoordinate;
            
            // Determine biome
            if (BiomeSystem)
            {
                FBiomeEvaluation BiomeEval = BiomeSystem->EvaluateBiome(Location.X, Location.Y);
                POIInstance.BiomeName = BiomeEval.DominantBiome;
            }
            
            // Flatten terrain around POI
            FlattenTerrainAroundPOI(Location, SpawnRule.FlattenRadius, World);
            
            // Spawn the prefab
            AActor* SpawnedActor = SpawnPOIPrefab(POIInstance, SpawnRule, World);
            if (SpawnedActor)
            {
                POIInstance.bIsSpawned = true;
                POIInstance.SpawnedActor = SpawnedActor;
                
                Result.bSuccess = true;
                Result.POIInstance = POIInstance;
                
                UE_LOG(LogPOISystem, Log, TEXT("Successfully placed POI %s at %s (attempt %d/%d)"), 
                       *SpawnRule.POITypeName, *Location.ToString(), Attempt + 1, SpawnRule.MaxRetryAttempts);
                
                return Result;
            }
            else
            {
                Result.FailureReason = TEXT("Failed to spawn prefab");
            }
        }
        else
        {
            Result.FailureReason = TEXT("No valid location found");
        }
    }
    
    UE_LOG(LogPOISystem, Warning, TEXT("Failed to place POI %s in chunk %s after %d attempts: %s"), 
           *SpawnRule.POITypeName, *ChunkCoordinate.ToString(), SpawnRule.MaxRetryAttempts, *Result.FailureReason);
    
    return Result;
}

bool FPOISystem::FindValidPOILocation(const FIntVector& ChunkCoordinate, const FPOISpawnRule& SpawnRule, 
                                     FVector& OutLocation, FRotator& OutRotation)
{
    // Generate deterministic seed for location finding
    uint64 LocationSeed = GeneratePOISeed(ChunkCoordinate, SpawnRule.POITypeName + TEXT("_Location"));
    FRandomStream RandomStream(LocationSeed);
    
    // Get chunk bounds
    FVector ChunkCenter = ChunkCoordinateToWorldLocation(ChunkCoordinate);
    float ChunkSize = WorldGenSettings.ChunkSize * WorldGenSettings.VoxelSizeCm;
    float HalfChunkSize = ChunkSize * 0.5f;
    
    // Try multiple random locations within the chunk
    const int32 MaxLocationAttempts = 20;
    for (int32 i = 0; i < MaxLocationAttempts; ++i)
    {
        // Generate random location within chunk bounds
        FVector TestLocation;
        TestLocation.X = ChunkCenter.X + RandomStream.FRandRange(-HalfChunkSize, HalfChunkSize);
        TestLocation.Y = ChunkCenter.Y + RandomStream.FRandRange(-HalfChunkSize, HalfChunkSize);
        TestLocation.Z = ChunkCenter.Z; // Will be adjusted based on terrain height
        
        // Sample terrain height at this location
        if (BiomeSystem)
        {
            FBiomeEvaluation BiomeEval = BiomeSystem->EvaluateBiome(TestLocation.X, TestLocation.Y);
            TestLocation.Z = BiomeEval.TerrainHeight;
        }
        
        // Validate location
        FString FailureReason;
        if (IsValidPOILocation(TestLocation, SpawnRule, FailureReason))
        {
            OutLocation = TestLocation;
            
            // Generate random rotation
            OutRotation = FRotator(0.0f, RandomStream.FRandRange(0.0f, 360.0f), 0.0f);
            
            return true;
        }
    }
    
    return false;
}

bool FPOISystem::CheckSpacingRequirements(const FVector& Location, const FPOISpawnRule& SpawnRule) const
{
    // Check distance to all existing POIs of the same type
    for (const FPOIInstance& ExistingPOI : AllPOIInstances)
    {
        if (ExistingPOI.POITypeName == SpawnRule.POITypeName)
        {
            float Distance = FVector::Dist(Location, ExistingPOI.WorldLocation);
            if (Distance < SpawnRule.MinSpacing)
            {
                return false;
            }
        }
    }
    
    return true;
}

bool FPOISystem::CheckTerrainSlope(const FVector& Location, float MaxSlope) const
{
    if (!BiomeSystem)
    {
        return true; // Can't check slope without biome system
    }
    
    // Sample terrain height at multiple points around the location
    const float SampleRadius = 5.0f; // 5 meter radius for slope calculation
    const int32 NumSamples = 8;
    
    TArray<float> Heights;
    Heights.Reserve(NumSamples + 1);
    
    // Center height
    FBiomeEvaluation CenterEval = BiomeSystem->EvaluateBiome(Location.X, Location.Y);
    Heights.Add(CenterEval.TerrainHeight);
    
    // Sample around the center
    for (int32 i = 0; i < NumSamples; ++i)
    {
        float Angle = (2.0f * PI * i) / NumSamples;
        FVector SamplePos = Location + FVector(
            FMath::Cos(Angle) * SampleRadius,
            FMath::Sin(Angle) * SampleRadius,
            0.0f
        );
        
        FBiomeEvaluation SampleEval = BiomeSystem->EvaluateBiome(SamplePos.X, SamplePos.Y);
        Heights.Add(SampleEval.TerrainHeight);
    }
    
    // Calculate maximum slope
    float MaxHeightDiff = 0.0f;
    for (int32 i = 1; i < Heights.Num(); ++i)
    {
        float HeightDiff = FMath::Abs(Heights[i] - Heights[0]);
        MaxHeightDiff = FMath::Max(MaxHeightDiff, HeightDiff);
    }
    
    // Convert to slope angle in degrees
    float SlopeAngle = FMath::RadiansToDegrees(FMath::Atan(MaxHeightDiff / SampleRadius));
    
    return SlopeAngle <= MaxSlope;
}

bool FPOISystem::CheckAltitudeRequirements(const FVector& Location, float MinAltitude, float MaxAltitude) const
{
    return Location.Z >= MinAltitude && Location.Z <= MaxAltitude;
}

bool FPOISystem::CheckWaterlineClearance(const FVector& Location, float MinClearance) const
{
    // For now, assume water level is at Z=0
    // In a more sophisticated system, this would query actual water bodies
    const float WaterLevel = 0.0f;
    
    float ClearanceDistance = Location.Z - WaterLevel;
    return ClearanceDistance >= MinClearance;
}

void FPOISystem::FlattenTerrainAroundPOI(const FVector& Location, float Radius, UWorld* World)
{
    // This is a placeholder implementation
    // In a real implementation, this would use the VoxelPluginAdapter to modify terrain
    // For now, we just log the operation
    
    UE_LOG(LogPOISystem, Log, TEXT("Flattening terrain at %s with radius %.1f"), *Location.ToString(), Radius);
    
    // TODO: Integrate with VoxelPluginAdapter to actually flatten terrain
    // This would involve:
    // 1. Sampling the terrain height at the center location
    // 2. Creating a circular CSG operation to flatten the area
    // 3. Applying the operation through the voxel edit service
}

AActor* FPOISystem::SpawnPOIPrefab(const FPOIInstance& POIInstance, const FPOISpawnRule& SpawnRule, UWorld* World)
{
    if (!World)
    {
        UE_LOG(LogPOISystem, Error, TEXT("World is null - cannot spawn POI prefab"));
        return nullptr;
    }
    
    // For now, create a simple placeholder actor with a static mesh
    // In a real implementation, this would load and spawn the actual prefab
    
    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = FName(*FString::Printf(TEXT("POI_%s_%s"), *POIInstance.POITypeName, *POIInstance.ChunkCoordinate.ToString()));
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    
    AStaticMeshActor* POIActor = World->SpawnActor<AStaticMeshActor>(
        AStaticMeshActor::StaticClass(),
        POIInstance.WorldLocation,
        POIInstance.Rotation,
        SpawnParams
    );
    
    if (POIActor)
    {
        // Set up the static mesh component with a placeholder mesh
        UStaticMeshComponent* MeshComponent = POIActor->GetStaticMeshComponent();
        if (MeshComponent)
        {
            // Load a default cube mesh as placeholder
            UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
            if (CubeMesh)
            {
                MeshComponent->SetStaticMesh(CubeMesh);
                
                // Scale based on POI type
                FVector Scale = FVector(2.0f, 2.0f, 3.0f); // Default scale
                if (POIInstance.POITypeName.Contains(TEXT("Tower")))
                {
                    Scale = FVector(1.5f, 1.5f, 5.0f); // Taller for towers
                }
                else if (POIInstance.POITypeName.Contains(TEXT("Hut")))
                {
                    Scale = FVector(3.0f, 3.0f, 2.0f); // Wider and shorter for huts
                }
                
                POIActor->SetActorScale3D(Scale);
            }
        }
        
        // Add tags for identification
        POIActor->Tags.Add(FName(TEXT("POI")));
        POIActor->Tags.Add(FName(*POIInstance.POITypeName));
        POIActor->Tags.Add(FName(*POIInstance.BiomeName));
        
        UE_LOG(LogPOISystem, Log, TEXT("Spawned POI actor %s at %s"), 
               *POIActor->GetName(), *POIInstance.WorldLocation.ToString());
    }
    else
    {
        UE_LOG(LogPOISystem, Error, TEXT("Failed to spawn POI actor for %s"), *POIInstance.POITypeName);
    }
    
    return POIActor;
}

FIntVector FPOISystem::WorldLocationToChunkCoordinate(const FVector& WorldLocation) const
{
    float ChunkSize = WorldGenSettings.ChunkSize * WorldGenSettings.VoxelSizeCm;
    
    return FIntVector(
        FMath::FloorToInt(WorldLocation.X / ChunkSize),
        FMath::FloorToInt(WorldLocation.Y / ChunkSize),
        FMath::FloorToInt(WorldLocation.Z / ChunkSize)
    );
}

FVector FPOISystem::ChunkCoordinateToWorldLocation(const FIntVector& ChunkCoordinate) const
{
    float ChunkSize = WorldGenSettings.ChunkSize * WorldGenSettings.VoxelSizeCm;
    
    return FVector(
        ChunkCoordinate.X * ChunkSize + ChunkSize * 0.5f,
        ChunkCoordinate.Y * ChunkSize + ChunkSize * 0.5f,
        ChunkCoordinate.Z * ChunkSize + ChunkSize * 0.5f
    );
}

uint64 FPOISystem::GeneratePOISeed(const FIntVector& ChunkCoordinate, const FString& POITypeName) const
{
    // Create deterministic seed using world generation seed, chunk coordinate, and POI type
    uint64 BaseSeed = WorldGenSettings.Seed;
    
    // Hash chunk coordinate
    uint64 ChunkHash = 0;
    ChunkHash ^= static_cast<uint64>(ChunkCoordinate.X) + 0x9e3779b9 + (ChunkHash << 6) + (ChunkHash >> 2);
    ChunkHash ^= static_cast<uint64>(ChunkCoordinate.Y) + 0x9e3779b9 + (ChunkHash << 6) + (ChunkHash >> 2);
    ChunkHash ^= static_cast<uint64>(ChunkCoordinate.Z) + 0x9e3779b9 + (ChunkHash << 6) + (ChunkHash >> 2);
    
    // Hash POI type name
    uint64 TypeHash = 0;
    for (TCHAR Char : POITypeName)
    {
        TypeHash ^= static_cast<uint64>(Char) + 0x9e3779b9 + (TypeHash << 6) + (TypeHash >> 2);
    }
    
    // Combine all hashes with the base seed
    return BaseSeed ^ ChunkHash ^ TypeHash ^ static_cast<uint64>(0x504F49); // 'POI' in hex
}