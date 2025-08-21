#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/WorldGenTypes.h"
#include "Services/HeightfieldService.h"
#include "TileStreamingService.generated.h"

// Forward declarations
class UHeightfieldService;
class UBiomeService;
class UPCGWorldService;

/**
 * Tile state enumeration for streaming system
 */
UENUM(BlueprintType)
enum class ETileState : uint8
{
	Unloaded,    // Tile is not in memory
	Generating,  // Tile is currently being generated
	Generated,   // Tile has been generated but not loaded
	Loading,     // Tile is being loaded into memory
	Loaded,      // Tile is loaded and cached
	Active       // Tile is actively being used (within ActiveRadius)
};

/**
 * Tile streaming data structure
 */
USTRUCT()
struct VIBEHEIM_API FTileStreamingData
{
	GENERATED_BODY()

	UPROPERTY()
	FTileCoord TileCoord;

	UPROPERTY()
	ETileState State = ETileState::Unloaded;

	UPROPERTY()
	FHeightfieldData HeightfieldData;

	UPROPERTY()
	EBiomeType BiomeType = EBiomeType::Meadows;

	UPROPERTY()
	float LastAccessTime = 0.0f;

	UPROPERTY()
	float GenerationTimeMs = 0.0f;

	UPROPERTY()
	bool bHasPCGContent = false;

	FTileStreamingData() = default;
	FTileStreamingData(const FTileCoord& InTileCoord) : TileCoord(InTileCoord) {}
};

/**
 * LRU cache entry for efficient eviction
 */
struct VIBEHEIM_API FLRUCacheEntry
{
	FTileCoord TileCoord;
	float LastAccessTime;
	
	FLRUCacheEntry(const FTileCoord& InTileCoord, float InLastAccessTime)
		: TileCoord(InTileCoord), LastAccessTime(InLastAccessTime) {}
};

/**
 * Performance metrics for tile streaming
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FTileStreamingMetrics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 ActiveTiles = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 LoadedTiles = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 GeneratedTiles = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 PendingGenerations = 0;

	UPROPERTY(BlueprintReadOnly)
	float AverageGenerationTimeMs = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float PeakGenerationTimeMs = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	int32 CacheHits = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 CacheMisses = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 TilesEvicted = 0;

	UPROPERTY(BlueprintReadOnly)
	float CacheEfficiency = 0.0f; // CacheHits / (CacheHits + CacheMisses)
};

/**
 * Tile streaming service with LRU cache and radius-based loading
 * Manages procedural tile generation, loading, and eviction
 */
UCLASS(BlueprintType)
class VIBEHEIM_API UTileStreamingService : public UObject
{
	GENERATED_BODY()

public:
	UTileStreamingService();

	/**
	 * Initialize the tile streaming system
	 */
	UFUNCTION(BlueprintCallable, Category = "Tile Streaming")
	bool Initialize(const FWorldGenConfig& Settings, 
		UHeightfieldService* InHeightfieldService, 
		UBiomeService* InBiomeService, 
		UPCGWorldService* InPCGWorldService);

	/**
	 * Update streaming based on player position
	 */
	UFUNCTION(BlueprintCallable, Category = "Tile Streaming")
	void UpdateStreaming(const FTileCoord& PlayerTileCoord);

	/**
	 * Get tile data if available (cached or generate if needed)
	 */
	UFUNCTION(Category = "Tile Streaming")
	bool GetTileData(const FTileCoord& TileCoord, FTileStreamingData& OutTileData);

	/**
	 * Force generation of a specific tile
	 */
	UFUNCTION(BlueprintCallable, Category = "Tile Streaming")
	bool GenerateTile(const FTileCoord& TileCoord);

	/**
	 * Get current streaming performance metrics
	 */
	UFUNCTION(BlueprintCallable, Category = "Tile Streaming")
	FTileStreamingMetrics GetPerformanceMetrics() const;

	/**
	 * Clear all cached tiles
	 */
	UFUNCTION(BlueprintCallable, Category = "Tile Streaming")
	void ClearTileCache();

	/**
	 * Get tiles within specified radius and state
	 */
	UFUNCTION(BlueprintCallable, Category = "Tile Streaming")
	TArray<FTileCoord> GetTilesInRadius(const FTileCoord& CenterTile, int32 Radius, ETileState MinState = ETileState::Loaded) const;

	/**
	 * Check if a tile is within active radius
	 */
	UFUNCTION(BlueprintCallable, Category = "Tile Streaming")
	bool IsTileActive(const FTileCoord& TileCoord, const FTileCoord& PlayerTileCoord) const;

private:
	UPROPERTY()
	FWorldGenConfig WorldGenSettings;

	// Service references
	UPROPERTY()
	UHeightfieldService* HeightfieldService;

	UPROPERTY()
	UBiomeService* BiomeService;

	UPROPERTY()
	UPCGWorldService* PCGWorldService;

	// Tile cache and LRU management
	UPROPERTY()
	TMap<FTileCoord, FTileStreamingData> TileCache;

	TArray<FLRUCacheEntry> LRUList;
	int32 MaxCacheSize;

	// Current streaming state
	FTileCoord LastPlayerTileCoord;
	float CurrentTime;

	// Performance metrics
	mutable FTileStreamingMetrics PerformanceMetrics;
	TArray<float> RecentGenerationTimes;
	static const int32 MaxRecentTimes = 100;

	/**
	 * Calculate which tiles need to be in each state based on player position
	 */
	void CalculateRequiredTiles(const FTileCoord& PlayerTileCoord,
		TArray<FTileCoord>& OutActiveTiles,
		TArray<FTileCoord>& OutLoadTiles,
		TArray<FTileCoord>& OutGenerateTiles);

	/**
	 * Process tiles that need to be generated
	 */
	void ProcessTileGeneration(const TArray<FTileCoord>& TilesToGenerate);

	/**
	 * Process tiles that need to be loaded
	 */
	void ProcessTileLoading(const TArray<FTileCoord>& TilesToLoad);

	/**
	 * Update tile states based on player position
	 */
	void UpdateTileStates(const FTileCoord& PlayerTileCoord,
		const TArray<FTileCoord>& ActiveTiles,
		const TArray<FTileCoord>& LoadTiles);

	/**
	 * Evict tiles that are outside the load radius using LRU policy
	 */
	void EvictDistantTiles(const FTileCoord& PlayerTileCoord);

	/**
	 * Generate heightfield and content for a single tile
	 */
	bool GenerateSingleTile(const FTileCoord& TileCoord, FTileStreamingData& OutTileData);

	/**
	 * Update LRU list when a tile is accessed
	 */
	void UpdateLRUAccess(const FTileCoord& TileCoord);

	/**
	 * Add tile to LRU cache with eviction if necessary
	 */
	void AddTileToCache(const FTileCoord& TileCoord, const FTileStreamingData& TileData);

	/**
	 * Remove tile from cache and LRU list
	 */
	void RemoveTileFromCache(const FTileCoord& TileCoord);

	/**
	 * Calculate distance between two tiles
	 */
	int32 CalculateTileDistance(const FTileCoord& TileA, const FTileCoord& TileB) const;

	/**
	 * Update performance metrics
	 */
	void UpdatePerformanceMetrics() const;

	/**
	 * Record generation time for performance tracking
	 */
	void RecordGenerationTime(float GenerationTimeMs);

	/**
	 * Check if tile is within specified radius
	 */
	bool IsTileInRadius(const FTileCoord& TileCoord, const FTileCoord& CenterTile, int32 Radius) const;
};
