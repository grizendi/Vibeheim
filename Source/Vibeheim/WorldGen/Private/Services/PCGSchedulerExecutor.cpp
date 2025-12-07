#include "Services/PCGSchedulerExecutor.h"

#if VHM_PCG_ENABLED

#include "Services/PCGExecutionHelpers.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGSubsystem.h"
#include "PCGParamData.h"
#include "Data/PCGPointData.h"
#include "PCGData.h"
#include "Async/Async.h"
#include "Misc/ScopeExit.h"
#include "HAL/PlatformTime.h"
#include "HAL/IConsoleManager.h"
#include "Trace/Trace.h"

DEFINE_LOG_CATEGORY_STATIC(LogPCGScheduler, Log, All);

#if WITH_AUTOMATION_TESTS
bool FPCGSchedulerExecutorTestAccessor::ResolveInputs(FPCGSchedulerExecutor& Executor,
        UPCGGraph& Graph,
        const FPCGInputSet& InputSet,
        TArray<FPCGSchedulerExecutor::FResolvedInput>& OutResolvedInputs,
        TArray<FString>& OutWarnings,
        TArray<FString>& OutErrors,
        bool& bOutUsedFallback)
{
        return Executor.ResolveInputs(Graph, InputSet, OutResolvedInputs, OutWarnings, OutErrors, bOutUsedFallback);
}

FPCGSchedulerExecutor::FScheduledTask& FPCGSchedulerExecutorTestAccessor::AddTask(FPCGSchedulerExecutor& Executor, FPCGTaskId TaskId)
{
        FPCGSchedulerExecutor::FScheduledTask& Task = Executor.ActiveTasks.Add(TaskId);
        Task.Context.TaskId = TaskId;
        return Task;
}

void FPCGSchedulerExecutorTestAccessor::CacheOutput(FPCGSchedulerExecutor& Executor, FPCGTaskId TaskId, const FPCGOutputSet& Output)
{
        if (FPCGSchedulerExecutor::FScheduledTask* Task = Executor.ActiveTasks.Find(TaskId))
        {
                Task->CachedOutput = Output;
                Task->bOutputCached = true;
                Task->Context.TaskId = TaskId;
                Task->Context.State = EPCGTaskState::Completed;
        }
}
#endif
class FGTCall
{
public:
	template<typename Func>
	static void Execute(Func&& Callable)
	{
		if (IsInGameThread())
		{
			Callable();
		}
		else
		{
			AsyncTask(ENamedThreads::GameThread, [Callable = Forward<Func>(Callable)]() mutable
			{
				Callable();
			});
		}
	}
};

static TAutoConsoleVariable<int32> CVarVibeheimPCGPollMs(
	TEXT("vhm.pcg.poll_ms"),
	12,
	TEXT("Polling cadence (in ms) when waiting for PCG scheduler tasks to complete."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarVibeheimPCGTimeoutMs(
	TEXT("vhm.pcg.timeout_ms"),
	5000,
	TEXT("Timeout (in ms) for synchronous PCG scheduler execution."),
	ECVF_Default);

namespace
{
        FORCEINLINE FString FormatTileLabel(const FTileCoord& TileCoord)
        {
                return FString::Printf(TEXT("(%d,%d)"), TileCoord.X, TileCoord.Y);
        }

        template<typename SettingsType>
        auto SetFrustumEnabled(SettingsType& Settings, bool bEnable, int) -> decltype((void)(Settings.bEnableFrustumCulling), void())
        {
                Settings.bEnableFrustumCulling = bEnable;
        }

        template<typename SettingsType>
        auto SetFrustumEnabled(SettingsType& Settings, bool bEnable, long) -> decltype((void)(Settings.bEnabled), void())
        {
                Settings.bEnabled = bEnable;
        }

        template<typename SettingsType>
        void SetFrustumEnabled(SettingsType&, bool, ...)
        {
        }

        template<typename SettingsType>
        auto SetFrustumMargin(SettingsType& Settings, float Margin, int) -> decltype((void)(Settings.CullingMargin), void())
        {
                Settings.CullingMargin = Margin;
        }

        template<typename SettingsType>
        auto SetFrustumMargin(SettingsType& Settings, float Margin, long) -> decltype((void)(Settings.FrustumCullingMargin), void())
        {
                Settings.FrustumCullingMargin = Margin;
        }

        template<typename SettingsType>
        void SetFrustumMargin(SettingsType&, float, ...)
        {
        }

        template<typename ParamsType>
        auto ApplyFrustumPolicyInternal(ParamsType& Params, bool bEnableFrustumCulling, float FrustumMargin, int)
                -> decltype((void)(Params.FrustumSettings), void())
        {
                SetFrustumEnabled(Params.FrustumSettings, bEnableFrustumCulling, 0);
                SetFrustumMargin(Params.FrustumSettings, FrustumMargin, 0);
        }

        template<typename ParamsType>
        void ApplyFrustumPolicyInternal(ParamsType&, bool, float, ...)
        {
        }
}

FPCGSchedulerExecutor::FScheduledTask* FPCGSchedulerExecutor::FindTask(FPCGTaskId TaskId)
{
	return ActiveTasks.Find(TaskId);
}

const FPCGSchedulerExecutor::FScheduledTask* FPCGSchedulerExecutor::FindTask(FPCGTaskId TaskId) const
{
	return ActiveTasks.Find(TaskId);
}

bool FPCGSchedulerExecutor::ResolveInputs(UPCGGraph& Graph,
	const FPCGInputSet& InputSet,
	TArray<FResolvedInput>& OutResolvedInputs,
	TArray<FString>& OutWarnings,
	TArray<FString>& OutErrors,
	bool& bOutUsedFallback) const
{
	bOutUsedFallback = false;

	UPCGNode* InputNode = Graph.GetInputNode();
	if (!InputNode)
	{
		OutErrors.Add(TEXT("Graph has no input node; cannot schedule."));
		return false;
	}

	const TArray<TObjectPtr<UPCGPin>>& OutputPins = InputNode->GetOutputPins();
	if (OutputPins.IsEmpty())
	{
		OutWarnings.Add(TEXT("Graph input node exposes no output pins; proceeding with empty input."));
		return true;
	}

	TMap<FName, UPCGData*> NameToData;
	TArray<UPCGData*> OrderedInputs;
	OrderedInputs.Reserve(InputSet.InputOrder.Num());

	for (const TPair<FName, TObjectPtr<UPCGData>>& Pair : InputSet.Inputs)
	{
		if (!Pair.Value)
		{
			continue;
		}

		NameToData.Add(Pair.Key, Pair.Value.Get());
	}

	for (const FName& OrderedKey : InputSet.InputOrder)
	{
		if (UPCGData** DataPtr = NameToData.Find(OrderedKey))
		{
			OrderedInputs.Add(*DataPtr);
		}
	}

	TSet<const UPCGData*> ConsumedData;
	int32 FallbackIndex = 0;

	for (const TObjectPtr<UPCGPin>& PinPtr : OutputPins)
	{
		if (!PinPtr)
		{
			continue;
		}

		const FPCGPinProperties& PinProps = PinPtr->Properties;
		UPCGData* const* ExactData = NameToData.Find(PinProps.Label);

		UPCGData* ResolvedData = nullptr;
		if (ExactData && *ExactData)
		{
			ResolvedData = *ExactData;
		}
		else
		{
			while (FallbackIndex < OrderedInputs.Num())
			{
				UPCGData* Candidate = OrderedInputs[FallbackIndex++];
				if (!Candidate || ConsumedData.Contains(Candidate))
				{
					continue;
				}

				ResolvedData = Candidate;
				bOutUsedFallback = true;
				OutWarnings.Add(FString::Printf(
					TEXT("Pin '%s' missing explicit mapping; using positional input '%s'."),
					*PinProps.Label.ToString(),
					Candidate ? *Candidate->GetName() : TEXT("<positional>")));
				break;
			}
		}

		if (!ResolvedData)
		{
			const bool bIsRequired = (PinProps.PinStatus == EPCGPinStatus::Required);
			if (bIsRequired)
			{
				OutErrors.Add(FString::Printf(TEXT("Required input pin '%s' has no data."), *PinProps.Label.ToString()));
			}
			continue;
		}

		if (PinProps.AllowedTypes != EPCGDataType::Any)
		{
#pragma warning(push)
#pragma warning(disable: 4996)
			const bool bTypeCompatible = EnumHasAnyFlags(static_cast<EPCGDataType>(PinProps.AllowedTypes), ResolvedData->GetDataType());
#pragma warning(pop)
			if (!bTypeCompatible)
			{
				OutErrors.Add(FString::Printf(
					TEXT("Input '%s' expects type %s but received %s."),
					*PinProps.Label.ToString(),
#pragma warning(push)
#pragma warning(disable: 4996)
					*StaticEnum<EPCGDataType>()->GetNameStringByValue(static_cast<int64>(PinProps.AllowedTypes)),
#pragma warning(pop)
					*StaticEnum<EPCGDataType>()->GetNameStringByValue(static_cast<int64>(ResolvedData->GetDataType()))));
				continue;
			}
		}

		ConsumedData.Add(ResolvedData);
		OutResolvedInputs.Emplace(PinProps.Label, ResolvedData);
	}

	return OutErrors.IsEmpty();
}

TSharedPtr<FPCGDataCollection> FPCGSchedulerExecutor::BuildDataCollection(const TArray<FResolvedInput>& ResolvedInputs) const
{
	if (ResolvedInputs.IsEmpty())
	{
		return MakeShared<FPCGDataCollection>();
	}

	TSharedPtr<FPCGDataCollection> Collection = MakeShared<FPCGDataCollection>();
	Collection->TaggedData.Reserve(ResolvedInputs.Num());

	for (const FResolvedInput& Entry : ResolvedInputs)
	{
		if (!Entry.Value)
		{
			continue;
		}

		FPCGTaggedData& Tagged = Collection->TaggedData.AddDefaulted_GetRef();
		Tagged.Data = Entry.Value;
		Tagged.Pin = Entry.Key;
	}

	return Collection;
}

void FPCGSchedulerExecutor::CacheOutput(FScheduledTask& TaskInfo, const FPCGDataCollection& OutputData)
{
        TaskInfo.CachedOutput.Reset();
        TaskInfo.CachedOutput.Outputs.Reserve(OutputData.TaggedData.Num());

        for (const FPCGTaggedData& Tagged : OutputData.TaggedData)
        {
                if (const UPCGData* TaggedData = Tagged.Data.Get())
                {
                        TaskInfo.CachedOutput.Outputs.Add(const_cast<UPCGData*>(TaggedData));
                }
        }

        TaskInfo.bOutputCached = true;
        TaskInfo.Context.State = EPCGTaskState::Completed;
}

void FPCGSchedulerExecutor::ApplyFrustumPolicy(FPCGScheduleGraphParams& Params, bool bEnableFrustumCulling, float FrustumMargin)
{
        ApplyFrustumPolicyInternal(Params, bEnableFrustumCulling, FrustumMargin, 0);
}

FPCGTaskId FPCGSchedulerExecutor::ScheduleGraphAsync(UPCGSubsystem& Subsystem,
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
        TArray<FString>& OutWarnings)
{
	check(IsInGameThread());
	TRACE_CPUPROFILER_EVENT_SCOPE(PCG_Schedule);

	TArray<FResolvedInput> ResolvedInputs;
	bool bUsedFallback = false;
	if (!ResolveInputs(Graph, InputSet, ResolvedInputs, OutWarnings, OutErrors, bUsedFallback))
	{
		return InvalidPCGTaskId;
	}

	TSharedPtr<FPCGDataCollection> DataCollection = BuildDataCollection(ResolvedInputs);
	if (!DataCollection.IsValid())
	{
		OutErrors.Add(TEXT("Failed to build PCG data collection for scheduler."));
		return InvalidPCGTaskId;
	}

	FPCGElementPtr InputElement = MakeShared<FVibeheimPCGInputElement>(DataCollection.ToSharedRef());

	TArray<FPCGTaskId> Dependencies;
        FPCGScheduleGraphParams Params(&Graph, &SourceComponent, nullptr, InputElement, MoveTemp(Dependencies), nullptr, true);
        ApplyFrustumPolicy(Params, bEnableFrustumCulling, FrustumMargin);

	const FPCGTaskId TaskId = Subsystem.ScheduleGraph(Params);
	if (TaskId == InvalidPCGTaskId)
	{
		OutErrors.Add(TEXT("UPCGSubsystem::ScheduleGraph returned InvalidPCGTaskId."));
		return TaskId;
	}

	FScheduledTask& Task = ActiveTasks.Add(TaskId);
	Task.Context.World = &ExecutionWorld;
	Task.Context.TileCoord = TileCoord;
	Task.Context.TaskId = TaskId;
	Task.Context.State = EPCGTaskState::Scheduled;
	Task.DebugName = DebugLabel;
	Task.SourceComponent = &SourceComponent;
	Task.Graph = &Graph;
	Task.StartSeconds = FPlatformTime::Seconds();
	Task.InputCollection = DataCollection;
	Task.InputElement = InputElement;
	Task.bUsedFallback = bUsedFallback;

	for (const TPair<FName, TObjectPtr<UPCGData>>& Pair : InputSet.Inputs)
	{
		if (Pair.Value)
		{
			Task.Context.InputDataRefs.Emplace(Pair.Value.Get());
		}
	}

	OutContext = Task.Context;

	UE_LOG(LogPCGScheduler, Verbose, TEXT("Scheduled graph %s for tile %s (TaskId=%d)."),
		*DebugLabel,
		*FormatTileLabel(TileCoord),
		TaskId);

	return TaskId;
}

bool FPCGSchedulerExecutor::IsTaskComplete(UPCGSubsystem& Subsystem, FPCGTaskId TaskId, FPCGTaskContext& InOutContext)
{
	check(IsInGameThread());

	FScheduledTask* Task = FindTask(TaskId);
	if (!Task)
	{
		return false;
	}

	if (Task->Context.State == EPCGTaskState::Completed || Task->Context.State == EPCGTaskState::Abandoned)
	{
		InOutContext = Task->Context;
		return true;
	}

	FPCGDataCollection Output;
	if (Subsystem.GetOutputData(TaskId, Output))
	{
		CacheOutput(*Task, Output);
		InOutContext = Task->Context;
		return true;
	}

	return false;
}

bool FPCGSchedulerExecutor::GetTaskOutput(UPCGSubsystem& Subsystem,
        FPCGTaskId TaskId,
	FPCGTaskContext& Context,
	FPCGOutputSet& OutOutput,
	int32& OutPointCount,
	double& OutElapsedMs,
	TArray<FString>& OutWarnings,
	TArray<FString>& OutErrors)
{
	check(IsInGameThread());

	FScheduledTask* Task = FindTask(TaskId);
	if (!Task)
	{
		OutErrors.Add(TEXT("Attempted to fetch output for unknown task."));
		return false;
	}

	if (!Task->bOutputCached)
	{
		FPCGDataCollection Output;
		if (!Subsystem.GetOutputData(TaskId, Output))
		{
			OutWarnings.Add(TEXT("Task output not ready when requested."));
			return false;
		}

		CacheOutput(*Task, Output);
	}

        OutOutput = Task->CachedOutput;
        OutPointCount = 0;

        for (const TObjectPtr<UPCGData>& OutputData : Task->CachedOutput.Outputs)
        {
                UPCGData* const Data = OutputData.Get();
                if (!Data)
                {
                        continue;
                }

                if (const UPCGPointData* PointData = Cast<UPCGPointData>(Data))
                {
                        OutPointCount += PointData->GetPoints().Num();
                }
        }

	const double ElapsedSeconds = FPlatformTime::Seconds() - Task->StartSeconds;
	OutElapsedMs = ElapsedSeconds * 1000.0;

	if (Task->bUsedFallback)
	{
		OutWarnings.Add(TEXT("Input pins required positional fallback; verify pin names for deterministic execution."));
	}

	Context = Task->Context;
	return true;
}

void FPCGSchedulerExecutor::ReleaseTask(UPCGSubsystem& Subsystem, FPCGTaskId TaskId, FPCGTaskContext& Context)
{
	check(IsInGameThread());

	if (FScheduledTask* Task = FindTask(TaskId))
	{
		if (Task->Context.State == EPCGTaskState::Released)
		{
			return;
		}

		if (Task->Context.State == EPCGTaskState::Scheduled)
		{
			UE_LOG(LogPCGScheduler, Warning, TEXT("Releasing task %d while still scheduled."), TaskId);
		}

		Subsystem.ClearOutputData(TaskId);
		Task->Context.State = EPCGTaskState::Released;
		Context = Task->Context;
		ActiveTasks.Remove(TaskId);
	}
}

void FPCGSchedulerExecutor::AbandonTask(UPCGSubsystem& Subsystem, FPCGTaskId TaskId, FPCGTaskContext& Context, const FString& Reason)
{
	check(IsInGameThread());

	if (FScheduledTask* Task = FindTask(TaskId))
	{
		if (Task->Context.State == EPCGTaskState::Abandoned)
		{
			return;
		}

		Task->Context.State = EPCGTaskState::Abandoned;
		Task->bAbandoned = true;
		Context = Task->Context;

		if (UPCGComponent* Component = Task->SourceComponent.Get())
		{
			Subsystem.CancelGeneration(Component);
		}

		Subsystem.ClearOutputData(TaskId);

		UE_LOG(LogPCGScheduler, Warning, TEXT("Abandoned task %d (%s): %s"), TaskId, *Task->DebugName, *Reason);
	}
}

#if WITH_EDITOR || WITH_AUTOMATION_TESTS
FPCGScheduleResult FPCGSchedulerExecutor::RunGraphSync(UPCGSubsystem& Subsystem,
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
        TArray<FString>& OutErrors)
{
	check(IsInGameThread());

	FPCGScheduleResult Result;

	FPCGTaskContext TaskContext;
        const FPCGTaskId TaskId = ScheduleGraphAsync(Subsystem, SourceComponent, Graph, ExecutionWorld, TileCoord, InputSet, DebugLabel, ExecutionBounds, Seed, bEnableFrustumCulling, FrustumMargin, TaskContext, OutErrors, OutWarnings);
	if (TaskId == InvalidPCGTaskId)
	{
		Result.Errors = OutErrors;
		return Result;
	}

	const int32 PollMs = FMath::Max(1, CVarVibeheimPCGPollMs.GetValueOnGameThread());
	const int32 TimeoutMs = FMath::Max(PollMs, CVarVibeheimPCGTimeoutMs.GetValueOnGameThread());

	const double StartTime = FPlatformTime::Seconds();
	const double TimeoutSeconds = static_cast<double>(TimeoutMs) / 1000.0;

	while (true)
	{
		if (IsTaskComplete(Subsystem, TaskId, TaskContext))
		{
			Result.bSuccess = true;

			const bool bGotOutput = GetTaskOutput(Subsystem, TaskId, TaskContext, Result.Output, Result.GeneratedPointCount, Result.ExecutionTimeMs, OutWarnings, OutErrors);
			if (!bGotOutput)
			{
				Result.bSuccess = false;
			}

			ReleaseTask(Subsystem, TaskId, TaskContext);
			break;
		}

		const double Elapsed = FPlatformTime::Seconds() - StartTime;
		if (Elapsed > TimeoutSeconds)
		{
			FString TimeoutReason = FString::Printf(TEXT("Scheduler timeout after %.2fs waiting for graph %s"), Elapsed, *DebugLabel);
			AbandonTask(Subsystem, TaskId, TaskContext, TimeoutReason);
			Result.Errors.Add(MoveTemp(TimeoutReason));
			ReleaseTask(Subsystem, TaskId, TaskContext);
			break;
		}

		const double SleepSeconds = static_cast<double>(PollMs) / 1000.0;
		FPlatformProcess::SleepNoStats(SleepSeconds);
	}

	Result.Warnings = OutWarnings;
	Result.Errors = OutErrors;
	Result.ExecutionTimeMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;

	if (!Result.bSuccess && Result.Errors.IsEmpty())
	{
		Result.Errors.Add(TEXT("Unknown scheduler failure"));
	}

	return Result;
}
#endif // WITH_EDITOR || WITH_AUTOMATION_TESTS

#endif // VHM_PCG_ENABLED



