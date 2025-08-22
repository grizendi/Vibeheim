#include "Services/TileStreamingService.h"
#include "Services/HeightfieldService.h"
#include "Services/BiomeService.h"
#include "Services/PCGWorldService.h"
#include "Data/WorldGenTypes.h"
#include "Utils/WorldGenLogging.h"
#include "Engine/Engine.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/DateTime.h"

UTileStreamingService::UTileStreamingService()
{
	HeightfieldService = nullptr;
	BiomeService = nullptr;
	PCGWorldService = nullptr;
	MaxCacheSize = 81; // 9x9 grid as per task requirements (Generate=9, so max 81 tiles)
	CurrentTime = 0.0f;
	LastPlayerTileCoord = FTileCoord(INT32_MAX, INT32_MAX); // Initialize to invalid coord
}

bool UTileStreamingService::Initialize(const FWorldGenConfig& Settings, 
	UHeightfieldService* InHeightfieldService, 
	UBiomeService* InBiomeService, 
	UPCGWorldService* InPCGWorldService)
{
	WorldGenSettings = Settings;
	HeightfieldService = InHeightfieldService;
	BiomeService = InBiomeService;
	PCGWorldService = InPCGWorldService;

	if (!HeightfieldService || !BiomeService || !PCGWorldService)
	{
		WORLDGEN_LOG_WITH_SEED(Error, WorldGenSettings.Seed, TEXT("Failed to initialize TileStreamingService: Missing required services"));
		return false;
	}

	// Clear any existing cache
	TileCache.Empty();
	LRUList.Empty();

	// Reset performance metrics
	PerformanceMetrics = FTileStreamingMetrics();
	RecentGenerationTimes.Empty();

	WORLDGEN_LOG_WITH_SEED(Log, WorldGenSettings.Seed, TEXT("TileStreamingService initialized with cache size %d"), MaxCacheSize);
	return true;
}

void UTileStreamingService::UpdateStreaming(const FTileCoord& PlayerTileCoord)
{
	WORLDGEN_TIMER_WITH_CONTEXT("Streaming tick", WorldGenSettings.Seed, PlayerTileCoord);
	
	CurrentTime = FPlatformTime::Seconds();

	// Skip update if player hasn't moved significantly
	if (CalculateTileDistance(PlayerTileCoord, LastPlayerTileCoord) == 0 && LastPlayerTileCoord.X != INT32_MAX)
	{
		return;
	}

	LastPlayerTileCoord = PlayerTileCoord;

	WORLDGEN_LOG_WITH_SEED_TILE(Verbose, WorldGenSettings.Seed, PlayerTileCoord, TEXT("Updating streaming for player tile"));

	// Calculate required tiles based on streaming radii
	TArray<FTileCoord> ActiveTiles, LoadTiles, GenerateTiles;
	CalculateRequiredTiles(PlayerTileCoord, ActiveTiles, LoadTiles, GenerateTiles);

	// Process tile generation first
	ProcessTileGeneration(GenerateTiles);

	// Process tile loading
	ProcessTileLoading(LoadTiles);

	// Update tile states
	UpdateTileStates(PlayerTileCoord, ActiveTiles, LoadTiles);

	// Evict distant tiles
	EvictDistantTiles(PlayerTileCoord);

	// Update performance metrics
	UpdatePerformanceMetrics();

	WORLDGEN_LOG_WITH_SEED_TILE(Verbose, WorldGenSettings.Seed, PlayerTileCoord, TEXT("Streaming update complete: %d active, %d loaded, %d generated tiles"), 
		ActiveTiles.Num(), LoadTiles.Num(), GenerateTiles.Num());
}

void UTileStreamingService::CalculateRequiredTiles(const FTileCoord& PlayerTileCoord,
	TArray<FTileCoord>& OutActiveTiles,
	TArray<FTileCoord>& OutLoadTiles,
	TArray<FTileCoord>& OutGenerateTiles)
{
	int32 GenerateRadius = WorldGenSettings.GenerateRadius; // 9
	int32 LoadRadius = WorldGenSettings.LoadRadius;         // 5
	int32 ActiveRadius = WorldGenSettings.ActiveRadius;     // 3

	// Generate tiles within GenerateRadius (but not already cached)
	for (int32 X = PlayerTileCoord.X - GenerateRadius; X <= PlayerTileCoord.X + GenerateRadius; X++)
	{
		for (int32 Y = PlayerTileCoord.Y - GenerateRadius; Y <= PlayerTileCoord.Y + GenerateRadius; Y++)
		{
			FTileCoord TileCoord(X, Y);
			
			// Check if tile is in cache
			FTileStreamingData* CachedTile = TileCache.Find(TileCoord);
			if (!CachedTile || CachedTile->State == ETileState::Unloaded)
			{
				OutGenerateTiles.Add(TileCoord);
			}
		}
	}

	// Load tiles within LoadRadius
	for (int32 X = PlayerTileCoord.X - LoadRadius; X <= PlayerTileCoord.X + LoadRadius; X++)
	{
		for (int32 Y = PlayerTileCoord.Y - LoadRadius; Y <= PlayerTileCoord.Y + LoadRadius; Y++)
		{
			FTileCoord TileCoord(X, Y);
			OutLoadTiles.Add(TileCoord);
		}
	}

	// Active tiles within ActiveRadius
	for (int32 X = PlayerTileCoord.X - ActiveRadius; X <= PlayerTileCoord.X + ActiveRadius; X++)
	{
		for (int32 Y = PlayerTileCoord.Y - ActiveRadius; Y <= PlayerTileCoord.Y + ActiveRadius; Y++)
		{
			FTileCoord TileCoord(X, Y);
			OutActiveTiles.Add(TileCoord);
		}
	}
}

void UTileStreamingService::ProcessTileGeneration(const TArray<FTileCoord>& TilesToGenerate)
{
	for (const FTileCoord& TileCoord : TilesToGenerate)
	{
		// Skip if tile is already in cache or being generated
		FTileStreamingData* ExistingTile = TileCache.Find(TileCoord);
		if (ExistingTile && (ExistingTile->State == ETileState::Generating || 
			ExistingTile->State == ETileState::Generated ||
			ExistingTile->State == ETileState::Loaded ||
			ExistingTile->State == ETileState::Active))
		{
			continue;
		}

		// Generate the tile
		FTileStreamingData NewTileData(TileCoord);
		if (GenerateSingleTile(TileCoord, NewTileData))
		{
			NewTileData.State = ETileState::Generated;
			AddTileToCache(TileCoord, NewTileData);
			
			WORLDGEN_LOG_WITH_SEED_TILE(Verbose, WorldGenSettings.Seed, TileCoord, TEXT("Generated tile in %.2fms"), 
				NewTileData.GenerationTimeMs);
		}
		else
		{
			WORLDGEN_LOG_WITH_SEED_TILE(Warning, WorldGenSettings.Seed, TileCoord, TEXT("Failed to generate tile"));
		}
	}
}

void UTileStreamingService::ProcessTileLoading(const TArray<FTileCoord>& TilesToLoad)
{
	for (const FTileCoord& TileCoord : TilesToLoad)
	{
		FTileStreamingData* TileData = TileCache.Find(TileCoord);
		if (TileData && TileData->State == ETileState::Generated)
		{
			// Mark as loaded
			TileData->State = ETileState::Loaded;
			TileData->LastAccessTime = CurrentTime;
			UpdateLRUAccess(TileCoord);

			WORLDGEN_LOG_WITH_SEED_TILE(Verbose, WorldGenSettings.Seed, TileCoord, TEXT("Loaded tile"));
		}
	}
}

void UTileStreamingService::UpdateTileStates(const FTileCoord& PlayerTileCoord,
	const TArray<FTileCoord>& ActiveTiles,
	const TArray<FTileCoord>& LoadTiles)
{
	// Update active tiles
	for (const FTileCoord& TileCoord : ActiveTiles)
	{
		FTileStreamingData* TileData = TileCache.Find(TileCoord);
		if (TileData && (TileData->State == ETileState::Loaded || TileData->State == ETileState::Generated))
		{
			TileData->State = ETileState::Active;
			TileData->LastAccessTime = CurrentTime;
			UpdateLRUAccess(TileCoord);
		}
	}

	// Demote tiles outside active radius back to loaded
	for (auto& TilePair : TileCache)
	{
		FTileCoord TileCoord = TilePair.Key;
		FTileStreamingData& TileData = TilePair.Value;

		if (TileData.State == ETileState::Active)
		{
			if (!IsTileInRadius(TileCoord, PlayerTileCoord, WorldGenSettings.ActiveRadius))
			{
				TileData.State = ETileState::Loaded;
			}
		}
	}
}

void UTileStreamingService::EvictDistantTiles(const FTileCoord& PlayerTileCoord)
{
	TArray<FTileCoord> TilesToEvict;

	// Find tiles outside LoadRadius
	for (const auto& TilePair : TileCache)
	{
		FTileCoord TileCoord = TilePair.Key;
		if (!IsTileInRadius(TileCoord, PlayerTileCoord, WorldGenSettings.LoadRadius))
		{
			TilesToEvict.Add(TileCoord);
		}
	}

	// If cache is above max size, evict additional tiles using LRU
	if (TileCache.Num() > MaxCacheSize)
	{
		// Sort LRU list by access time (oldest first)
		LRUList.Sort([](const FLRUCacheEntry& A, const FLRUCacheEntry& B) {
			return A.LastAccessTime < B.LastAccessTime;
		});

		int32 TilesToRemove = TileCache.Num() - MaxCacheSize;
		for (int32 i = 0; i < FMath::Min(TilesToRemove, LRUList.Num()); i++)
		{
			FTileCoord TileCoord = LRUList[i].TileCoord;
			if (!TilesToEvict.Contains(TileCoord))
			{
				// Only evict if not within LoadRadius
				if (!IsTileInRadius(TileCoord, PlayerTileCoord, WorldGenSettings.LoadRadius))
				{
					TilesToEvict.Add(TileCoord);
				}
			}
		}
	}

	// Evict selected tiles
	for (const FTileCoord& TileCoord : TilesToEvict)
	{
		RemoveTileFromCache(TileCoord);
		PerformanceMetrics.TilesEvicted++;
		
		UE_LOG(LogTileStreaming, Verbose, TEXT("Evicted tile (%d, %d)"), TileCoord.X, TileCoord.Y);
	}
}

bool UTileStreamingService::GenerateSingleTile(const FTileCoord& TileCoord, FTileStreamingData& OutTileData)
{
	double StartTime = FPlatformTime::Seconds();

	try
	{
		// Set state to generating
		OutTileData.State = ETileState::Generating;
		OutTileData.TileCoord = TileCoord;

		// Generate heightfield
		OutTileData.HeightfieldData = HeightfieldService->GenerateHeightfield(WorldGenSettings.Seed, TileCoord);

		// Determine biome
		OutTileData.BiomeType = BiomeService->DetermineTileBiome(TileCoord, OutTileData.HeightfieldData.HeightData);

		// Generate PCG content
		FPCGGenerationData PCGData = PCGWorldService->GenerateBiomeContent(
			TileCoord, OutTileData.BiomeType, OutTileData.HeightfieldData.HeightData);
		
		OutTileData.bHasPCGContent = PCGData.TotalInstanceCount > 0;

		// Calculate generation time
		double EndTime = FPlatformTime::Seconds();
		OutTileData.GenerationTimeMs = static_cast<float>((EndTime - StartTime) * 1000.0);
		OutTileData.LastAccessTime = CurrentTime;

		// Record performance
		RecordGenerationTime(OutTileData.GenerationTimeMs);

		return true;
	}
	catch (...)
	{
		UE_LOG(LogTileStreaming, Error, TEXT("Exception during tile generation for (%d, %d)"), 
			TileCoord.X, TileCoord.Y);
		return false;
	}
}

bool UTileStreamingService::GetTileData(const FTileCoord& TileCoord, FTileStreamingData& OutTileData)
{
	FTileStreamingData* CachedTile = TileCache.Find(TileCoord);
	if (CachedTile)
	{
		// Update access time and LRU
		CachedTile->LastAccessTime = CurrentTime;
		UpdateLRUAccess(TileCoord);
		OutTileData = *CachedTile;
		PerformanceMetrics.CacheHits++;
		return true;
	}

	PerformanceMetrics.CacheMisses++;

	// Generate tile if not in cache
	if (GenerateSingleTile(TileCoord, OutTileData))
	{
		OutTileData.State = ETileState::Generated;
		AddTileToCache(TileCoord, OutTileData);
		return true;
	}

	return false;
}

bool UTileStreamingService::GenerateTile(const FTileCoord& TileCoord)
{
	FTileStreamingData TileData;
	return GetTileData(TileCoord, TileData);
}

void UTileStreamingService::AddTileToCache(const FTileCoord& TileCoord, const FTileStreamingData& TileData)
{
	// Add to cache
	TileCache.Add(TileCoord, TileData);

	// Update LRU list
	UpdateLRUAccess(TileCoord);

	// Update metrics
	PerformanceMetrics.GeneratedTiles++;
}

void UTileStreamingService::RemoveTileFromCache(const FTileCoord& TileCoord)
{
	TileCache.Remove(TileCoord);

	// Remove from LRU list
	LRUList.RemoveAll([&TileCoord](const FLRUCacheEntry& Entry) {
		return Entry.TileCoord == TileCoord;
	});
}

void UTileStreamingService::UpdateLRUAccess(const FTileCoord& TileCoord)
{
	// Remove existing entry
	LRUList.RemoveAll([&TileCoord](const FLRUCacheEntry& Entry) {
		return Entry.TileCoord == TileCoord;
	});

	// Add new entry at end (most recently used)
	LRUList.Emplace(TileCoord, CurrentTime);
}

int32 UTileStreamingService::CalculateTileDistance(const FTileCoord& TileA, const FTileCoord& TileB) const
{
	return FMath::Max(FMath::Abs(TileA.X - TileB.X), FMath::Abs(TileA.Y - TileB.Y));
}

bool UTileStreamingService::IsTileInRadius(const FTileCoord& TileCoord, const FTileCoord& CenterTile, int32 Radius) const
{
	return CalculateTileDistance(TileCoord, CenterTile) <= Radius;
}

bool UTileStreamingService::IsTileActive(const FTileCoord& TileCoord, const FTileCoord& PlayerTileCoord) const
{
	return IsTileInRadius(TileCoord, PlayerTileCoord, WorldGenSettings.ActiveRadius);
}

TArray<FTileCoord> UTileStreamingService::GetTilesInRadius(const FTileCoord& CenterTile, int32 Radius, ETileState MinState) const
{
	TArray<FTileCoord> Result;

	for (const auto& TilePair : TileCache)
	{
		const FTileCoord& TileCoord = TilePair.Key;
		const FTileStreamingData& TileData = TilePair.Value;

		if (IsTileInRadius(TileCoord, CenterTile, Radius) && static_cast<int32>(TileData.State) >= static_cast<int32>(MinState))
		{
			Result.Add(TileCoord);
		}
	}

	return Result;
}

FTileStreamingMetrics UTileStreamingService::GetPerformanceMetrics() const
{
	UpdatePerformanceMetrics();
	return PerformanceMetrics;
}

void UTileStreamingService::UpdatePerformanceMetrics() const
{
	// Count tiles by state
	PerformanceMetrics.ActiveTiles = 0;
	PerformanceMetrics.LoadedTiles = 0;
	PerformanceMetrics.GeneratedTiles = TileCache.Num();
	PerformanceMetrics.PendingGenerations = 0;

	for (const auto& TilePair : TileCache)
	{
		const FTileStreamingData& TileData = TilePair.Value;
		
		switch (TileData.State)
		{
			case ETileState::Active:
				PerformanceMetrics.ActiveTiles++;
				break;
			case ETileState::Loaded:
				PerformanceMetrics.LoadedTiles++;
				break;
			case ETileState::Generating:
				PerformanceMetrics.PendingGenerations++;
				break;
			default:
				break;
		}
	}

	// Calculate cache efficiency
	int32 TotalAccesses = PerformanceMetrics.CacheHits + PerformanceMetrics.CacheMisses;
	if (TotalAccesses > 0)
	{
		PerformanceMetrics.CacheEfficiency = static_cast<float>(PerformanceMetrics.CacheHits) / TotalAccesses;
	}
}

void UTileStreamingService::RecordGenerationTime(float GenerationTimeMs)
{
	RecentGenerationTimes.Add(GenerationTimeMs);
	
	// Keep only recent times
	if (RecentGenerationTimes.Num() > MaxRecentTimes)
	{
		RecentGenerationTimes.RemoveAt(0);
	}

	// Update performance metrics
	if (RecentGenerationTimes.Num() > 0)
	{
		float Total = 0.0f;
		float Peak = 0.0f;
		
		for (float Time : RecentGenerationTimes)
		{
			Total += Time;
			Peak = FMath::Max(Peak, Time);
		}
		
		PerformanceMetrics.AverageGenerationTimeMs = Total / RecentGenerationTimes.Num();
		PerformanceMetrics.PeakGenerationTimeMs = Peak;
	}
}

void UTileStreamingService::ClearTileCache()
{
	TileCache.Empty();
	LRUList.Empty();
	PerformanceMetrics = FTileStreamingMetrics();
	RecentGenerationTimes.Empty();
	
	UE_LOG(LogTileStreaming, Log, TEXT("Tile cache cleared"));
}
