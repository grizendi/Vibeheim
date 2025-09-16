#include "Services/TileStreamingService.h"
#include "Algo/Sort.h"
#include "Services/HeightfieldService.h"
#include "Services/BiomeService.h"
#include "Services/PCGWorldService.h"
#include "VHMTerrainRendering/VHMTerrainRenderer.h"
#include "Data/WorldGenTypes.h"
#include "Utils/WorldGenLogging.h"
#include "WorldGenSettings.h"
#include "Engine/Engine.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/App.h"

DEFINE_LOG_CATEGORY_STATIC(LogTileStreaming, Log, All);

UTileStreamingService::UTileStreamingService()
{
	HeightfieldService = nullptr;
	BiomeService = nullptr;
	PCGWorldService = nullptr;
	VHMTerrainRenderer = nullptr;
	MaxCacheSize = 81; // 9x9 grid as per task requirements (Generate=9, so max 81 tiles)
	CurrentTime = 0.0f;
	LastPlayerTileCoord = FTileCoord(INT32_MAX, INT32_MAX); // Initialize to invalid coord
	StreamingBudgets = FStreamingBudgetsConfig();
	bHasStreamingBudgets = true;
	CurrentBudgets.Clear();
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

	// Enhanced defensive checks with detailed error messages
	if (!HeightfieldService || !BiomeService || !PCGWorldService)
	{
		UE_LOG(LogTileStreaming, Error, TEXT("Failed to initialize TileStreamingService - missing required services: HeightfieldService=%s, BiomeService=%s, PCGWorldService=%s"),
			HeightfieldService ? TEXT("OK") : TEXT("NULL"),
			BiomeService ? TEXT("OK") : TEXT("NULL"),
			PCGWorldService ? TEXT("OK") : TEXT("NULL"));
		WORLDGEN_LOG_WITH_SEED(Error, WorldGenSettings.Seed, TEXT("Failed to initialize TileStreamingService: Missing required services"));
		return false;
	}

	// Clear any existing cache
	TileCache.Empty();
	LRUList.Empty();

	// Clear pending work queues
	FTileGenerationTask DummyTask;
	while (RequestedGenerationQueue.Dequeue(DummyTask)) {}
	while (PrefetchGenerationQueue.Dequeue(DummyTask)) {}
	EnqueuedTiles.Empty();
	EnqueuedPrefetchTiles.Empty();

	// Resolve streaming budgets from world settings (if provided)
	StreamingBudgets = FStreamingBudgetsConfig();
	bHasStreamingBudgets = true;
	if (const UWorldGenSettings* WGSettings = UWorldGenSettings::GetWorldGenSettings())
	{
		if (WGSettings->StreamingBudgetsConfig.IsSet())
		{
			StreamingBudgets = WGSettings->StreamingBudgetsConfig.GetValue();
		}
	}
	CurrentBudgets.Clear();

	// Ensure cache size respects configured limits
	MaxCacheSize = FMath::Max(MaxCacheSize, StreamingBudgets.MaxActiveTiles);

    // Reset performance metrics
    PerformanceMetrics = FTileStreamingMetrics();
    RecentGenerationTimes.Empty();
    FrameTimeSamples.Empty();
    ErrorEntries.Empty();

	WORLDGEN_LOG_WITH_SEED(Log, WorldGenSettings.Seed, TEXT("TileStreamingService initialized with cache size %d"), MaxCacheSize);
	return true;
}

void UTileStreamingService::UpdateStreaming(const FTileCoord& PlayerTileCoord)
{
	// Defensive programming guards - ensure services are initialized before streaming
	ensureMsgf(HeightfieldService != nullptr, 
		TEXT("UTileStreamingService: HeightfieldService is null. Call Initialize() before using UpdateStreaming()."));
	ensureMsgf(BiomeService != nullptr, 
		TEXT("UTileStreamingService: BiomeService is null. Call Initialize() before using UpdateStreaming()."));
	ensureMsgf(PCGWorldService != nullptr, 
		TEXT("UTileStreamingService: PCGWorldService is null. Call Initialize() before using UpdateStreaming()."));

	// Early return if services are not properly initialized
	if (!HeightfieldService || !BiomeService || !PCGWorldService)
	{
		UE_LOG(LogTileStreaming, Error, TEXT("UTileStreamingService::UpdateStreaming failed - missing required services. Skipping streaming update."));
		return;
	}

	WORLDGEN_TIMER_WITH_CONTEXT("Streaming tick", WorldGenSettings.Seed, PlayerTileCoord);
	
    CurrentTime = FPlatformTime::Seconds();
    SampleFrameTime();
	ResetBudgetsForTick();

	const bool bHasPendingWork = EnqueuedTiles.Num() > 0 || EnqueuedPrefetchTiles.Num() > 0;
	// Skip update if player hasn't moved significantly and no pending generation work
	if (!bHasPendingWork && CalculateTileDistance(PlayerTileCoord, LastPlayerTileCoord) == 0 && LastPlayerTileCoord.X != INT32_MAX)
	{
		return;
	}

	LastPlayerTileCoord = PlayerTileCoord;

	WORLDGEN_LOG_WITH_SEED_TILE(Verbose, WorldGenSettings.Seed, PlayerTileCoord, TEXT("Updating streaming for player tile"));

	// Calculate required tiles based on streaming radii
	TArray<FTileCoord> ActiveTiles, LoadTiles, GenerateTiles, PrefetchTiles;
	CalculateRequiredTiles(PlayerTileCoord, ActiveTiles, LoadTiles, GenerateTiles, PrefetchTiles);

	// Process tile generation first
	ProcessTileGeneration(GenerateTiles);
	ProcessPrefetchGeneration(PrefetchTiles);
	DrainGenerationQueues();

	// Process tile loading
	ProcessTileLoading(LoadTiles);

	// Update tile states
	UpdateTileStates(PlayerTileCoord, ActiveTiles, LoadTiles);

	// Evict distant tiles
	EvictDistantTiles(PlayerTileCoord);

	// Notify VHM renderer about tile streaming events
	NotifyVHMRenderer(ActiveTiles, LoadTiles);

	// Update performance metrics
	UpdatePerformanceMetrics();

	WORLDGEN_LOG_WITH_SEED_TILE(Verbose, WorldGenSettings.Seed, PlayerTileCoord, TEXT("Streaming update complete: %d active, %d loaded, %d generated tiles"), 
		ActiveTiles.Num(), LoadTiles.Num(), GenerateTiles.Num());
}

void UTileStreamingService::CalculateRequiredTiles(const FTileCoord& PlayerTileCoord,
	TArray<FTileCoord>& OutActiveTiles,
	TArray<FTileCoord>& OutLoadTiles,
	TArray<FTileCoord>& OutGenerateTiles,
	TArray<FTileCoord>& OutPrefetchTiles)
{
	int32 GenerateRadius = WorldGenSettings.GenerateRadius; // 9
	int32 LoadRadius = WorldGenSettings.LoadRadius;         // 5
	int32 ActiveRadius = WorldGenSettings.ActiveRadius;     // 3

	TArray<FTileCoord> GenerateCandidates;
	GenerateCandidates.Reserve(FMath::Square(GenerateRadius * 2 + 1));

	for (int32 X = PlayerTileCoord.X - GenerateRadius; X <= PlayerTileCoord.X + GenerateRadius; X++)
	{
		for (int32 Y = PlayerTileCoord.Y - GenerateRadius; Y <= PlayerTileCoord.Y + GenerateRadius; Y++)
		{
			FTileCoord TileCoord(X, Y);
			FTileStreamingData* CachedTile = TileCache.Find(TileCoord);
			if (!CachedTile || CachedTile->State == ETileState::Unloaded)
			{
				GenerateCandidates.Add(TileCoord);
			}
		}
	}

	// Prioritise closer tiles so the player sees results quickly
	Algo::Sort(GenerateCandidates, [this, &PlayerTileCoord](const FTileCoord& A, const FTileCoord& B)
	{
		const int32 DistA = CalculateTileDistance(A, PlayerTileCoord);
		const int32 DistB = CalculateTileDistance(B, PlayerTileCoord);
		if (DistA != DistB)
		{
			return DistA < DistB;
		}
		if (A.X != B.X)
		{
			return A.X < B.X;
		}
		return A.Y < B.Y;
	});

	OutGenerateTiles = MoveTemp(GenerateCandidates);

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

	// Prefetch tiles just beyond the generate radius for smoother transitions
	if (StreamingBudgets.PrefetchRings > 0)
	{
		const int32 PrefetchRadius = GenerateRadius + StreamingBudgets.PrefetchRings;
		for (int32 X = PlayerTileCoord.X - PrefetchRadius; X <= PlayerTileCoord.X + PrefetchRadius; ++X)
		{
			for (int32 Y = PlayerTileCoord.Y - PrefetchRadius; Y <= PlayerTileCoord.Y + PrefetchRadius; ++Y)
			{
				FTileCoord TileCoord(X, Y);
				const int32 Distance = CalculateTileDistance(TileCoord, PlayerTileCoord);
				if (Distance > GenerateRadius && Distance <= PrefetchRadius)
				{
					OutPrefetchTiles.Add(TileCoord);
				}
			}
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

		EnqueueGenerationTask(TileCoord, /*bIsPrefetch*/false);
	}
}

void UTileStreamingService::ProcessPrefetchGeneration(const TArray<FTileCoord>& TilesToPrefetch)
{
	if (TilesToPrefetch.Num() == 0)
	{
		return;
	}

	for (const FTileCoord& TileCoord : TilesToPrefetch)
	{
		// Skip if tile already requested or cached in a ready state
		if (EnqueuedTiles.Contains(TileCoord))
		{
			continue; // It will be generated in the high-priority queue
		}

		FTileStreamingData* ExistingTile = TileCache.Find(TileCoord);
		if (ExistingTile && (ExistingTile->State == ETileState::Generating ||
			ExistingTile->State == ETileState::Generated ||
			ExistingTile->State == ETileState::Loaded ||
			ExistingTile->State == ETileState::Active))
		{
			continue;
		}

		EnqueueGenerationTask(TileCoord, /*bIsPrefetch*/true);
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

void UTileStreamingService::EnqueueGenerationTask(const FTileCoord& TileCoord, bool bIsPrefetch)
{
	if (!bIsPrefetch)
	{
		EnqueuedPrefetchTiles.Remove(TileCoord);
	}

	TSet<FTileCoord>& PendingSet = bIsPrefetch ? EnqueuedPrefetchTiles : EnqueuedTiles;
	if (PendingSet.Contains(TileCoord))
	{
		return;
	}

	PendingSet.Add(TileCoord);
	const double EnqueueTime = CurrentTime > 0.0f ? CurrentTime : FPlatformTime::Seconds();

	if (bIsPrefetch)
	{
		PrefetchGenerationQueue.Enqueue(FTileGenerationTask(TileCoord, true, EnqueueTime));
	}
	else
	{
		RequestedGenerationQueue.Enqueue(FTileGenerationTask(TileCoord, false, EnqueueTime));
	}
}

void UTileStreamingService::DrainGenerationQueues()
{
	if (!bHasStreamingBudgets)
	{
		CurrentBudgets.Clear();
	}

	if (!CurrentBudgets.HasRemainingGenerationBudget())
	{
		return;
	}

	FTileGenerationTask Task;
	while (CurrentBudgets.HasRemainingGenerationBudget() && DequeueNextTask(Task))
	{
		FTileStreamingData NewTileData(Task.TileCoord);
		float HeightMs = 0.0f;
		float BiomeMs = 0.0f;
		float PCGMs = 0.0f;

		double StageStart = FPlatformTime::Seconds();
		bool bGenerated = GenerateSingleTileInternal(Task.TileCoord, NewTileData, HeightMs, BiomeMs, PCGMs);
		double StageEnd = FPlatformTime::Seconds();
		const float TotalMs = static_cast<float>((StageEnd - StageStart) * 1000.0);

		if (!bGenerated)
		{
			WORLDGEN_LOG_WITH_SEED_TILE(Warning, WorldGenSettings.Seed, Task.TileCoord, TEXT("GEN_FAIL: Failed to generate tile"));
			EnqueuedTiles.Remove(Task.TileCoord);
			EnqueuedPrefetchTiles.Remove(Task.TileCoord);
			continue;
		}

		NewTileData.State = ETileState::Generated;
		AddTileToCache(Task.TileCoord, NewTileData);
		EnqueuedTiles.Remove(Task.TileCoord);
		EnqueuedPrefetchTiles.Remove(Task.TileCoord);
		CurrentBudgets.ConsumeGeneration(TotalMs, HeightMs, BiomeMs, PCGMs);

		WORLDGEN_LOG_WITH_SEED_TILE(Verbose, WorldGenSettings.Seed, Task.TileCoord, TEXT("Generated tile in %.2fms (prefetch=%s)"),
			NewTileData.GenerationTimeMs,
			Task.bIsPrefetch ? TEXT("true") : TEXT("false"));

		// Stop if we exhausted primary budget but still have long queue
		if (!CurrentBudgets.HasRemainingGenerationBudget())
		{
			break;
		}
	}
}

bool UTileStreamingService::DequeueNextTask(FTileGenerationTask& OutTask)
{
	auto ConsumeQueue = [this](TQueue<FTileGenerationTask>& Queue, TSet<FTileCoord>& PendingSet, bool bPrefetch, FTileGenerationTask& OutTaskParam) -> bool
	{
		FTileGenerationTask LocalTask;
		while (Queue.Dequeue(LocalTask))
		{
			if (!PendingSet.Contains(LocalTask.TileCoord))
			{
				continue; // stale task
			}

			if (FTileStreamingData* Existing = TileCache.Find(LocalTask.TileCoord))
			{
				if (Existing->State == ETileState::Generated || Existing->State == ETileState::Loaded || Existing->State == ETileState::Active)
				{
					PendingSet.Remove(LocalTask.TileCoord);
					continue;
				}
			}

			LocalTask.bIsPrefetch = bPrefetch;
			OutTaskParam = LocalTask;
			return true;
		}
		return false;
	};

	if (ConsumeQueue(RequestedGenerationQueue, EnqueuedTiles, false, OutTask))
	{
		return true;
	}

	if (ConsumeQueue(PrefetchGenerationQueue, EnqueuedPrefetchTiles, true, OutTask))
	{
		return true;
	}

	return false;
}

void UTileStreamingService::ResetBudgetsForTick()
{
	if (!bHasStreamingBudgets)
	{
		CurrentBudgets.Clear();
		return;
	}

	CurrentBudgets.Reset(StreamingBudgets);
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

	// Implement hysteresis: keep tiles alive 1 ring beyond Active radius
	int32 HysteresisRadius = WorldGenSettings.ActiveRadius + 1; // Keep tiles alive 1 ring beyond Active
	
	// Find tiles outside hysteresis radius (not just LoadRadius)
	for (const auto& TilePair : TileCache)
	{
		FTileCoord TileCoord = TilePair.Key;
		int32 DistanceFromPlayer = CalculateTileDistance(TileCoord, PlayerTileCoord);
		
		// Only evict tiles beyond hysteresis radius to prevent ping-pong
		if (DistanceFromPlayer > HysteresisRadius)
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
				// Only evict if beyond hysteresis radius to prevent thrashing
				int32 DistanceFromPlayer = CalculateTileDistance(TileCoord, PlayerTileCoord);
				if (DistanceFromPlayer > HysteresisRadius)
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
		
		UE_LOG(LogTileStreaming, Verbose, TEXT("Evicted tile (%d, %d) beyond hysteresis radius %d"), 
			TileCoord.X, TileCoord.Y, HysteresisRadius);
	}
}

bool UTileStreamingService::GenerateSingleTile(const FTileCoord& TileCoord, FTileStreamingData& OutTileData)
{
	float HeightMs = 0.0f;
	float BiomeMs = 0.0f;
	float PCGMs = 0.0f;
	return GenerateSingleTileInternal(TileCoord, OutTileData, HeightMs, BiomeMs, PCGMs);
}

bool UTileStreamingService::GenerateSingleTileInternal(const FTileCoord& TileCoord, FTileStreamingData& OutTileData, float& OutHeightMs, float& OutBiomeMs, float& OutPCGMs)
{
	// Defensive programming guards - check all required services are non-null
	ensureMsgf(HeightfieldService != nullptr, 
		TEXT("UTileStreamingService: HeightfieldService is null. Call Initialize() before using GenerateSingleTile()."));
	ensureMsgf(BiomeService != nullptr, 
		TEXT("UTileStreamingService: BiomeService is null. Call Initialize() before using GenerateSingleTile()."));
	ensureMsgf(PCGWorldService != nullptr, 
		TEXT("UTileStreamingService: PCGWorldService is null. Call Initialize() before using GenerateSingleTile()."));

	// Early return with error logging if any required service is missing
	if (!HeightfieldService || !BiomeService || !PCGWorldService)
	{
		UE_LOG(LogTileStreaming, Error, TEXT("UTileStreamingService::GenerateSingleTile failed for tile (%d, %d) - missing required services: HeightfieldService=%s, BiomeService=%s, PCGWorldService=%s"),
			TileCoord.X, TileCoord.Y,
			HeightfieldService ? TEXT("OK") : TEXT("NULL"),
			BiomeService ? TEXT("OK") : TEXT("NULL"),
			PCGWorldService ? TEXT("OK") : TEXT("NULL"));
		ErrorEntries.Add({TileCoord, TEXT("GEN_FAIL_MISSING_SERVICE")});
		return false;
	}

	double StartTimeTotal = FPlatformTime::Seconds();

	try
	{
		// Set state to generating
		OutTileData.State = ETileState::Generating;
		OutTileData.TileCoord = TileCoord;

		// Heightfield generation
		const double HeightStart = FPlatformTime::Seconds();
		OutTileData.HeightfieldData = HeightfieldService->GenerateHeightfield(WorldGenSettings.Seed, TileCoord);
		const double HeightEnd = FPlatformTime::Seconds();
		OutHeightMs = static_cast<float>((HeightEnd - HeightStart) * 1000.0);
		HeightfieldService->CacheHeightfield(OutTileData.HeightfieldData);

		// Biome calculation
		const double BiomeStart = FPlatformTime::Seconds();
		OutTileData.BiomeType = BiomeService->DetermineTileBiome(TileCoord, OutTileData.HeightfieldData.HeightData);
		const double BiomeEnd = FPlatformTime::Seconds();
		OutBiomeMs = static_cast<float>((BiomeEnd - BiomeStart) * 1000.0);
		OutTileData.GenerationTimeMs = OutHeightMs + OutBiomeMs;

		// Generate PCG content
		const double PCGStart = FPlatformTime::Seconds();
		FPCGGenerationData PCGData = PCGWorldService->GenerateBiomeContent(
			TileCoord, OutTileData.BiomeType, OutTileData.HeightfieldData.HeightData);
		const double PCGEnd = FPlatformTime::Seconds();
		OutPCGMs = static_cast<float>((PCGEnd - PCGStart) * 1000.0);
		OutTileData.PCGGenerationTimeMs = OutPCGMs;
		OutTileData.bHasPCGContent = PCGData.TotalInstanceCount > 0;
		OutTileData.State = ETileState::Generated;

		// Calculate total generation time for rolling metrics (heightfield+biome+pcg)
		double EndTimeTotal = FPlatformTime::Seconds();
		const float TotalGenMs = static_cast<float>((EndTimeTotal - StartTimeTotal) * 1000.0);
		OutTileData.LastAccessTime = CurrentTime;

		// Record performance
		RecordGenerationTime(TotalGenMs);

		return true;
	}
	catch (...)
	{
		UE_LOG(LogTileStreaming, Error, TEXT("Exception during tile generation for (%d, %d)"), 
			TileCoord.X, TileCoord.Y);
		ErrorEntries.Add({TileCoord, TEXT("GEN_EXCEPTION")});
		return false;
	}
}

bool UTileStreamingService::GetTileData(const FTileCoord& TileCoord, FTileStreamingData& OutTileData)
{
	// Defensive programming guard - verify WorldGenSettings are properly loaded
	ensureMsgf(WorldGenSettings.Seed != 0 || WorldGenSettings.GenerateRadius > 0, 
		TEXT("UTileStreamingService: WorldGenSettings appear to be uninitialized. Call Initialize() before using GetTileData()."));

	// Early return with error logging if WorldGenSettings are invalid
	if (WorldGenSettings.Seed == 0 && WorldGenSettings.GenerateRadius == 0)
	{
		UE_LOG(LogTileStreaming, Error, TEXT("UTileStreamingService::GetTileData failed for tile (%d, %d) - WorldGenSettings are not properly initialized (Seed=%d, GenerateRadius=%d)"),
			TileCoord.X, TileCoord.Y, WorldGenSettings.Seed, WorldGenSettings.GenerateRadius);
		return false;
	}

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
		EnqueuedTiles.Remove(TileCoord);
		EnqueuedPrefetchTiles.Remove(TileCoord);
		return true;
	}

	return false;
}

bool UTileStreamingService::GenerateTile(const FTileCoord& TileCoord)
{
	// Defensive programming guard - ensure services are initialized
	ensureMsgf(HeightfieldService != nullptr && BiomeService != nullptr && PCGWorldService != nullptr, 
		TEXT("UTileStreamingService: Required services are null. Call Initialize() before using GenerateTile()."));

	// Early return if services are not properly initialized
	if (!HeightfieldService || !BiomeService || !PCGWorldService)
	{
		UE_LOG(LogTileStreaming, Error, TEXT("UTileStreamingService::GenerateTile failed for tile (%d, %d) - services not initialized"),
			TileCoord.X, TileCoord.Y);
		return false;
	}

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

	EnqueuedTiles.Remove(TileCoord);
	EnqueuedPrefetchTiles.Remove(TileCoord);
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

	PerformanceMetrics.PendingGenerations += EnqueuedTiles.Num() + EnqueuedPrefetchTiles.Num();

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
	FTileGenerationTask DummyTask;
	while (RequestedGenerationQueue.Dequeue(DummyTask)) {}
	while (PrefetchGenerationQueue.Dequeue(DummyTask)) {}
	EnqueuedTiles.Empty();
	EnqueuedPrefetchTiles.Empty();
	
	UE_LOG(LogTileStreaming, Log, TEXT("Tile cache cleared"));
}

void UTileStreamingService::SetVHMTerrainRenderer(UVHMTerrainRenderer* InVHMTerrainRenderer)
{
	VHMTerrainRenderer = InVHMTerrainRenderer;
	UE_LOG(LogTileStreaming, Log, TEXT("VHM Terrain Renderer connected to tile streaming service"));
}

void UTileStreamingService::NotifyVHMRenderer(const TArray<FTileCoord>& ActiveTiles, const TArray<FTileCoord>& LoadTiles)
{
	if (!VHMTerrainRenderer)
	{
		return;
	}

	// Notify VHM renderer about tiles that should have VHM components (within Active radius)
	for (const FTileCoord& TileCoord : ActiveTiles)
	{
		if (!CurrentBudgets.HasVHMBudget())
		{
			break; // Defer remaining activations to future ticks
		}

		FTileStreamingData* TileData = TileCache.Find(TileCoord);
		if (TileData && TileData->State == ETileState::Active)
		{
            // Notify VHM renderer that this tile is active and should have a mesh
            const double Before = FPlatformTime::Seconds();
            VHMTerrainRenderer->OnTileStreamingEvent(TileCoord, true);
            const double After = FPlatformTime::Seconds();

            // Capture mesh creation time and overhead
            float MeshMs = 0.0f;
            if (UVHMTerrainRenderer* Renderer = VHMTerrainRenderer)
            {
                MeshMs = Renderer->GetLastMeshGenerationTimeMs(TileCoord);
            }
            const float CallMs = static_cast<float>((After - Before) * 1000.0);
            TileData->StreamInTimeMs = MeshMs;
            TileData->GTOverheadMs = FMath::Max(0.0f, CallMs - MeshMs);

            // Compute recent thread spike around activation
            const float SpikeMs = ComputeRecentSpikeMs(After, /*WindowSec=*/3.0);
            TileData->ThreadSpikesMs = SpikeMs;

			CurrentBudgets.ConsumeVHM(CallMs);
        }
    }

	// Check for tiles that are no longer active and should be removed
	TArray<FTileCoord> CurrentVHMTiles = VHMTerrainRenderer->GetActiveMeshTiles();
	for (const FTileCoord& VHMTileCoord : CurrentVHMTiles)
	{
		// If VHM has a tile that's not in our active list, remove it
		if (!ActiveTiles.Contains(VHMTileCoord))
		{
			VHMTerrainRenderer->OnTileStreamingEvent(VHMTileCoord, false);
		}
	}
}

void UTileStreamingService::SampleFrameTime()
{
    const float FrameMs = FApp::GetDeltaTime() * 1000.0f;
    const double Now = FPlatformTime::Seconds();
    FrameTimeSamples.Emplace(Now, FrameMs);
    if (FrameTimeSamples.Num() > MaxFrameSamples)
    {
        const int32 Excess = FrameTimeSamples.Num() - MaxFrameSamples;
        FrameTimeSamples.RemoveAt(0, Excess, EAllowShrinking::No);
    }
}

float UTileStreamingService::ComputeRecentSpikeMs(double NowSeconds, double WindowSec) const
{
    if (FrameTimeSamples.Num() == 0)
    {
        return 0.0f;
    }

    // Compute baseline as average of samples prior to window
    const double WindowStart = NowSeconds - WindowSec;
    float SumBefore = 0.0f; int32 CountBefore = 0;
    float MaxInWindow = 0.0f; int32 CountInWindow = 0;
    for (const auto& S : FrameTimeSamples)
    {
        if (S.Key < WindowStart)
        {
            SumBefore += S.Value;
            CountBefore++;
        }
        else
        {
            MaxInWindow = FMath::Max(MaxInWindow, S.Value);
            CountInWindow++;
        }
    }

    if (CountInWindow == 0)
    {
        return 0.0f;
    }

    const float Baseline = (CountBefore > 0) ? (SumBefore / CountBefore) : MaxInWindow;
    return FMath::Max(0.0f, MaxInWindow - Baseline);
}

bool UTileStreamingService::ExportPerformanceCSV(const FString& OptionalFileName)
{
    const FString Dir = FPaths::ProjectSavedDir() / TEXT("Vibeheim/WorldGen/Perf");
    IFileManager::Get().MakeDirectory(*Dir, /*Tree*/true);

    const FString Filename = OptionalFileName.Len() > 0
        ? OptionalFileName
        : FString::Printf(TEXT("perf_%s.csv"), *FDateTime::Now().ToString(TEXT("yyyyMMdd_HHmmss")));
    const FString Path = Dir / Filename;

    FString Out;
    Out += TEXT("TileX,TileY,GenMs,PCGMs,StreamInMs,GTOverheadMs,ThreadSpikesMs\n");

    // Write rows for cached tiles
    for (const auto& Pair : TileCache)
    {
        const FTileCoord& T = Pair.Key;
        const FTileStreamingData& D = Pair.Value;

        if (!D.ErrorCode.IsEmpty())
        {
            const FString Err = FString::Printf(TEXT("ERR:%s"), *D.ErrorCode);
            Out += FString::Printf(TEXT("%d,%d,%s,%s,%s,%s,%s\n"), T.X, T.Y, *Err, *Err, *Err, *Err, *Err);
        }
        else
        {
            Out += FString::Printf(TEXT("%d,%d,%.2f,%.2f,%.2f,%.2f,%.2f\n"),
                T.X, T.Y,
                D.GenerationTimeMs,
                D.PCGGenerationTimeMs,
                D.StreamInTimeMs,
                D.GTOverheadMs,
                D.ThreadSpikesMs);
        }
    }

    // Write rows for explicit error entries that may not be cached
    for (const FTileErrorEntry& E : ErrorEntries)
    {
        const FString Err = FString::Printf(TEXT("ERR:%s"), *E.Code);
        Out += FString::Printf(TEXT("%d,%d,%s,%s,%s,%s,%s\n"), E.Tile.X, E.Tile.Y, *Err, *Err, *Err, *Err, *Err);
    }

    const bool bSaved = FFileHelper::SaveStringToFile(Out, *Path);
    if (!bSaved)
    {
        UE_LOG(LogTileStreaming, Error, TEXT("Failed to write perf CSV to %s"), *Path);
    }
    else
    {
        UE_LOG(LogTileStreaming, Log, TEXT("Wrote perf CSV to %s"), *Path);
    }
    return bSaved;
}
