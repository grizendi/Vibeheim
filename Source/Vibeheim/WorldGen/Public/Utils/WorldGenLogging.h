#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogWorldGen, Log, All);

// Macro helpers for consistent logging with seed and tile coordinates
#define WORLDGEN_LOG(Verbosity, Format, ...) UE_LOG(LogWorldGen, Verbosity, Format, ##__VA_ARGS__)

#define WORLDGEN_LOG_WITH_SEED(Verbosity, Seed, Format, ...) \
	UE_LOG(LogWorldGen, Verbosity, TEXT("[Seed:%llu] ") Format, Seed, ##__VA_ARGS__)

#define WORLDGEN_LOG_WITH_TILE(Verbosity, TileCoord, Format, ...) \
	UE_LOG(LogWorldGen, Verbosity, TEXT("[Tile:(%d,%d)] ") Format, TileCoord.X, TileCoord.Y, ##__VA_ARGS__)

#define WORLDGEN_LOG_WITH_SEED_TILE(Verbosity, Seed, TileCoord, Format, ...) \
	UE_LOG(LogWorldGen, Verbosity, TEXT("[Seed:%llu|Tile:(%d,%d)] ") Format, Seed, TileCoord.X, TileCoord.Y, ##__VA_ARGS__)

/**
 * Timer utility for measuring operation durations
 */
struct VIBEHEIM_API FWorldGenTimer
{
	FWorldGenTimer(const FString& OperationName)
		: Operation(OperationName)
		, StartTime(FPlatformTime::Seconds())
	{
	}

	~FWorldGenTimer()
	{
		double EndTime = FPlatformTime::Seconds();
		double DurationMs = (EndTime - StartTime) * 1000.0;
		WORLDGEN_LOG(Log, TEXT("%s completed in %.2fms"), *Operation, DurationMs);
	}

	float GetElapsedMs() const
	{
		double CurrentTime = FPlatformTime::Seconds();
		return static_cast<float>((CurrentTime - StartTime) * 1000.0);
	}

private:
	FString Operation;
	double StartTime;
};

/**
 * Scoped timer macro for automatic timing with cleanup
 */
#define WORLDGEN_TIMER(OperationName) FWorldGenTimer Timer_##__LINE__(OperationName)

/**
 * Scoped timer macro with seed and tile context
 */
#define WORLDGEN_TIMER_WITH_CONTEXT(OperationName, Seed, TileCoord) \
	FString TimerName = FString::Printf(TEXT("%s [Seed:%llu|Tile:(%d,%d)]"), \
		TEXT(OperationName), Seed, TileCoord.X, TileCoord.Y); \
	FWorldGenTimer Timer_##__LINE__(TimerName)
