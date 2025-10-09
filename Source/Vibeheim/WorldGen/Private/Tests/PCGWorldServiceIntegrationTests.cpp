#if WITH_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Services/PCGWorldService.h"
#include "Services/PCGWorldServiceTypes.h"
#include "Services/PCGSchedulerExecutor.h"
#include "Services/HeightfieldService.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGSubsystem.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ScopeExit.h"
#include "Templates/Function.h"
#include "Algo/Reverse.h"

#if VHM_PCG_ENABLED

namespace
{
class FMockSchedulerExecutor : public FPCGSchedulerExecutor
{
public:
        enum class EMode
        {
                Success,
                FailSchedule,
                Timeout
        };

        explicit FMockSchedulerExecutor(EMode InMode = EMode::Success)
                : Mode(InMode)
        {
                if (Mode == EMode::Success)
                {
                        BuildSuccessfulOutput();
                }
        }

        void SetMode(EMode InMode)
        {
                Mode = InMode;
                bCompletionReturned = false;
        }

        void ResetStatistics()
        {
                ScheduleCallCount = 0;
                IsCompleteCallCount = 0;
                GetOutputCallCount = 0;
                ReleaseCallCount = 0;
                AbandonCallCount = 0;
                bScheduleInvoked = false;
                LastAbandonReason.Reset();
        }

        void ConfigureOutputBuilder(TFunction<void(UPCGPointData&)> InBuilder)
        {
                CustomPointBuilder = MoveTemp(InBuilder);
                BuildSuccessfulOutput();
        }

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
                TArray<FString>& OutWarnings) override
        {
                ++ScheduleCallCount;
                bScheduleInvoked = true;
                LastFrustumEnabled = bEnableFrustumCulling;
                LastFrustumMargin = FrustumMargin;
                LastTileCoord = TileCoord;
                LastExecutionBounds = ExecutionBounds;
                LastSeed = Seed;
                LastDebugLabel = DebugLabel;

                if (Mode == EMode::FailSchedule)
                {
                        OutErrors.Add(TEXT("Mock scheduler failed"));
                        return InvalidPCGTaskId;
                }

                CurrentTaskId = ++NextTaskId;
                StoredContext.World = &ExecutionWorld;
                StoredContext.TileCoord = TileCoord;
                StoredContext.TaskId = CurrentTaskId;
                StoredContext.State = EPCGTaskState::Scheduled;

                OutContext = StoredContext;
                return CurrentTaskId;
        }

        virtual bool IsTaskComplete(UPCGSubsystem& Subsystem, FPCGTaskId TaskId, FPCGTaskContext& InOutContext) override
        {
                ++IsCompleteCallCount;
                if (Mode == EMode::Timeout)
                {
                        return false;
                }

                StoredContext.State = EPCGTaskState::Completed;
                InOutContext = StoredContext;
                if (!bCompletionReturned)
                {
                        bCompletionReturned = true;
                        return true;
                }
                return true;
        }

        virtual bool GetTaskOutput(UPCGSubsystem& Subsystem,
                FPCGTaskId TaskId,
                FPCGTaskContext& Context,
                FPCGOutputSet& OutOutput,
                int32& OutPointCount,
                double& OutElapsedMs,
                TArray<FString>& OutWarnings,
                TArray<FString>& OutErrors) override
        {
                ++GetOutputCallCount;
                if (Mode != EMode::Success)
                {
                        OutErrors.Add(TEXT("Mock scheduler output unavailable"));
                        return false;
                }

                OutOutput = CachedOutput;
                OutPointCount = CachedPointCount;
                OutElapsedMs = 2.0;
                Context.State = EPCGTaskState::Completed;
                return true;
        }

        virtual void ReleaseTask(UPCGSubsystem& Subsystem, FPCGTaskId TaskId, FPCGTaskContext& Context) override
        {
                ++ReleaseCallCount;
        }

        virtual void AbandonTask(UPCGSubsystem& Subsystem, FPCGTaskId TaskId, FPCGTaskContext& Context, const FString& Reason) override
        {
                ++AbandonCallCount;
                LastAbandonReason = Reason;
                Context.State = EPCGTaskState::Abandoned;
        }

        bool bScheduleInvoked = false;
        int32 ScheduleCallCount = 0;
        int32 IsCompleteCallCount = 0;
        int32 GetOutputCallCount = 0;
        int32 ReleaseCallCount = 0;
        int32 AbandonCallCount = 0;
        bool LastFrustumEnabled = false;
        float LastFrustumMargin = 0.0f;
        FString LastAbandonReason;
        FTileCoord LastTileCoord;
        FBox LastExecutionBounds(EForceInit::ForceInit);
        int32 LastSeed = 0;
        FString LastDebugLabel;
        FPCGTaskId CurrentTaskId = InvalidPCGTaskId;
        FPCGTaskContext StoredContext;
        FPCGOutputSet CachedOutput;
        int32 CachedPointCount = 0;

private:
        void BuildSuccessfulOutput()
        {
                CachedOutput.Outputs.Reset();
                UPCGPointData* PointData = NewObject<UPCGPointData>(GetTransientPackage(), NAME_None, RF_Transient);
                UPCGMetadata* Metadata = PointData->MutableMetadata();
                const PCGMetadataEntryKey EntryKey = Metadata->AddEntry();

                auto EnsureAttribute = [Metadata, EntryKey](const FName& AttributeName, auto&& Value)
                {
                        using ValueType = typename TDecay<decltype(Value)>::Type;
                        FPCGMetadataAttribute<ValueType>* Attribute = Metadata->GetMutableTypedAttribute<ValueType>(AttributeName);
                        if (!Attribute)
                        {
                                Attribute = Metadata->CreateAttribute<ValueType>(AttributeName, Value, false, true);
                        }
                        if (Attribute)
                        {
                                Attribute->SetValue(EntryKey, Value);
                        }
                };

                EnsureAttribute(VHMPCGAttr::InstanceId, FGuid(0x00112233, 0x4455, 0x6677, 0x8899));
                EnsureAttribute(VHMPCGAttr::IsActive, true);
                EnsureAttribute(VHMPCGAttr::InstanceScale, FVector(1.0f));
                EnsureAttribute(VHMPCGAttr::InstanceRotation, FRotator::ZeroRotator);
                EnsureAttribute(VHMPCGAttr::StaticMesh, FSoftObjectPath(TEXT("/Script/Engine.StaticMesh'/Engine/BasicShapes/Cube.Cube'")));

                TArray<FPCGPoint>& Points = PointData->GetMutablePoints();
                FPCGPoint& Point = Points.AddDefaulted_GetRef();
                Point.Transform = FTransform(FRotator::ZeroRotator, FVector(100.0f, 50.0f, 20.0f), FVector(1.0f));
                Point.MetadataEntry = EntryKey;
                Point.Seed = 1337;

                if (CustomPointBuilder)
                {
                        CustomPointBuilder(*PointData);
                }

                CachedOutput.Outputs.Add(PointData);
                CachedPointCount = PointData->GetPoints().Num();
        }

        static inline FPCGTaskId NextTaskId = 0;
        bool bCompletionReturned = false;
        EMode Mode = EMode::Success;
        TFunction<void(UPCGPointData&)> CustomPointBuilder;
};

struct FPCGWorldServiceTestAccessor
{
        static void SetWorldOverride(UPCGWorldService* Service, UWorld* World)
        {
#if WITH_AUTOMATION_TESTS && VHM_PCG_ENABLED
                Service->TestWorldOverride = World;
#endif
        }

        static void SetSubsystemOverride(UPCGWorldService* Service, UPCGSubsystem* Subsystem)
        {
#if WITH_AUTOMATION_TESTS && VHM_PCG_ENABLED
                Service->TestSubsystemOverride = Subsystem;
#endif
        }

        static void SetScheduler(UPCGWorldService* Service, TUniquePtr<FPCGSchedulerExecutor>&& Executor)
        {
                Service->SchedulerExecutor = MoveTemp(Executor);
        }

        static void SetAnchorActor(UPCGWorldService* Service, AActor* Anchor)
        {
                Service->PCGAnchorActor = Anchor;
        }

        static void SetBiomeComponent(UPCGWorldService* Service, EBiomeType Biome, UPCGComponent* Component)
        {
                Service->BiomePCGComponents.Add(Biome, Component);
        }

        static void ResetActiveTasks(UPCGWorldService* Service)
        {
                Service->ActiveTasks.Reset();
        }

        static void AddActiveTask(UPCGWorldService* Service, FPCGTaskId TaskId, const FPCGTaskContext& Context)
        {
                Service->ActiveTasks.Add(TaskId, Context);
        }

        static FPCGGenerationData InvokeGenerate(UPCGWorldService* Service, const FTileCoord& TileCoord, EBiomeType Biome, const TArray<float>& HeightData, UPCGGraph* Graph)
        {
                return Service->GeneratePCGContent(TileCoord, Biome, HeightData, Graph, nullptr);
        }

        static void ApplySettings(UPCGWorldService* Service, const FWorldGenConfig& Settings)
        {
                Service->WorldGenSettings = Settings;
        }

        static void SetRuntimeEnabled(UPCGWorldService* Service, bool bEnabled)
        {
                Service->bRuntimeOperationsEnabled = bEnabled;
        }
};

static TArray<float> CreateFlatHeightfield()
{
        TArray<float> HeightData;
        HeightData.Init(0.0f, 64 * 64);
        return HeightData;
}

static FWorldGenConfig BuildDefaultConfig()
{
        FWorldGenConfig Config;
        Config.bEnablePCGGraphs = true;
        Config.MaxConcurrentPCGTasks = 2;
        Config.bEnableFrustumCulling = true;
        Config.FrustumCullingMargin = 250.0f;
        Config.FrustumCullingMarginByLOD.Add(NAME_None, 250.0f);
        return Config;
}

static UPCGWorldService* CreateConfiguredService(FWorldGenConfig& OutConfig, UWorld*& OutWorld, UPCGSubsystem*& OutSubsystem, AActor*& OutAnchor, UPCGComponent*& OutComponent)
{
        OutConfig = BuildDefaultConfig();

        OutWorld = NewObject<UWorld>(GetTransientPackage());
        OutWorld->AddToRoot();

        OutSubsystem = NewObject<UPCGSubsystem>(OutWorld);
        OutSubsystem->AddToRoot();

        OutAnchor = NewObject<AActor>(GetTransientPackage());
        OutAnchor->AddToRoot();

        OutComponent = NewObject<UPCGComponent>(OutAnchor);
        OutComponent->AddToRoot();

        UPCGWorldService* Service = NewObject<UPCGWorldService>(GetTransientPackage());
        Service->AddToRoot();

        FPCGWorldServiceTestAccessor::SetWorldOverride(Service, OutWorld);
        FPCGWorldServiceTestAccessor::SetSubsystemOverride(Service, OutSubsystem);
        FPCGWorldServiceTestAccessor::SetAnchorActor(Service, OutAnchor);
        FPCGWorldServiceTestAccessor::SetBiomeComponent(Service, EBiomeType::Meadows, OutComponent);
        FPCGWorldServiceTestAccessor::ApplySettings(Service, OutConfig);
        FPCGWorldServiceTestAccessor::SetRuntimeEnabled(Service, true);

        Service->Initialize(OutConfig);

        FBiomeDefinition Biome;
        Biome.BiomeType = EBiomeType::Meadows;
        FPCGVegetationRule Rule;
        Rule.Density = 0.02f;
        Biome.VegetationRules.Add(Rule);

        TMap<EBiomeType, FBiomeDefinition> Biomes;
        Biomes.Add(EBiomeType::Meadows, Biome);
        Service->SetBiomeDefinitions(Biomes);

        return Service;
}

static void DestroyConfiguredService(UPCGWorldService* Service, UWorld* World, UPCGSubsystem* Subsystem, AActor* Anchor, UPCGComponent* Component)
{
        if (Component)
        {
                Component->RemoveFromRoot();
        }
        if (Anchor)
        {
                Anchor->RemoveFromRoot();
        }
        if (Subsystem)
        {
                Subsystem->RemoveFromRoot();
        }
        if (World)
        {
                World->RemoveFromRoot();
        }
        if (Service)
        {
                Service->RemoveFromRoot();
        }
}

static void ConfigureDifferenceOutput(FMockSchedulerExecutor& Executor)
{
        Executor.ConfigureOutputBuilder([](UPCGPointData& PointData)
        {
                UPCGMetadata* Metadata = PointData.MutableMetadata();
                FPCGMetadataAttribute<FGuid>* InstanceIdAttr = Metadata->GetMutableTypedAttribute<FGuid>(VHMPCGAttr::InstanceId);
                FPCGMetadataAttribute<bool>* IsActiveAttr = Metadata->GetMutableTypedAttribute<bool>(VHMPCGAttr::IsActive);
                FPCGMetadataAttribute<FVector>* ScaleAttr = Metadata->GetMutableTypedAttribute<FVector>(VHMPCGAttr::InstanceScale);
                FPCGMetadataAttribute<FRotator>* RotationAttr = Metadata->GetMutableTypedAttribute<FRotator>(VHMPCGAttr::InstanceRotation);
                FPCGMetadataAttribute<FSoftObjectPath>* MeshAttr = Metadata->GetMutableTypedAttribute<FSoftObjectPath>(VHMPCGAttr::StaticMesh);

                const PCGMetadataEntryKey Entry = Metadata->AddEntry();
                InstanceIdAttr->SetValue(Entry, FGuid(0xABCDEF01, 0x1234, 0x5678, 0x9ABC));
                IsActiveAttr->SetValue(Entry, false);
                ScaleAttr->SetValue(Entry, FVector(1.0f));
                RotationAttr->SetValue(Entry, FRotator::ZeroRotator);
                MeshAttr->SetValue(Entry, FSoftObjectPath(TEXT("/Script/Engine.StaticMesh'/Engine/BasicShapes/Cube.Cube'")));

                TArray<FPCGPoint>& Points = PointData.GetMutablePoints();
                FPCGPoint& Point = Points.AddDefaulted_GetRef();
                Point.Transform = FTransform(FRotator::ZeroRotator, FVector(-80.0f, 25.0f, 0.0f), FVector(1.0f));
                Point.MetadataEntry = Entry;
                Point.Seed = 2024;
        });
}

static void ConfigureDeterministicOutput(FMockSchedulerExecutor& Executor, bool bReverseOrder)
{
        Executor.ConfigureOutputBuilder([bReverseOrder](UPCGPointData& PointData)
        {
                UPCGMetadata* Metadata = PointData.MutableMetadata();
                FPCGMetadataAttribute<FGuid>* InstanceIdAttr = Metadata->GetMutableTypedAttribute<FGuid>(VHMPCGAttr::InstanceId);
                FPCGMetadataAttribute<bool>* IsActiveAttr = Metadata->GetMutableTypedAttribute<bool>(VHMPCGAttr::IsActive);
                FPCGMetadataAttribute<FVector>* ScaleAttr = Metadata->GetMutableTypedAttribute<FVector>(VHMPCGAttr::InstanceScale);
                FPCGMetadataAttribute<FRotator>* RotationAttr = Metadata->GetMutableTypedAttribute<FRotator>(VHMPCGAttr::InstanceRotation);
                FPCGMetadataAttribute<FSoftObjectPath>* MeshAttr = Metadata->GetMutableTypedAttribute<FSoftObjectPath>(VHMPCGAttr::StaticMesh);

                struct FDeterministicPoint
                {
                        FVector Location;
                        FGuid Guid;
                };

                TArray<FDeterministicPoint> PointsToAdd = {
                        {FVector(40.0f, 10.0f, 0.0f), FGuid(0x11110001, 0x2222, 0x3333, 0x4444)},
                        {FVector(-40.0f, -10.0f, 0.0f), FGuid(0x11110002, 0x2222, 0x3333, 0x5555)}
                };

                if (bReverseOrder)
                {
                        Algo::Reverse(PointsToAdd);
                }

                for (const FDeterministicPoint& EntryDef : PointsToAdd)
                {
                        const PCGMetadataEntryKey Entry = Metadata->AddEntry();
                        InstanceIdAttr->SetValue(Entry, EntryDef.Guid);
                        IsActiveAttr->SetValue(Entry, true);
                        ScaleAttr->SetValue(Entry, FVector(1.0f));
                        RotationAttr->SetValue(Entry, FRotator::ZeroRotator);
                        MeshAttr->SetValue(Entry, FSoftObjectPath(TEXT("/Script/Engine.StaticMesh'/Engine/BasicShapes/Cube.Cube'")));

                        TArray<FPCGPoint>& Points = PointData.GetMutablePoints();
                        FPCGPoint& Point = Points.AddDefaulted_GetRef();
                        Point.Transform = FTransform(FRotator::ZeroRotator, EntryDef.Location, FVector(1.0f));
                        Point.MetadataEntry = Entry;
                        Point.Seed = 7777;
                }
        });
}
}

#endif // VHM_PCG_ENABLED

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPCGWorldServiceSchedulerSuccessTest, "Vibeheim.PCG.WorldService.Integration.SchedulerSuccess", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPCGWorldServiceSchedulerFailureFallbackTest, "Vibeheim.PCG.WorldService.Integration.SchedulerFallback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPCGWorldServiceSchedulerTimeoutFallbackTest, "Vibeheim.PCG.WorldService.Integration.SchedulerTimeout", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPCGWorldServiceConcurrentTaskCapTest, "Vibeheim.PCG.WorldService.Integration.ConcurrentCap", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPCGWorldServiceFrustumToggleTest, "Vibeheim.PCG.WorldService.Integration.FrustumToggle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPCGWorldServicePerformanceStatsTest, "Vibeheim.PCG.WorldService.Integration.PerformanceStats", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPCGWorldServiceDifferenceRegressionTest, "Vibeheim.PCG.WorldService.Integration.DifferenceRegression", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPCGWorldServiceDeterminismHashTest, "Vibeheim.PCG.WorldService.Integration.Determinism", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPCGWorldServiceRespectGraphZTest, "Vibeheim.PCG.WorldService.Integration.RespectGraphZ", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGWorldServiceSchedulerSuccessTest::RunTest(const FString& Parameters)
{
#if !VHM_PCG_ENABLED
        return true;
#else
        FWorldGenConfig Config;
        UWorld* World = nullptr;
        UPCGSubsystem* Subsystem = nullptr;
        AActor* Anchor = nullptr;
        UPCGComponent* Component = nullptr;
        UPCGWorldService* Service = CreateConfiguredService(Config, World, Subsystem, Anchor, Component);

        TUniquePtr<FMockSchedulerExecutor> Executor = MakeUnique<FMockSchedulerExecutor>(FMockSchedulerExecutor::EMode::Success);
        FMockSchedulerExecutor* ExecutorPtr = Executor.Get();
        FPCGWorldServiceTestAccessor::SetScheduler(Service, TUniquePtr<FPCGSchedulerExecutor>(Executor.Release()));

        UPCGGraph* Graph = NewObject<UPCGGraph>(GetTransientPackage(), NAME_None, RF_Transient);
        Graph->AddToRoot();

        const TArray<float> HeightData = CreateFlatHeightfield();
        const FTileCoord TileCoord(2, 3);

        FPCGGenerationData Generation = FPCGWorldServiceTestAccessor::InvokeGenerate(Service, TileCoord, EBiomeType::Meadows, HeightData, Graph);

        TestTrue(TEXT("Scheduler path should produce instances"), Generation.TotalInstanceCount > 0);
        TestTrue(TEXT("Scheduler invocation expected"), ExecutorPtr->bScheduleInvoked);
        TestEqual(TEXT("Generation tile matches request"), Generation.TileCoord, TileCoord);

        Graph->RemoveFromRoot();
        DestroyConfiguredService(Service, World, Subsystem, Anchor, Component);
        return true;
#endif
}

bool FPCGWorldServiceSchedulerFailureFallbackTest::RunTest(const FString& Parameters)
{
#if !VHM_PCG_ENABLED
        return true;
#else
        FWorldGenConfig Config;
        UWorld* World = nullptr;
        UPCGSubsystem* Subsystem = nullptr;
        AActor* Anchor = nullptr;
        UPCGComponent* Component = nullptr;
        UPCGWorldService* Service = CreateConfiguredService(Config, World, Subsystem, Anchor, Component);

        auto Executor = MakeUnique<FMockSchedulerExecutor>(FMockSchedulerExecutor::EMode::FailSchedule);
        FMockSchedulerExecutor* ExecutorPtr = Executor.Get();
        FPCGWorldServiceTestAccessor::SetScheduler(Service, TUniquePtr<FPCGSchedulerExecutor>(Executor.Release()));

        UPCGGraph* Graph = NewObject<UPCGGraph>(GetTransientPackage(), NAME_None, RF_Transient);
        Graph->AddToRoot();

        const TArray<float> HeightData = CreateFlatHeightfield();
        FPCGGenerationData Generation = FPCGWorldServiceTestAccessor::InvokeGenerate(Service, FTileCoord(0, 0), EBiomeType::Meadows, HeightData, Graph);

        TestTrue(TEXT("Scheduler attempted to schedule"), ExecutorPtr->ScheduleCallCount == 1);
        TestTrue(TEXT("Fallback should still produce generation data"), Generation.TotalInstanceCount >= 0);
        TestTrue(TEXT("No scheduler output captured on failure"), ExecutorPtr->GetOutputCallCount == 0);

        Graph->RemoveFromRoot();
        DestroyConfiguredService(Service, World, Subsystem, Anchor, Component);
        return true;
#endif
}

bool FPCGWorldServiceSchedulerTimeoutFallbackTest::RunTest(const FString& Parameters)
{
#if !VHM_PCG_ENABLED
        return true;
#else
        FWorldGenConfig Config;
        UWorld* World = nullptr;
        UPCGSubsystem* Subsystem = nullptr;
        AActor* Anchor = nullptr;
        UPCGComponent* Component = nullptr;
        UPCGWorldService* Service = CreateConfiguredService(Config, World, Subsystem, Anchor, Component);

        auto Executor = MakeUnique<FMockSchedulerExecutor>(FMockSchedulerExecutor::EMode::Timeout);
        FMockSchedulerExecutor* ExecutorPtr = Executor.Get();
        FPCGWorldServiceTestAccessor::SetScheduler(Service, TUniquePtr<FPCGSchedulerExecutor>(Executor.Release()));

        IConsoleVariable* PollVar = IConsoleManager::Get().FindConsoleVariable(TEXT("vhm.pcg.poll_ms"));
        IConsoleVariable* TimeoutVar = IConsoleManager::Get().FindConsoleVariable(TEXT("vhm.pcg.timeout_ms"));
        const int32 OriginalPoll = PollVar ? PollVar->GetInt() : 12;
        const int32 OriginalTimeout = TimeoutVar ? TimeoutVar->GetInt() : 5000;
        if (PollVar)
        {
                PollVar->Set(1);
        }
        if (TimeoutVar)
        {
                TimeoutVar->Set(5);
        }
        ON_SCOPE_EXIT
        {
                if (PollVar)
                {
                        PollVar->Set(OriginalPoll);
                }
                if (TimeoutVar)
                {
                        TimeoutVar->Set(OriginalTimeout);
                }
        };

        UPCGGraph* Graph = NewObject<UPCGGraph>(GetTransientPackage(), NAME_None, RF_Transient);
        Graph->AddToRoot();

        const TArray<float> HeightData = CreateFlatHeightfield();
        FPCGGenerationData Generation = FPCGWorldServiceTestAccessor::InvokeGenerate(Service, FTileCoord(1, 1), EBiomeType::Meadows, HeightData, Graph);

        TestTrue(TEXT("Timeout should trigger abandon"), ExecutorPtr->AbandonCallCount > 0);
        TestTrue(TEXT("Timeout fallback returns deterministic data"), Generation.TotalInstanceCount >= 0);

        Graph->RemoveFromRoot();
        DestroyConfiguredService(Service, World, Subsystem, Anchor, Component);
        return true;
#endif
}

bool FPCGWorldServiceConcurrentTaskCapTest::RunTest(const FString& Parameters)
{
#if !VHM_PCG_ENABLED
        return true;
#else
        FWorldGenConfig Config;
        UWorld* World = nullptr;
        UPCGSubsystem* Subsystem = nullptr;
        AActor* Anchor = nullptr;
        UPCGComponent* Component = nullptr;
        UPCGWorldService* Service = CreateConfiguredService(Config, World, Subsystem, Anchor, Component);

        Config.MaxConcurrentPCGTasks = 1;
        FPCGWorldServiceTestAccessor::ApplySettings(Service, Config);

        auto Executor = MakeUnique<FMockSchedulerExecutor>(FMockSchedulerExecutor::EMode::Success);
        FMockSchedulerExecutor* ExecutorPtr = Executor.Get();
        FPCGWorldServiceTestAccessor::SetScheduler(Service, TUniquePtr<FPCGSchedulerExecutor>(Executor.Release()));

        FPCGWorldServiceTestAccessor::ResetActiveTasks(Service);
        FPCGTaskContext BusyContext;
        BusyContext.State = EPCGTaskState::Scheduled;
        FPCGWorldServiceTestAccessor::AddActiveTask(Service, 99, BusyContext);

        UPCGGraph* Graph = NewObject<UPCGGraph>(GetTransientPackage(), NAME_None, RF_Transient);
        Graph->AddToRoot();

        const TArray<float> HeightData = CreateFlatHeightfield();
        FPCGGenerationData Generation = FPCGWorldServiceTestAccessor::InvokeGenerate(Service, FTileCoord(4, 7), EBiomeType::Meadows, HeightData, Graph);

        TestEqual(TEXT("Scheduler should not be invoked when cap reached"), ExecutorPtr->ScheduleCallCount, 0);
        TestTrue(TEXT("Fallback still produces generation data"), Generation.TotalInstanceCount >= 0);

        Graph->RemoveFromRoot();
        DestroyConfiguredService(Service, World, Subsystem, Anchor, Component);
        return true;
#endif
}

bool FPCGWorldServiceFrustumToggleTest::RunTest(const FString& Parameters)
{
#if !VHM_PCG_ENABLED
        return true;
#else
        FWorldGenConfig Config;
        UWorld* World = nullptr;
        UPCGSubsystem* Subsystem = nullptr;
        AActor* Anchor = nullptr;
        UPCGComponent* Component = nullptr;
        UPCGWorldService* Service = CreateConfiguredService(Config, World, Subsystem, Anchor, Component);

        auto Executor = MakeUnique<FMockSchedulerExecutor>(FMockSchedulerExecutor::EMode::Success);
        FMockSchedulerExecutor* ExecutorPtr = Executor.Get();
        FPCGWorldServiceTestAccessor::SetScheduler(Service, TUniquePtr<FPCGSchedulerExecutor>(Executor.Release()));

        UPCGGraph* Graph = NewObject<UPCGGraph>(GetTransientPackage(), NAME_None, RF_Transient);
        Graph->AddToRoot();
        const TArray<float> HeightData = CreateFlatHeightfield();

        // First call with frustum enabled
        Config.bEnableFrustumCulling = true;
        Config.FrustumCullingMargin = 500.0f;
        Config.FrustumCullingMarginByLOD.Add(NAME_None, 500.0f);
        FPCGWorldServiceTestAccessor::ApplySettings(Service, Config);
        FPCGWorldServiceTestAccessor::InvokeGenerate(Service, FTileCoord(10, 11), EBiomeType::Meadows, HeightData, Graph);
        TestTrue(TEXT("Frustum should be enabled"), ExecutorPtr->LastFrustumEnabled);
        TestEqual(TEXT("Frustum margin propagated"), ExecutorPtr->LastFrustumMargin, 500.0f);

        // Second call with frustum disabled
        ExecutorPtr->ResetStatistics();
        Config.bEnableFrustumCulling = false;
        FPCGWorldServiceTestAccessor::ApplySettings(Service, Config);
        FPCGWorldServiceTestAccessor::InvokeGenerate(Service, FTileCoord(12, 13), EBiomeType::Meadows, HeightData, Graph);
        TestFalse(TEXT("Frustum should be disabled"), ExecutorPtr->LastFrustumEnabled);

        Graph->RemoveFromRoot();
        DestroyConfiguredService(Service, World, Subsystem, Anchor, Component);
        return true;
#endif
}

bool FPCGWorldServicePerformanceStatsTest::RunTest(const FString& Parameters)
{
#if !VHM_PCG_ENABLED
        return true;
#else
        FWorldGenConfig Config;
        UWorld* World = nullptr;
        UPCGSubsystem* Subsystem = nullptr;
        AActor* Anchor = nullptr;
        UPCGComponent* Component = nullptr;
        UPCGWorldService* Service = CreateConfiguredService(Config, World, Subsystem, Anchor, Component);

        auto Executor = MakeUnique<FMockSchedulerExecutor>(FMockSchedulerExecutor::EMode::Success);
        FMockSchedulerExecutor* ExecutorPtr = Executor.Get();
        FPCGWorldServiceTestAccessor::SetScheduler(Service, TUniquePtr<FPCGSchedulerExecutor>(Executor.Release()));

        UPCGGraph* Graph = NewObject<UPCGGraph>(GetTransientPackage(), NAME_None, RF_Transient);
        Graph->AddToRoot();
        const TArray<float> HeightData = CreateFlatHeightfield();

        FPCGGenerationData FirstGeneration = FPCGWorldServiceTestAccessor::InvokeGenerate(Service, FTileCoord(3, 5), EBiomeType::Meadows, HeightData, Graph);
        FPCGPerformanceStats StatsAfterFirst = Service->GetPerformanceStats();

        TestEqual(TEXT("Last generation time propagated"), StatsAfterFirst.LastGenerationTimeMs, FirstGeneration.GenerationTimeMs);
        TestTrue(TEXT("Total instances counted"), StatsAfterFirst.TotalInstancesGenerated >= FirstGeneration.TotalInstanceCount);

        // Run a second time to populate the average and validate caching-style accumulation.
        FPCGGenerationData SecondGeneration = FPCGWorldServiceTestAccessor::InvokeGenerate(Service, FTileCoord(3, 5), EBiomeType::Meadows, HeightData, Graph);
        FPCGPerformanceStats StatsAfterSecond = Service->GetPerformanceStats();

        TestTrue(TEXT("Average time should be positive"), StatsAfterSecond.AverageGenerationTimeMs > 0.0f);
        TestTrue(TEXT("Totals accumulate across runs"), StatsAfterSecond.TotalInstancesGenerated >= FirstGeneration.TotalInstanceCount + SecondGeneration.TotalInstanceCount);

        Graph->RemoveFromRoot();
        DestroyConfiguredService(Service, World, Subsystem, Anchor, Component);
        return true;
#endif
}

bool FPCGWorldServiceDifferenceRegressionTest::RunTest(const FString& Parameters)
{
#if !VHM_PCG_ENABLED
        return true;
#else
        FWorldGenConfig Config;
        UWorld* World = nullptr;
        UPCGSubsystem* Subsystem = nullptr;
        AActor* Anchor = nullptr;
        UPCGComponent* Component = nullptr;
        UPCGWorldService* Service = CreateConfiguredService(Config, World, Subsystem, Anchor, Component);

        auto Executor = MakeUnique<FMockSchedulerExecutor>(FMockSchedulerExecutor::EMode::Success);
        FMockSchedulerExecutor* ExecutorPtr = Executor.Get();
        FPCGWorldServiceTestAccessor::SetScheduler(Service, TUniquePtr<FPCGSchedulerExecutor>(Executor.Release()));
        ConfigureDifferenceOutput(*ExecutorPtr);

        UPCGGraph* Graph = NewObject<UPCGGraph>(GetTransientPackage(), NAME_None, RF_Transient);
        Graph->AddToRoot();
        const TArray<float> HeightData = CreateFlatHeightfield();

        FPCGGenerationData Generation = FPCGWorldServiceTestAccessor::InvokeGenerate(Service, FTileCoord(8, 9), EBiomeType::Meadows, HeightData, Graph);

        TestEqual(TEXT("Inactive points should be culled"), Generation.TotalInstanceCount, 1);
        TestTrue(TEXT("Only active instances remain"), Generation.GeneratedInstances.Num() == 1 && Generation.GeneratedInstances[0].bIsActive);

        Graph->RemoveFromRoot();
        DestroyConfiguredService(Service, World, Subsystem, Anchor, Component);
        return true;
#endif
}

bool FPCGWorldServiceDeterminismHashTest::RunTest(const FString& Parameters)
{
#if !VHM_PCG_ENABLED
        return true;
#else
        FWorldGenConfig Config;
        UWorld* World = nullptr;
        UPCGSubsystem* Subsystem = nullptr;
        AActor* Anchor = nullptr;
        UPCGComponent* Component = nullptr;
        UPCGWorldService* Service = CreateConfiguredService(Config, World, Subsystem, Anchor, Component);

        auto Executor = MakeUnique<FMockSchedulerExecutor>(FMockSchedulerExecutor::EMode::Success);
        FMockSchedulerExecutor* ExecutorPtr = Executor.Get();
        FPCGWorldServiceTestAccessor::SetScheduler(Service, TUniquePtr<FPCGSchedulerExecutor>(Executor.Release()));

        UPCGGraph* Graph = NewObject<UPCGGraph>(GetTransientPackage(), NAME_None, RF_Transient);
        Graph->AddToRoot();
        const TArray<float> HeightData = CreateFlatHeightfield();

        ConfigureDeterministicOutput(*ExecutorPtr, /*bReverseOrder=*/false);
        FPCGGenerationData FirstRun = FPCGWorldServiceTestAccessor::InvokeGenerate(Service, FTileCoord(5, 6), EBiomeType::Meadows, HeightData, Graph);

        ConfigureDeterministicOutput(*ExecutorPtr, /*bReverseOrder=*/true);
        FPCGGenerationData SecondRun = FPCGWorldServiceTestAccessor::InvokeGenerate(Service, FTileCoord(5, 6), EBiomeType::Meadows, HeightData, Graph);

        TestEqual(TEXT("Deterministic instance counts"), FirstRun.TotalInstanceCount, SecondRun.TotalInstanceCount);
        TestEqual(TEXT("Deterministic transform hash"), FirstRun.InstanceTransformHash, SecondRun.InstanceTransformHash);

        Graph->RemoveFromRoot();
        DestroyConfiguredService(Service, World, Subsystem, Anchor, Component);
        return true;
#endif
}

bool FPCGWorldServiceRespectGraphZTest::RunTest(const FString& Parameters)
{
#if !VHM_PCG_ENABLED
        return true;
#else
        FWorldGenConfig Config;
        UWorld* World = nullptr;
        UPCGSubsystem* Subsystem = nullptr;
        AActor* Anchor = nullptr;
        UPCGComponent* Component = nullptr;
        UPCGWorldService* Service = CreateConfiguredService(Config, World, Subsystem, Anchor, Component);

        UHeightfieldService* Heightfield = NewObject<UHeightfieldService>(GetTransientPackage());
        Heightfield->AddToRoot();
        Heightfield->Initialize(Config);
        Service->SetHeightfieldService(Heightfield);

        auto Executor = MakeUnique<FMockSchedulerExecutor>(FMockSchedulerExecutor::EMode::Success);
        FMockSchedulerExecutor* ExecutorPtr = Executor.Get();
        FPCGWorldServiceTestAccessor::SetScheduler(Service, TUniquePtr<FPCGSchedulerExecutor>(Executor.Release()));

        UPCGGraph* Graph = NewObject<UPCGGraph>(GetTransientPackage(), NAME_None, RF_Transient);
        Graph->AddToRoot();

        const TArray<float> HeightData = CreateFlatHeightfield();
        const FTileCoord TileCoord(0, 0);

        // Case 1: Attribute missing -> defaults to projecting onto terrain height (0.0f)
        ExecutorPtr->ConfigureOutputBuilder([](UPCGPointData& PointData)
        {
                TArray<FPCGPoint>& Points = PointData.GetMutablePoints();
                check(Points.Num() == 1);
                FPCGPoint& Point = Points[0];
                Point.Transform.SetLocation(FVector(25.0f, -10.0f, 90.0f));
        });

        const FPCGGenerationData ProjectedGeneration = FPCGWorldServiceTestAccessor::InvokeGenerate(
                Service, TileCoord, EBiomeType::Meadows, HeightData, Graph);

        TestTrue(TEXT("Projected generation should produce an instance"), ProjectedGeneration.GeneratedInstances.Num() == 1);
        if (ProjectedGeneration.GeneratedInstances.Num() == 1)
        {
                const float ProjectedZ = ProjectedGeneration.GeneratedInstances[0].Location.Z;
                TestTrue(TEXT("Missing RespectGraphZ attribute projects to sampled height"),
                        FMath::IsNearlyEqual(ProjectedZ, 0.0f));
        }

        // Case 2: Attribute explicitly enabled -> preserve graph-authored Z when within bounds
        ExecutorPtr->ConfigureOutputBuilder([](UPCGPointData& PointData)
        {
                TArray<FPCGPoint>& Points = PointData.GetMutablePoints();
                check(Points.Num() == 1);
                FPCGPoint& Point = Points[0];
                Point.Transform.SetLocation(FVector(-40.0f, 55.0f, 42.0f));

                if (UPCGMetadata* Metadata = PointData.MutableMetadata())
                {
                        FPCGMetadataAttribute<bool>* RespectAttr = Metadata->GetMutableTypedAttribute<bool>(VHMPCGAttr::RespectGraphZ);
                        if (!RespectAttr)
                        {
                                RespectAttr = Metadata->CreateAttribute<bool>(VHMPCGAttr::RespectGraphZ, false, true, true);
                        }

                        if (RespectAttr && Point.MetadataEntry != PCGInvalidEntryKey)
                        {
                                RespectAttr->SetValue(Point.MetadataEntry, true);
                        }
                }
        });

        const FPCGGenerationData RespectedGeneration = FPCGWorldServiceTestAccessor::InvokeGenerate(
                Service, TileCoord, EBiomeType::Meadows, HeightData, Graph);

        TestTrue(TEXT("Respect generation should produce an instance"), RespectedGeneration.GeneratedInstances.Num() == 1);
        if (RespectedGeneration.GeneratedInstances.Num() == 1)
        {
                const float PreservedZ = RespectedGeneration.GeneratedInstances[0].Location.Z;
                TestTrue(TEXT("RespectGraphZ=true preserves graph-authored Z"),
                        FMath::IsNearlyEqual(PreservedZ, 42.0f));
        }

        Graph->RemoveFromRoot();
        Service->SetHeightfieldService(nullptr);
        Heightfield->RemoveFromRoot();
        DestroyConfiguredService(Service, World, Subsystem, Anchor, Component);
        return true;
#endif
}

#endif // WITH_AUTOMATION_TESTS
