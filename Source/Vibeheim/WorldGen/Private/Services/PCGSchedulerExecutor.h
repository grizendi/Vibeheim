#pragma once

#include "CoreMinimal.h"
#include "Services/PCGWorldServiceTypes.h"

#if VHM_PCG_ENABLED
#include "PCGCommon.h"

class UPCGSubsystem;
class UPCGComponent;
class UPCGGraph;
class UPCGData;
struct FPCGDataCollection;

/** Scheduler helper for invoking UE 5.6 PCG graphs from world generation code. */
class FPCGSchedulerExecutor
{
public:
	FPCGSchedulerExecutor() = default;

        FPCGTaskId ScheduleGraphAsync(UPCGSubsystem& Subsystem,
                UPCGComponent& SourceComponent,
                UPCGGraph& Graph,
                UWorld& ExecutionWorld,
                const FTileCoord& TileCoord,
                const FPCGInputSet& InputSet,
                const FString& DebugLabel,
                const FBox& ExecutionBounds,
                int32 Seed,
                bool bEnableFrustumCulling,
                float FrustumMargin,
                FPCGTaskContext& OutContext,
                TArray<FString>& OutErrors,
                TArray<FString>& OutWarnings);

	bool IsTaskComplete(UPCGSubsystem& Subsystem, FPCGTaskId TaskId, FPCGTaskContext& InOutContext);

	bool GetTaskOutput(UPCGSubsystem& Subsystem,
		FPCGTaskId TaskId,
		FPCGTaskContext& Context,
		FPCGOutputSet& OutOutput,
		int32& OutPointCount,
		double& OutElapsedMs,
		TArray<FString>& OutWarnings,
		TArray<FString>& OutErrors);

	void ReleaseTask(UPCGSubsystem& Subsystem, FPCGTaskId TaskId, FPCGTaskContext& Context);

	void AbandonTask(UPCGSubsystem& Subsystem, FPCGTaskId TaskId, FPCGTaskContext& Context, const FString& Reason);

#if WITH_EDITOR || WITH_AUTOMATION_TESTS
        FPCGScheduleResult RunGraphSync(UPCGSubsystem& Subsystem,
                UPCGComponent& SourceComponent,
                UPCGGraph& Graph,
                UWorld& ExecutionWorld,
                const FTileCoord& TileCoord,
                const FPCGInputSet& InputSet,
                const FString& DebugLabel,
                const FBox& ExecutionBounds,
                int32 Seed,
                bool bEnableFrustumCulling,
                float FrustumMargin,
                TArray<FString>& OutWarnings,
                TArray<FString>& OutErrors);
#endif

private:
        friend struct FSchedulerTestHelper;
        struct FScheduledTask
        {
                FPCGTaskContext Context;
                TSharedPtr<const FPCGDataCollection> InputCollection;
                FPCGElementPtr InputElement;
                TWeakObjectPtr<UPCGComponent> SourceComponent;
                TWeakObjectPtr<UPCGGraph> Graph;
                FString DebugName;
                double StartSeconds = 0.0;
                bool bUsedFallback = false;
                bool bOutputCached = false;
                bool bAbandoned = false;
                FPCGOutputSet CachedOutput;
        };

        using FResolvedInput = TPair<FName, UPCGData*>;

	bool ResolveInputs(UPCGGraph& Graph,
		const FPCGInputSet& InputSet,
		TArray<FResolvedInput>& OutResolvedInputs,
		TArray<FString>& OutWarnings,
		TArray<FString>& OutErrors,
		bool& bOutUsedFallback) const;

	TSharedPtr<FPCGDataCollection> BuildDataCollection(const TArray<FResolvedInput>& ResolvedInputs) const;

        void CacheOutput(FScheduledTask& TaskInfo, const FPCGDataCollection& OutputData);

        void ApplyFrustumPolicy(FPCGScheduleGraphParams& Params, bool bEnableFrustumCulling, float FrustumMargin);

	FScheduledTask* FindTask(FPCGTaskId TaskId);
        const FScheduledTask* FindTask(FPCGTaskId TaskId) const;

private:
        TMap<FPCGTaskId, FScheduledTask> ActiveTasks;
};

#endif // VHM_PCG_ENABLED



