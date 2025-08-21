#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/Object.h"
#include "WorldGenManager.generated.h"

// Forward declarations
class UWorldGenSettings;
class UHeightfieldService;
class UClimateSystem;
class UBiomeService;
class UPCGWorldService;
class UTileStreamingService;
struct FTileCoord;

/**
 * World Generation Manager responsible for coordinating all world generation systems
 * Handles initialization, streaming, and performance monitoring
 */
UCLASS(Blueprintable, BlueprintType)
class VIBEHEIM_API AWorldGenManager : public AActor
{
    GENERATED_BODY()

public:
    AWorldGenManager();

    // AActor interface
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    /**
     * Initialize all world generation systems
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    bool InitializeWorldGenSystems();

    /**
     * Update streaming based on player position
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    void UpdateWorldStreaming();

    /**
     * Get current world generation performance statistics
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    void GetWorldGenPerformanceStats(
        UPARAM(ref) float& OutTileGenerationTimeMs, 
        UPARAM(ref) float& OutPCGGenerationTimeMs, 
        UPARAM(ref) int32& OutLoadedTiles, 
        UPARAM(ref) int32& OutPendingLoads
    );

    /**
     * Handle world generation error fallback
     */
    UFUNCTION(BlueprintCallable, Category = "World Generation")
    void HandleWorldGenerationError(const FString& ErrorMessage);

protected:
    // World generation configuration
    UPROPERTY(BlueprintReadOnly, Category = "World Generation")
    UWorldGenSettings* WorldGenSettings;

    // Core world generation services
    UPROPERTY(BlueprintReadOnly, Category = "World Generation")
    UHeightfieldService* HeightfieldService;

    UPROPERTY(BlueprintReadOnly, Category = "World Generation")
    UClimateSystem* ClimateSystem;

    UPROPERTY(BlueprintReadOnly, Category = "World Generation")
    UBiomeService* BiomeService;

    UPROPERTY(BlueprintReadOnly, Category = "World Generation")
    UPCGWorldService* PCGWorldService;

    UPROPERTY(BlueprintReadOnly, Category = "World Generation")
    UTileStreamingService* TileStreamingService;

    // Streaming parameters
    UPROPERTY(BlueprintReadOnly, Category = "World Generation")
    FVector LastPlayerPosition;

    UPROPERTY(BlueprintReadOnly, Category = "World Generation")
    float StreamingUpdateInterval;

    UPROPERTY(BlueprintReadOnly, Category = "World Generation")
    float LastStreamingUpdateTime;

    // Performance tracking
    UPROPERTY(BlueprintReadOnly, Category = "World Generation")
    float TotalTileGenerationTime;

    UPROPERTY(BlueprintReadOnly, Category = "World Generation")
    float TotalPCGGenerationTime;

    UPROPERTY(BlueprintReadOnly, Category = "World Generation")
    int32 TotalTilesGenerated;

private:
    /**
     * Calculate player's current tile coordinate
     */
    FTileCoord GetPlayerTileCoordinate() const;

    /**
     * Determine tiles that need to be generated or loaded
     */
    TArray<FTileCoord> CalculateTilesToGenerate(const FTileCoord& PlayerTileCoord);

    /**
     * Generate tiles around the player
     */
    void GenerateSurroundingTiles(const TArray<FTileCoord>& TilesToGenerate);

    /**
     * Update performance tracking metrics
     */
    void UpdatePerformanceMetrics(float TileGenTime, float PCGGenTime);
};
