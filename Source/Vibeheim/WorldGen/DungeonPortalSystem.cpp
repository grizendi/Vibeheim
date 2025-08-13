#include "DungeonPortalSystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Kismet/GameplayStatics.h"
#include "Math/UnrealMathUtility.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

DEFINE_LOG_CATEGORY(LogDungeonPortalSystem);

// ADungeonPortalActor Implementation
ADungeonPortalActor::ADungeonPortalActor()
{
    PrimaryActorTick.bCanEverTick = true;

    // Create root component
    RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));
    RootComponent = RootSceneComponent;

    // Create portal mesh component
    PortalMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PortalMeshComponent"));
    PortalMeshComponent->SetupAttachment(RootComponent);
    
    // Create interaction sphere
    InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
    InteractionSphere->SetupAttachment(RootComponent);
    InteractionSphere->SetSphereRadius(300.0f); // Default 3 meter radius
    InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    // Create interaction widget
    InteractionWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionWidget"));
    InteractionWidget->SetupAttachment(RootComponent);
    InteractionWidget->SetWidgetSpace(EWidgetSpace::Screen);
    InteractionWidget->SetDrawSize(FVector2D(300.0f, 100.0f));
    InteractionWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 200.0f)); // Position above portal
    InteractionWidget->SetVisibility(false); // Hidden by default

    // Bind overlap events
    InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &ADungeonPortalActor::OnInteractionBeginOverlap);
    InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &ADungeonPortalActor::OnInteractionEndOverlap);
}

void ADungeonPortalActor::BeginPlay()
{
    Super::BeginPlay();
    
    // Set up default portal mesh if none is set
    if (PortalMeshComponent && !PortalMeshComponent->GetStaticMesh())
    {
        UStaticMesh* DefaultMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
        if (DefaultMesh)
        {
            PortalMeshComponent->SetStaticMesh(DefaultMesh);
            PortalMeshComponent->SetRelativeScale3D(FVector(2.0f, 2.0f, 0.5f));
            PortalMeshComponent->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
        }
    }
    
    // Set up the interaction widget
    if (InteractionWidget)
    {
        // Try to create the widget class
        UClass* WidgetClass = UDungeonPortalWidget::StaticClass();
        if (WidgetClass)
        {
            InteractionWidget->SetWidgetClass(WidgetClass);
        }
    }
    
    UpdateVisualEffects();
}

void ADungeonPortalActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    AnimationTimer += DeltaTime;
    UpdateVisualEffects();
}

void ADungeonPortalActor::InitializePortal(const FDungeonPortal& InPortalData, const FPortalSpawnRule& InSpawnRule)
{
    this->PortalData = InPortalData;
    this->SpawnRule = InSpawnRule;
    
    // Update interaction sphere radius
    if (InteractionSphere)
    {
        InteractionSphere->SetSphereRadius(InSpawnRule.InteractionRadius * 100.0f); // Convert to cm
    }
    
    // Set up the widget with portal information
    if (InteractionWidget && InteractionWidget->GetWidget())
    {
        UDungeonPortalWidget* PortalWidget = Cast<UDungeonPortalWidget>(InteractionWidget->GetWidget());
        if (PortalWidget)
        {
            PortalWidget->SetPortalInfo(InPortalData.PortalTypeName, InPortalData.TargetLevelName);
            PortalWidget->SetPortalActive(InPortalData.bIsActive);
        }
    }
    
    // Add tags for identification
    Tags.Add(FName(TEXT("DungeonPortal")));
    Tags.Add(FName(*InPortalData.PortalTypeName));
    Tags.Add(FName(*InPortalData.BiomeName));
    
    UE_LOG(LogDungeonPortalSystem, Log, TEXT("Initialized portal %s targeting level %s"), 
           *InPortalData.PortalTypeName, *InPortalData.TargetLevelName);
}

void ADungeonPortalActor::ActivatePortal(AActor* InteractingActor)
{
    if (!PortalData.bIsActive)
    {
        UE_LOG(LogDungeonPortalSystem, Warning, TEXT("Portal %s is not active"), *PortalData.PortalTypeName);
        return;
    }
    
    if (!InteractingActor)
    {
        UE_LOG(LogDungeonPortalSystem, Warning, TEXT("No interacting actor provided for portal activation"));
        return;
    }
    
    UE_LOG(LogDungeonPortalSystem, Log, TEXT("Activating portal %s for actor %s"), 
           *PortalData.PortalTypeName, *InteractingActor->GetName());
    
    PerformTeleportation(InteractingActor);
}

void ADungeonPortalActor::SetPortalActive(bool bActive)
{
    PortalData.bIsActive = bActive;
    
    // Update the widget
    if (InteractionWidget && InteractionWidget->GetWidget())
    {
        UDungeonPortalWidget* PortalWidget = Cast<UDungeonPortalWidget>(InteractionWidget->GetWidget());
        if (PortalWidget)
        {
            PortalWidget->SetPortalActive(bActive);
        }
    }
    
    UpdateVisualEffects();
    
    UE_LOG(LogDungeonPortalSystem, Log, TEXT("Portal %s set to %s"), 
           *PortalData.PortalTypeName, bActive ? TEXT("active") : TEXT("inactive"));
}

void ADungeonPortalActor::OnInteractionBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
                                                   UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, 
                                                   bool bFromSweep, const FHitResult& SweepResult)
{
    // Check if the overlapping actor is a player pawn
    APawn* PlayerPawn = Cast<APawn>(OtherActor);
    if (!PlayerPawn || !PlayerPawn->IsPlayerControlled())
    {
        return;
    }
    
    bPlayerInRange = true;
    CurrentInteractingActor = OtherActor;
    
    // Show interaction widget
    if (InteractionWidget)
    {
        InteractionWidget->SetVisibility(true);
    }
    
    UE_LOG(LogDungeonPortalSystem, Log, TEXT("Player entered portal interaction range: %s"), 
           *PortalData.PortalTypeName);
}

void ADungeonPortalActor::OnInteractionEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
                                                 UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    // Check if the leaving actor is a player pawn
    APawn* PlayerPawn = Cast<APawn>(OtherActor);
    if (!PlayerPawn || !PlayerPawn->IsPlayerControlled())
    {
        return;
    }
    
    bPlayerInRange = false;
    CurrentInteractingActor = nullptr;
    
    // Hide interaction widget
    if (InteractionWidget)
    {
        InteractionWidget->SetVisibility(false);
    }
    
    UE_LOG(LogDungeonPortalSystem, Log, TEXT("Player left portal interaction range: %s"), 
           *PortalData.PortalTypeName);
}

void ADungeonPortalActor::UpdateVisualEffects()
{
    if (!PortalMeshComponent)
    {
        return;
    }
    
    // Simple pulsing animation for active portals
    if (PortalData.bIsActive)
    {
        float PulseValue = 0.8f + 0.2f * FMath::Sin(AnimationTimer * 2.0f);
        FVector CurrentScale = PortalMeshComponent->GetRelativeScale3D();
        CurrentScale.Z = 0.5f * PulseValue;
        PortalMeshComponent->SetRelativeScale3D(CurrentScale);
        
        // Change color based on player proximity
        if (bPlayerInRange)
        {
            // Could set material parameters here for glowing effect
        }
    }
    else
    {
        // Inactive portal - static appearance
        FVector CurrentScale = PortalMeshComponent->GetRelativeScale3D();
        CurrentScale.Z = 0.3f;
        PortalMeshComponent->SetRelativeScale3D(CurrentScale);
    }
}

void ADungeonPortalActor::PerformTeleportation(AActor* TargetActor)
{
    if (PortalData.TargetLevelName.IsEmpty())
    {
        UE_LOG(LogDungeonPortalSystem, Error, TEXT("Portal %s has no target level specified"), 
               *PortalData.PortalTypeName);
        return;
    }
    
    // Get the player controller
    APawn* PlayerPawn = Cast<APawn>(TargetActor);
    if (!PlayerPawn)
    {
        UE_LOG(LogDungeonPortalSystem, Warning, TEXT("Target actor is not a pawn, cannot teleport"));
        return;
    }
    
    APlayerController* PlayerController = PlayerPawn->GetController<APlayerController>();
    if (!PlayerController)
    {
        UE_LOG(LogDungeonPortalSystem, Warning, TEXT("No player controller found for teleportation"));
        return;
    }
    
    UE_LOG(LogDungeonPortalSystem, Log, TEXT("Teleporting player to level: %s"), *PortalData.TargetLevelName);
    
    // Use UGameplayStatics::OpenLevel for simple teleportation
    UGameplayStatics::OpenLevel(GetWorld(), FName(*PortalData.TargetLevelName));
}

// FDungeonPortalSystem Implementation
FDungeonPortalSystem::FDungeonPortalSystem()
    : NoiseGenerator(nullptr)
    , BiomeSystem(nullptr)
    , POISystem(nullptr)
    , TotalPlacementAttempts(0)
    , SuccessfulPlacements(0)
    , FailedPlacements(0)
    , bIsInitialized(false)
{
}

FDungeonPortalSystem::~FDungeonPortalSystem()
{
    Reset();
}

void FDungeonPortalSystem::Initialize(const FWorldGenSettings& Settings, FNoiseGenerator* InNoiseGenerator, 
                                     const FBiomeSystem* InBiomeSystem, FPOISystem* InPOISystem)
{
    WorldGenSettings = Settings;
    NoiseGenerator = InNoiseGenerator;
    BiomeSystem = InBiomeSystem;
    POISystem = InPOISystem;
    
    if (!NoiseGenerator)
    {
        UE_LOG(LogDungeonPortalSystem, Error, TEXT("NoiseGenerator is null - Portal system cannot function"));
        return;
    }
    
    if (!BiomeSystem)
    {
        UE_LOG(LogDungeonPortalSystem, Error, TEXT("BiomeSystem is null - Portal system cannot function"));
        return;
    }
    
    if (!POISystem)
    {
        UE_LOG(LogDungeonPortalSystem, Error, TEXT("POISystem is null - Portal system cannot function"));
        return;
    }
    
    // Initialize default portal spawn rules
    PortalSpawnRules.Empty();
    
    // Add default portal types for different biomes
    FPortalSpawnRule MeadowsDungeon;
    MeadowsDungeon.PortalTypeName = TEXT("MeadowsDungeonPortal");
    MeadowsDungeon.TargetLevelName = TEXT("MeadowsDungeon");
    MeadowsDungeon.MinSpacing = 800.0f;
    MeadowsDungeon.MaxSlope = 10.0f;
    MeadowsDungeon.MinAltitude = 20.0f;
    MeadowsDungeon.MaxAltitude = 150.0f;
    MeadowsDungeon.SpawnProbability = 0.03f;
    MeadowsDungeon.AllowedBiomes = {TEXT("Meadows")};
    MeadowsDungeon.FlattenRadius = 20.0f;
    MeadowsDungeon.InteractionRadius = 4.0f;
    PortalSpawnRules.Add(MeadowsDungeon);
    
    FPortalSpawnRule BlackForestDungeon;
    BlackForestDungeon.PortalTypeName = TEXT("BlackForestDungeonPortal");
    BlackForestDungeon.TargetLevelName = TEXT("BlackForestDungeon");
    BlackForestDungeon.MinSpacing = 1000.0f;
    BlackForestDungeon.MaxSlope = 15.0f;
    BlackForestDungeon.MinAltitude = 30.0f;
    BlackForestDungeon.MaxAltitude = 250.0f;
    BlackForestDungeon.SpawnProbability = 0.02f;
    BlackForestDungeon.AllowedBiomes = {TEXT("BlackForest")};
    BlackForestDungeon.FlattenRadius = 25.0f;
    BlackForestDungeon.InteractionRadius = 5.0f;
    PortalSpawnRules.Add(BlackForestDungeon);
    
    // Register portal types as POI types in the POI system for placement rules integration
    for (const FPortalSpawnRule& PortalRule : PortalSpawnRules)
    {
        FPOISpawnRule POIRule;
        POIRule.POITypeName = PortalRule.PortalTypeName;
        POIRule.MinSpacing = PortalRule.MinSpacing;
        POIRule.MaxSlope = PortalRule.MaxSlope;
        POIRule.MinAltitude = PortalRule.MinAltitude;
        POIRule.MaxAltitude = PortalRule.MaxAltitude;
        POIRule.MinWaterlineClearance = PortalRule.MinWaterlineClearance;
        POIRule.SpawnProbability = 0.0f; // Set to 0 so POI system doesn't spawn them
        POIRule.AllowedBiomes = PortalRule.AllowedBiomes;
        POIRule.FlattenRadius = PortalRule.FlattenRadius;
        POIRule.MaxRetryAttempts = PortalRule.MaxRetryAttempts;
        
        POISystem->AddPOISpawnRule(POIRule);
    }
    
    bIsInitialized = true;
    
    UE_LOG(LogDungeonPortalSystem, Log, TEXT("Dungeon Portal System initialized with %d spawn rules"), 
           PortalSpawnRules.Num());
}

TArray<FPortalPlacementResult> FDungeonPortalSystem::GeneratePortalsForChunk(const FIntVector& ChunkCoordinate, UWorld* World)
{
    TArray<FPortalPlacementResult> Results;
    
    if (!bIsInitialized)
    {
        UE_LOG(LogDungeonPortalSystem, Warning, TEXT("Portal System not initialized - cannot generate portals for chunk %s"), 
               *ChunkCoordinate.ToString());
        return Results;
    }
    
    if (!World)
    {
        UE_LOG(LogDungeonPortalSystem, Error, TEXT("World is null - cannot generate portals for chunk %s"), 
               *ChunkCoordinate.ToString());
        return Results;
    }
    
    // Check if we already have portals for this chunk
    if (ChunkPortalMap.Contains(ChunkCoordinate))
    {
        UE_LOG(LogDungeonPortalSystem, Log, TEXT("Chunk %s already has portals generated"), 
               *ChunkCoordinate.ToString());
        return Results;
    }
    
    UE_LOG(LogDungeonPortalSystem, Log, TEXT("Generating portals for chunk %s"), *ChunkCoordinate.ToString());
    
    TArray<FDungeonPortal> ChunkPortals;
    
    // Attempt to place each portal type
    for (const FPortalSpawnRule& SpawnRule : PortalSpawnRules)
    {
        FPortalPlacementResult Result = AttemptPortalPlacement(ChunkCoordinate, SpawnRule, World);
        Results.Add(Result);
        
        if (Result.bSuccess)
        {
            ChunkPortals.Add(Result.PortalInstance);
            AllPortalInstances.Add(Result.PortalInstance);
            SuccessfulPlacements++;
        }
        else
        {
            FailedPlacements++;
        }
        
        TotalPlacementAttempts += Result.AttemptsUsed;
    }
    
    // Store portals for this chunk
    ChunkPortalMap.Add(ChunkCoordinate, ChunkPortals);
    
    UE_LOG(LogDungeonPortalSystem, Log, TEXT("Generated %d portals for chunk %s"), 
           ChunkPortals.Num(), *ChunkCoordinate.ToString());
    
    return Results;
}

void FDungeonPortalSystem::RemovePortalsFromChunk(const FIntVector& ChunkCoordinate)
{
    if (!ChunkPortalMap.Contains(ChunkCoordinate))
    {
        return;
    }
    
    TArray<FDungeonPortal>& ChunkPortals = ChunkPortalMap[ChunkCoordinate];
    
    // Destroy spawned actors
    for (FDungeonPortal& Portal : ChunkPortals)
    {
        if (Portal.SpawnedActor.IsValid())
        {
            Portal.SpawnedActor->Destroy();
            Portal.SpawnedActor = nullptr;
            Portal.bIsSpawned = false;
        }
        
        // Remove from global list
        AllPortalInstances.RemoveAll([&Portal](const FDungeonPortal& Instance) {
            return Instance.WorldLocation.Equals(Portal.WorldLocation, 1.0f) && 
                   Instance.PortalTypeName == Portal.PortalTypeName;
        });
    }
    
    ChunkPortalMap.Remove(ChunkCoordinate);
    
    UE_LOG(LogDungeonPortalSystem, Log, TEXT("Removed %d portals from chunk %s"), 
           ChunkPortals.Num(), *ChunkCoordinate.ToString());
}

TArray<FDungeonPortal> FDungeonPortalSystem::GetPortalsInChunk(const FIntVector& ChunkCoordinate) const
{
    if (ChunkPortalMap.Contains(ChunkCoordinate))
    {
        return ChunkPortalMap[ChunkCoordinate];
    }
    
    return TArray<FDungeonPortal>();
}

TArray<FDungeonPortal> FDungeonPortalSystem::GetAllActivePortals() const
{
    return AllPortalInstances;
}

void FDungeonPortalSystem::AddPortalSpawnRule(const FPortalSpawnRule& SpawnRule)
{
    // Remove existing rule with same type name
    RemovePortalSpawnRule(SpawnRule.PortalTypeName);
    
    PortalSpawnRules.Add(SpawnRule);
    
    UE_LOG(LogDungeonPortalSystem, Log, TEXT("Added portal spawn rule: %s"), *SpawnRule.PortalTypeName);
}

bool FDungeonPortalSystem::RemovePortalSpawnRule(const FString& PortalTypeName)
{
    int32 RemovedCount = PortalSpawnRules.RemoveAll([&PortalTypeName](const FPortalSpawnRule& Rule) {
        return Rule.PortalTypeName == PortalTypeName;
    });
    
    if (RemovedCount > 0)
    {
        UE_LOG(LogDungeonPortalSystem, Log, TEXT("Removed portal spawn rule: %s"), *PortalTypeName);
        return true;
    }
    
    return false;
}

TArray<FPortalSpawnRule> FDungeonPortalSystem::GetPortalSpawnRules() const
{
    return PortalSpawnRules;
}

bool FDungeonPortalSystem::IsValidPortalLocation(const FVector& Location, const FPortalSpawnRule& SpawnRule, FString& OutFailureReason) const
{
    // Check spacing requirements
    if (!CheckSpacingRequirements(Location, SpawnRule))
    {
        OutFailureReason = FString::Printf(TEXT("Spacing requirement not met (min: %.1fm)"), SpawnRule.MinSpacing);
        return false;
    }
    
    // Use POI system validation for terrain checks
    if (POISystem)
    {
        // Create a temporary POI spawn rule for validation
        FPOISpawnRule TempPOIRule;
        TempPOIRule.MaxSlope = SpawnRule.MaxSlope;
        TempPOIRule.MinAltitude = SpawnRule.MinAltitude;
        TempPOIRule.MaxAltitude = SpawnRule.MaxAltitude;
        TempPOIRule.MinWaterlineClearance = SpawnRule.MinWaterlineClearance;
        TempPOIRule.AllowedBiomes = SpawnRule.AllowedBiomes;
        
        return POISystem->IsValidPOILocation(Location, TempPOIRule, OutFailureReason);
    }
    
    return true;
}

void FDungeonPortalSystem::GetPlacementStats(int32& OutTotalAttempts, int32& OutSuccessfulPlacements, 
                                            int32& OutFailedPlacements, float& OutAverageAttemptsPerPortal) const
{
    OutTotalAttempts = TotalPlacementAttempts;
    OutSuccessfulPlacements = SuccessfulPlacements;
    OutFailedPlacements = FailedPlacements;
    
    if (SuccessfulPlacements > 0)
    {
        OutAverageAttemptsPerPortal = static_cast<float>(TotalPlacementAttempts) / static_cast<float>(SuccessfulPlacements);
    }
    else
    {
        OutAverageAttemptsPerPortal = 0.0f;
    }
}

void FDungeonPortalSystem::Reset()
{
    // Destroy all spawned actors
    for (const FDungeonPortal& Portal : AllPortalInstances)
    {
        if (Portal.SpawnedActor.IsValid())
        {
            Portal.SpawnedActor->Destroy();
        }
    }
    
    ChunkPortalMap.Empty();
    AllPortalInstances.Empty();
    
    TotalPlacementAttempts = 0;
    SuccessfulPlacements = 0;
    FailedPlacements = 0;
    
    UE_LOG(LogDungeonPortalSystem, Log, TEXT("Dungeon Portal System reset"));
}

FPortalPlacementResult FDungeonPortalSystem::AttemptPortalPlacement(const FIntVector& ChunkCoordinate, const FPortalSpawnRule& SpawnRule, UWorld* World)
{
    FPortalPlacementResult Result;
    Result.bSuccess = false;
    Result.AttemptsUsed = 0;
    
    // Generate deterministic seed for this chunk and portal type
    uint64 PortalSeed = GeneratePortalSeed(ChunkCoordinate, SpawnRule.PortalTypeName);
    FRandomStream RandomStream(PortalSeed);
    
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
        
        if (FindValidPortalLocation(ChunkCoordinate, SpawnRule, Location, Rotation))
        {
            // Create portal instance
            FDungeonPortal PortalInstance;
            PortalInstance.WorldLocation = Location;
            PortalInstance.Rotation = Rotation;
            PortalInstance.PortalTypeName = SpawnRule.PortalTypeName;
            PortalInstance.TargetLevelName = SpawnRule.TargetLevelName;
            PortalInstance.ChunkCoordinate = ChunkCoordinate;
            
            // Determine biome
            if (BiomeSystem)
            {
                FBiomeEvaluation BiomeEval = BiomeSystem->EvaluateBiome(Location.X, Location.Y);
                PortalInstance.BiomeName = BiomeEval.DominantBiome;
            }
            
            // Flatten terrain around portal using POI system
            if (POISystem)
            {
                // This would be handled by the POI system's terrain flattening
                UE_LOG(LogDungeonPortalSystem, Log, TEXT("Requesting terrain flattening at %s with radius %.1f"), 
                       *Location.ToString(), SpawnRule.FlattenRadius);
            }
            
            // Spawn the portal actor
            ADungeonPortalActor* SpawnedActor = SpawnPortalActor(PortalInstance, SpawnRule, World);
            if (SpawnedActor)
            {
                PortalInstance.bIsSpawned = true;
                PortalInstance.SpawnedActor = SpawnedActor;
                
                Result.bSuccess = true;
                Result.PortalInstance = PortalInstance;
                
                UE_LOG(LogDungeonPortalSystem, Log, TEXT("Successfully placed portal %s at %s (attempt %d/%d)"), 
                       *SpawnRule.PortalTypeName, *Location.ToString(), Attempt + 1, SpawnRule.MaxRetryAttempts);
                
                return Result;
            }
            else
            {
                Result.FailureReason = TEXT("Failed to spawn portal actor");
            }
        }
        else
        {
            Result.FailureReason = TEXT("No valid location found");
        }
    }
    
    UE_LOG(LogDungeonPortalSystem, Warning, TEXT("Failed to place portal %s in chunk %s after %d attempts: %s"), 
           *SpawnRule.PortalTypeName, *ChunkCoordinate.ToString(), SpawnRule.MaxRetryAttempts, *Result.FailureReason);
    
    return Result;
}

bool FDungeonPortalSystem::FindValidPortalLocation(const FIntVector& ChunkCoordinate, const FPortalSpawnRule& SpawnRule, 
                                                  FVector& OutLocation, FRotator& OutRotation)
{
    // Generate deterministic seed for location finding
    uint64 LocationSeed = GeneratePortalSeed(ChunkCoordinate, SpawnRule.PortalTypeName + TEXT("_Location"));
    FRandomStream RandomStream(LocationSeed);
    
    // Get chunk bounds
    FVector ChunkCenter = ChunkCoordinateToWorldLocation(ChunkCoordinate);
    float ChunkSize = WorldGenSettings.ChunkSize * WorldGenSettings.VoxelSizeCm;
    float HalfChunkSize = ChunkSize * 0.5f;
    
    // Try multiple random locations within the chunk
    const int32 MaxLocationAttempts = 15;
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
        if (IsValidPortalLocation(TestLocation, SpawnRule, FailureReason))
        {
            OutLocation = TestLocation;
            
            // Generate random rotation
            OutRotation = FRotator(0.0f, RandomStream.FRandRange(0.0f, 360.0f), 0.0f);
            
            return true;
        }
    }
    
    return false;
}

bool FDungeonPortalSystem::CheckSpacingRequirements(const FVector& Location, const FPortalSpawnRule& SpawnRule) const
{
    // Check distance to all existing portals of the same type
    for (const FDungeonPortal& ExistingPortal : AllPortalInstances)
    {
        if (ExistingPortal.PortalTypeName == SpawnRule.PortalTypeName)
        {
            float Distance = FVector::Dist(Location, ExistingPortal.WorldLocation);
            if (Distance < SpawnRule.MinSpacing)
            {
                return false;
            }
        }
    }
    
    return true;
}

ADungeonPortalActor* FDungeonPortalSystem::SpawnPortalActor(const FDungeonPortal& PortalInstance, const FPortalSpawnRule& SpawnRule, UWorld* World)
{
    if (!World)
    {
        UE_LOG(LogDungeonPortalSystem, Error, TEXT("World is null - cannot spawn portal actor"));
        return nullptr;
    }
    
    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = FName(*FString::Printf(TEXT("Portal_%s_%s"), *PortalInstance.PortalTypeName, *PortalInstance.ChunkCoordinate.ToString()));
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    
    ADungeonPortalActor* PortalActor = World->SpawnActor<ADungeonPortalActor>(
        ADungeonPortalActor::StaticClass(),
        PortalInstance.WorldLocation,
        PortalInstance.Rotation,
        SpawnParams
    );
    
    if (PortalActor)
    {
        // Initialize the portal with data
        PortalActor->InitializePortal(PortalInstance, SpawnRule);
        
        UE_LOG(LogDungeonPortalSystem, Log, TEXT("Spawned portal actor %s at %s"), 
               *PortalActor->GetName(), *PortalInstance.WorldLocation.ToString());
    }
    else
    {
        UE_LOG(LogDungeonPortalSystem, Error, TEXT("Failed to spawn portal actor for %s"), *PortalInstance.PortalTypeName);
    }
    
    return PortalActor;
}

uint64 FDungeonPortalSystem::GeneratePortalSeed(const FIntVector& ChunkCoordinate, const FString& PortalTypeName) const
{
    // Create deterministic seed using world generation seed, chunk coordinate, and portal type
    uint64 BaseSeed = WorldGenSettings.Seed;
    
    // Hash chunk coordinate
    uint64 ChunkHash = 0;
    ChunkHash ^= static_cast<uint64>(ChunkCoordinate.X) + 0x9e3779b9 + (ChunkHash << 6) + (ChunkHash >> 2);
    ChunkHash ^= static_cast<uint64>(ChunkCoordinate.Y) + 0x9e3779b9 + (ChunkHash << 6) + (ChunkHash >> 2);
    ChunkHash ^= static_cast<uint64>(ChunkCoordinate.Z) + 0x9e3779b9 + (ChunkHash << 6) + (ChunkHash >> 2);
    
    // Hash portal type name
    uint64 TypeHash = 0;
    for (TCHAR Char : PortalTypeName)
    {
        TypeHash ^= static_cast<uint64>(Char) + 0x9e3779b9 + (TypeHash << 6) + (TypeHash >> 2);
    }
    
    // Combine all hashes with the base seed
    return BaseSeed ^ ChunkHash ^ TypeHash ^ static_cast<uint64>(0x504F5254414C); // 'PORTAL' in hex
}

FIntVector FDungeonPortalSystem::WorldLocationToChunkCoordinate(const FVector& WorldLocation) const
{
    float ChunkSize = WorldGenSettings.ChunkSize * WorldGenSettings.VoxelSizeCm;
    
    return FIntVector(
        FMath::FloorToInt(WorldLocation.X / ChunkSize),
        FMath::FloorToInt(WorldLocation.Y / ChunkSize),
        FMath::FloorToInt(WorldLocation.Z / ChunkSize)
    );
}

FVector FDungeonPortalSystem::ChunkCoordinateToWorldLocation(const FIntVector& ChunkCoordinate) const
{
    float ChunkSize = WorldGenSettings.ChunkSize * WorldGenSettings.VoxelSizeCm;
    
    return FVector(
        ChunkCoordinate.X * ChunkSize + ChunkSize * 0.5f,
        ChunkCoordinate.Y * ChunkSize + ChunkSize * 0.5f,
        ChunkCoordinate.Z * ChunkSize + ChunkSize * 0.5f
    );
}