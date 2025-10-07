#pragma once

#include "CoreMinimal.h"
#include "PCGVersionGuard.h"
#include "Data/WorldGenTypes.h"
#include "Misc/DateTime.h"
#include "UObject/StrongObjectPtr.h"

#if VHM_PCG_ENABLED
#include "PCGData.h"
#include "PCGSubsystem.h"
#else
class UPCGData;
using FPCGTaskId = int32;
static constexpr FPCGTaskId InvalidPCGTaskId = static_cast<FPCGTaskId>(-1);
#endif // VHM_PCG_ENABLED

/** Attribute name constants shared between PCG services and validation. */
namespace VHMPCGAttr
{
	inline const FName AverageHeight(TEXT("AverageHeight"));
	inline const FName MinHeight(TEXT("MinHeight"));
	inline const FName MaxHeight(TEXT("MaxHeight"));
	inline const FName AverageSlope(TEXT("AverageSlope"));
	inline const FName MaxSlope(TEXT("MaxSlope"));
	inline const FName WaterCoverage(TEXT("WaterCoverage"));
	inline const FName AverageAboveWater(TEXT("AverageAboveWater"));
	inline const FName AverageBelowWater(TEXT("AverageBelowWater"));
	inline const FName MinWaterDistance(TEXT("MinWaterDistance"));
	inline const FName SeaLevel(TEXT("SeaLevel"));
	inline const FName Biome(TEXT("Biome"));
	inline const FName BiomeId(TEXT("BiomeId"));
	inline const FName TileSeed(TEXT("TileSeed")); // Hash(TileX, TileY, BiomeId, GlobalSeed)
	inline const FName TileX(TEXT("TileX"));
	inline const FName TileY(TEXT("TileY"));
	inline const FName TileSize(TEXT("TileSize"));
	inline const FName BiomeWeight(TEXT("BiomeWeight"));
	inline const FName StaticMesh(TEXT("StaticMesh"));
	inline const FName Mesh(TEXT("Mesh"));
	inline const FName InstanceScale(TEXT("InstanceScale"));
	inline const FName InstanceRotation(TEXT("InstanceRotation"));
	inline const FName IsActive(TEXT("IsActive"));
	inline const FName InstanceId(TEXT("InstanceId"));
}

/** Aggregated tile metrics computed prior to scheduling PCG graphs. */
struct FPCGTileMetrics
{
	float AverageHeight = 0.0f;
	float MinHeight = 0.0f;
	float MaxHeight = 0.0f;
	float AverageSlope = 0.0f;
	float MaxSlope = 0.0f;
	float WaterCoverageRatio = 0.0f;
	float AverageAboveWater = 0.0f;
	float AverageBelowWater = 0.0f;
	float MinAbsWaterDistance = 0.0f;
};

/**
 * Wrapper around graph inputs used for scheduling (pin name ?+' data mapping).
 * Pin ordering is unstable across editor edits, so names are preferred.
 */
struct FPCGInputSet
{
	TMap<FName, TObjectPtr<UPCGData>> Inputs;
	TArray<FName> InputOrder;

	void Add(FName PinName, UPCGData* Data)
	{
		if (!Data)
		{
			return;
		}

		Inputs.Add(PinName, Data);
		if (!InputOrder.Contains(PinName))
		{
			InputOrder.Add(PinName);
		}
	}

	void Reset()
	{
		Inputs.Reset();
		InputOrder.Reset();
	}

	bool IsEmpty() const
	{
		return Inputs.Num() == 0;
	}
};

/** Output bundle returned from scheduler extraction. */
struct FPCGOutputSet
{
        TArray<TObjectPtr<UPCGData>> Outputs;

	void Reset()
	{
		Outputs.Reset();
	}

	int32 Num() const
	{
		return Outputs.Num();
        }
};

/** Validation report returned from graph validation tooling. */
struct FPCGGraphValidationResult
{
        bool bIsValid = false;
        FString GraphPath;
        FString GraphName;
        TArray<FString> Errors;
        TArray<FString> Warnings;
        TArray<FName> MissingAttributes;
        TArray<FString> UnwiredDependencyNodes;
        TArray<FString> Suggestions;
};

/** High-level lifecycle states recorded for telemetry logging. */
enum class EPCGTaskTelemetryStatus : uint8
{
        Scheduled,
        Running,
        Completed,
        Failed,
        Fallback
};

/** Structured telemetry captured for each scheduled PCG task. */
struct FPCGTaskTelemetry
{
        EBiomeType Biome = EBiomeType::None;
        FString GraphAssetPath;
        FTileCoord Tile;
        FPCGTaskId TaskId = InvalidPCGTaskId;
        FDateTime SubmitTimestamp = FDateTime::MinValue();
        FDateTime StartTimestamp = FDateTime::MinValue();
        FDateTime DoneTimestamp = FDateTime::MinValue();
        EPCGTaskTelemetryStatus Status = EPCGTaskTelemetryStatus::Scheduled;
        int32 PointsOut = 0;
        int32 NodesExecutedProxy = 0;
        int32 NodesCachedProxy = 0;
        bool bFallbackUsed = false;
        double ElapsedMs = 0.0;
};

/** Result container for synchronous scheduler helpers (editor/tests). */
struct FPCGScheduleResult
{
	bool bSuccess = false;
	int32 GeneratedPointCount = 0;
	TArray<FString> Warnings;
	TArray<FString> Errors;
	double ExecutionTimeMs = 0.0;
	FPCGOutputSet Output;
};

/** Task lifecycle states tracked by the scheduler executor. */
enum class EPCGTaskState : uint8
{
	Scheduled,
	Completing,
	Completed,
	Released,
	Abandoned
};

/** Minimal bookkeeping per scheduled task (lifetime and GC safety). */
struct FPCGTaskContext
{
	UWorld* World = nullptr;
	FTileCoord TileCoord;
	FPCGTaskId TaskId = InvalidPCGTaskId;
	EPCGTaskState State = EPCGTaskState::Scheduled;
	TArray<TStrongObjectPtr<UPCGData>> InputDataRefs;

	bool IsValid() const
	{
		return TaskId != InvalidPCGTaskId;
	}

	void Reset()
	{
		World = nullptr;
		TileCoord = FTileCoord();
		TaskId = InvalidPCGTaskId;
		State = EPCGTaskState::Scheduled;
		InputDataRefs.Reset();
	}
};



