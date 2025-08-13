#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Data/WorldGenSettings.h"
#include "NoiseGenerator.h"
#include "BiomeSystem.h"
#include "POISystem.h"
#include "DungeonPortalWidget.h"
#include "DungeonPortalSystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDungeonPortalSystem, Log, All);

/**
 * Defines spawn rules for dungeon portals
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FPortalSpawnRule
{
    GENERATED_BODY()

    /** Name of the portal type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal")
    FString PortalTypeName;

    /** Target level to teleport to */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal")
    FString TargetLevelName;

    /** Minimum distance between portals of this type (in meters) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal", meta = (ClampMin = "100.0", ClampMax = "2000.0"))
    float MinSpacing = 500.0f;

    /** Maximum slope angle for placement (in degrees) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal", meta = (ClampMin = "0.0", ClampMax = "30.0"))
    float MaxSlope = 15.0f;

    /** Minimum altitude for placement (in meters) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal")
    float MinAltitude = 10.0f;

    /** Maximum altitude for placement (in meters) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal")
    float MaxAltitude = 500.0f;

    /** Minimum distance from water surface (in meters) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal")
    float MinWaterlineClearance = 10.0f;

    /** Spawn probability (0.0 to 1.0) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float SpawnProbability = 0.05f;

    /** Biomes where this portal can spawn */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal")
    TArray<FString> AllowedBiomes;

    /** Terrain flattening radius around portal (in meters) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal", meta = (ClampMin = "5.0", ClampMax = "30.0"))
    float FlattenRadius = 15.0f;

    /** Maximum number of retry attempts for placement */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal", meta = (ClampMin = "1", ClampMax = "10"))
    int32 MaxRetryAttempts = 5;

    /** Interaction radius for portal activation (in meters) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal", meta = (ClampMin = "1.0", ClampMax = "10.0"))
    float InteractionRadius = 3.0f;

    FPortalSpawnRule()
    {
        PortalTypeName = TEXT("DefaultPortal");
        TargetLevelName = TEXT("DefaultDungeon");
        AllowedBiomes.Add(TEXT("Meadows"));
    }
};

/**
 * Represents a placed dungeon portal instance in the world
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FDungeonPortal
{
    GENERATED_BODY()

    /** World location of the portal */
    UPROPERTY(BlueprintReadOnly, Category = "Portal")
    FVector WorldLocation;

    /** Rotation of the portal */
    UPROPERTY(BlueprintReadOnly, Category = "Portal")
    FRotator Rotation;

    /** Type name of the portal */
    UPROPERTY(BlueprintReadOnly, Category = "Portal")
    FString PortalTypeName;

    /** Target level name for teleportation */
    UPROPERTY(BlueprintReadOnly, Category = "Portal")
    FString TargetLevelName;

    /** Biome where this portal was placed */
    UPROPERTY(BlueprintReadOnly, Category = "Portal")
    FString BiomeName;

    /** Chunk coordinate where this portal is located */
    UPROPERTY(BlueprintReadOnly, Category = "Portal")
    FIntVector ChunkCoordinate;

    /** Whether this portal has been successfully spawned */
    UPROPERTY(BlueprintReadOnly, Category = "Portal")
    bool bIsSpawned = false;

    /** Whether this portal is currently active */
    UPROPERTY(BlueprintReadOnly, Category = "Portal")
    bool bIsActive = true;

    /** Reference to the spawned portal actor (if any) */
    UPROPERTY(BlueprintReadOnly, Category = "Portal")
    TWeakObjectPtr<AActor> SpawnedActor;

    FDungeonPortal()
    {
        WorldLocation = FVector::ZeroVector;
        Rotation = FRotator::ZeroRotator;
        PortalTypeName = TEXT("");
        TargetLevelName = TEXT("");
        BiomeName = TEXT("");
        ChunkCoordinate = FIntVector::ZeroValue;
    }
};

/**
 * Result of a portal placement attempt
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FPortalPlacementResult
{
    GENERATED_BODY()

    /** Whether placement was successful */
    UPROPERTY(BlueprintReadOnly, Category = "Portal")
    bool bSuccess = false;

    /** The placed portal instance (if successful) */
    UPROPERTY(BlueprintReadOnly, Category = "Portal")
    FDungeonPortal PortalInstance;

    /** Reason for failure (if unsuccessful) */
    UPROPERTY(BlueprintReadOnly, Category = "Portal")
    FString FailureReason;

    /** Number of attempts made */
    UPROPERTY(BlueprintReadOnly, Category = "Portal")
    int32 AttemptsUsed = 0;
};

/**
 * Actor class for dungeon portal instances
 * Handles visual representation, interaction detection, and teleportation
 */
UCLASS(BlueprintType, Blueprintable)
class VIBEHEIM_API ADungeonPortalActor : public AActor
{
    GENERATED_BODY()

public:
    ADungeonPortalActor();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

public:
    /**
     * Initialize the portal with portal data
     * @param PortalData The portal instance data
     * @param SpawnRule The spawn rule used for this portal
     */
    UFUNCTION(BlueprintCallable, Category = "Portal")
    void InitializePortal(const FDungeonPortal& PortalData, const FPortalSpawnRule& SpawnRule);

    /**
     * Activate the portal for teleportation
     * @param InteractingActor The actor that triggered the portal
     */
    UFUNCTION(BlueprintCallable, Category = "Portal")
    void ActivatePortal(AActor* InteractingActor);

    /**
     * Set whether the portal is active
     * @param bActive Whether the portal should be active
     */
    UFUNCTION(BlueprintCallable, Category = "Portal")
    void SetPortalActive(bool bActive);

    /**
     * Get the portal data
     * @return The portal instance data
     */
    UFUNCTION(BlueprintCallable, Category = "Portal")
    const FDungeonPortal& GetPortalData() const { return PortalData; }

    /**
     * Get the spawn rule
     * @return The spawn rule for this portal
     */
    UFUNCTION(BlueprintCallable, Category = "Portal")
    const FPortalSpawnRule& GetSpawnRule() const { return SpawnRule; }

protected:
    /**
     * Handle interaction detection
     * @param OverlappedComponent The component that was overlapped
     * @param OtherActor The actor that entered the interaction area
     * @param OtherComp The component of the other actor
     * @param OtherBodyIndex Body index of the other actor
     * @param bFromSweep Whether this was from a sweep
     * @param SweepResult Sweep result data
     */
    UFUNCTION()
    void OnInteractionBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
                                  UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, 
                                  bool bFromSweep, const FHitResult& SweepResult);

    /**
     * Handle interaction end
     * @param OverlappedComponent The component that was overlapped
     * @param OtherActor The actor that left the interaction area
     * @param OtherComp The component of the other actor
     * @param OtherBodyIndex Body index of the other actor
     */
    UFUNCTION()
    void OnInteractionEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
                                UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    /**
     * Update visual effects based on portal state
     */
    void UpdateVisualEffects();

    /**
     * Perform the actual teleportation
     * @param TargetActor The actor to teleport
     */
    void PerformTeleportation(AActor* TargetActor);

protected:
    /** Root scene component */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> RootSceneComponent;

    /** Portal mesh component */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> PortalMeshComponent;

    /** Interaction detection sphere */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USphereComponent> InteractionSphere;

    /** Widget component for interaction prompts */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UWidgetComponent> InteractionWidget;

    /** Portal instance data */
    UPROPERTY(BlueprintReadOnly, Category = "Portal")
    FDungeonPortal PortalData;

    /** Spawn rule for this portal */
    UPROPERTY(BlueprintReadOnly, Category = "Portal")
    FPortalSpawnRule SpawnRule;

    /** Whether a player is currently in interaction range */
    UPROPERTY(BlueprintReadOnly, Category = "Portal")
    bool bPlayerInRange = false;

    /** Current interacting actor */
    UPROPERTY(BlueprintReadOnly, Category = "Portal")
    TWeakObjectPtr<AActor> CurrentInteractingActor;

    /** Animation timer for visual effects */
    float AnimationTimer = 0.0f;
};

/**
 * Dungeon portal placement and management system
 * Handles deterministic placement of dungeon portals as special POI types
 */
class VIBEHEIM_API FDungeonPortalSystem
{
public:
    FDungeonPortalSystem();
    ~FDungeonPortalSystem();

    /**
     * Initialize the dungeon portal system
     * @param Settings World generation settings
     * @param InNoiseGenerator Noise generator for deterministic placement
     * @param InBiomeSystem Biome system for biome evaluation
     * @param InPOISystem POI system for integration
     */
    void Initialize(const FWorldGenSettings& Settings, FNoiseGenerator* InNoiseGenerator, 
                   const FBiomeSystem* InBiomeSystem, FPOISystem* InPOISystem);

    /**
     * Generate dungeon portals for a specific chunk
     * @param ChunkCoordinate The chunk coordinate to generate portals for
     * @param World The world context for spawning actors
     * @return Array of placement results for this chunk
     */
    TArray<FPortalPlacementResult> GeneratePortalsForChunk(const FIntVector& ChunkCoordinate, UWorld* World);

    /**
     * Remove portals from a specific chunk (for chunk unloading)
     * @param ChunkCoordinate The chunk coordinate to remove portals from
     */
    void RemovePortalsFromChunk(const FIntVector& ChunkCoordinate);

    /**
     * Get all portal instances in a specific chunk
     * @param ChunkCoordinate The chunk coordinate to query
     * @return Array of portal instances in the chunk
     */
    TArray<FDungeonPortal> GetPortalsInChunk(const FIntVector& ChunkCoordinate) const;

    /**
     * Get all currently active portal instances
     * @return Array of all active portal instances
     */
    TArray<FDungeonPortal> GetAllActivePortals() const;

    /**
     * Add a custom portal spawn rule
     * @param SpawnRule The spawn rule to add
     */
    void AddPortalSpawnRule(const FPortalSpawnRule& SpawnRule);

    /**
     * Remove a portal spawn rule by type name
     * @param PortalTypeName The type name of the rule to remove
     * @return True if the rule was found and removed
     */
    bool RemovePortalSpawnRule(const FString& PortalTypeName);

    /**
     * Get all current portal spawn rules
     * @return Array of all spawn rules
     */
    TArray<FPortalSpawnRule> GetPortalSpawnRules() const;

    /**
     * Check if a location is valid for portal placement
     * @param Location World location to check
     * @param SpawnRule The spawn rule to validate against
     * @param OutFailureReason Reason for failure (if invalid)
     * @return True if the location is valid for placement
     */
    bool IsValidPortalLocation(const FVector& Location, const FPortalSpawnRule& SpawnRule, FString& OutFailureReason) const;

    /**
     * Get portal placement statistics
     * @param OutTotalAttempts Total placement attempts made
     * @param OutSuccessfulPlacements Number of successful placements
     * @param OutFailedPlacements Number of failed placements
     * @param OutAverageAttemptsPerPortal Average attempts per successful portal
     */
    void GetPlacementStats(int32& OutTotalAttempts, int32& OutSuccessfulPlacements, 
                          int32& OutFailedPlacements, float& OutAverageAttemptsPerPortal) const;

    /**
     * Clear all portal instances and reset the system
     */
    void Reset();

private:
    /**
     * Attempt to place a single portal of a specific type in a chunk
     * @param ChunkCoordinate The chunk to place the portal in
     * @param SpawnRule The spawn rule for this portal type
     * @param World The world context for spawning
     * @return Placement result
     */
    FPortalPlacementResult AttemptPortalPlacement(const FIntVector& ChunkCoordinate, const FPortalSpawnRule& SpawnRule, UWorld* World);

    /**
     * Find a valid location for portal placement within a chunk
     * @param ChunkCoordinate The chunk to search in
     * @param SpawnRule The spawn rule to validate against
     * @param OutLocation The found valid location (if successful)
     * @param OutRotation The rotation for the portal
     * @return True if a valid location was found
     */
    bool FindValidPortalLocation(const FIntVector& ChunkCoordinate, const FPortalSpawnRule& SpawnRule, 
                                FVector& OutLocation, FRotator& OutRotation);

    /**
     * Check if a location meets spacing requirements
     * @param Location The location to check
     * @param SpawnRule The spawn rule with spacing requirements
     * @return True if spacing requirements are met
     */
    bool CheckSpacingRequirements(const FVector& Location, const FPortalSpawnRule& SpawnRule) const;

    /**
     * Spawn the actual portal actor at a location
     * @param PortalInstance The portal instance data
     * @param SpawnRule The spawn rule for this portal
     * @param World The world context for spawning
     * @return The spawned portal actor, or nullptr if spawning failed
     */
    ADungeonPortalActor* SpawnPortalActor(const FDungeonPortal& PortalInstance, const FPortalSpawnRule& SpawnRule, UWorld* World);

    /**
     * Generate a deterministic random seed for portal placement in a chunk
     * @param ChunkCoordinate The chunk coordinate
     * @param PortalTypeName The portal type name
     * @return Deterministic seed for this chunk and portal type
     */
    uint64 GeneratePortalSeed(const FIntVector& ChunkCoordinate, const FString& PortalTypeName) const;

    /**
     * Convert world location to chunk coordinate
     * @param WorldLocation The world location
     * @return The chunk coordinate containing this location
     */
    FIntVector WorldLocationToChunkCoordinate(const FVector& WorldLocation) const;

    /**
     * Get the center world location of a chunk
     * @param ChunkCoordinate The chunk coordinate
     * @return The center world location of the chunk
     */
    FVector ChunkCoordinateToWorldLocation(const FIntVector& ChunkCoordinate) const;

private:
    /** World generation settings */
    FWorldGenSettings WorldGenSettings;

    /** Noise generator for deterministic placement */
    FNoiseGenerator* NoiseGenerator;

    /** Biome system for biome evaluation */
    const FBiomeSystem* BiomeSystem;

    /** POI system for integration */
    FPOISystem* POISystem;

    /** Array of portal spawn rules */
    TArray<FPortalSpawnRule> PortalSpawnRules;

    /** Map of chunk coordinates to portal instances */
    TMap<FIntVector, TArray<FDungeonPortal>> ChunkPortalMap;

    /** All active portal instances for quick lookup */
    TArray<FDungeonPortal> AllPortalInstances;

    /** Placement statistics */
    int32 TotalPlacementAttempts;
    int32 SuccessfulPlacements;
    int32 FailedPlacements;

    /** Whether the system has been initialized */
    bool bIsInitialized;
};