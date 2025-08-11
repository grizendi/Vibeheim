#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Data/WorldGenSettings.h"
#include "NoiseGenerator.h"
#include "BiomeSystem.h"
#include "POISystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPOISystem, Log, All);

/**
 * Defines spawn rules for a specific POI type
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FPOISpawnRule
{
    GENERATED_BODY()

    /** Name of the POI type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI")
    FString POITypeName;

    /** Minimum distance between POIs of this type (in meters) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI", meta = (ClampMin = "50.0", ClampMax = "1000.0"))
    float MinSpacing = 150.0f;

    /** Maximum slope angle for placement (in degrees) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI", meta = (ClampMin = "0.0", ClampMax = "45.0"))
    float MaxSlope = 20.0f;

    /** Minimum altitude for placement (in meters) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI")
    float MinAltitude = -100.0f;

    /** Maximum altitude for placement (in meters) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI")
    float MaxAltitude = 1000.0f;

    /** Minimum distance from water surface (in meters) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI")
    float MinWaterlineClearance = 5.0f;

    /** Spawn probability (0.0 to 1.0) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float SpawnProbability = 0.1f;

    /** Biomes where this POI can spawn */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI")
    TArray<FString> AllowedBiomes;

    /** Path to the prefab asset for this POI */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI")
    FSoftObjectPath PrefabAssetPath;

    /** Terrain flattening radius around POI (in meters) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI", meta = (ClampMin = "0.0", ClampMax = "50.0"))
    float FlattenRadius = 10.0f;

    /** Maximum number of retry attempts for placement */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI", meta = (ClampMin = "1", ClampMax = "10"))
    int32 MaxRetryAttempts = 5;

    FPOISpawnRule()
    {
        POITypeName = TEXT("DefaultPOI");
        AllowedBiomes.Add(TEXT("Meadows"));
    }
};

/**
 * Represents a placed POI instance in the world
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FPOIInstance
{
    GENERATED_BODY()

    /** World location of the POI */
    UPROPERTY(BlueprintReadOnly, Category = "POI")
    FVector WorldLocation;

    /** Rotation of the POI */
    UPROPERTY(BlueprintReadOnly, Category = "POI")
    FRotator Rotation;

    /** Type name of the POI */
    UPROPERTY(BlueprintReadOnly, Category = "POI")
    FString POITypeName;

    /** Biome where this POI was placed */
    UPROPERTY(BlueprintReadOnly, Category = "POI")
    FString BiomeName;

    /** Chunk coordinate where this POI is located */
    UPROPERTY(BlueprintReadOnly, Category = "POI")
    FIntVector ChunkCoordinate;

    /** Whether this POI has been successfully spawned */
    UPROPERTY(BlueprintReadOnly, Category = "POI")
    bool bIsSpawned = false;

    /** Reference to the spawned actor (if any) */
    UPROPERTY(BlueprintReadOnly, Category = "POI")
    TWeakObjectPtr<AActor> SpawnedActor;

    FPOIInstance()
    {
        WorldLocation = FVector::ZeroVector;
        Rotation = FRotator::ZeroRotator;
        POITypeName = TEXT("");
        BiomeName = TEXT("");
        ChunkCoordinate = FIntVector::ZeroValue;
    }
};

/**
 * Result of a POI placement attempt
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FPOIPlacementResult
{
    GENERATED_BODY()

    /** Whether placement was successful */
    UPROPERTY(BlueprintReadOnly, Category = "POI")
    bool bSuccess = false;

    /** The placed POI instance (if successful) */
    UPROPERTY(BlueprintReadOnly, Category = "POI")
    FPOIInstance POIInstance;

    /** Reason for failure (if unsuccessful) */
    UPROPERTY(BlueprintReadOnly, Category = "POI")
    FString FailureReason;

    /** Number of attempts made */
    UPROPERTY(BlueprintReadOnly, Category = "POI")
    int32 AttemptsUsed = 0;
};

/**
 * Point of Interest placement and management system
 * Handles deterministic placement of POIs throughout the world using placement rules
 */
class VIBEHEIM_API FPOISystem
{
public:
    FPOISystem();
    ~FPOISystem();

    /**
     * Initialize the POI system with world generation settings
     * @param Settings World generation settings containing POI configuration
     * @param InNoiseGenerator Noise generator for deterministic placement
     * @param InBiomeSystem Biome system for biome evaluation
     */
    void Initialize(const FWorldGenSettings& Settings, FNoiseGenerator* InNoiseGenerator, const FBiomeSystem* InBiomeSystem);

    /**
     * Generate POIs for a specific chunk
     * @param ChunkCoordinate The chunk coordinate to generate POIs for
     * @param World The world context for spawning actors
     * @return Array of placement results for this chunk
     */
    TArray<FPOIPlacementResult> GeneratePOIsForChunk(const FIntVector& ChunkCoordinate, UWorld* World);

    /**
     * Remove POIs from a specific chunk (for chunk unloading)
     * @param ChunkCoordinate The chunk coordinate to remove POIs from
     */
    void RemovePOIsFromChunk(const FIntVector& ChunkCoordinate);

    /**
     * Get all POI instances in a specific chunk
     * @param ChunkCoordinate The chunk coordinate to query
     * @return Array of POI instances in the chunk
     */
    TArray<FPOIInstance> GetPOIsInChunk(const FIntVector& ChunkCoordinate) const;

    /**
     * Get all currently active POI instances
     * @return Array of all active POI instances
     */
    TArray<FPOIInstance> GetAllActivePOIs() const;

    /**
     * Add a custom POI spawn rule
     * @param SpawnRule The spawn rule to add
     */
    void AddPOISpawnRule(const FPOISpawnRule& SpawnRule);

    /**
     * Remove a POI spawn rule by type name
     * @param POITypeName The type name of the rule to remove
     * @return True if the rule was found and removed
     */
    bool RemovePOISpawnRule(const FString& POITypeName);

    /**
     * Get all current POI spawn rules
     * @return Array of all spawn rules
     */
    TArray<FPOISpawnRule> GetPOISpawnRules() const;

    /**
     * Check if a location is valid for POI placement
     * @param Location World location to check
     * @param SpawnRule The spawn rule to validate against
     * @param OutFailureReason Reason for failure (if invalid)
     * @return True if the location is valid for placement
     */
    bool IsValidPOILocation(const FVector& Location, const FPOISpawnRule& SpawnRule, FString& OutFailureReason) const;

    /**
     * Get POI placement statistics
     * @param OutTotalAttempts Total placement attempts made
     * @param OutSuccessfulPlacements Number of successful placements
     * @param OutFailedPlacements Number of failed placements
     * @param OutAverageAttemptsPerPOI Average attempts per successful POI
     */
    void GetPlacementStats(int32& OutTotalAttempts, int32& OutSuccessfulPlacements, 
                          int32& OutFailedPlacements, float& OutAverageAttemptsPerPOI) const;

    /**
     * Clear all POI instances and reset the system
     */
    void Reset();

private:
    /**
     * Attempt to place a single POI of a specific type in a chunk
     * @param ChunkCoordinate The chunk to place the POI in
     * @param SpawnRule The spawn rule for this POI type
     * @param World The world context for spawning
     * @return Placement result
     */
    FPOIPlacementResult AttemptPOIPlacement(const FIntVector& ChunkCoordinate, const FPOISpawnRule& SpawnRule, UWorld* World);

    /**
     * Find a valid location for POI placement within a chunk
     * @param ChunkCoordinate The chunk to search in
     * @param SpawnRule The spawn rule to validate against
     * @param OutLocation The found valid location (if successful)
     * @param OutRotation The rotation for the POI
     * @return True if a valid location was found
     */
    bool FindValidPOILocation(const FIntVector& ChunkCoordinate, const FPOISpawnRule& SpawnRule, 
                             FVector& OutLocation, FRotator& OutRotation);

    /**
     * Check if a location meets spacing requirements
     * @param Location The location to check
     * @param SpawnRule The spawn rule with spacing requirements
     * @return True if spacing requirements are met
     */
    bool CheckSpacingRequirements(const FVector& Location, const FPOISpawnRule& SpawnRule) const;

    /**
     * Check terrain slope at a location
     * @param Location The location to check
     * @param MaxSlope Maximum allowed slope in degrees
     * @return True if slope is within acceptable range
     */
    bool CheckTerrainSlope(const FVector& Location, float MaxSlope) const;

    /**
     * Check altitude requirements for a location
     * @param Location The location to check
     * @param MinAltitude Minimum altitude requirement
     * @param MaxAltitude Maximum altitude requirement
     * @return True if altitude is within range
     */
    bool CheckAltitudeRequirements(const FVector& Location, float MinAltitude, float MaxAltitude) const;

    /**
     * Check waterline clearance requirements
     * @param Location The location to check
     * @param MinClearance Minimum clearance from water surface
     * @return True if clearance requirements are met
     */
    bool CheckWaterlineClearance(const FVector& Location, float MinClearance) const;

    /**
     * Flatten terrain around a POI location
     * @param Location The center location for flattening
     * @param Radius The radius to flatten
     * @param World The world context
     */
    void FlattenTerrainAroundPOI(const FVector& Location, float Radius, UWorld* World);

    /**
     * Spawn the actual POI prefab at a location
     * @param POIInstance The POI instance data
     * @param SpawnRule The spawn rule for this POI
     * @param World The world context for spawning
     * @return The spawned actor, or nullptr if spawning failed
     */
    AActor* SpawnPOIPrefab(const FPOIInstance& POIInstance, const FPOISpawnRule& SpawnRule, UWorld* World);

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

    /**
     * Generate a deterministic random seed for POI placement in a chunk
     * @param ChunkCoordinate The chunk coordinate
     * @param POITypeName The POI type name
     * @return Deterministic seed for this chunk and POI type
     */
    uint64 GeneratePOISeed(const FIntVector& ChunkCoordinate, const FString& POITypeName) const;

private:
    /** World generation settings */
    FWorldGenSettings WorldGenSettings;

    /** Noise generator for deterministic placement */
    FNoiseGenerator* NoiseGenerator;

    /** Biome system for biome evaluation */
    const FBiomeSystem* BiomeSystem;

    /** Array of POI spawn rules */
    TArray<FPOISpawnRule> POISpawnRules;

    /** Map of chunk coordinates to POI instances */
    TMap<FIntVector, TArray<FPOIInstance>> ChunkPOIMap;

    /** All active POI instances for quick lookup */
    TArray<FPOIInstance> AllPOIInstances;

    /** Placement statistics */
    int32 TotalPlacementAttempts;
    int32 SuccessfulPlacements;
    int32 FailedPlacements;

    /** Whether the system has been initialized */
    bool bIsInitialized;
};