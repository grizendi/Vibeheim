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
        virtual ~FPCGSchedulerExecutor() = default;

        virtual FPCGTaskId ScheduleGraphAsync(UPCGSubsystem& Subsystem,
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

        virtual bool IsTaskComplete(UPCGSubsystem& Subsystem, FPCGTaskId TaskId, FPCGTaskContext& InOutContext);

        virtual bool GetTaskOutput(UPCGSubsystem& Subsystem,
                FPCGTaskId TaskId,
                FPCGTaskContext& Context,
                FPCGOutputSet& OutOutput,
                int32& OutPointCount,
                double& OutElapsedMs,
                TArray<FString>& OutWarnings,
                TArray<FString>& OutErrors);

        virtual void ReleaseTask(UPCGSubsystem& Subsystem, FPCGTaskId TaskId, FPCGTaskContext& Context);

        virtual void AbandonTask(UPCGSubsystem& Subsystem, FPCGTaskId TaskId, FPCGTaskContext& Context, const FString& Reason);

#if WITH_EDITOR || WITH_AUTOMATION_TESTS
        virtual FPCGScheduleResult RunGraphSync(UPCGSubsystem& Subsystem,
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

        using FResolvedInput = TPair<FName, UPCGData*>;

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

private:
#if WITH_AUTOMATION_TESTS
        friend struct FPCGSchedulerExecutorTestAccessor;
#endif

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

#if WITH_AUTOMATION_TESTS
struct FPCGSchedulerExecutorTestAccessor
{
        static bool ResolveInputs(FPCGSchedulerExecutor& Executor,
                UPCGGraph& Graph,
                const FPCGInputSet& InputSet,
                TArray<FPCGSchedulerExecutor::FResolvedInput>& OutResolvedInputs,
                TArray<FString>& OutWarnings,
                TArray<FString>& OutErrors,
                bool& bOutUsedFallback);

        static FPCGSchedulerExecutor::FScheduledTask& AddTask(FPCGSchedulerExecutor& Executor, FPCGTaskId TaskId);

        static void CacheOutput(FPCGSchedulerExecutor& Executor, FPCGTaskId TaskId, const FPCGOutputSet& Output);
};
#endif // WITH_AUTOMATION_TESTS

#endif // VHM_PCG_ENABLED



