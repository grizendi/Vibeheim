#if WITH_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Services/PCGSchedulerExecutor.h"
#include "Services/PCGWorldServiceTypes.h"

#if VHM_PCG_ENABLED
#include "PCGGraph.h"
#include "PCGInputOutputSettings.h"
#include "PCGParamData.h"
#include "Data/PCGPointData.h"

namespace
{
	struct FSchedulerTestHelper
	{
		static bool ResolveInputs(FPCGSchedulerExecutor& Executor,
			UPCGGraph& Graph,
			const FPCGInputSet& InputSet,
			TArray<FPCGSchedulerExecutor::FResolvedInput>& OutResolvedInputs,
			TArray<FString>& OutWarnings,
			TArray<FString>& OutErrors,
			bool& bOutUsedFallback)
		{
			return Executor.ResolveInputs(Graph, InputSet, OutResolvedInputs, OutWarnings, OutErrors, bOutUsedFallback);
		}

		static FPCGSchedulerExecutor::FScheduledTask& AddTask(FPCGSchedulerExecutor& Executor, FPCGTaskId TaskId)
		{
			return Executor.ActiveTasks.Add(TaskId);
		}

		static void CacheOutput(FPCGSchedulerExecutor& Executor, FPCGTaskId TaskId, const FPCGDataCollection& Output)
		{
			FPCGSchedulerExecutor::FScheduledTask& Task = Executor.ActiveTasks.FindChecked(TaskId);
			Executor.CacheOutput(Task, Output);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFPCGSchedulerResolveInputsTest, "Vibeheim.PCG.Scheduler.ResolveInputs", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFPCGSchedulerFallbackInputsTest, "Vibeheim.PCG.Scheduler.FallbackOrdering", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFPCGSchedulerValidationFailureTest, "Vibeheim.PCG.Scheduler.ValidationFailure", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFPCGSchedulerCacheStateTest, "Vibeheim.PCG.Scheduler.CacheState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFPCGSchedulerResolveInputsTest::RunTest(const FString& Parameters)
{
	FPCGSchedulerExecutor Executor;
	UPCGGraph* Graph = NewObject<UPCGGraph>();
	TestNotNull(TEXT("Graph should be created"), Graph);

	FPCGInputSet InputSet;
	UPCGPointData* PointData = NewObject<UPCGPointData>();
	InputSet.Add(PCGInputOutputConstants::DefaultInputLabel, PointData);

	TArray<FPCGSchedulerExecutor::FResolvedInput> ResolvedInputs;
	TArray<FString> Warnings;
	TArray<FString> Errors;
	bool bUsedFallback = false;

	const bool bResult = FSchedulerTestHelper::ResolveInputs(Executor, *Graph, InputSet, ResolvedInputs, Warnings, Errors, bUsedFallback);

	TestTrue(TEXT("ResolveInputs should succeed when required pins are supplied"), bResult);
	TestEqual(TEXT("Exactly one resolved input"), ResolvedInputs.Num(), 1);
	TestEqual(TEXT("No warnings expected"), Warnings.Num(), 0);
	TestEqual(TEXT("No errors expected"), Errors.Num(), 0);
	TestFalse(TEXT("No fallback path used when names match"), bUsedFallback);

	return true;
}

bool FFPCGSchedulerFallbackInputsTest::RunTest(const FString& Parameters)
{
	FPCGSchedulerExecutor Executor;
	UPCGGraph* Graph = NewObject<UPCGGraph>();
	TestNotNull(TEXT("Graph should be created"), Graph);

	FPCGInputSet InputSet;
	InputSet.Add(FName(TEXT("ArbitraryA")), NewObject<UPCGPointData>());
	InputSet.Add(FName(TEXT("ArbitraryB")), NewObject<UPCGPointData>());

	TArray<FPCGSchedulerExecutor::FResolvedInput> ResolvedInputs;
	TArray<FString> Warnings;
	TArray<FString> Errors;
	bool bUsedFallback = false;

	const bool bResult = FSchedulerTestHelper::ResolveInputs(Executor, *Graph, InputSet, ResolvedInputs, Warnings, Errors, bUsedFallback);

	TestTrue(TEXT("ResolveInputs should still succeed with fallback ordering"), bResult);
	TestTrue(TEXT("Fallback should trigger warning"), Warnings.Num() > 0);
	TestEqual(TEXT("No hard errors when fallback works"), Errors.Num(), 0);
	TestTrue(TEXT("Fallback flag should be set"), bUsedFallback);

	return true;
}

bool FFPCGSchedulerValidationFailureTest::RunTest(const FString& Parameters)
{
	FPCGSchedulerExecutor Executor;
	UPCGGraph* Graph = NewObject<UPCGGraph>();
	TestNotNull(TEXT("Graph should be created"), Graph);

	FPCGInputSet EmptyInputSet;
	TArray<FPCGSchedulerExecutor::FResolvedInput> ResolvedInputs;
	TArray<FString> Warnings;
	TArray<FString> Errors;
	bool bUsedFallback = false;

	const bool bResult = FSchedulerTestHelper::ResolveInputs(Executor, *Graph, EmptyInputSet, ResolvedInputs, Warnings, Errors, bUsedFallback);

	TestFalse(TEXT("ResolveInputs should fail when no data provided"), bResult);
	TestTrue(TEXT("Error list should be populated"), Errors.Num() > 0);
	TestFalse(TEXT("Fallback flag should remain un-set"), bUsedFallback);

	return true;
}

bool FFPCGSchedulerCacheStateTest::RunTest(const FString& Parameters)
{
	FPCGSchedulerExecutor Executor;
	const FPCGTaskId TaskId = 42;
	FPCGSchedulerExecutor::FScheduledTask& Task = FSchedulerTestHelper::AddTask(Executor, TaskId);
\n\tTask.Context.TaskId = TaskId;

	FPCGDataCollection Output;
	FPCGTaggedData& Tagged = Output.TaggedData.AddDefaulted_GetRef();
	Tagged.Data = NewObject<UPCGPointData>();
	Tagged.Pin = PCGInputOutputConstants::DefaultInputLabel;

	FSchedulerTestHelper::CacheOutput(Executor, TaskId, Output);

	TestEqual(TEXT("Task state transitions to Completed"), Task.Context.State, EPCGTaskState::Completed);
	TestTrue(TEXT("Output should be cached"), Task.bOutputCached);
	TestNotNull(TEXT("Cached collection should exist"), Task.CachedOutput.Get());
	TestEqual(TEXT("Cached entry count"), Task.CachedOutput->TaggedData.Num(), 1);

	return true;
}

#endif // VHM_PCG_ENABLED
#endif // WITH_AUTOMATION_TESTS


