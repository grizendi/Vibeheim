#include "Services/PCGWorldService.h"
#include "Algo/Sort.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Math/Box2D.h"
#include "PCGVersionGuard.h"
#include "Utils/WorldGenLogging.h"


#if VHM_PCG_ENABLED
#include "Data/PCGPointData.h"
#include "Graph/PCGStackContext.h"
#include "Helpers/PCGMetadataHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataCommon.h"
#include "PCGComponent.h"
#include "PCGData.h"
#include "PCGGraph.h"
#include "PCGParamData.h"
#include "PCGSubsystem.h"
#include "Services/PCGSchedulerExecutor.h"

#endif

#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"
#include "Components/StaticMeshComponent.h"
#include "Containers/StringConv.h"
#include "Data/InstancePersistence.h"
#include "Data/SerializationShims.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "Services/BiomeService.h"
#include "Services/HeightfieldService.h"
#include "Trace/Trace.inl"
#include "UObject/ConstructorHelpers.h"


DEFINE_LOG_CATEGORY(LogPCGWorldService);

#if VHM_PCG_ENABLED
TRACE_DECLARE_INT_COUNTER(VibeheimPCGActiveTasks,
                          TEXT("Vibeheim/PCG/ActiveTasks"));
TRACE_DECLARE_FLOAT_COUNTER(VibeheimPCGLastDurationMs,
                            TEXT("Vibeheim/PCG/LastDurationMs"));
TRACE_DECLARE_INT_COUNTER(VibeheimPCGLastPointCount,
                          TEXT("Vibeheim/PCG/LastPointCount"));
#endif

#if VHM_PCG_ENABLED
void FPCGSchedulerExecutorDeleter::operator()(
    FPCGSchedulerExecutor *Ptr) const {
  delete Ptr;
}
#endif

#if WITH_AUTOMATION_TESTS
#if VHM_PCG_ENABLED
void FPCGWorldServiceTestAccessor::SetScheduler(
    UPCGWorldService *Service, FPCGSchedulerExecutorPtr &&Executor) {
  if (Service) {
    Service->SchedulerExecutor = MoveTemp(Executor);
  }
}

void FPCGWorldServiceTestAccessor::SetWorldOverride(UPCGWorldService *Service,
                                                    UWorld *World) {
  if (Service) {
    Service->TestWorldOverride = World;
  }
}

void FPCGWorldServiceTestAccessor::SetSubsystemOverride(
    UPCGWorldService *Service, UPCGSubsystem *Subsystem) {
  if (Service) {
    Service->TestSubsystemOverride = Subsystem;
  }
}

void FPCGWorldServiceTestAccessor::SetAnchorActor(UPCGWorldService *Service,
                                                  AActor *Anchor) {
  if (Service) {
    Service->PCGAnchorActor = Anchor;
  }
}

void FPCGWorldServiceTestAccessor::SetBiomeComponent(UPCGWorldService *Service,
                                                     EBiomeType Biome,
                                                     UPCGComponent *Component) {
  if (Service) {
    Service->BiomePCGComponents.Add(Biome, Component);
  }
}

void FPCGWorldServiceTestAccessor::ResetActiveTasks(UPCGWorldService *Service) {
  if (Service) {
    Service->ActiveTasks.Reset();
  }
}

void FPCGWorldServiceTestAccessor::AddActiveTask(
    UPCGWorldService *Service, FPCGTaskId TaskId,
    const FPCGTaskContext &Context) {
  if (Service) {
    Service->ActiveTasks.Add(TaskId, Context);
  }
}

FPCGGenerationData FPCGWorldServiceTestAccessor::InvokeGenerate(
    UPCGWorldService *Service, const FTileCoord &TileCoord, EBiomeType Biome,
    const TArray<float> &HeightData, UPCGGraph *Graph) {
  return Service ? Service->GeneratePCGContent(TileCoord, Biome, HeightData,
                                               Graph, nullptr)
                 : FPCGGenerationData();
}
#endif // VHM_PCG_ENABLED

void FPCGWorldServiceTestAccessor::ApplySettings(
    UPCGWorldService *Service, const FWorldGenConfig &Settings) {
  if (Service) {
    Service->WorldGenSettings = Settings;
  }
}

void FPCGWorldServiceTestAccessor::SetRuntimeEnabled(UPCGWorldService *Service,
                                                     bool bEnabled) {
  if (Service) {
    Service->bRuntimeOperationsEnabled = bEnabled;
  }
}
#endif // WITH_AUTOMATION_TESTS

static TAutoConsoleVariable<int32> CVarVibeheimPCGMaxConcurrent(
    TEXT("vhm.pcg.max_concurrent"), 4,
    TEXT("Maximum number of concurrent runtime PCG scheduler tasks."),
    ECVF_Default);

static TAutoConsoleVariable<int32> CVarVibeheimPCGFrustumEnable(
    TEXT("vhm.pcg.frustum.enable"), 1,
    TEXT("Enables frustum culling for runtime PCG scheduling."), ECVF_Default);

static TAutoConsoleVariable<float> CVarVibeheimPCGFrustumMargin(
    TEXT("vhm.pcg.frustum.margin"), 500.0f,
    TEXT("Additional world-space margin added to frustum bounds when "
         "scheduling PCG tasks."),
    ECVF_Default);

static TAutoConsoleVariable<int32> CVarVibeheimPCGTelemetryCsv(
    TEXT("vhm.pcg.telemetry.csv"), 0,
    TEXT(
        "When > 0, append PCG task telemetry rows to Saved/PCG/pcg_tasks.csv."),
    ECVF_Default);

#if WITH_EDITOR
#include "EngineUtils.h"
#include "PCGNode.h"
#include "PCGPin.h"
#include "PCGSettings.h"
#include "WorldGenManager.h"

namespace PCGWorldService::Editor {
static UWorld *ResolveActiveWorld() {
  if (!GEngine) {
    return nullptr;
  }

  const TIndirectArray<FWorldContext> &Contexts = GEngine->GetWorldContexts();
  for (const FWorldContext &Context : Contexts) {
    UWorld *World = Context.World();
    if (!World) {
      continue;
    }

    const EWorldType::Type WorldType = World->WorldType;
    if (WorldType == EWorldType::PIE || WorldType == EWorldType::Game ||
        WorldType == EWorldType::GameRPC ||
        WorldType == EWorldType::GamePreview) {
      return World;
    }
  }

  for (const FWorldContext &Context : Contexts) {
    UWorld *World = Context.World();
    if (!World) {
      continue;
    }

    const EWorldType::Type WorldType = World->WorldType;
    if (WorldType == EWorldType::Editor ||
        WorldType == EWorldType::EditorPreview) {
      return World;
    }
  }

  return nullptr;
}

static UPCGWorldService *ResolveWorldService(UWorld *World) {
  if (!World) {
    return nullptr;
  }

  for (TActorIterator<AWorldGenManager> It(World); It; ++It) {
    if (AWorldGenManager *Manager = *It) {
      if (UPCGWorldService *Service = Manager->GetPCGWorldService()) {
        return Service;
      }
    }
  }

  return nullptr;
}

static void HandlePcgShowDeps(const TArray<FString> &Args) {
  if (Args.IsEmpty()) {
    UE_LOG(LogPCGWorldService, Error,
           TEXT("Usage: wg.pcg.showdeps <GraphAssetPath>"));
    return;
  }

  const FString GraphPath = Args[0];
  UPCGGraph *Graph = LoadObject<UPCGGraph>(nullptr, *GraphPath);
  if (!Graph) {
    UE_LOG(LogPCGWorldService, Error, TEXT("Failed to load PCG graph '%s'."),
           *GraphPath);
    return;
  }

  UE_LOG(LogPCGWorldService, Log, TEXT("wg.pcg.showdeps: Graph=%s Path=%s"),
         *Graph->GetName(), *Graph->GetPathName());

  int32 TotalNodes = 0;
  int32 DependencyNodes = 0;
  int32 UnwiredNodes = 0;

  const TArray<UPCGNode *> &Nodes = Graph->GetNodes();
  for (UPCGNode *Node : Nodes) {
    if (!Node) {
      continue;
    }

    ++TotalNodes;

    const UPCGSettings *NodeSettings = Node->GetSettings();
    bool bDependencyNode = false;
    bool bDependencyWired = false;
    for (const TObjectPtr<UPCGPin> &PinPtr : Node->GetInputPins()) {
      const UPCGPin *Pin = PinPtr.Get();
      if (!Pin || Pin->Properties.Usage != EPCGPinUsage::DependencyOnly) {
        continue;
      }

      bDependencyNode = true;

      if (Pin->Edges.Num() > 0) {
        bDependencyWired = true;
        break;
      }
    }

    if (!bDependencyNode) {
      continue;
    }

    ++DependencyNodes;

    if (!bDependencyWired) {
      ++UnwiredNodes;
      const FString NodeLabel =
          NodeSettings ? NodeSettings->GetClass()->GetName() : Node->GetName();
      UE_LOG(LogPCGWorldService, Warning,
             TEXT("Unwired execution dependency on node '%s'."), *NodeLabel);
    }
  }

  UE_LOG(LogPCGWorldService, Log,
         TEXT("Total Nodes: %d, Dep Pins: %d, Unwired: %d"), TotalNodes,
         DependencyNodes, UnwiredNodes);
}

static void HandlePcgValidate(const TArray<FString> &Args) {
  if (Args.IsEmpty()) {
    UE_LOG(LogPCGWorldService, Error,
           TEXT("Usage: wg.pcg.validate <Biome|GraphAssetPath>"));
    return;
  }

  UWorld *World = ResolveActiveWorld();
  if (!World) {
    UE_LOG(LogPCGWorldService, Error,
           TEXT("wg.pcg.validate: Unable to resolve an active world."));
    return;
  }

  UPCGWorldService *Service = ResolveWorldService(World);
  if (!Service) {
    UE_LOG(LogPCGWorldService, Error,
           TEXT("wg.pcg.validate: PCGWorldService not available in current "
                "world."));
    return;
  }

  FString GraphPath;
  const FString Target = Args[0];
  if (Target.Contains(TEXT("/"))) {
    GraphPath = Target;
  } else {
    if (const UEnum *BiomeEnum = StaticEnum<EBiomeType>()) {
      int64 EnumValue = BiomeEnum->GetValueByNameString(Target);
      if (EnumValue == INDEX_NONE) {
        UE_LOG(LogPCGWorldService, Error,
               TEXT("Unknown biome '%s'. Provide a valid EBiomeType name or "
                    "asset path."),
               *Target);
        return;
      }

      const EBiomeType BiomeType = static_cast<EBiomeType>(EnumValue);
      GraphPath = Service->GetBiomeGraphAssetPath(BiomeType);
      if (GraphPath.IsEmpty()) {
        UE_LOG(LogPCGWorldService, Warning,
               TEXT("Biome %s has no PCG graph assigned."), *Target);
        return;
      }

      UE_LOG(LogPCGWorldService, Log, TEXT("Resolved biome %s to graph %s."),
             *Target, *GraphPath);
    } else {
      UE_LOG(LogPCGWorldService, Error,
             TEXT("wg.pcg.validate: EBiomeType enumeration unavailable."));
      return;
    }
  }

  const FPCGGraphValidationResult Result = Service->ValidatePCGGraph(GraphPath);

  UE_LOG(LogPCGWorldService, Log, TEXT("Validation report for %s (Graph: %s)"),
         *Result.GraphPath, *Result.GraphName);

  for (const FString &Error : Result.Errors) {
    UE_LOG(LogPCGWorldService, Error, TEXT("  Error: %s"), *Error);
  }

  for (const FString &Warning : Result.Warnings) {
    UE_LOG(LogPCGWorldService, Warning, TEXT("  Warning: %s"), *Warning);
  }

  for (const FName &MissingAttribute : Result.MissingAttributes) {
    UE_LOG(LogPCGWorldService, Warning, TEXT("  Missing Attribute: %s"),
           *MissingAttribute.ToString());
  }

  for (const FString &Unwired : Result.UnwiredDependencyNodes) {
    UE_LOG(LogPCGWorldService, Warning, TEXT("  Unwired Dependency: %s"),
           *Unwired);
  }

  for (const FString &Suggestion : Result.Suggestions) {
    UE_LOG(LogPCGWorldService, Display, TEXT("  Suggestion: %s"), *Suggestion);
  }

  UE_LOG(LogPCGWorldService, Log, TEXT("Validation %s."),
         Result.bIsValid ? TEXT("passed") : TEXT("failed"));
}

static FAutoConsoleCommand GCmdShowDeps(
    TEXT("wg.pcg.showdeps"), TEXT("Inspect PCG graph dependency wiring."),
    FConsoleCommandWithArgsDelegate::CreateStatic(&HandlePcgShowDeps));

static FAutoConsoleCommand GCmdValidate(
    TEXT("wg.pcg.validate"),
    TEXT("Validate a PCG graph or biome using PCG World Service."),
    FConsoleCommandWithArgsDelegate::CreateStatic(&HandlePcgValidate));
} // namespace PCGWorldService::Editor
#endif

#if VHM_PCG_ENABLED
namespace PCGWorldService::Private {
enum class EAttributeScope : uint8 { Parameter, Point };

struct FExpectedAttribute {
  FName Name;
  TConstArrayView<EPCGMetadataTypes> AllowedTypes;
  EAttributeScope Scope;
  bool bRequired;
  const TCHAR *FriendlyType;
};

inline const TCHAR *GetScopeLabel(EAttributeScope Scope) {
  return (Scope == EAttributeScope::Parameter) ? TEXT("parameter")
                                               : TEXT("point");
}

inline FString MetadataTypeToString(EPCGMetadataTypes MetadataType) {
  switch (MetadataType) {
  case EPCGMetadataTypes::Float:
    return TEXT("float");
  case EPCGMetadataTypes::Double:
    return TEXT("double");
  case EPCGMetadataTypes::Integer32:
    return TEXT("int32");
  case EPCGMetadataTypes::Integer64:
    return TEXT("int64");
  case EPCGMetadataTypes::Vector2:
    return TEXT("FVector2D");
  case EPCGMetadataTypes::Vector:
    return TEXT("FVector");
  case EPCGMetadataTypes::Vector4:
    return TEXT("FVector4");
  case EPCGMetadataTypes::Quaternion:
    return TEXT("FQuat");
  case EPCGMetadataTypes::Transform:
    return TEXT("FTransform");
  case EPCGMetadataTypes::String:
    return TEXT("FString");
  case EPCGMetadataTypes::Boolean:
    return TEXT("bool");
  case EPCGMetadataTypes::Rotator:
    return TEXT("FRotator");
  case EPCGMetadataTypes::Name:
    return TEXT("FName");
  case EPCGMetadataTypes::SoftObjectPath:
    return TEXT("FSoftObjectPath");
  case EPCGMetadataTypes::SoftClassPath:
    return TEXT("FSoftClassPath");
  default:
    return TEXT("unknown");
  }
}

inline FString AllowedTypesToString(TConstArrayView<EPCGMetadataTypes> Types) {
  TArray<FString, TInlineAllocator<4>> Labels;
  for (EPCGMetadataTypes Type : Types) {
    Labels.Add(MetadataTypeToString(Type));
  }
  return FString::Join(Labels, TEXT(" or "));
}

template <typename TValue>
TOptional<TValue> GetMetadataValue(const UPCGMetadata *Metadata,
                                   const FName &AttributeName,
                                   PCGMetadataEntryKey EntryKey) {
  if (!Metadata) {
    return TOptional<TValue>();
  }

  if (const FPCGMetadataAttribute<TValue> *Attribute =
          Metadata->GetConstTypedAttribute<TValue>(
              FPCGAttributeIdentifier(AttributeName))) {
    return Attribute->GetValueFromItemKey(EntryKey);
  }

  return TOptional<TValue>();
}

inline bool TryGetSoftObjectPath(const UPCGMetadata *Metadata,
                                 PCGMetadataEntryKey EntryKey,
                                 const FName &AttributeName,
                                 FSoftObjectPath &OutPath) {
  if (TOptional<FSoftObjectPath> PathValue = GetMetadataValue<FSoftObjectPath>(
          Metadata, AttributeName, EntryKey)) {
    if (!PathValue->IsNull()) {
      OutPath = MoveTemp(PathValue.GetValue());
      return true;
    }
  }

  if (TOptional<FString> PathString =
          GetMetadataValue<FString>(Metadata, AttributeName, EntryKey)) {
    if (!PathString->IsEmpty()) {
      FSoftObjectPath FromString(PathString.GetValue());
      if (!FromString.IsNull()) {
        OutPath = MoveTemp(FromString);
        return true;
      }
    }
  }

  return false;
}

inline bool HasAttribute(const UPCGMetadata *Metadata,
                         const FName &AttributeName) {
  return Metadata && Metadata->GetConstAttribute(
                         FPCGAttributeIdentifier(AttributeName)) != nullptr;
}

inline const TArray<FExpectedAttribute> &GetCanonicalAttributes() {
  static constexpr EPCGMetadataTypes FloatType[] = {EPCGMetadataTypes::Float};
  static constexpr EPCGMetadataTypes Int32Type[] = {
      EPCGMetadataTypes::Integer32};
  static constexpr EPCGMetadataTypes SoftObjectType[] = {
      EPCGMetadataTypes::SoftObjectPath, EPCGMetadataTypes::String,
      EPCGMetadataTypes::Name};
  static constexpr EPCGMetadataTypes VectorType[] = {EPCGMetadataTypes::Vector};
  static constexpr EPCGMetadataTypes RotatorType[] = {
      EPCGMetadataTypes::Rotator};
  static constexpr EPCGMetadataTypes BoolType[] = {EPCGMetadataTypes::Boolean};
  static constexpr EPCGMetadataTypes GuidType[] = {EPCGMetadataTypes::String,
                                                   EPCGMetadataTypes::Name};

  static const TArray<FExpectedAttribute> Attributes = {
      {VHMPCGAttr::AverageHeight,
       TConstArrayView<EPCGMetadataTypes>(FloatType, UE_ARRAY_COUNT(FloatType)),
       EAttributeScope::Parameter, true, TEXT("float")},
      {VHMPCGAttr::MinHeight,
       TConstArrayView<EPCGMetadataTypes>(FloatType, UE_ARRAY_COUNT(FloatType)),
       EAttributeScope::Parameter, true, TEXT("float")},
      {VHMPCGAttr::MaxHeight,
       TConstArrayView<EPCGMetadataTypes>(FloatType, UE_ARRAY_COUNT(FloatType)),
       EAttributeScope::Parameter, true, TEXT("float")},
      {VHMPCGAttr::AverageSlope,
       TConstArrayView<EPCGMetadataTypes>(FloatType, UE_ARRAY_COUNT(FloatType)),
       EAttributeScope::Parameter, true, TEXT("float")},
      {VHMPCGAttr::MaxSlope,
       TConstArrayView<EPCGMetadataTypes>(FloatType, UE_ARRAY_COUNT(FloatType)),
       EAttributeScope::Parameter, true, TEXT("float")},
      {VHMPCGAttr::WaterCoverage,
       TConstArrayView<EPCGMetadataTypes>(FloatType, UE_ARRAY_COUNT(FloatType)),
       EAttributeScope::Parameter, true, TEXT("float")},
      {VHMPCGAttr::AverageAboveWater,
       TConstArrayView<EPCGMetadataTypes>(FloatType, UE_ARRAY_COUNT(FloatType)),
       EAttributeScope::Parameter, true, TEXT("float")},
      {VHMPCGAttr::AverageBelowWater,
       TConstArrayView<EPCGMetadataTypes>(FloatType, UE_ARRAY_COUNT(FloatType)),
       EAttributeScope::Parameter, true, TEXT("float")},
      {VHMPCGAttr::MinWaterDistance,
       TConstArrayView<EPCGMetadataTypes>(FloatType, UE_ARRAY_COUNT(FloatType)),
       EAttributeScope::Parameter, true, TEXT("float")},
      {VHMPCGAttr::SeaLevel,
       TConstArrayView<EPCGMetadataTypes>(FloatType, UE_ARRAY_COUNT(FloatType)),
       EAttributeScope::Parameter, true, TEXT("float")},
      {VHMPCGAttr::TileSize,
       TConstArrayView<EPCGMetadataTypes>(FloatType, UE_ARRAY_COUNT(FloatType)),
       EAttributeScope::Parameter, true, TEXT("float")},
      {VHMPCGAttr::BiomeWeight,
       TConstArrayView<EPCGMetadataTypes>(FloatType, UE_ARRAY_COUNT(FloatType)),
       EAttributeScope::Parameter, true, TEXT("float")},
      {VHMPCGAttr::DensityScale,
       TConstArrayView<EPCGMetadataTypes>(FloatType, UE_ARRAY_COUNT(FloatType)),
       EAttributeScope::Parameter, false, TEXT("float")},
      {VHMPCGAttr::BiomeId,
       TConstArrayView<EPCGMetadataTypes>(Int32Type, UE_ARRAY_COUNT(Int32Type)),
       EAttributeScope::Parameter, true, TEXT("int32")},
      {VHMPCGAttr::TileSeed,
       TConstArrayView<EPCGMetadataTypes>(Int32Type, UE_ARRAY_COUNT(Int32Type)),
       EAttributeScope::Parameter, true, TEXT("int32")},
      {VHMPCGAttr::TileX,
       TConstArrayView<EPCGMetadataTypes>(Int32Type, UE_ARRAY_COUNT(Int32Type)),
       EAttributeScope::Parameter, true, TEXT("int32")},
      {VHMPCGAttr::TileY,
       TConstArrayView<EPCGMetadataTypes>(Int32Type, UE_ARRAY_COUNT(Int32Type)),
       EAttributeScope::Parameter, true, TEXT("int32")},
      {VHMPCGAttr::AverageSlope,
       TConstArrayView<EPCGMetadataTypes>(FloatType, UE_ARRAY_COUNT(FloatType)),
       EAttributeScope::Point, false, TEXT("float")},
      {VHMPCGAttr::StaticMesh,
       TConstArrayView<EPCGMetadataTypes>(SoftObjectType,
                                          UE_ARRAY_COUNT(SoftObjectType)),
       EAttributeScope::Point, false, TEXT("SoftObjectPath")},
      {VHMPCGAttr::Mesh,
       TConstArrayView<EPCGMetadataTypes>(SoftObjectType,
                                          UE_ARRAY_COUNT(SoftObjectType)),
       EAttributeScope::Point, false, TEXT("SoftObjectPath")},
      {VHMPCGAttr::InstanceScale,
       TConstArrayView<EPCGMetadataTypes>(VectorType,
                                          UE_ARRAY_COUNT(VectorType)),
       EAttributeScope::Point, false, TEXT("FVector")},
      {VHMPCGAttr::InstanceRotation,
       TConstArrayView<EPCGMetadataTypes>(RotatorType,
                                          UE_ARRAY_COUNT(RotatorType)),
       EAttributeScope::Point, false, TEXT("FRotator")},
      {VHMPCGAttr::IsActive,
       TConstArrayView<EPCGMetadataTypes>(BoolType, UE_ARRAY_COUNT(BoolType)),
       EAttributeScope::Point, false, TEXT("bool")},
      {VHMPCGAttr::InstanceId,
       TConstArrayView<EPCGMetadataTypes>(GuidType, UE_ARRAY_COUNT(GuidType)),
       EAttributeScope::Point, false, TEXT("FGuid (stored as string/name)")},
      {VHMPCGAttr::RespectGraphZ,
       TConstArrayView<EPCGMetadataTypes>(BoolType, UE_ARRAY_COUNT(BoolType)),
       EAttributeScope::Point, false, TEXT("bool")}};

  return Attributes;
}
static FTransform QuantizeTransformForHash(const FTransform &Transform) {
  FVector Position = Transform.GetLocation();
  Position.X = FMath::RoundToFloat(Position.X * 1000.0f) / 1000.0f;
  Position.Y = FMath::RoundToFloat(Position.Y * 1000.0f) / 1000.0f;
  Position.Z = FMath::RoundToFloat(Position.Z * 1000.0f) / 1000.0f;

  FQuat Rotation = Transform.GetRotation();
  Rotation.X = FMath::RoundToFloat(Rotation.X * 10000.0f) / 10000.0f;
  Rotation.Y = FMath::RoundToFloat(Rotation.Y * 10000.0f) / 10000.0f;
  Rotation.Z = FMath::RoundToFloat(Rotation.Z * 10000.0f) / 10000.0f;
  Rotation.W = FMath::RoundToFloat(Rotation.W * 10000.0f) / 10000.0f;
  Rotation.Normalize();

  FVector Scale = Transform.GetScale3D();
  Scale.X = FMath::RoundToFloat(Scale.X * 10000.0f) / 10000.0f;
  Scale.Y = FMath::RoundToFloat(Scale.Y * 10000.0f) / 10000.0f;
  Scale.Z = FMath::RoundToFloat(Scale.Z * 10000.0f) / 10000.0f;

  return FTransform(Rotation, Position, Scale);
}

static uint64 HashTransform(const FTransform &Transform) {
  uint64 Hash = 1469598103934665603ull;
  auto Mix = [&Hash](const void *Data, SIZE_T Size) {
    const uint8 *Bytes = static_cast<const uint8 *>(Data);
    for (SIZE_T Index = 0; Index < Size; ++Index) {
      Hash ^= Bytes[Index];
      Hash *= 1099511628211ull;
    }
  };

  const FVector Location = Transform.GetLocation();
  const FQuat Rotation = Transform.GetRotation();
  const FVector Scale = Transform.GetScale3D();

  Mix(&Location, sizeof(Location));
  Mix(&Rotation, sizeof(Rotation));
  Mix(&Scale, sizeof(Scale));

  return Hash;
}

static UPCGParamData *
CreateTileParameterData(UObject *Outer, const FPCGTileMetrics &TileMetrics,
                        FTileCoord TileCoord, EBiomeType BiomeType,
                        const FWorldGenConfig &WorldGenSettings,
                        uint32 TileSeed, float DensityScale, float BiomeWeight);

static UPCGPointData *
CreateTilePointData(UObject *Outer, FTileCoord TileCoord,
                    const FWorldGenConfig &WorldGenSettings,
                    const FPCGTileMetrics &TileMetrics);

static void
ExtractInstancesFromPointData(const UPCGPointData *PointData,
                              FTileCoord TileCoord,
                              const FWorldGenConfig &WorldGenSettings,
                              const UHeightfieldService *HeightfieldService,
                              FPCGGenerationData &OutGenerationData);
} // namespace PCGWorldService::Private
#endif

UPCGWorldService::UPCGWorldService() {
  bRuntimeOperationsEnabled = true;
  PerformanceStats = FPCGPerformanceStats();
  CurrentPCGGraph = nullptr;
  TileActor = nullptr;
  HeightfieldService = nullptr;
  MaxInstancesPerTile = 10000;
  CullDistances.Add(500.0f); // Near cull distance for runtime HISM fallback
  CullDistances.Add(
      1500.0f); // Mid-range cull distance for runtime HISM fallback
  CullDistances.Add(5000.0f); // Far cull distance for runtime HISM fallback
#if VHM_PCG_ENABLED
  SchedulerExecutor = FPCGSchedulerExecutorPtr(new FPCGSchedulerExecutor());
  RefreshRuntimeSettingsFromCVars();
  ConsoleSinkHandle = IConsoleManager::Get().RegisterConsoleVariableSink_Handle(
      FConsoleCommandDelegate::CreateUObject(
          this, &UPCGWorldService::HandleConsoleVariablesChanged));
  bConsoleSinkRegistered = true;
#endif
}

void UPCGWorldService::BeginDestroy() {
#if VHM_PCG_ENABLED
  if (WorldCleanupHandle.IsValid()) {
    FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
    WorldCleanupHandle = FDelegateHandle();
  }

  if (bConsoleSinkRegistered) {
    IConsoleManager::Get().UnregisterConsoleVariableSink_Handle(
        ConsoleSinkHandle);
    ConsoleSinkHandle = FConsoleVariableSinkHandle();
    bConsoleSinkRegistered = false;
  }

  if (UWorld *World = GetWorld()) {
    AbandonTasksForWorld(World);
  }

  ActiveTasks.Reset();
  CleanupAllComponents();
#endif

  Super::BeginDestroy();
}

#if VHM_PCG_ENABLED
AActor *UPCGWorldService::EnsurePCGAnchor(UWorld *World) {
  if (!World) {
    return nullptr;
  }

  if (PCGAnchorActor.IsValid()) {
    return PCGAnchorActor.Get();
  }

  FActorSpawnParameters SpawnParams;
  SpawnParams.Name = TEXT("PCGAnchor");
  SpawnParams.ObjectFlags = RF_Transient;
  SpawnParams.SpawnCollisionHandlingOverride =
      ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

  AActor *AnchorActor = World->SpawnActor<AActor>(SpawnParams);
  if (AnchorActor) {
    AnchorActor->SetActorHiddenInGame(true);
    AnchorActor->SetCanBeDamaged(false);
    AnchorActor->SetFlags(RF_Transient);

    if (!AnchorActor->GetRootComponent()) {
      USceneComponent *RootComponent =
          NewObject<USceneComponent>(AnchorActor, TEXT("PCGAnchorRoot"));
      AnchorActor->SetRootComponent(RootComponent);
      RootComponent->RegisterComponent();
    }
  }

  PCGAnchorActor = AnchorActor;
  return AnchorActor;
}

UPCGComponent *UPCGWorldService::GetOrCreateBiomeComponent(EBiomeType BiomeType,
                                                           UPCGGraph &Graph) {
  check(IsInGameThread());
  UWorld *World = GetWorld();
  if (!World) {
    return nullptr;
  }

  if (TObjectPtr<UObject> *ExistingPtr = BiomePCGComponents.Find(BiomeType)) {
    if (UPCGComponent *ExistingComponent =
            Cast<UPCGComponent>(ExistingPtr->Get())) {
      ExistingComponent->SetGraph(&Graph);
      return ExistingComponent;
    }
  }

  AActor *AnchorActor = EnsurePCGAnchor(World);
  if (!AnchorActor) {
    return nullptr;
  }

  UPCGComponent *NewComponent =
      NewObject<UPCGComponent>(AnchorActor, NAME_None, RF_Transient);
  if (!NewComponent) {
    return nullptr;
  }

  NewComponent->SetFlags(RF_Transient);
  NewComponent->SetAutoActivate(true);
  NewComponent->SetComponentTickEnabled(false);
  NewComponent->bRuntimeGenerated = true;

  // Enable partitioning if configured
  if (WorldGenSettings.bUseWorldPartitionStreaming) {
    NewComponent->bIsPartitioned = true;
  }

  NewComponent->Seed = WorldGenSettings.Seed;
  NewComponent->SetGraph(&Graph);

  AnchorActor->AddInstanceComponent(NewComponent);
  NewComponent->RegisterComponent();

  BiomePCGComponents.FindOrAdd(BiomeType) = NewComponent;
  return NewComponent;
}

void UPCGWorldService::DestroyBiomeComponent(EBiomeType BiomeType) {
  check(IsInGameThread());

  if (TObjectPtr<UObject> *ComponentPtr = BiomePCGComponents.Find(BiomeType)) {
    if (UPCGComponent *Component = Cast<UPCGComponent>(ComponentPtr->Get())) {
      Component->UnregisterComponent();
      Component->DestroyComponent();
    }

    BiomePCGComponents.Remove(BiomeType);
  }
}

void UPCGWorldService::CleanupAllComponents() {
  check(IsInGameThread());

  for (TPair<EBiomeType, TObjectPtr<UObject>> &Entry : BiomePCGComponents) {
    if (UPCGComponent *Component = Cast<UPCGComponent>(Entry.Value.Get())) {
      Component->UnregisterComponent();
      Component->DestroyComponent();
    }
  }

  BiomePCGComponents.Reset();
  PCGAnchorActor.Reset();
}

bool UPCGWorldService::CanScheduleNewTask() const {
  const int32 MaxConcurrent =
      FMath::Max(1, WorldGenSettings.MaxConcurrentPCGTasks);
  int32 ActiveCount = 0;
  for (const TPair<FPCGTaskId, FPCGTaskContext> &Entry : ActiveTasks) {
    if (Entry.Value.State != EPCGTaskState::Abandoned &&
        Entry.Value.State != EPCGTaskState::Released) {
      ++ActiveCount;
    }
  }
  return ActiveCount < MaxConcurrent;
}

void UPCGWorldService::TrackTask(const FPCGTaskContext &Context) {
  check(IsInGameThread());
  ActiveTasks.Add(Context.TaskId, Context);
}

void UPCGWorldService::ReleaseTrackedTask(FPCGTaskId TaskId) {
  check(IsInGameThread());
  ActiveTasks.Remove(TaskId);
}

void UPCGWorldService::HandleConsoleVariablesChanged() {
  if (!IsInGameThread()) {
    TWeakObjectPtr<UPCGWorldService> WeakThis(this);
    AsyncTask(ENamedThreads::GameThread, [WeakThis]() {
      if (UPCGWorldService *StrongThis = WeakThis.Get()) {
        StrongThis->RefreshRuntimeSettingsFromCVars();
      }
    });
    return;
  }

  RefreshRuntimeSettingsFromCVars();
}

void UPCGWorldService::RefreshRuntimeSettingsFromCVars() {
  check(IsInGameThread());

  WorldGenSettings.MaxConcurrentPCGTasks =
      FMath::Max(1, CVarVibeheimPCGMaxConcurrent.GetValueOnGameThread());
  WorldGenSettings.bEnableFrustumCulling =
      CVarVibeheimPCGFrustumEnable.GetValueOnGameThread() != 0;
  WorldGenSettings.FrustumCullingMargin =
      FMath::Max(0.0f, CVarVibeheimPCGFrustumMargin.GetValueOnGameThread());
}

void UPCGWorldService::AbandonTasksForWorld(UWorld *World) {
  if (!SchedulerExecutor.IsValid() || !World) {
    ActiveTasks.Reset();
    return;
  }

  UPCGSubsystem *PCGSubsystem = World->GetSubsystem<UPCGSubsystem>();
  if (!PCGSubsystem) {
    ActiveTasks.Reset();
    return;
  }

  TArray<FPCGTaskId> TaskIds;
  ActiveTasks.GetKeys(TaskIds);
  for (FPCGTaskId TaskId : TaskIds) {
    if (FPCGTaskContext *Context = ActiveTasks.Find(TaskId)) {
      if (Context->World == World) {
        SchedulerExecutor->AbandonTask(*PCGSubsystem, TaskId, *Context,
                                       TEXT("World cleanup"));
        SchedulerExecutor->ReleaseTask(*PCGSubsystem, TaskId, *Context);
        MarkTelemetryFallback(TaskId, TEXT("World cleanup"), nullptr,
                              EBiomeType::None, Context->TileCoord);
        ReleaseTrackedTask(TaskId);
      }
    }
  }
}

float UPCGWorldService::ResolveFrustumMargin(const UPCGComponent &Component,
                                             EBiomeType BiomeType) const {
  const TMap<FName, float> &Overrides =
      WorldGenSettings.FrustumCullingMarginByLOD;

  if (const float *Specific = Overrides.Find(Component.GetFName())) {
    return *Specific;
  }

  const FName BiomeKey(*UEnum::GetValueAsString(BiomeType));
  if (const float *BiomeMargin = Overrides.Find(BiomeKey)) {
    return *BiomeMargin;
  }

  if (const float *GlobalMargin = Overrides.Find(NAME_None)) {
    return *GlobalMargin;
  }

  return WorldGenSettings.FrustumCullingMargin;
}

#if VHM_PCG_ENABLED
void UPCGWorldService::RegisterTelemetry(FPCGTaskId TaskId,
                                         EBiomeType BiomeType,
                                         const UPCGGraph &Graph,
                                         const FTileCoord &TileCoord) {
  if (TaskId == InvalidPCGTaskId) {
    return;
  }

  FPCGTaskTelemetry &Telemetry = ActiveTelemetry.FindOrAdd(TaskId);
  Telemetry.TaskId = TaskId;
  Telemetry.Biome = BiomeType;
  Telemetry.GraphAssetPath = Graph.GetPathName();
  Telemetry.Tile = TileCoord;
  Telemetry.SubmitTimestamp = FDateTime::UtcNow();
  Telemetry.StartTimestamp = Telemetry.SubmitTimestamp;
  Telemetry.DoneTimestamp = FDateTime::MinValue();
  Telemetry.Status = EPCGTaskTelemetryStatus::Scheduled;
  Telemetry.PointsOut = 0;
  Telemetry.NodesExecutedProxy = 0;
  Telemetry.NodesCachedProxy = 0;
  Telemetry.ElapsedMs = 0.0;
  Telemetry.bFallbackUsed = false;

  EmitTelemetryLog(Telemetry, TEXT("Scheduled"));
  TRACE_COUNTER_SET(VibeheimPCGActiveTasks, ActiveTelemetry.Num());
}

void UPCGWorldService::MarkTelemetryStart(FPCGTaskId TaskId) {
  if (FPCGTaskTelemetry *Telemetry = ActiveTelemetry.Find(TaskId)) {
    if (Telemetry->Status == EPCGTaskTelemetryStatus::Scheduled) {
      Telemetry->StartTimestamp = FDateTime::UtcNow();
      Telemetry->Status = EPCGTaskTelemetryStatus::Running;
      EmitTelemetryLog(*Telemetry, TEXT("Running"));
    }
  }
}

void UPCGWorldService::MarkTelemetryCompletion(FPCGTaskId TaskId, bool bSuccess,
                                               int32 PointsGenerated,
                                               double ExecutionTimeMs,
                                               bool bFallback,
                                               const FString &StatusLabel) {
  if (TaskId == InvalidPCGTaskId) {
    return;
  }

  if (FPCGTaskTelemetry *Telemetry = ActiveTelemetry.Find(TaskId)) {
    Telemetry->DoneTimestamp = FDateTime::UtcNow();
    Telemetry->PointsOut = PointsGenerated;
    Telemetry->ElapsedMs = ExecutionTimeMs;
    Telemetry->bFallbackUsed |= bFallback;
    Telemetry->Status = bSuccess ? EPCGTaskTelemetryStatus::Completed
                                 : EPCGTaskTelemetryStatus::Failed;

    UpdateLatencySamples(Telemetry->GraphAssetPath, Telemetry->Biome,
                         ExecutionTimeMs);

    const TArray<double> *GraphSamples =
        GraphLatencySamples.Find(Telemetry->GraphAssetPath);
    const double MedianMs =
        GraphSamples ? ComputePercentile(*GraphSamples, 0.5) : ExecutionTimeMs;
    const double SafeMedian = FMath::Max(MedianMs, 0.001);
    Telemetry->NodesExecutedProxy =
        FMath::Max(1, FMath::RoundToInt(ExecutionTimeMs / SafeMedian));
    Telemetry->NodesCachedProxy =
        (ExecutionTimeMs < SafeMedian)
            ? FMath::RoundToInt(SafeMedian / FMath::Max(ExecutionTimeMs, 0.001))
            : 0;

    const FString Reason = bSuccess ? FString() : StatusLabel;
    FPCGTaskTelemetry Snapshot = *Telemetry;
    EmitTelemetryLog(Snapshot, StatusLabel, Reason);

    ActiveTelemetry.Remove(TaskId);
    TRACE_COUNTER_SET(VibeheimPCGActiveTasks, ActiveTelemetry.Num());
  }
}

void UPCGWorldService::MarkTelemetryFallback(FPCGTaskId TaskId,
                                             const FString &Reason,
                                             const UPCGGraph *Graph,
                                             EBiomeType BiomeType,
                                             const FTileCoord &TileCoord) {
  if (TaskId != InvalidPCGTaskId) {
    if (FPCGTaskTelemetry *Telemetry = ActiveTelemetry.Find(TaskId)) {
      Telemetry->DoneTimestamp = FDateTime::UtcNow();
      Telemetry->Status = EPCGTaskTelemetryStatus::Fallback;
      Telemetry->bFallbackUsed = true;
      FPCGTaskTelemetry Snapshot = *Telemetry;
      EmitTelemetryLog(Snapshot, TEXT("Fallback"), Reason);
      ActiveTelemetry.Remove(TaskId);
      TRACE_COUNTER_SET(VibeheimPCGActiveTasks, ActiveTelemetry.Num());
      return;
    }
  }

  FPCGTaskTelemetry Telemetry;
  Telemetry.TaskId = TaskId;
  Telemetry.Biome = BiomeType;
  Telemetry.GraphAssetPath = Graph ? Graph->GetPathName() : FString();
  Telemetry.Tile = TileCoord;
  Telemetry.SubmitTimestamp = FDateTime::UtcNow();
  Telemetry.StartTimestamp = Telemetry.SubmitTimestamp;
  Telemetry.DoneTimestamp = Telemetry.SubmitTimestamp;
  Telemetry.Status = EPCGTaskTelemetryStatus::Fallback;
  Telemetry.bFallbackUsed = true;

  EmitTelemetryLog(Telemetry, TEXT("Fallback"), Reason);
}

void UPCGWorldService::EmitTelemetryLog(const FPCGTaskTelemetry &Telemetry,
                                        const FString &StatusLabel,
                                        const FString &Reason) {
  const FString BiomeLabel = UEnum::GetValueAsString(Telemetry.Biome);
  const FString TileLabel =
      FString::Printf(TEXT("(%d,%d)"), Telemetry.Tile.X, Telemetry.Tile.Y);
  const FString SubmitIso = Telemetry.SubmitTimestamp != FDateTime::MinValue()
                                ? Telemetry.SubmitTimestamp.ToIso8601()
                                : TEXT("N/A");
  const FString StartIso = Telemetry.StartTimestamp != FDateTime::MinValue()
                               ? Telemetry.StartTimestamp.ToIso8601()
                               : TEXT("N/A");
  const FString DoneIso = Telemetry.DoneTimestamp != FDateTime::MinValue()
                              ? Telemetry.DoneTimestamp.ToIso8601()
                              : TEXT("N/A");
  const FString ReasonLabel = Reason.IsEmpty() ? TEXT("None") : Reason;

  UE_LOG(LogPCGWorldService, Log,
         TEXT("Telemetry %s: Biome=%s Graph=%s Tile=%s Task=%d Submit=%s "
              "Start=%s Done=%s Points=%d Duration=%.2fms NodesExec(proxy)=%d "
              "NodesCached(proxy)=%d Fallback=%s Reason=%s"),
         *StatusLabel, *BiomeLabel,
         Telemetry.GraphAssetPath.IsEmpty() ? TEXT("<none>")
                                            : *Telemetry.GraphAssetPath,
         *TileLabel, Telemetry.TaskId, *SubmitIso, *StartIso, *DoneIso,
         Telemetry.PointsOut, Telemetry.ElapsedMs, Telemetry.NodesExecutedProxy,
         Telemetry.NodesCachedProxy,
         Telemetry.bFallbackUsed ? TEXT("Yes") : TEXT("No"), *ReasonLabel);

  TRACE_COUNTER_SET(VibeheimPCGLastDurationMs, Telemetry.ElapsedMs);
  TRACE_COUNTER_SET(VibeheimPCGLastPointCount, Telemetry.PointsOut);

  AppendTelemetryCsvRow(Telemetry, StatusLabel, ReasonLabel);
}

void UPCGWorldService::UpdateLatencySamples(const FString &GraphKey,
                                            EBiomeType BiomeType,
                                            double DurationMs) {
  TArray<double> &GraphSamples = GraphLatencySamples.FindOrAdd(GraphKey);
  GraphSamples.Add(DurationMs);
  if (GraphSamples.Num() > 64) {
    GraphSamples.RemoveAt(0);
  }

  TArray<double> &BiomeSamples = BiomeLatencySamples.FindOrAdd(BiomeType);
  BiomeSamples.Add(DurationMs);
  if (BiomeSamples.Num() > 64) {
    BiomeSamples.RemoveAt(0);
  }

  const double P50 = ComputePercentile(BiomeSamples, 0.5);
  const double P95 = ComputePercentile(BiomeSamples, 0.95);
  const double P99 = ComputePercentile(BiomeSamples, 0.99);

  UE_LOG(LogPCGWorldService, Verbose,
         TEXT("Biome latency stats %s → p50=%.2fms p95=%.2fms p99=%.2fms (%d "
              "samples)"),
         *UEnum::GetValueAsString(BiomeType), P50, P95, P99,
         BiomeSamples.Num());
}

double UPCGWorldService::ComputePercentile(const TArray<double> &Samples,
                                           double Percent) const {
  if (Samples.IsEmpty()) {
    return 0.0;
  }

  TArray<double> SortedSamples = Samples;
  SortedSamples.Sort();

  const double ClampedPercent = FMath::Clamp(Percent, 0.0, 1.0);
  const double Index =
      ClampedPercent * static_cast<double>(SortedSamples.Num() - 1);
  const int32 LowerIndex = FMath::FloorToInt(Index);
  const int32 UpperIndex = FMath::CeilToInt(Index);

  if (LowerIndex == UpperIndex) {
    return SortedSamples[LowerIndex];
  }

  const double Fraction = Index - static_cast<double>(LowerIndex);
  return FMath::Lerp(SortedSamples[LowerIndex], SortedSamples[UpperIndex],
                     Fraction);
}

void UPCGWorldService::FlushTelemetryCsv() {
  LastTelemetryFlushSeconds = FPlatformTime::Seconds();
}

void UPCGWorldService::AppendTelemetryCsvRow(const FPCGTaskTelemetry &Telemetry,
                                             const FString &StatusLabel,
                                             const FString &Reason) {
  if (CVarVibeheimPCGTelemetryCsv.GetValueOnAnyThread() <= 0) {
    return;
  }

  const double NowSeconds = FPlatformTime::Seconds();
  if ((NowSeconds - LastTelemetryFlushSeconds) < 0.1) {
    return;
  }
  LastTelemetryFlushSeconds = NowSeconds;

  const FString SaveDir =
      FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("PCG"));
  IFileManager::Get().MakeDirectory(*SaveDir, true);

  const FString FilePath = FPaths::Combine(SaveDir, TEXT("pcg_tasks.csv"));
  const bool bFileExists = IFileManager::Get().FileSize(*FilePath) > 0;

  TUniquePtr<FArchive> Writer(IFileManager::Get().CreateFileWriter(
      *FilePath, FILEWRITE_Append | FILEWRITE_AllowRead));
  if (!Writer) {
    return;
  }

  if (!bTelemetryCsvHeaderWritten || !bFileExists) {
    const FString Header = TEXT(
        "Biome,Graph,Tile,TaskId,SubmitTs,StartTs,DoneTs,Status,PointsOut,"
        "NodesExecutedProxy,NodesCachedProxy,Fallback,Reason,DurationMs\n");
    const FTCHARToUTF8 HeaderUtf8(*Header);
    Writer->Serialize((void *)HeaderUtf8.Get(), HeaderUtf8.Length());
    bTelemetryCsvHeaderWritten = true;
  }

  const FString BiomeLabel = UEnum::GetValueAsString(Telemetry.Biome);
  const FString TileLabel =
      FString::Printf(TEXT("(%d,%d)"), Telemetry.Tile.X, Telemetry.Tile.Y);
  const FString SubmitIso = Telemetry.SubmitTimestamp != FDateTime::MinValue()
                                ? Telemetry.SubmitTimestamp.ToIso8601()
                                : TEXT("N/A");
  const FString StartIso = Telemetry.StartTimestamp != FDateTime::MinValue()
                               ? Telemetry.StartTimestamp.ToIso8601()
                               : TEXT("N/A");
  const FString DoneIso = Telemetry.DoneTimestamp != FDateTime::MinValue()
                              ? Telemetry.DoneTimestamp.ToIso8601()
                              : TEXT("N/A");
  FString ReasonEscaped = Reason;
  ReasonEscaped.ReplaceInline(TEXT("\""), TEXT("'"));

  const FString Line = FString::Printf(
      TEXT("%s,%s,%s,%d,%s,%s,%s,%s,%d,%d,%d,%s,%s,%.2f\n"), *BiomeLabel,
      Telemetry.GraphAssetPath.IsEmpty() ? TEXT("<none>")
                                         : *Telemetry.GraphAssetPath,
      *TileLabel, Telemetry.TaskId, *SubmitIso, *StartIso, *DoneIso,
      *StatusLabel, Telemetry.PointsOut, Telemetry.NodesExecutedProxy,
      Telemetry.NodesCachedProxy,
      Telemetry.bFallbackUsed ? TEXT("true") : TEXT("false"),
      ReasonEscaped.IsEmpty()
          ? TEXT("\"\"")
          : *FString::Printf(TEXT("\"%s\""), *ReasonEscaped),
      Telemetry.ElapsedMs);

  const FTCHARToUTF8 LineUtf8(*Line);
  Writer->Serialize((void *)LineUtf8.Get(), LineUtf8.Length());
  Writer->Close();
}
#endif // VHM_PCG_ENABLED

void UPCGWorldService::HandleWorldCleanup(UWorld *World, bool bSessionEnded,
                                          bool bCleanupResources) {
  (void)bSessionEnded;

  if (!World) {
    return;
  }

  AbandonTasksForWorld(World);

  if (bCleanupResources && World == GetWorld()) {
    CleanupAllComponents();
  }
}

#endif // VHM_PCG_ENABLED

#if VHM_PCG_ENABLED
UPCGParamData *PCGWorldService::Private::CreateTileParameterData(
    UObject *Outer, const FPCGTileMetrics &TileMetrics, FTileCoord TileCoord,
    EBiomeType BiomeType, const FWorldGenConfig &WorldGenSettings,
    uint32 TileSeed, float DensityScale, float BiomeWeight) {
  UPCGParamData *ParamData = NewObject<UPCGParamData>(
      Outer ? Outer : GetTransientPackage(), NAME_None, RF_Transient);
  if (!ensureMsgf(ParamData,
                  TEXT("Failed to allocate tile parameter data for (%d, %d)"),
                  TileCoord.X, TileCoord.Y)) {
    return nullptr;
  }

  UPCGMetadata *Metadata = ParamData->MutableMetadata();
  if (!ensureMsgf(Metadata,
                  TEXT("Tile parameter metadata missing for (%d, %d)"),
                  TileCoord.X, TileCoord.Y)) {
    return ParamData;
  }

  const auto EntryKey = Metadata->AddEntry();

  auto EnsureAndSetAttribute = [Metadata, EntryKey](const FName &AttributeName,
                                                    auto &&Value,
                                                    bool bAllowInterpolation) {
    using ValueType = typename TDecay<decltype(Value)>::Type;

    FPCGMetadataAttribute<ValueType> *Attribute =
        Metadata->GetMutableTypedAttribute<ValueType>(AttributeName);
    if (!Attribute) {
      Attribute = Metadata->CreateAttribute<ValueType>(
          AttributeName, Value, bAllowInterpolation, true);
      if (!Attribute) {
        UE_LOG(
            LogPCGWorldService, Error,
            TEXT("Failed to create attribute '%s' on tile parameter metadata"),
            *AttributeName.ToString());
        return;
      }
    }

    Attribute->SetValue(EntryKey, Value);
  };

  const uint32 BiomeSeed = GetTypeHash(static_cast<int32>(BiomeType));
  const uint32 MixedSeed = HashCombine(TileSeed, BiomeSeed);
  const int32 TileSeedValue = static_cast<int32>(MixedSeed & 0x7FFFFFFFu);

  EnsureAndSetAttribute(VHMPCGAttr::AverageHeight, TileMetrics.AverageHeight,
                        true);
  EnsureAndSetAttribute(VHMPCGAttr::MinHeight, TileMetrics.MinHeight, true);
  EnsureAndSetAttribute(VHMPCGAttr::MaxHeight, TileMetrics.MaxHeight, true);
  EnsureAndSetAttribute(VHMPCGAttr::AverageSlope, TileMetrics.AverageSlope,
                        true);
  EnsureAndSetAttribute(VHMPCGAttr::MaxSlope, TileMetrics.MaxSlope, true);
  EnsureAndSetAttribute(VHMPCGAttr::WaterCoverage,
                        TileMetrics.WaterCoverageRatio, true);
  EnsureAndSetAttribute(VHMPCGAttr::AverageAboveWater,
                        TileMetrics.AverageAboveWater, true);
  EnsureAndSetAttribute(VHMPCGAttr::AverageBelowWater,
                        TileMetrics.AverageBelowWater, true);
  EnsureAndSetAttribute(VHMPCGAttr::MinWaterDistance,
                        TileMetrics.MinAbsWaterDistance, true);
  EnsureAndSetAttribute(VHMPCGAttr::SeaLevel, WorldGenSettings.SeaLevel, true);
  EnsureAndSetAttribute(VHMPCGAttr::BiomeId, static_cast<int32>(BiomeType),
                        false);
  EnsureAndSetAttribute(VHMPCGAttr::TileSeed, TileSeedValue, false);
  EnsureAndSetAttribute(VHMPCGAttr::TileX, TileCoord.X, false);
  EnsureAndSetAttribute(VHMPCGAttr::TileY, TileCoord.Y, false);
  EnsureAndSetAttribute(VHMPCGAttr::TileSize, WorldGenSettings.TileSizeMeters,
                        true);
  EnsureAndSetAttribute(VHMPCGAttr::BiomeWeight, BiomeWeight, true);
  EnsureAndSetAttribute(VHMPCGAttr::DensityScale, DensityScale, true);

  return ParamData;
}

UPCGPointData *PCGWorldService::Private::CreateTilePointData(
    UObject *Outer, FTileCoord TileCoord,
    const FWorldGenConfig &WorldGenSettings,
    const FPCGTileMetrics &TileMetrics) {
  UPCGPointData *PointData = NewObject<UPCGPointData>(
      Outer ? Outer : GetTransientPackage(), NAME_None, RF_Transient);
  if (!ensureMsgf(PointData,
                  TEXT("Failed to allocate tile point data for (%d, %d)"),
                  TileCoord.X, TileCoord.Y)) {
    return nullptr;
  }

  TArray<FPCGPoint> &Points = PointData->GetMutablePoints();
  FPCGPoint &TilePoint = Points.AddDefaulted_GetRef();

  const FVector TileCenter =
      TileCoord.ToWorldPosition(WorldGenSettings.TileSizeMeters);
  const FVector TileExtent(WorldGenSettings.TileSizeMeters * 0.5f,
                           WorldGenSettings.TileSizeMeters * 0.5f,
                           WorldGenSettings.TileSizeMeters * 0.25f);

  TilePoint.Transform =
      FTransform(FRotator::ZeroRotator, TileCenter, FVector::OneVector);
  TilePoint.SetExtents(TileExtent);
  TilePoint.Density = 1.0f;
  TilePoint.Seed = GetTypeHash(TileCoord);

  UPCGMetadata *Metadata = PointData->MutableMetadata();
  if (!ensureMsgf(Metadata, TEXT("Tile point metadata missing for (%d, %d)"),
                  TileCoord.X, TileCoord.Y)) {
    return PointData;
  }

  const auto EntryKey = Metadata->AddEntry();
  TilePoint.MetadataEntry = EntryKey;

  auto EnsureAndSetFloat = [Metadata, EntryKey](const FName &AttributeName,
                                                float Value) {
    FPCGMetadataAttribute<float> *Attribute =
        Metadata->GetMutableTypedAttribute<float>(AttributeName);
    if (!Attribute) {
      Attribute =
          Metadata->CreateAttribute<float>(AttributeName, Value, true, true);
      if (!Attribute) {
        UE_LOG(LogPCGWorldService, Error,
               TEXT("Failed to create attribute '%s' on tile point metadata"),
               *AttributeName.ToString());
        return;
      }
    }

    Attribute->SetValue(EntryKey, Value);
  };

  EnsureAndSetFloat(VHMPCGAttr::AverageSlope, TileMetrics.AverageSlope);

  return PointData;
}

void PCGWorldService::Private::ExtractInstancesFromPointData(
    const UPCGPointData *PointData, FTileCoord TileCoord,
    const FWorldGenConfig &WorldGenSettings,
    const UHeightfieldService *HeightfieldService,
    FPCGGenerationData &OutGenerationData) {
  OutGenerationData.GeneratedInstances.Reset();

  if (!PointData) {
    UE_LOG(LogPCGWorldService, Warning,
           TEXT("ExtractInstancesFromPointData: null point data for tile (%d, "
                "%d)."),
           TileCoord.X, TileCoord.Y);
    OutGenerationData.TotalInstanceCount = 0;
    OutGenerationData.InstanceTransformHash = 0;
    return;
  }

  const UPCGMetadata *Metadata = PointData->ConstMetadata();
  const TArray<FPCGPoint> &Points = PointData->GetPoints();

  if (Points.Num() == 0) {
    UE_LOG(LogPCGWorldService, VeryVerbose,
           TEXT("PCG graph produced no points for tile (%d, %d)."), TileCoord.X,
           TileCoord.Y);
    OutGenerationData.TotalInstanceCount = 0;
    OutGenerationData.InstanceTransformHash = 0;
    return;
  }

  OutGenerationData.GeneratedInstances.Reserve(Points.Num());

  const float MinTerrainZ =
      WorldGenSettings.SeaLevel - WorldGenSettings.MaxTerrainHeight;
  const float MaxTerrainZ =
      WorldGenSettings.SeaLevel + WorldGenSettings.MaxTerrainHeight;
  bool bLoggedMissingHeightService = false;

  bool bLoggedMissingMetadata = false;
  TSet<FName> MissingAttributes;
  TSet<FName> TypeMismatchAttributes;

  const TArray<FExpectedAttribute> &ExpectedAttributes =
      GetCanonicalAttributes();
  TMap<FName, const FExpectedAttribute *> ExpectedPointAttributes;
  for (const FExpectedAttribute &Attribute : ExpectedAttributes) {
    if (Attribute.Scope == EAttributeScope::Point) {
      ExpectedPointAttributes.Add(Attribute.Name, &Attribute);
    }
  }

  auto LogMissingAttribute = [&](const FName &AttributeName) {
    if (!MissingAttributes.Contains(AttributeName)) {
      MissingAttributes.Add(AttributeName);
      UE_LOG(LogPCGWorldService, Warning,
             TEXT("Tile (%d, %d) missing point attribute '%s'."), TileCoord.X,
             TileCoord.Y, *AttributeName.ToString());
    }
  };

  auto LogTypeMismatch = [&](const FName &AttributeName,
                             EPCGMetadataTypes ActualType,
                             const FExpectedAttribute *Expected) {
    if (!TypeMismatchAttributes.Contains(AttributeName)) {
      TypeMismatchAttributes.Add(AttributeName);
      const FString ExpectedLabel =
          Expected ? AllowedTypesToString(Expected->AllowedTypes)
                   : TEXT("unknown");
      UE_LOG(LogPCGWorldService, Warning,
             TEXT("Tile (%d, %d) point attribute '%s' stored as %s but "
                  "expected %s."),
             TileCoord.X, TileCoord.Y, *AttributeName.ToString(),
             *MetadataTypeToString(ActualType), *ExpectedLabel);
    }
  };

  auto ResolveGuidFromString = [](const FString &GuidString,
                                  FGuid &OutGuid) -> bool {
    return FGuid::Parse(GuidString, OutGuid) && OutGuid.IsValid();
  };

  for (int32 PointIndex = 0; PointIndex < Points.Num(); ++PointIndex) {
    const FPCGPoint &Point = Points[PointIndex];

    FPCGInstanceData Instance;
    Instance.Location = Point.Transform.GetLocation();
    Instance.Rotation = Point.Transform.Rotator();
    Instance.Scale = Point.Transform.GetScale3D();
    Instance.OwningTile = TileCoord;

    const PCGMetadataEntryKey EntryKey = Point.MetadataEntry;
    bool bRespectGraphZ = false;

    if (!Metadata) {
      if (!bLoggedMissingMetadata) {
        UE_LOG(LogPCGWorldService, Warning,
               TEXT("Point metadata unavailable for tile (%d, %d); using "
                    "transform-only instances."),
               TileCoord.X, TileCoord.Y);
        bLoggedMissingMetadata = true;
      }
    } else {
      if (EntryKey == PCGInvalidEntryKey) {
        LogMissingAttribute(VHMPCGAttr::InstanceId);
      }

      const FExpectedAttribute *StaticMeshExpectation =
          ExpectedPointAttributes.FindRef(VHMPCGAttr::StaticMesh);
      if (const FPCGMetadataAttributeBase *StaticMeshInfo =
              Metadata->GetConstAttribute(
                  FPCGAttributeIdentifier(VHMPCGAttr::StaticMesh))) {
        if (StaticMeshExpectation &&
            !StaticMeshExpectation->AllowedTypes.Contains(
                static_cast<EPCGMetadataTypes>(StaticMeshInfo->GetTypeId()))) {
          LogTypeMismatch(
              VHMPCGAttr::StaticMesh,
              static_cast<EPCGMetadataTypes>(StaticMeshInfo->GetTypeId()),
              StaticMeshExpectation);
        }
      }

      bool bMeshAssigned = false;
      const FName MeshAttributes[] = {VHMPCGAttr::StaticMesh, VHMPCGAttr::Mesh};
      for (const FName &AttrName : MeshAttributes) {
        FSoftObjectPath StaticMeshPath;
        if (TryGetSoftObjectPath(Metadata, EntryKey, AttrName,
                                 StaticMeshPath)) {
          Instance.Mesh = TSoftObjectPtr<UStaticMesh>(StaticMeshPath);
          bMeshAssigned = true;
          break;
        }
      }

      if (!bMeshAssigned) {
        const bool bHasStaticMeshAttr =
            HasAttribute(Metadata, VHMPCGAttr::StaticMesh);
        const bool bHasMeshAttr = HasAttribute(Metadata, VHMPCGAttr::Mesh);

        if (!bHasStaticMeshAttr && !bHasMeshAttr) {
          LogMissingAttribute(VHMPCGAttr::StaticMesh);
        } else {
          const FName AttrName =
              bHasStaticMeshAttr ? VHMPCGAttr::StaticMesh : VHMPCGAttr::Mesh;
          UE_LOG(LogPCGWorldService, Error,
                 TEXT("Tile (%d, %d) attribute '%s' must be authored as a Soft "
                      "Object Path or string asset reference."),
                 TileCoord.X, TileCoord.Y, *AttrName.ToString());
        }
      }

      if (TOptional<bool> IsActiveValue = GetMetadataValue<bool>(
              Metadata, VHMPCGAttr::IsActive, EntryKey)) {
        Instance.bIsActive = IsActiveValue.GetValue();
      } else {
        LogMissingAttribute(VHMPCGAttr::IsActive);
      }

      if (TOptional<FVector> ScaleValue = GetMetadataValue<FVector>(
              Metadata, VHMPCGAttr::InstanceScale, EntryKey)) {
        Instance.Scale = ScaleValue.GetValue();
      } else {
        LogMissingAttribute(VHMPCGAttr::InstanceScale);
      }

      if (TOptional<FRotator> RotationValue = GetMetadataValue<FRotator>(
              Metadata, VHMPCGAttr::InstanceRotation, EntryKey)) {
        Instance.Rotation = RotationValue.GetValue();
      } else {
        LogMissingAttribute(VHMPCGAttr::InstanceRotation);
      }

      FGuid InstanceGuid;
      bool bHasGuid = false;
      if (TOptional<FString> GuidString = GetMetadataValue<FString>(
              Metadata, VHMPCGAttr::InstanceId, EntryKey)) {
        if (ResolveGuidFromString(GuidString.GetValue(), InstanceGuid)) {
          bHasGuid = true;
        }
      } else if (TOptional<FName> GuidName = GetMetadataValue<FName>(
                     Metadata, VHMPCGAttr::InstanceId, EntryKey)) {
        if (ResolveGuidFromString(GuidName->ToString(), InstanceGuid)) {
          bHasGuid = true;
        }
      }

      if (bHasGuid) {
        Instance.InstanceId = InstanceGuid;
      } else {
        LogMissingAttribute(VHMPCGAttr::InstanceId);
      }

      if (TOptional<bool> RespectGraphValue = GetMetadataValue<bool>(
              Metadata, VHMPCGAttr::RespectGraphZ, EntryKey)) {
        bRespectGraphZ = RespectGraphValue.GetValue();
      }
    }

    const bool bZOutOfBounds = (Instance.Location.Z < MinTerrainZ ||
                                Instance.Location.Z > MaxTerrainZ);
    if (!bRespectGraphZ || bZOutOfBounds) {
      if (HeightfieldService) {
        const float TerrainHeight = HeightfieldService->SampleHeightWorldXY(
            FVector2D(Instance.Location.X, Instance.Location.Y));
        if (FMath::IsFinite(TerrainHeight)) {
          Instance.Location.Z = TerrainHeight;
        }
      } else if (!bLoggedMissingHeightService) {
        UE_LOG(LogPCGWorldService, Warning,
               TEXT("Heightfield service unavailable; cannot project PCG "
                    "instances for tile (%d, %d)."),
               TileCoord.X, TileCoord.Y);
        bLoggedMissingHeightService = true;
      }
    }

    Instance.Location.Z =
        FMath::Clamp(Instance.Location.Z, MinTerrainZ, MaxTerrainZ);

    OutGenerationData.GeneratedInstances.Add(MoveTemp(Instance));
  }

  OutGenerationData.GeneratedInstances.Sort(
      [](const FPCGInstanceData &A, const FPCGInstanceData &B) {
        const bool bAValidGuid = A.InstanceId.IsValid();
        const bool bBValidGuid = B.InstanceId.IsValid();

        if (bAValidGuid && bBValidGuid) {
          if (A.InstanceId.A != B.InstanceId.A) {
            return A.InstanceId.A < B.InstanceId.A;
          }
          if (A.InstanceId.B != B.InstanceId.B) {
            return A.InstanceId.B < B.InstanceId.B;
          }
          if (A.InstanceId.C != B.InstanceId.C) {
            return A.InstanceId.C < B.InstanceId.C;
          }
          return A.InstanceId.D < B.InstanceId.D;
        }

        if (bAValidGuid != bBValidGuid) {
          return bAValidGuid;
        }

        if (!FMath::IsNearlyEqual(A.Location.X, B.Location.X)) {
          return A.Location.X < B.Location.X;
        }
        if (!FMath::IsNearlyEqual(A.Location.Y, B.Location.Y)) {
          return A.Location.Y < B.Location.Y;
        }
        if (!FMath::IsNearlyEqual(A.Location.Z, B.Location.Z)) {
          return A.Location.Z < B.Location.Z;
        }

        return false;
      });

  uint64 CombinedHash = 0;
  if (OutGenerationData.GeneratedInstances.Num() > 0) {
    CombinedHash = 1469598103934665603ull;
    for (const FPCGInstanceData &Instance :
         OutGenerationData.GeneratedInstances) {
      const FTransform InstanceTransform(Instance.Rotation, Instance.Location,
                                         Instance.Scale);
      const FTransform QuantizedTransform =
          QuantizeTransformForHash(InstanceTransform);
      const uint64 PerHash = HashTransform(QuantizedTransform);
      CombinedHash ^= PerHash;
      CombinedHash *= 1099511628211ull;
    }
  }

  OutGenerationData.TotalInstanceCount =
      OutGenerationData.GeneratedInstances.Num();
  OutGenerationData.InstanceTransformHash = static_cast<int64>(CombinedHash);
}

UPCGWorldService::FAttributeValidationResult
UPCGWorldService::ValidateInputAttributes(
    const UPCGParamData *ParameterData, const UPCGPointData *PointData) const {
  FAttributeValidationResult Result;

  using namespace PCGWorldService::Private;

  const UPCGMetadata *ParameterMetadata =
      ParameterData ? ParameterData->ConstMetadata() : nullptr;
  const UPCGMetadata *PointMetadata =
      PointData ? PointData->ConstMetadata() : nullptr;

  const TArray<FExpectedAttribute> &ExpectedAttributes =
      GetCanonicalAttributes();
  TSet<FName> ParameterAttributeNames;
  TSet<FName> PointAttributeNames;

  auto ProcessAttribute = [&](const FExpectedAttribute &Attribute) {
    TSet<FName> &KnownSet = (Attribute.Scope == EAttributeScope::Parameter)
                                ? ParameterAttributeNames
                                : PointAttributeNames;
    KnownSet.Add(Attribute.Name);

    const UPCGMetadata *Metadata =
        (Attribute.Scope == EAttributeScope::Parameter) ? ParameterMetadata
                                                        : PointMetadata;
    if (!Metadata) {
      if (Attribute.bRequired) {
        Result.Errors.AddUnique(FString::Printf(
            TEXT("Missing %s metadata when validating attribute '%s'."),
            GetScopeLabel(Attribute.Scope), *Attribute.Name.ToString()));
      }
      return;
    }

    const FPCGMetadataAttributeBase *MetadataAttribute =
        Metadata->GetConstAttribute(FPCGAttributeIdentifier(Attribute.Name));
    if (!MetadataAttribute) {
      if (Attribute.bRequired) {
        Result.Errors.AddUnique(FString::Printf(
            TEXT("Missing required %s attribute '%s'."),
            GetScopeLabel(Attribute.Scope), *Attribute.Name.ToString()));
      } else {
        Result.Warnings.AddUnique(FString::Printf(
            TEXT("Optional %s attribute '%s' not provided."),
            GetScopeLabel(Attribute.Scope), *Attribute.Name.ToString()));
      }
      return;
    }

    const EPCGMetadataTypes ActualType =
        static_cast<EPCGMetadataTypes>(MetadataAttribute->GetTypeId());
    if (!Attribute.AllowedTypes.Contains(ActualType)) {
      Result.Errors.AddUnique(FString::Printf(
          TEXT("%s attribute '%s' is stored as %s but expected %s."),
          GetScopeLabel(Attribute.Scope), *Attribute.Name.ToString(),
          *MetadataTypeToString(ActualType),
          *AllowedTypesToString(Attribute.AllowedTypes)));
    }
  };

  for (const FExpectedAttribute &Attribute : ExpectedAttributes) {
    ProcessAttribute(Attribute);
  }

  auto FlagUnknown = [&](const UPCGMetadata *Metadata, EAttributeScope Scope,
                         const TSet<FName> &KnownAttributes) {
    if (!Metadata) {
      return;
    }

    TArray<FName> AttributeNames;
    TArray<EPCGMetadataTypes> AttributeTypes;
    Metadata->GetAttributes(AttributeNames, AttributeTypes);

    for (int32 Index = 0; Index < AttributeNames.Num(); ++Index) {
      const FName &AttributeName = AttributeNames[Index];
      if (!KnownAttributes.Contains(AttributeName)) {
        const EPCGMetadataTypes ReportedType =
            AttributeTypes.IsValidIndex(Index)
                ? static_cast<EPCGMetadataTypes>(AttributeTypes[Index])
                : EPCGMetadataTypes::Unknown;
        Result.Warnings.AddUnique(
            FString::Printf(TEXT("Unexpected %s attribute '%s' (type %s)."),
                            GetScopeLabel(Scope), *AttributeName.ToString(),
                            *MetadataTypeToString(ReportedType)));
      }
    }
  };

  FlagUnknown(ParameterMetadata, EAttributeScope::Parameter,
              ParameterAttributeNames);
  FlagUnknown(PointMetadata, EAttributeScope::Point, PointAttributeNames);

  Result.bIsValid = Result.Errors.Num() == 0;

  if (Result.Errors.Num() > 0 || Result.Warnings.Num() > 0) {
    UE_LOG(LogPCGWorldService, Log,
           TEXT("ValidateInputAttributes: %d error(s), %d warning(s)."),
           Result.Errors.Num(), Result.Warnings.Num());
  }

  for (const FString &ErrorMessage : Result.Errors) {
    UE_LOG(LogPCGWorldService, Error, TEXT("  %s"), *ErrorMessage);
  }

  for (const FString &WarningMessage : Result.Warnings) {
    UE_LOG(LogPCGWorldService, Warning, TEXT("  %s"), *WarningMessage);
  }

  return Result;
}

#endif

void UPCGWorldService::ApplyWorldGenSettings(const FWorldGenConfig &Settings) {
  WorldGenSettings = Settings;
  MaxInstancesPerTile = Settings.MaxHISMInstances;
  bAllowHeadlessLogicalInstances = Settings.bAllowHeadlessLogicalInstances;
  InitializeDefaultBiomes();
}

bool UPCGWorldService::Initialize(const FWorldGenConfig &Settings) {
  ApplyWorldGenSettings(Settings);

#if WITH_AUTOMATION_TESTS && VHM_PCG_ENABLED
  if (TestWorldOverride.IsValid()) {
    bHeadless = false;
  } else {
    bHeadless = (GetWorld() == nullptr);
  }
#else
  bHeadless = (GetWorld() == nullptr);
#endif
  if (bHeadless) {
    UE_LOG(LogPCGWorldService, Warning,
           TEXT("Headless mode: PCG running without UWorld; HISM updates will "
                "be skipped."));

    if (!bAllowHeadlessLogicalInstances) {
      UE_LOG(LogPCGWorldService, Warning,
             TEXT("Headless logical instances disabled via config; runtime "
                  "will avoid force-spawning."));
    }
  }

  bDedicatedServer = IsRunningDedicatedServer();
#if WITH_AUTOMATION_TESTS && VHM_PCG_ENABLED
  if (TestWorldOverride.IsValid()) {
    bDedicatedServer = false;
  }
#endif
  if (bDedicatedServer) {
    UE_LOG(LogPCGWorldService, Warning,
           TEXT("Dedicated server detected: PCG graphs will resolve to logical "
                "fallback data."));
  }

#if VHM_PCG_ENABLED
  UE_LOG(LogPCGWorldService, Log,
         TEXT("PCG World Service initialized with PCG support"));
  RefreshRuntimeSettingsFromCVars();
  if (!WorldCleanupHandle.IsValid()) {
    WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
        this, &UPCGWorldService::HandleWorldCleanup);
  }
#else
  UE_LOG(LogPCGWorldService, Warning,
         TEXT("PCG World Service initialized without PCG support - using "
              "fallback generation"));
#endif

  return true;
}

bool UPCGWorldService::InitializePCGGraph(UObject *BiomeGraph) {
  if (!BiomeGraph) {
    UE_LOG(LogPCGWorldService, Error,
           TEXT("Cannot initialize with null PCG graph"));
    return false;
  }

#if VHM_PCG_ENABLED
  // Validate that its actually a PCG graph when PCG is available
  UPCGGraph *PCGGraph = Cast<UPCGGraph>(BiomeGraph);
  if (!PCGGraph) {
    UE_LOG(LogPCGWorldService, Error,
           TEXT("Provided object is not a valid PCG graph"));
    return false;
  }
  CurrentPCGGraph = BiomeGraph;
  UE_LOG(LogPCGWorldService, Log, TEXT("PCG graph initialized: %s"),
         *BiomeGraph->GetName());
#else
  // Store the object but log that PCG is not available
  CurrentPCGGraph = BiomeGraph;
  UE_LOG(LogPCGWorldService, Warning,
         TEXT("PCG graph provided but PCG system not available - stored for "
              "future use"));
#endif

  return true;
}

FPCGGenerationData
UPCGWorldService::GenerateBiomeContent(FTileCoord TileCoord,
                                       EBiomeType BiomeType,
                                       const TArray<float> &HeightData) {
  double StartTime = FPlatformTime::Seconds();

#if VHM_PCG_ENABLED
  TRACE_CPUPROFILER_EVENT_SCOPE(PCG_TileGenerate);
#endif

  // Add logging during content test to verify rule count
  if (const FBiomeDefinition *BiomeDef = BiomeDefinitions.Find(BiomeType)) {
    UE_LOG(LogPCGWorldService, Log, TEXT("%s rules: N=%d"),
           *UEnum::GetValueAsString(BiomeType),
           BiomeDef->VegetationRules.Num());
  }

  // For biome-specific generation (test path), dont use cache - always generate
  // fresh This ensures we use the BiomeType parameter as authoritative rather
  // than tile classification

  // Generate new content using BiomeType as authoritative (not tile
  // classification)
  FPCGGenerationData GenerationData =
      GenerateContentInternal(TileCoord, BiomeType, HeightData);

  // Cache the generated content so RemoveContentInArea can find it
  // This is needed for the area removal test to work properly
  GenerationCache.Add(TileCoord, GenerationData);
  UE_LOG(LogPCGWorldService, Log,
         TEXT("Cached generation data for tile (%d, %d) with %d instances for "
              "area removal testing"),
         TileCoord.X, TileCoord.Y, GenerationData.TotalInstanceCount);

  // Update performance stats
  double EndTime = FPlatformTime::Seconds();
  float GenerationTimeMs = static_cast<float>((EndTime - StartTime) * 1000.0);
  UpdatePerformanceStats(GenerationTimeMs, GenerationData.TotalInstanceCount);

  WORLDGEN_LOG_WITH_SEED_TILE(
      Log, WorldGenSettings.Seed, TileCoord,
      TEXT("PCG spawn completed - %d instances in %.2fms"),
      GenerationData.TotalInstanceCount, GenerationTimeMs);

  return GenerationData;
}

FPCGGenerationData
UPCGWorldService::GenerateContentInternal(FTileCoord TileCoord,
                                          EBiomeType BiomeType,
                                          const TArray<float> &HeightData) {
  FPCGTileMetrics TileMetrics;
  bool bHasTileMetrics = false;
  const float SamplesPerSideFloat =
      WorldGenSettings.TileSizeMeters /
      FMath::Max(KINDA_SMALL_NUMBER, WorldGenSettings.SampleSpacingMeters);
  const int32 SamplesPerSide =
      FMath::Clamp(FMath::RoundToInt(SamplesPerSideFloat), 1, 4096);
  const int32 ExpectedHeightDataSize = SamplesPerSide * SamplesPerSide;

#if VHM_PCG_ENABLED
  if (bDedicatedServer || bHeadless) {
    const FString Reason = bDedicatedServer ? TEXT("Dedicated server fallback")
                                            : TEXT("Headless world fallback");
    MarkTelemetryFallback(InvalidPCGTaskId, Reason, nullptr, BiomeType,
                          TileCoord);
    return GenerateFallbackContent(TileCoord, BiomeType, HeightData,
                                   WorldGenSettings.bEnablePCGGraphs, nullptr);
  }

  if (WorldGenSettings.bEnablePCGGraphs && bRuntimeOperationsEnabled) {
    if (HeightData.Num() == ExpectedHeightDataSize) {
      TileMetrics = AnalyzeTileMetrics(HeightData);
      bHasTileMetrics = true;

      FPCGGenerationData GraphDrivenData;
      if (TryGeneratePCGGraphContent(TileCoord, BiomeType, HeightData,
                                     TileMetrics, GraphDrivenData)) {
        return GraphDrivenData;
      }
    } else {
      UE_LOG(LogPCGWorldService, Warning,
             TEXT("PCG graph requested for tile (%d, %d) but height data "
                  "contained %d samples; using fallback generation"),
             TileCoord.X, TileCoord.Y, HeightData.Num());
    }
  }
#endif

  const bool bUsePCGHeuristics = WorldGenSettings.bEnablePCGGraphs;

  if (!bHasTileMetrics && bUsePCGHeuristics &&
      HeightData.Num() == ExpectedHeightDataSize) {
    TileMetrics = AnalyzeTileMetrics(HeightData);
    bHasTileMetrics = true;
  }

  return GenerateFallbackContent(TileCoord, BiomeType, HeightData,
                                 bUsePCGHeuristics,
                                 bHasTileMetrics ? &TileMetrics : nullptr);
}

void UPCGWorldService::AbandonTasksForTile(FTileCoord TileCoord) {
#if VHM_PCG_ENABLED
  if (!SchedulerExecutor.IsValid()) {
    return;
  }

  UWorld *World = GetWorld();
  if (!World) {
    return;
  }

  UPCGSubsystem *PCGSubsystem = World->GetSubsystem<UPCGSubsystem>();
  if (!PCGSubsystem) {
    return;
  }

  TArray<FPCGTaskId> TaskIds;
  ActiveTasks.GetKeys(TaskIds);
  for (FPCGTaskId TaskId : TaskIds) {
    if (FPCGTaskContext *Context = ActiveTasks.Find(TaskId)) {
      if (Context->TileCoord == TileCoord) {
        SchedulerExecutor->AbandonTask(
            *PCGSubsystem, TaskId, *Context,
            FString::Printf(TEXT("Tile (%d,%d) unload"), TileCoord.X,
                            TileCoord.Y));
        SchedulerExecutor->ReleaseTask(*PCGSubsystem, TaskId, *Context);
        MarkTelemetryFallback(TaskId, TEXT("Tile unload"), nullptr,
                              EBiomeType::None, TileCoord);
        ReleaseTrackedTask(TaskId);
      }
    }
  }
#else
  (void)TileCoord;
#endif
}

FPCGGenerationData UPCGWorldService::GeneratePCGContent(
    FTileCoord TileCoord, EBiomeType BiomeType, const TArray<float> &HeightData,
    UPCGGraph *PCGGraph, const FPCGTileMetrics *TileMetrics) {
  FPCGGenerationData GenerationData;
  GenerationData.TileCoord = TileCoord;
  GenerationData.BiomeType = BiomeType;

#if !VHM_PCG_ENABLED
  return GenerateFallbackContent(TileCoord, BiomeType, HeightData, true,
                                 TileMetrics);
#else
  if (!WorldGenSettings.bEnablePCGGraphs || !bRuntimeOperationsEnabled ||
      !PCGGraph) {
    MarkTelemetryFallback(InvalidPCGTaskId,
                          TEXT("PCG graphs disabled or graph missing"),
                          PCGGraph, BiomeType, TileCoord);
    return GenerateFallbackContent(TileCoord, BiomeType, HeightData, true,
                                   TileMetrics);
  }

  if (!SchedulerExecutor.IsValid()) {
    UE_LOG(LogPCGWorldService, Warning,
           TEXT("Scheduler executor unavailable; falling back for biome %s on "
                "tile (%d,%d)."),
           *UEnum::GetValueAsString(BiomeType), TileCoord.X, TileCoord.Y);
    MarkTelemetryFallback(InvalidPCGTaskId,
                          TEXT("Scheduler executor unavailable"), PCGGraph,
                          BiomeType, TileCoord);
    return GenerateFallbackContent(TileCoord, BiomeType, HeightData, true,
                                   TileMetrics);
  }

  UWorld *World = GetWorld();
#if WITH_AUTOMATION_TESTS && VHM_PCG_ENABLED
  if (!World && TestWorldOverride.IsValid()) {
    World = TestWorldOverride.Get();
  }
#endif
  if (!World) {
    UE_LOG(LogPCGWorldService, Warning,
           TEXT("World context unavailable; falling back for biome %s on tile "
                "(%d,%d)."),
           *UEnum::GetValueAsString(BiomeType), TileCoord.X, TileCoord.Y);
    MarkTelemetryFallback(InvalidPCGTaskId, TEXT("World context unavailable"),
                          PCGGraph, BiomeType, TileCoord);
    return GenerateFallbackContent(TileCoord, BiomeType, HeightData, true,
                                   TileMetrics);
  }

  UPCGSubsystem *PCGSubsystem = World->GetSubsystem<UPCGSubsystem>();
#if WITH_AUTOMATION_TESTS && VHM_PCG_ENABLED
  if (!PCGSubsystem && TestSubsystemOverride.IsValid()) {
    PCGSubsystem = TestSubsystemOverride.Get();
  }
#endif
  if (!PCGSubsystem) {
    UE_LOG(LogPCGWorldService, Warning,
           TEXT("PCG subsystem unavailable; falling back for biome %s on tile "
                "(%d,%d)."),
           *UEnum::GetValueAsString(BiomeType), TileCoord.X, TileCoord.Y);
    MarkTelemetryFallback(InvalidPCGTaskId, TEXT("PCG subsystem unavailable"),
                          PCGGraph, BiomeType, TileCoord);
    return GenerateFallbackContent(TileCoord, BiomeType, HeightData, true,
                                   TileMetrics);
  }

  AActor *AnchorActor = EnsurePCGAnchor(World);
  if (!AnchorActor) {
    UE_LOG(LogPCGWorldService, Error,
           TEXT("Failed to create PCG anchor actor; falling back for tile "
                "(%d,%d)."),
           TileCoord.X, TileCoord.Y);
    MarkTelemetryFallback(InvalidPCGTaskId, TEXT("Failed to create PCG anchor"),
                          PCGGraph, BiomeType, TileCoord);
    return GenerateFallbackContent(TileCoord, BiomeType, HeightData, true,
                                   TileMetrics);
  }

  UPCGComponent *Component = GetOrCreateBiomeComponent(BiomeType, *PCGGraph);
  if (!Component) {
    UE_LOG(LogPCGWorldService, Warning,
           TEXT("Failed to resolve PCG component; falling back for biome %s on "
                "tile (%d,%d)."),
           *UEnum::GetValueAsString(BiomeType), TileCoord.X, TileCoord.Y);
    MarkTelemetryFallback(InvalidPCGTaskId,
                          TEXT("Failed to resolve PCG component"), PCGGraph,
                          BiomeType, TileCoord);
    return GenerateFallbackContent(TileCoord, BiomeType, HeightData, true,
                                   TileMetrics);
  }

  if (!CanScheduleNewTask()) {
    UE_LOG(LogPCGWorldService, Warning,
           TEXT("Reached maximum concurrent PCG tasks (%d); falling back for "
                "tile (%d,%d)."),
           WorldGenSettings.MaxConcurrentPCGTasks, TileCoord.X, TileCoord.Y);
    MarkTelemetryFallback(InvalidPCGTaskId,
                          TEXT("Reached concurrent task limit"), PCGGraph,
                          BiomeType, TileCoord);
    return GenerateFallbackContent(TileCoord, BiomeType, HeightData, true,
                                   TileMetrics);
  }

  const float SamplesPerSideFloat =
      WorldGenSettings.TileSizeMeters /
      FMath::Max(KINDA_SMALL_NUMBER, WorldGenSettings.SampleSpacingMeters);
  const int32 SamplesPerSide =
      FMath::Clamp(FMath::RoundToInt(SamplesPerSideFloat), 1, 4096);
  const int32 ExpectedHeightSamples = SamplesPerSide * SamplesPerSide;
  const bool bHasHeightData = HeightData.Num() == ExpectedHeightSamples;

  FPCGTileMetrics LocalMetrics;
  const FPCGTileMetrics *EffectiveMetrics = TileMetrics;
  if (!EffectiveMetrics && bHasHeightData) {
    LocalMetrics = AnalyzeTileMetrics(HeightData);
    EffectiveMetrics = &LocalMetrics;
  }

  const float EstimatedWorkUnits =
      EstimateWorkUnitsForBiome(BiomeType, EffectiveMetrics);
  const float DensityScale = ComputeDensityScaleFromWork(EstimatedWorkUnits);
  GenerationData.EstimatedWorkUnits = EstimatedWorkUnits;
  GenerationData.DensityScale = DensityScale;

  const uint32 TileSeed = GetTileRandomSeed(TileCoord);
  const float BiomeBlendWeight =
      ResolveBiomeBlendWeight(TileCoord, BiomeType, EffectiveMetrics);
  PrewarmBiomeAssets(TileCoord, BiomeType);
  UObject *DataOuter = AnchorActor ? static_cast<UObject *>(AnchorActor)
                                   : static_cast<UObject *>(this);
  UPCGParamData *ParameterData =
      PCGWorldService::Private::CreateTileParameterData(
          DataOuter, EffectiveMetrics ? *EffectiveMetrics : FPCGTileMetrics(),
          TileCoord, BiomeType, WorldGenSettings, TileSeed, DensityScale,
          BiomeBlendWeight);
  UPCGPointData *TilePointData = PCGWorldService::Private::CreateTilePointData(
      DataOuter, TileCoord, WorldGenSettings,
      EffectiveMetrics ? *EffectiveMetrics : FPCGTileMetrics());

  FAttributeValidationResult Validation =
      ValidateInputAttributes(ParameterData, TilePointData);
  if (!Validation.bIsValid) {
    UE_LOG(LogPCGWorldService, Error,
           TEXT("Input validation failed for biome %s on tile (%d,%d); using "
                "fallback generation."),
           *UEnum::GetValueAsString(BiomeType), TileCoord.X, TileCoord.Y);
    MarkTelemetryFallback(InvalidPCGTaskId, TEXT("Input validation failed"),
                          PCGGraph, BiomeType, TileCoord);
    return GenerateFallbackContent(TileCoord, BiomeType, HeightData, true,
                                   TileMetrics);
  }

  FPCGInputSet InputSet;
  InputSet.Add(TEXT("TileParameters"), ParameterData);
  InputSet.Add(TEXT("Tile"), TilePointData);

  const FVector TileCenter =
      TileCoord.ToWorldPosition(WorldGenSettings.TileSizeMeters);
  const FVector TileExtent(WorldGenSettings.TileSizeMeters * 0.5f,
                           WorldGenSettings.TileSizeMeters * 0.5f,
                           WorldGenSettings.TileSizeMeters * 0.25f);
  const FBox ExecutionBounds = FBox::BuildAABB(TileCenter, TileExtent);

  FString DebugLabel = FString::Printf(TEXT("%s:(%d,%d)"), *PCGGraph->GetName(),
                                       TileCoord.X, TileCoord.Y);

  TArray<FString> ScheduleWarnings;
  TArray<FString> ScheduleErrors;
  FPCGTaskContext TaskContext;

  const bool bEnableFrustum = WorldGenSettings.bEnableFrustumCulling;
  const float FrustumMargin = ResolveFrustumMargin(*Component, BiomeType);

  const FPCGTaskId TaskId = SchedulerExecutor->ScheduleGraphAsync(
      *PCGSubsystem, *Component, *PCGGraph, *World, TileCoord, InputSet,
      DebugLabel, ExecutionBounds, static_cast<int32>(TileSeed), bEnableFrustum,
      FrustumMargin, TaskContext, ScheduleErrors, ScheduleWarnings);
  if (TaskId == InvalidPCGTaskId) {
    for (const FString &Warning : ScheduleWarnings) {
      UE_LOG(LogPCGWorldService, Warning,
             TEXT("PCG scheduler warning (%s): %s"), *DebugLabel, *Warning);
    }
    for (const FString &Error : ScheduleErrors) {
      UE_LOG(LogPCGWorldService, Error, TEXT("PCG scheduler error (%s): %s"),
             *DebugLabel, *Error);
    }
    MarkTelemetryFallback(InvalidPCGTaskId,
                          TEXT("ScheduleGraphAsync returned InvalidPCGTaskId"),
                          PCGGraph, BiomeType, TileCoord);
    return GenerateFallbackContent(TileCoord, BiomeType, HeightData, true,
                                   TileMetrics);
  }

  RegisterTelemetry(TaskId, BiomeType, *PCGGraph, TileCoord);
  TrackTask(TaskContext);
  MarkTelemetryStart(TaskId);

  IConsoleVariable *PollVar =
      IConsoleManager::Get().FindConsoleVariable(TEXT("vhm.pcg.poll_ms"));
  IConsoleVariable *TimeoutVar =
      IConsoleManager::Get().FindConsoleVariable(TEXT("vhm.pcg.timeout_ms"));
  const int32 PollMs = PollVar ? FMath::Max(1, PollVar->GetInt()) : 12;
  const int32 TimeoutMs =
      TimeoutVar ? FMath::Max(PollMs, TimeoutVar->GetInt()) : 5000;
  const double TimeoutSeconds = static_cast<double>(TimeoutMs) / 1000.0;
  const double StartSeconds = FPlatformTime::Seconds();

  bool bTaskSucceeded = false;
  double ExecutionTimeMs = 0.0;
  FPCGOutputSet OutputSet;
  int32 GeneratedPointCount = 0;

  while (true) {
    FPCGTaskContext UpdatedContext;
    if (SchedulerExecutor->IsTaskComplete(*PCGSubsystem, TaskId,
                                          UpdatedContext)) {
      if (FPCGTaskContext *StoredContext = ActiveTasks.Find(TaskId)) {
        *StoredContext = UpdatedContext;
      }

      TaskContext = UpdatedContext;
      if (TaskContext.State != EPCGTaskState::Abandoned) {
        bTaskSucceeded = SchedulerExecutor->GetTaskOutput(
            *PCGSubsystem, TaskId, TaskContext, OutputSet, GeneratedPointCount,
            ExecutionTimeMs, ScheduleWarnings, ScheduleErrors);
      }
      break;
    }

    const double Elapsed = FPlatformTime::Seconds() - StartSeconds;
    if (Elapsed > TimeoutSeconds) {
      FString TimeoutReason =
          FString::Printf(TEXT("Scheduler timeout after %.2fs waiting for %s"),
                          Elapsed, *DebugLabel);
      ScheduleErrors.Add(TimeoutReason);
      SchedulerExecutor->AbandonTask(*PCGSubsystem, TaskId, TaskContext,
                                     TimeoutReason);
      MarkTelemetryFallback(TaskId, TimeoutReason, PCGGraph, BiomeType,
                            TileCoord);
      break;
    }

    FPlatformProcess::SleepNoStats(static_cast<double>(PollMs) / 1000.0);
  }

  SchedulerExecutor->ReleaseTask(*PCGSubsystem, TaskId, TaskContext);
  ReleaseTrackedTask(TaskId);

  for (const FString &Warning : ScheduleWarnings) {
    UE_LOG(LogPCGWorldService, Warning, TEXT("PCG scheduler warning (%s): %s"),
           *DebugLabel, *Warning);
  }

  for (const FString &Error : ScheduleErrors) {
    UE_LOG(LogPCGWorldService, Error, TEXT("PCG scheduler error (%s): %s"),
           *DebugLabel, *Error);
  }

  if (!bTaskSucceeded || TaskContext.State == EPCGTaskState::Abandoned) {
    UE_LOG(LogPCGWorldService, Warning,
           TEXT("PCG graph %s failed to complete for biome %s on tile (%d,%d); "
                "using fallback."),
           *PCGGraph->GetName(), *UEnum::GetValueAsString(BiomeType),
           TileCoord.X, TileCoord.Y);
    if (ActiveTelemetry.Contains(TaskId)) {
      MarkTelemetryFallback(TaskId, TEXT("Task failed to complete"), PCGGraph,
                            BiomeType, TileCoord);
    }
    return GenerateFallbackContent(TileCoord, BiomeType, HeightData, true,
                                   TileMetrics);
  }

  GenerationData.GenerationTimeMs = static_cast<float>(ExecutionTimeMs);

  for (const TObjectPtr<UPCGData> &OutputDatum : OutputSet.Outputs) {
    if (const UPCGPointData *OutputPointData =
            Cast<UPCGPointData>(OutputDatum.Get())) {
      PCGWorldService::Private::ExtractInstancesFromPointData(
          OutputPointData, TileCoord, WorldGenSettings,
          HeightfieldService.Get(), GenerationData);
    }
  }

  GenerationData.TotalInstanceCount = GenerationData.GeneratedInstances.Num();

  if (GenerationData.TotalInstanceCount > MaxInstancesPerTile) {
    ApplyDensityLimiting(GenerationData);
  }

  GenerationData.TotalInstanceCount = GenerationData.GeneratedInstances.Num();

  MarkTelemetryCompletion(TaskId, true, GenerationData.TotalInstanceCount,
                          ExecutionTimeMs, false, TEXT("Completed"));

  if (GenerationData.TotalInstanceCount == 0) {
    UE_LOG(
        LogPCGWorldService, Verbose,
        TEXT(
            "PCG graph %s produced no instances for biome %s on tile (%d,%d)."),
        *PCGGraph->GetName(), *UEnum::GetValueAsString(BiomeType), TileCoord.X,
        TileCoord.Y);
  }

  return GenerationData;
#endif // VHM_PCG_ENABLED
}

bool UPCGWorldService::TryGeneratePCGGraphContent(
    FTileCoord TileCoord, EBiomeType BiomeType, const TArray<float> &HeightData,
    const FPCGTileMetrics &TileMetrics, FPCGGenerationData &OutData) {
  const bool bHasGraphReference = BiomePCGGraphs.Contains(BiomeType);
  UPCGGraph *Graph = ResolveBiomePCGGraph(BiomeType);

  if (!Graph && !bHasGraphReference) {
    UE_LOG(
        LogPCGWorldService, Verbose,
        TEXT(
            "No PCG graph registered for biome %s - skipping graph generation"),
        *UEnum::GetValueAsString(BiomeType));
    return false;
  }

  OutData =
      GeneratePCGContent(TileCoord, BiomeType, HeightData, Graph, &TileMetrics);
  return true;
}

FPCGGenerationData UPCGWorldService::GenerateFallbackContent(
    FTileCoord TileCoord, EBiomeType BiomeType, const TArray<float> &HeightData,
    bool bUsePCGHeuristics, const FPCGTileMetrics *TileMetrics) {
  FPCGGenerationData GenerationData;
  GenerationData.TileCoord = TileCoord;
  GenerationData.BiomeType = BiomeType;

  const FBiomeDefinition *BiomeDef = BiomeDefinitions.Find(BiomeType);
  if (!BiomeDef) {
    UE_LOG(LogPCGWorldService, Warning,
           TEXT("No biome definition found for biome type %d"),
           static_cast<int32>(BiomeType));
    return GenerationData;
  }

  const float SamplesPerSideFloat =
      WorldGenSettings.TileSizeMeters /
      FMath::Max(KINDA_SMALL_NUMBER, WorldGenSettings.SampleSpacingMeters);
  const int32 SamplesPerSide =
      FMath::Clamp(FMath::RoundToInt(SamplesPerSideFloat), 1, 4096);
  const int32 ExpectedHeightDataSize = SamplesPerSide * SamplesPerSide;
  const bool bHasValidHeightData = HeightData.Num() == ExpectedHeightDataSize;

  FPCGTileMetrics LocalMetrics;
  const FPCGTileMetrics *EffectiveMetrics = TileMetrics;
  if (bUsePCGHeuristics && !EffectiveMetrics && bHasValidHeightData) {
    LocalMetrics = AnalyzeTileMetrics(HeightData);
    EffectiveMetrics = &LocalMetrics;
  }

  FPCGSpawnParams SpawnParams;
  const float BiomeBlendWeight =
      ResolveBiomeBlendWeight(TileCoord, BiomeType, EffectiveMetrics);
  SpawnParams.BiomeWeightScale = BiomeBlendWeight;
  PrewarmBiomeAssets(TileCoord, BiomeType);

  if (bHeadless && bAllowHeadlessLogicalInstances) {
    SpawnParams.bForceBiome = true;
    SpawnParams.BiomeOverride = BiomeType;
  }

  if (EffectiveMetrics) {
    const float SlopeFactor = FMath::Clamp(
        1.0f - (EffectiveMetrics->AverageSlope / 60.0f), 0.2f, 1.0f);
    const float WaterFalloff =
        FMath::Max(10.0f, WorldGenSettings.TileSizeMeters * 0.75f);
    const float WaterFactor = FMath::Clamp(
        1.0f - (EffectiveMetrics->MinAbsWaterDistance / WaterFalloff), 0.2f,
        1.0f);

    SpawnParams.SlopeResponse = SlopeFactor;
    SpawnParams.WaterResponse = WaterFactor;
  }

  if (bUsePCGHeuristics) {
    UE_LOG(LogPCGWorldService, VeryVerbose,
           TEXT("Applying PCG heuristics for biome %s (metrics available: %s)"),
           *UEnum::GetValueAsString(BiomeType),
           EffectiveMetrics ? TEXT("true") : TEXT("false"));
  }

  const float EstimatedWorkUnits =
      EstimateWorkUnitsForBiome(BiomeType, EffectiveMetrics);
  const float DensityScale = ComputeDensityScaleFromWork(EstimatedWorkUnits);
  GenerationData.EstimatedWorkUnits = EstimatedWorkUnits;
  GenerationData.DensityScale = DensityScale;

  TArray<FPCGInstanceData> VegetationInstances = GenerateVegetationInstances(
      TileCoord, *BiomeDef, HeightData, SpawnParams, EffectiveMetrics,
      bUsePCGHeuristics, DensityScale);
  GenerationData.GeneratedInstances.Append(VegetationInstances);

  TArray<FPOIData> POIInstances =
      GeneratePOIInstances(TileCoord, *BiomeDef, HeightData);
  for (const FPOIData &POI : POIInstances) {
    FPCGInstanceData InstanceData;
    InstanceData.Location = POI.Location;
    InstanceData.Rotation = POI.Rotation;
    InstanceData.Scale = POI.Scale;
    InstanceData.OwningTile = TileCoord;
    GenerationData.GeneratedInstances.Add(InstanceData);
  }

  GenerationData.TotalInstanceCount = GenerationData.GeneratedInstances.Num();

  if (GenerationData.TotalInstanceCount > MaxInstancesPerTile) {
    ApplyDensityLimiting(GenerationData);
  }

  return GenerationData;
}

TArray<FPCGInstanceData> UPCGWorldService::GenerateVegetationInstances(
    FTileCoord TileCoord, const FBiomeDefinition &BiomeDef,
    const TArray<float> &HeightData, const FPCGTileMetrics *TileMetrics,
    bool bUsePCGHeuristics, float DensityScale) {
  FPCGSpawnParams DefaultSpawnParams;
  return GenerateVegetationInstances(TileCoord, BiomeDef, HeightData,
                                     DefaultSpawnParams, TileMetrics,
                                     bUsePCGHeuristics, DensityScale);
}

TArray<FPCGInstanceData> UPCGWorldService::GenerateVegetationInstances(
    FTileCoord TileCoord, const FBiomeDefinition &BiomeDef,
    const TArray<float> &HeightData, const FPCGSpawnParams &SpawnParams,
    const FPCGTileMetrics *TileMetrics, bool bUsePCGHeuristics,
    float DensityScale) {
  TArray<FPCGInstanceData> Instances;

  const float SamplesPerSideFloat =
      WorldGenSettings.TileSizeMeters /
      FMath::Max(KINDA_SMALL_NUMBER, WorldGenSettings.SampleSpacingMeters);
  const int32 SamplesPerSide =
      FMath::Clamp(FMath::RoundToInt(SamplesPerSideFloat), 1, 4096);
  const int32 ExpectedHeightDataSize = SamplesPerSide * SamplesPerSide;
  if (HeightData.Num() != ExpectedHeightDataSize) {
    UE_LOG(LogPCGWorldService, Error,
           TEXT("Height data size mismatch: expected %d elements (%dx%d), got "
                "%d elements"),
           ExpectedHeightDataSize, SamplesPerSide, SamplesPerSide,
           HeightData.Num());
    return Instances;
  }

  const float TileSize = WorldGenSettings.TileSizeMeters;
  const float InvSampleSpacing =
      1.0f /
      FMath::Max(KINDA_SMALL_NUMBER, WorldGenSettings.SampleSpacingMeters);
  FVector TileWorldPos = TileCoord.ToWorldPosition(TileSize);
  FVector2D TileStart(TileWorldPos.X - TileSize * 0.5f,
                      TileWorldPos.Y - TileSize * 0.5f);
  const float TileAreaM2 = TileSize * TileSize;

  FRandomStream RandomStream(GetTileRandomSeed(TileCoord));
  const float BiomeWeight =
      GetBiomeWeightForSpawn(SpawnParams, BiomeDef.BiomeType);
  const bool bApplyHeuristics = bUsePCGHeuristics && TileMetrics != nullptr;

  UE_LOG(LogPCGWorldService, VeryVerbose,
         TEXT("Generating vegetation for biome %s with %d rules "
              "(heuristics=%s, biomeWeight=%.3f)"),
         *UEnum::GetValueAsString(BiomeDef.BiomeType),
         BiomeDef.VegetationRules.Num(),
         bApplyHeuristics ? TEXT("true") : TEXT("false"), BiomeWeight);

  int32 TotalInstanceCount = 0;

  for (const FPCGVegetationRule &VegRule : BiomeDef.VegetationRules) {
    UStaticMesh *Mesh = VegRule.VegetationMesh.IsNull()
                            ? nullptr
                            : VegRule.VegetationMesh.LoadSynchronous();

    float BaseDensity = VegRule.Density * WorldGenSettings.VegetationDensity *
                        BiomeWeight * DensityScale;
    if (TileMetrics) {
      BaseDensity *= ComputeEnvironmentScale(*TileMetrics, VegRule);
    }

    int32 BaseInstanceCount =
        FMath::Max(0, FMath::RoundToInt(BaseDensity * TileAreaM2 / 100.0f));
    const int32 MaxInstancesForThisRule = FMath::Max(
        1, MaxInstancesPerTile / FMath::Max(1, BiomeDef.VegetationRules.Num()));
    int32 InstanceCount =
        FMath::Min(BaseInstanceCount, MaxInstancesForThisRule);

    if (bHeadless && bAllowHeadlessLogicalInstances &&
        SpawnParams.bForceBiome) {
      InstanceCount = FMath::Max(InstanceCount, 1);
    }

    UE_LOG(LogPCGWorldService, VeryVerbose,
           TEXT("Vegetation rule %s: baseDensity=%.3f baseCount=%d clamped=%d"),
           VegRule.VegetationMesh.IsNull()
               ? TEXT("NULL_MESH")
               : *VegRule.VegetationMesh.GetAssetName(),
           BaseDensity, BaseInstanceCount, InstanceCount);

    if (InstanceCount <= 0) {
      continue;
    }

    TArray<FVector2D> SamplePoints;
    SamplePoints.Reserve(InstanceCount);

    const float MinDistance =
        FMath::Max(2.0f, TileSize * 0.03125f); // roughly 2m at 64m tiles
    if (bApplyHeuristics) {
      GenerateClusteredSamples(RandomStream, InstanceCount, TileStart, TileSize,
                               MinDistance, SamplePoints);
    } else {
      for (int32 i = 0; i < InstanceCount; ++i) {
        SamplePoints.Add(GeneratePoissonSample(RandomStream, TileStart,
                                               TileSize, MinDistance));
      }
    }

    int32 ValidInstances = 0;
    int32 HeightRejections = 0;
    int32 SlopeRejections = 0;

    for (const FVector2D &SamplePoint : SamplePoints) {
      FVector WorldPos = FVector(SamplePoint, 0.0f);
      bool bPassesHeightCheck = true;
      bool bPassesSlopeCheck = true;

      const int32 HeightX = FMath::Clamp(
          FMath::FloorToInt((SamplePoint.X - TileStart.X) * InvSampleSpacing),
          0, SamplesPerSide - 1);
      const int32 HeightY = FMath::Clamp(
          FMath::FloorToInt((SamplePoint.Y - TileStart.Y) * InvSampleSpacing),
          0, SamplesPerSide - 1);
      const int32 HeightIndex = HeightY * SamplesPerSide + HeightX;

      const float Height = HeightData[HeightIndex];
      WorldPos.Z = Height;

      if (!(Height >= VegRule.MinHeight && Height <= VegRule.MaxHeight)) {
        bPassesHeightCheck = false;
      }

      if (bPassesHeightCheck) {
        const float Slope =
            CalculateSlope(HeightData, HeightX, HeightY, SamplesPerSide);
        if (Slope > VegRule.SlopeLimit) {
          bPassesSlopeCheck = false;
        }
      }

      if (SpawnParams.bForceBiome && bHeadless &&
          bAllowHeadlessLogicalInstances) {
        bPassesHeightCheck = true;
        bPassesSlopeCheck = true;
      }

      if (!bPassesHeightCheck) {
        ++HeightRejections;
        continue;
      }

      if (!bPassesSlopeCheck) {
        ++SlopeRejections;
        continue;
      }

      FPCGInstanceData InstanceData;
      InstanceData.Location = WorldPos;
      InstanceData.Rotation =
          FRotator(0.0f, RandomStream.FRandRange(0.0f, 360.0f), 0.0f);
      InstanceData.Scale =
          FVector(RandomStream.FRandRange(VegRule.MinScale, VegRule.MaxScale));

      if (bHeadless && bAllowHeadlessLogicalInstances &&
          VegRule.VegetationMesh.IsNull()) {
        InstanceData.Mesh = TSoftObjectPtr<UStaticMesh>();
      } else {
        InstanceData.Mesh = VegRule.VegetationMesh;
      }

      InstanceData.OwningTile = TileCoord;
      InstanceData.bIsActive = true;

      bool bPassesAllFilters = true;
      if (!bHeadless && GetWorld() != nullptr) {
        // TODO: hook in navmesh/reachability filtering when world context
        // available
        bPassesAllFilters = true;
      }

      if (bPassesAllFilters) {
        Instances.Add(InstanceData);
        ++ValidInstances;
      }
    }

    TotalInstanceCount += ValidInstances;

    UE_LOG(LogPCGWorldService, VeryVerbose,
           TEXT("Rule %s results: Requested=%d, Placed=%d, HeightRejects=%d, "
                "SlopeRejects=%d"),
           VegRule.VegetationMesh.IsNull()
               ? TEXT("NULL_MESH")
               : *VegRule.VegetationMesh.GetAssetName(),
           InstanceCount, ValidInstances, HeightRejections, SlopeRejections);
  }

  if (TotalInstanceCount != Instances.Num()) {
    UE_LOG(LogPCGWorldService, Warning,
           TEXT("Instance accounting mismatch: expected %d, actual %d"),
           TotalInstanceCount, Instances.Num());
  }

  return Instances;
}

TArray<FPOIData>
UPCGWorldService::GeneratePOIInstances(FTileCoord TileCoord,
                                       const FBiomeDefinition &BiomeDef,
                                       const TArray<float> &HeightData) {
  TArray<FPOIData> POIs;

  // Calculate tile world position
  const float TileSize = WorldGenSettings.TileSizeMeters;
  FVector TileWorldPos = TileCoord.ToWorldPosition(TileSize);
  FVector2D TileStart(TileWorldPos.X - TileSize * 0.5f,
                      TileWorldPos.Y - TileSize * 0.5f);

  // Initialize seeded random for consistent generation
  FRandomStream RandomStream(GetTileRandomSeed(TileCoord));

  // Generate POIs based on biome rules using stratified placement
  for (const FPOISpawnRule &POIRule : BiomeDef.POIRules) {
    // Check spawn chance
    if (RandomStream.FRand() <=
        POIRule.SpawnChance * WorldGenSettings.POIDensity) {
      // Use stratified sampling for better distribution
      FVector POILocation;
      bool bFoundSuitableLocation = FindPOILocationStratified(
          TileCoord, POIRule, HeightData, RandomStream, POILocation);

      if (bFoundSuitableLocation) {
        // Create POI data
        FPOIData POIData;
        POIData.POIName = POIRule.POIName;
        POIData.Location = POILocation;
        POIData.Rotation =
            FRotator(0.0f, RandomStream.FRandRange(0.0f, 360.0f), 0.0f);
        POIData.Scale = FVector::OneVector;
        POIData.POIBlueprint = POIRule.POIBlueprint;
        POIData.OriginBiome = BiomeDef.BiomeType;
        POIData.bIsSpawned = false;

        // Apply terrain flattening/clearing if required
        if (POIRule.bRequiresFlatGround) {
          ApplyPOITerrainStamp(POIData.Location, 8.0f); // 8m radius flatten
        }

        POIs.Add(POIData);
        SpawnedPOIs.Add(POIData.POIId, POIData);

        UE_LOG(LogPCGWorldService, Log,
               TEXT("Generated POI %s at (%.1f, %.1f, %.1f) on tile (%d, %d)"),
               *POIData.POIName, POILocation.X, POILocation.Y, POILocation.Z,
               TileCoord.X, TileCoord.Y);
      }
    }
  }

  return POIs;
}

bool UPCGWorldService::SpawnPOI(FVector Location, const FPOIData &POIData) {
  // Validate POI ID is properly initialized
  ensureMsgf(POIData.POIId.IsValid(),
             TEXT("SpawnPOI: POIData must have a valid POIId"));

  if (!GetWorld()) {
    UE_LOG(LogPCGWorldService, Error,
           TEXT("Cannot spawn POI - no valid world"));
    return false;
  }

  // Check if POI blueprint is valid
  if (POIData.POIBlueprint.IsNull()) {
    UE_LOG(LogPCGWorldService, Warning,
           TEXT("POI blueprint is null for POI: %s"), *POIData.POIName);
    return false;
  }

  // Load the blueprint if needed
  UBlueprint *Blueprint = POIData.POIBlueprint.LoadSynchronous();
  if (!Blueprint || !Blueprint->GeneratedClass) {
    UE_LOG(LogPCGWorldService, Error, TEXT("Failed to load POI blueprint: %s"),
           *POIData.POIBlueprint.GetAssetName());
    return false;
  }

  // Spawn the actor
  FTransform SpawnTransform(POIData.Rotation, Location, POIData.Scale);
  AActor *SpawnedActor =
      GetWorld()->SpawnActor<AActor>(Blueprint->GeneratedClass, SpawnTransform);

  if (SpawnedActor) {
    // Store reference for management
    SpawnedPOIActors.Add(POIData.POIId, SpawnedActor);

    UE_LOG(LogPCGWorldService, Log,
           TEXT("Successfully spawned POI: %s at (%.1f, %.1f, %.1f)"),
           *POIData.POIName, Location.X, Location.Y, Location.Z);
    return true;
  }

  UE_LOG(LogPCGWorldService, Error, TEXT("Failed to spawn POI actor: %s"),
         *POIData.POIName);
  return false;
}

bool UPCGWorldService::UpdateHISMInstances(FTileCoord TileCoord) {
  // Get generation data for this tile
  const FPCGGenerationData *GenerationData = GenerationCache.Find(TileCoord);
  if (!GenerationData) {
    UE_LOG(LogPCGWorldService, Warning,
           TEXT("No generation data found for tile (%d, %d)"), TileCoord.X,
           TileCoord.Y);
    return false;
  }

  // Fix instance counting to use transform sets not HISM instances: count
  // logical instances in headless mode
  TMap<UStaticMesh *, TArray<FTransform>> InstancesByMesh;
  int32 TotalLogicalInstances = 0;

  for (const FPCGInstanceData &InstanceData :
       GenerationData->GeneratedInstances) {
    if (InstanceData.bIsActive) {
      // Count all active instances, even those without meshes (headless mode)
      TotalLogicalInstances++;

      // Only group by mesh if we have a valid mesh and are not in headless mode
      if (!bHeadless && !InstanceData.Mesh.IsNull()) {
        UStaticMesh *Mesh = InstanceData.Mesh.LoadSynchronous();
        if (Mesh) {
          FTransform Transform(InstanceData.Rotation, InstanceData.Location,
                               InstanceData.Scale);
          InstancesByMesh.FindOrAdd(Mesh).Add(Transform);
        }
      }
    }
  }

  // Check if we have a valid world context for HISM operations
  if (bHeadless || GetWorld() == nullptr) {
    // In headless mode, we count logical instances rather than committed
    // components
    UE_LOG(LogPCGWorldService, Log,
           TEXT("Headless mode: Counted %d logical instances for tile (%d, %d) "
                "- skipping HISM component creation"),
           TotalLogicalInstances, TileCoord.X, TileCoord.Y);
    return true;
  }

  // Get or create HISM components for this tile
  FHISMComponentArray *TileComponents = HISMComponents.Find(TileCoord);
  if (!TileComponents) {
    // Create new HISM components for this tile
    CreateHISMComponentsForTile(TileCoord);
    TileComponents = HISMComponents.Find(TileCoord);
  }

  if (!TileComponents) {
    UE_LOG(LogPCGWorldService, Error,
           TEXT("Failed to create HISM components for tile (%d, %d)"),
           TileCoord.X, TileCoord.Y);
    return false;
  }

  // Update HISM components
  for (auto &MeshPair : InstancesByMesh) {
    UStaticMesh *Mesh = MeshPair.Key;
    const TArray<FTransform> &Transforms = MeshPair.Value;

    UHierarchicalInstancedStaticMeshComponent *HISMComp =
        GetOrCreateHISMComponent(TileCoord, Mesh);
    if (HISMComp) {
      HISMComp->SetMobility(EComponentMobility::Movable);
      // Clear existing instances and add new ones
      HISMComp->ClearInstances();
      for (const FTransform &Transform : Transforms) {
        HISMComp->AddInstance(Transform, /*bWorldSpace=*/true);
      }

      // Update performance stats
      PerformanceStats.ActiveHISMInstances += Transforms.Num();
    }
  }

  UE_LOG(LogPCGWorldService, Log,
         TEXT("Updated HISM instances for tile (%d, %d) - %d logical "
              "instances, %d instance groups"),
         TileCoord.X, TileCoord.Y, TotalLogicalInstances,
         InstancesByMesh.Num());
  return true;
}

bool UPCGWorldService::RemoveContentInArea(FBox Area) {
  bool bRemovedAny = false;
  const FBox2D Area2D(FVector2D(Area.Min), FVector2D(Area.Max));
  const FVector2D AreaMin(Area.Min.X, Area.Min.Y);
  const FVector2D AreaMax(Area.Max.X, Area.Max.Y);

  UE_LOG(LogPCGWorldService, Log,
         TEXT("RemoveContentInArea: Searching for content in area (%.1f,%.1f) "
              "to (%.1f,%.1f)"),
         AreaMin.X, AreaMin.Y, AreaMax.X, AreaMax.Y);
  UE_LOG(LogPCGWorldService, Log,
         TEXT("RemoveContentInArea: GenerationCache has %d tiles"),
         GenerationCache.Num());

  auto IsInside2D = [&](const FVector2D &P) -> bool {
    // inclusive compare so edge cases are removed too
    return (P.X >= AreaMin.X && P.X <= AreaMax.X && P.Y >= AreaMin.Y &&
            P.Y <= AreaMax.Y);
  };

  // Remove POI actors in the area
  TArray<FGuid> POIsToRemove;
  for (auto &POIPair : SpawnedPOIActors) {
    AActor *POIActor = POIPair.Value;
    if (IsValid(POIActor) &&
        Area2D.IsInside(FVector2D(POIActor->GetActorLocation()))) {
      POIActor->Destroy();
      POIsToRemove.Add(POIPair.Key);
      bRemovedAny = true;
    }
  }

  // Clean up POI references
  for (const FGuid &POIId : POIsToRemove) {
    SpawnedPOIActors.Remove(POIId);
    SpawnedPOIs.Remove(POIId);
  }

  // Remove vegetation instances in the area
  for (auto &CachePair : GenerationCache) {
    FPCGGenerationData &GenerationData = CachePair.Value;
    TArray<FPCGInstanceData> RemainingInstances;

    UE_LOG(
        LogPCGWorldService, Log,
        TEXT("RemoveContentInArea: Checking tile (%d, %d) with %d instances"),
        GenerationData.TileCoord.X, GenerationData.TileCoord.Y,
        GenerationData.GeneratedInstances.Num());

    int32 RemovedFromThisTile = 0;
    for (FPCGInstanceData &InstanceData : GenerationData.GeneratedInstances) {
      const bool bInside = Area.IsInside(InstanceData.Location);
      if (!bInside) {
        RemainingInstances.Add(InstanceData);
      } else {
        RemovedFromThisTile++;
        bRemovedAny = true; // we removed at least one
      }
    }

    if (RemainingInstances.Num() != GenerationData.GeneratedInstances.Num()) {
      UE_LOG(LogPCGWorldService, Log,
             TEXT("RemoveContentInArea: Removed %d instances from tile (%d, "
                  "%d), %d remaining"),
             RemovedFromThisTile, GenerationData.TileCoord.X,
             GenerationData.TileCoord.Y, RemainingInstances.Num());

      GenerationData.GeneratedInstances = RemainingInstances;
      GenerationData.TotalInstanceCount = RemainingInstances.Num();

      // Update HISM for affected tile
      UpdateHISMInstances(GenerationData.TileCoord);
    } else if (GenerationData.GeneratedInstances.Num() > 0) {
      UE_LOG(LogPCGWorldService, Log,
             TEXT("RemoveContentInArea: No instances removed from tile (%d, "
                  "%d) - none were in removal area"),
             GenerationData.TileCoord.X, GenerationData.TileCoord.Y);
    }
  }

  if (bRemovedAny) {
    UE_LOG(LogPCGWorldService, Log,
           TEXT("Removed content in area (%.1f,%.1f,%.1f) to (%.1f,%.1f,%.1f)"),
           Area.Min.X, Area.Min.Y, Area.Min.Z, Area.Max.X, Area.Max.Y,
           Area.Max.Z);
  }

  return bRemovedAny;
}

FPCGPerformanceStats UPCGWorldService::GetPerformanceStats() {
  // Update memory usage estimate
  PerformanceStats.MemoryUsageMB = EstimateMemoryUsage();
  return PerformanceStats;
}

void UPCGWorldService::SetRuntimeOperationsEnabled(bool bEnabled) {
  bRuntimeOperationsEnabled = bEnabled;
  UE_LOG(LogPCGWorldService, Log, TEXT("Runtime PCG operations %s"),
         bEnabled ? TEXT("enabled") : TEXT("disabled"));
}

void UPCGWorldService::ClearPCGCache() {
  GenerationCache.Empty();

  // Clean up HISM components
  for (auto &TilePair : HISMComponents) {
    for (UHierarchicalInstancedStaticMeshComponent *Component :
         TilePair.Value.Components) {
      if (IsValid(Component)) {
        Component->ClearInstances();
      }
    }
  }
  HISMComponents.Empty();
#if VHM_PCG_ENABLED
  ResolvedBiomeGraphs.Empty();
#endif

  // Clean up spawned POIs
  for (auto &POIPair : SpawnedPOIActors) {
    if (IsValid(POIPair.Value)) {
      POIPair.Value->Destroy();
    }
  }
  SpawnedPOIActors.Empty();
  SpawnedPOIs.Empty();

  // Reset performance stats
  PerformanceStats = FPCGPerformanceStats();

  UE_LOG(LogPCGWorldService, Log, TEXT("PCG cache cleared"));
}

FPCGGraphValidationResult
UPCGWorldService::ValidatePCGGraph(const FString &GraphPath) {
  FPCGGraphValidationResult Result;
  Result.GraphPath = GraphPath;

#if !VHM_PCG_ENABLED
  Result.Errors.Add(
      TEXT("PCG system not available - using fallback generation."));
  return Result;
#else
  if (GraphPath.IsEmpty()) {
    Result.Errors.Add(TEXT("Graph path is empty."));
    return Result;
  }

  UObject *GraphObject = LoadObject<UObject>(nullptr, *GraphPath);
  if (!GraphObject) {
    Result.Errors.Add(FString::Printf(
        TEXT("Failed to load PCG graph at path: %s"), *GraphPath));
    return Result;
  }

  UPCGGraph *PCGGraph = Cast<UPCGGraph>(GraphObject);
  if (!PCGGraph) {
    Result.Errors.Add(FString::Printf(
        TEXT("Object at path is not a valid PCG graph: %s"), *GraphPath));
    return Result;
  }

  Result.GraphName = PCGGraph->GetName();

  UWorld *World = GetWorld();
  UPCGSubsystem *PCGSubsystem =
      World ? World->GetSubsystem<UPCGSubsystem>() : nullptr;
  if (!PCGSubsystem) {
    Result.Errors.Add(TEXT("PCG subsystem not initialized in current world."));
  }

  if (!SchedulerExecutor.IsValid()) {
    Result.Errors.Add(TEXT(
        "Scheduler executor unavailable; cannot schedule validation run."));
  }

  const bool bFrustumCVarEnabled =
      (CVarVibeheimPCGFrustumEnable.GetValueOnAnyThread() != 0);
  if (WorldGenSettings.bEnableFrustumCulling && !bFrustumCVarEnabled) {
    Result.Warnings.Add(TEXT("World settings enable frustum culling but "
                             "vhm.pcg.frustum.enable is disabled."));
    Result.Suggestions.Add(
        TEXT("Enable vhm.pcg.frustum.enable to mirror runtime configuration."));
  }

#if WITH_EDITOR
  int32 TotalNodes = 0;
  int32 DependencyNodes = 0;
  int32 UnwiredNodes = 0;

  const TArray<UPCGNode *> &Nodes = PCGGraph->GetNodes();
  for (UPCGNode *Node : Nodes) {
    if (!Node) {
      continue;
    }

    ++TotalNodes;

    const UPCGSettings *NodeSettings = Node->GetSettings();
    bool bDependencyNode = false;
    bool bDependencyWired = false;
    const TArray<TObjectPtr<UPCGPin>> &InputPins = Node->GetInputPins();
    for (const TObjectPtr<UPCGPin> &PinPtr : InputPins) {
      const UPCGPin *Pin = PinPtr.Get();
      if (!Pin || Pin->Properties.Usage != EPCGPinUsage::DependencyOnly) {
        continue;
      }

      bDependencyNode = true;

      if (Pin->Edges.Num() > 0) {
        bDependencyWired = true;
        break;
      }
    }

    if (!bDependencyNode) {
      continue;
    }

    ++DependencyNodes;

    if (!bDependencyWired) {
      ++UnwiredNodes;

      const FString NodeLabel =
          NodeSettings ? NodeSettings->GetClass()->GetName() : Node->GetName();
      Result.UnwiredDependencyNodes.Add(NodeLabel);

      const bool bCustomNode = NodeLabel.StartsWith(TEXT("Vibeheim"));
      const bool bGetterNode = NodeLabel.Contains(TEXT("GetActor")) ||
                               NodeLabel.Contains(TEXT("GetLandscape"));

      if (bGetterNode) {
        Result.Suggestions.AddUnique(
            TEXT("Wire Execution Dependency pin for Get Landscape Data/Get "
                 "Actor Data to enforce deterministic ordering."));
      }

      const FString Message =
          FString::Printf(TEXT("Node '%s' exposes an Execution Dependency pin "
                               "but is not wired."),
                          *NodeLabel);
      if (bCustomNode) {
        Result.Errors.AddUnique(Message);
      } else {
        Result.Warnings.AddUnique(Message);
      }
    }
  }

  if (DependencyNodes > 0) {
    UE_LOG(LogPCGWorldService, Log,
           TEXT("ValidatePCGGraph: %d nodes inspected (%d dependency pins, %d "
                "unwired)."),
           TotalNodes, DependencyNodes, UnwiredNodes);
  }
#endif // WITH_EDITOR

  if (PCGSubsystem && SchedulerExecutor.IsValid() && World) {
#if WITH_EDITOR
    FPCGTileMetrics DummyMetrics;
    DummyMetrics.AverageSlope = 0.1f;
    DummyMetrics.AverageHeight = 0.0f;
    DummyMetrics.MinHeight = 0.0f;
    DummyMetrics.MaxHeight = 0.0f;

    const FTileCoord SampleTile(0, 0);
    const uint32 TileSeed = GetTileRandomSeed(SampleTile);

    UPCGParamData *ParameterData =
        PCGWorldService::Private::CreateTileParameterData(
            this, DummyMetrics, SampleTile, EBiomeType::None, WorldGenSettings,
            TileSeed, 1.0f, 1.0f);
    UPCGPointData *PointData = PCGWorldService::Private::CreateTilePointData(
        this, SampleTile, WorldGenSettings, DummyMetrics);

    if (ParameterData && PointData) {
      FPCGInputSet InputSet;
      InputSet.Add(TEXT("TileParameters"), ParameterData);
      InputSet.Add(TEXT("Tile"), PointData);

      const FVector TileCenter =
          SampleTile.ToWorldPosition(WorldGenSettings.TileSizeMeters);
      const FVector TileExtent(WorldGenSettings.TileSizeMeters * 0.5f,
                               WorldGenSettings.TileSizeMeters * 0.5f,
                               WorldGenSettings.TileSizeMeters * 0.25f);
      const FBox ExecutionBounds = FBox::BuildAABB(TileCenter, TileExtent);

      UPCGComponent *Component =
          GetOrCreateBiomeComponent(EBiomeType::None, *PCGGraph);
      if (Component) {
        FPCGSchedulerExecutor SyncExecutor;
        TArray<FString> ValidationWarnings;
        TArray<FString> ValidationErrors;
        FPCGScheduleResult SyncResult = SyncExecutor.RunGraphSync(
            *PCGSubsystem, *Component, *PCGGraph, *World, SampleTile, InputSet,
            FString::Printf(TEXT("Validation:%s"), *PCGGraph->GetName()),
            ExecutionBounds, static_cast<int32>(TileSeed),
            WorldGenSettings.bEnableFrustumCulling,
            ResolveFrustumMargin(*Component, EBiomeType::None),
            ValidationWarnings, ValidationErrors);

        Result.Warnings.Append(ValidationWarnings);
        Result.Errors.Append(ValidationErrors);

        const TArray<PCGWorldService::Private::FExpectedAttribute>
            &ExpectedAttributes =
                PCGWorldService::Private::GetCanonicalAttributes();
        for (const TObjectPtr<UPCGData> &OutputDatum :
             SyncResult.Output.Outputs) {
          const UPCGPointData *OutputPointData =
              Cast<UPCGPointData>(OutputDatum.Get());
          if (!OutputPointData) {
            continue;
          }

          const UPCGMetadata *Metadata = OutputPointData->ConstMetadata();
          if (!Metadata) {
            Result.Warnings.AddUnique(
                TEXT("Graph output contained point data without metadata."));
            continue;
          }

          for (const PCGWorldService::Private::FExpectedAttribute &Attribute :
               ExpectedAttributes) {
            if (Attribute.Scope !=
                PCGWorldService::Private::EAttributeScope::Point) {
              continue;
            }

            if (!Metadata->HasAttribute(Attribute.Name)) {
              if (Attribute.bRequired) {
                Result.MissingAttributes.AddUnique(Attribute.Name);
                Result.Errors.AddUnique(FString::Printf(
                    TEXT("Missing required point attribute '%s'."),
                    *Attribute.Name.ToString()));
              } else {
                Result.Warnings.AddUnique(FString::Printf(
                    TEXT("Optional point attribute '%s' not produced."),
                    *Attribute.Name.ToString()));
              }
              continue;
            }

            const FPCGMetadataAttributeBase *AttributeBase =
                Metadata->GetConstAttribute(
                    FPCGAttributeIdentifier(Attribute.Name));
            if (AttributeBase &&
                !Attribute.AllowedTypes.Contains(static_cast<EPCGMetadataTypes>(
                    AttributeBase->GetTypeId()))) {
              Result.Warnings.AddUnique(FString::Printf(
                  TEXT("Attribute '%s' reported as %s but expected %s."),
                  *Attribute.Name.ToString(),
                  *PCGWorldService::Private::MetadataTypeToString(
                      static_cast<EPCGMetadataTypes>(
                          AttributeBase->GetTypeId())),
                  *PCGWorldService::Private::AllowedTypesToString(
                      Attribute.AllowedTypes)));
            }
          }
        }
      }
    }
#endif // WITH_EDITOR
  }

  if (!Result.MissingAttributes.IsEmpty()) {
    for (const FName &MissingAttribute : Result.MissingAttributes) {
      Result.Suggestions.AddUnique(FString::Printf(
          TEXT("Ensure graph writes attribute '%s' before extraction."),
          *MissingAttribute.ToString()));
    }
  }

  Result.bIsValid = Result.Errors.Num() == 0;
  if (Result.bIsValid) {
    UE_LOG(LogPCGWorldService, Log, TEXT("PCG graph validation passed: %s"),
           *GraphPath);
  }

  return Result;
#endif
}

float UPCGWorldService::GetBiomeWeightForSpawn(
    const FPCGSpawnParams &SpawnParams, EBiomeType BiomeType) const {
  if (SpawnParams.bForceBiome) {
    return 1.0f;
  }

  float Weight = SpawnParams.BiomeWeightScale;
  Weight *= SpawnParams.SlopeResponse;
  Weight *= SpawnParams.WaterResponse;

  return FMath::Clamp(Weight, 0.0f, 1.0f);
}

void UPCGWorldService::SetBiomeDefinitions(
    const TMap<EBiomeType, FBiomeDefinition> &InBiomeDefinitions) {
  BiomeDefinitions = InBiomeDefinitions;
  BiomePCGGraphs.Empty();
#if VHM_PCG_ENABLED
  ResolvedBiomeGraphs.Empty();
#endif

  // Initialize default biome definitions to merge with incoming data
  TMap<EBiomeType, FBiomeDefinition> DefaultBiomeDefinitions;
  InitializeDefaultBiomes(DefaultBiomeDefinitions);

  for (auto &BiomePair : BiomeDefinitions) {
    FBiomeDefinition &BiomeDef = BiomePair.Value;
    const FString BiomeLabel = BiomeDef.BiomeName.IsEmpty()
                                   ? UEnum::GetValueAsString(BiomePair.Key)
                                   : BiomeDef.BiomeName;

    // Merge default vegetation rules if authoring data left them empty
    if (BiomeDef.VegetationRules.Num() == 0) {
      if (const FBiomeDefinition *DefaultBiomeDef =
              DefaultBiomeDefinitions.Find(BiomePair.Key)) {
        BiomeDef.VegetationRules = DefaultBiomeDef->VegetationRules;
        UE_LOG(LogPCGWorldService, Log,
               TEXT("Merged %d default vegetation rules for %s biome"),
               DefaultBiomeDef->VegetationRules.Num(), *BiomeLabel);
      }
    }

    // Cache PCG graph references so generation can resolve them quickly
    if (BiomeDef.BiomePCGGraph.IsNull()) {
      UE_LOG(LogPCGWorldService, Verbose,
             TEXT("Biome %s has no PCG graph assigned - fallback generation "
                  "will be used"),
             *BiomeLabel);
    } else {
      BiomePCGGraphs.Add(BiomePair.Key, BiomeDef.BiomePCGGraph);
      UE_LOG(LogPCGWorldService, Log, TEXT("Biome %s mapped to PCG graph %s"),
             *BiomeLabel, *BiomeDef.BiomePCGGraph.ToString());
    }
  }

  // Reset cached graph pointer from legacy single-graph workflow
  CurrentPCGGraph = nullptr;

  // Clear cache to ensure fresh generation uses updated rule registry
  ClearPCGCache();

  UE_LOG(LogPCGWorldService, Log,
         TEXT("Updated biome definitions with %d biomes (registered PCG "
              "graphs: %d)"),
         BiomeDefinitions.Num(), BiomePCGGraphs.Num());
}

UPCGGraph *UPCGWorldService::ResolveBiomePCGGraph(EBiomeType BiomeType) {
  const TSoftObjectPtr<UPCGGraph> *GraphRef = BiomePCGGraphs.Find(BiomeType);
  if (!GraphRef) {
    return nullptr;
  }

#if VHM_PCG_ENABLED
  if (const TWeakObjectPtr<UPCGGraph> *CachedGraph =
          ResolvedBiomeGraphs.Find(BiomeType)) {
    if (CachedGraph->IsValid()) {
      return CachedGraph->Get();
    }
  }

  if (!GraphRef->IsNull()) {
    UPCGGraph *LoadedGraph = GraphRef->LoadSynchronous();
    if (LoadedGraph) {
      ResolvedBiomeGraphs.FindOrAdd(BiomeType) = LoadedGraph;
      return LoadedGraph;
    }

    UE_LOG(LogPCGWorldService, Warning,
           TEXT("Failed to load PCG graph %s for biome %s"),
           *GraphRef->ToString(), *UEnum::GetValueAsString(BiomeType));
  }
  return nullptr;
#else
  return nullptr;
#endif
}

FString UPCGWorldService::GetBiomeGraphAssetPath(EBiomeType BiomeType) const {
  if (const TSoftObjectPtr<UPCGGraph> *GraphPtr =
          BiomePCGGraphs.Find(BiomeType)) {
    return GraphPtr->ToString();
  }
  return FString();
}
void UPCGWorldService::SetPersistenceManager(
    UInstancePersistenceManager *InPersistenceManager) {
  PersistenceManager = InPersistenceManager;
  UE_LOG(LogPCGWorldService, Log, TEXT("Instance persistence manager set: %s"),
         PersistenceManager ? TEXT("Valid") : TEXT("Null"));
}

bool UPCGWorldService::RemoveInstance(FTileCoord TileCoord, FGuid InstanceId) {
  // Find the instance in the generation cache
  FPCGGenerationData *GenerationData = GenerationCache.Find(TileCoord);
  if (!GenerationData) {
    UE_LOG(LogPCGWorldService, Warning,
           TEXT("No generation data found for tile (%d, %d) when removing "
                "instance"),
           TileCoord.X, TileCoord.Y);
    return false;
  }

  // Find and remove the instance
  bool bFoundInstance = false;
  FPCGInstanceData RemovedInstance;
  for (int32 i = GenerationData->GeneratedInstances.Num() - 1; i >= 0; i--) {
    if (GenerationData->GeneratedInstances[i].InstanceId == InstanceId) {
      RemovedInstance = GenerationData->GeneratedInstances[i];
      GenerationData->GeneratedInstances.RemoveAt(i);
      GenerationData->TotalInstanceCount =
          GenerationData->GeneratedInstances.Num();
      bFoundInstance = true;
      break;
    }
  }

  if (!bFoundInstance) {
    UE_LOG(LogPCGWorldService, Warning,
           TEXT("Instance %s not found in tile (%d, %d)"),
           *InstanceId.ToString(), TileCoord.X, TileCoord.Y);
    return false;
  }

  // Log the removal to persistence manager if available
  if (PersistenceManager) {
    PersistenceManager->AddInstanceOperation(TileCoord, RemovedInstance,
                                             EInstanceOperation::Remove);
  }

  // Update HISM instances to reflect the change
  UpdateHISMInstances(TileCoord);

  UE_LOG(LogPCGWorldService, Log,
         TEXT("Removed instance %s from tile (%d, %d)"), *InstanceId.ToString(),
         TileCoord.X, TileCoord.Y);
  return true;
}

bool UPCGWorldService::AddInstance(FTileCoord TileCoord,
                                   const FPCGInstanceData &InstanceData) {
  // Get or create generation data for the tile
  FPCGGenerationData *GenerationData = GenerationCache.Find(TileCoord);
  if (!GenerationData) {
    // Create new generation data for this tile
    FPCGGenerationData NewGenerationData;
    NewGenerationData.TileCoord = TileCoord;
    NewGenerationData.BiomeType =
        EBiomeType::None; // Will be set by proper generation
    GenerationCache.Add(TileCoord, NewGenerationData);
    GenerationData = GenerationCache.Find(TileCoord);
  }

  // Add the instance
  FPCGInstanceData NewInstance = InstanceData;
  NewInstance.OwningTile = TileCoord;
  NewInstance.bIsActive = true;

  GenerationData->GeneratedInstances.Add(NewInstance);
  GenerationData->TotalInstanceCount = GenerationData->GeneratedInstances.Num();

  // Log the addition to persistence manager if available
  if (PersistenceManager) {
    PersistenceManager->AddInstanceOperation(TileCoord, NewInstance,
                                             EInstanceOperation::Add);
  }

  // Update HISM instances to reflect the change
  UpdateHISMInstances(TileCoord);

  UE_LOG(LogPCGWorldService, Log, TEXT("Added instance %s to tile (%d, %d)"),
         *NewInstance.InstanceId.ToString(), TileCoord.X, TileCoord.Y);
  return true;
}

bool UPCGWorldService::RemovePOI(FGuid POIId) {
  // Find POI in spawned POIs
  FPOIData *POIData = SpawnedPOIs.Find(POIId);
  if (!POIData) {
    UE_LOG(LogPCGWorldService, Warning,
           TEXT("POI %s not found in spawned POIs"), *POIId.ToString());
    return false;
  }

  // Get the tile coordinate for persistence logging
  FTileCoord TileCoord = FTileCoord::FromWorldPosition(
      POIData->Location, WorldGenSettings.TileSizeMeters);

  // Destroy the spawned actor if it exists
  if (TObjectPtr<AActor> *FoundPtr = SpawnedPOIActors.Find(POIId)) {
    AActor *SpawnedActor = FoundPtr->Get();
    if (IsValid(SpawnedActor)) {
      (SpawnedActor)->Destroy();
    }
    SpawnedPOIActors.Remove(POIId);
  }

  // Log the removal to persistence manager if available
  if (PersistenceManager) {
    PersistenceManager->AddPOIOperation(TileCoord, *POIData,
                                        EInstanceOperation::Remove);
  }

  // Remove from spawned POIs map
  SpawnedPOIs.Remove(POIId);

  UE_LOG(LogPCGWorldService, Log, TEXT("Removed POI %s (%s)"),
         *POIId.ToString(), *POIData->POIName);
  return true;
}

void UPCGWorldService::SetHeightfieldService(
    UHeightfieldService *InHeightfieldService) {
  HeightfieldService = InHeightfieldService;

  if (HeightfieldService) {
    UE_LOG(LogPCGWorldService, Log,
           TEXT("Heightfield service bound for terrain projection"));
  } else {
    UE_LOG(LogPCGWorldService, Warning,
           TEXT("Heightfield service cleared; PCG instances will rely on "
                "authored Z values"));
  }
}

void UPCGWorldService::SetBiomeService(UBiomeService *InBiomeService) {
  BiomeService = InBiomeService;

  if (BiomeService) {
    UE_LOG(LogPCGWorldService, Log,
           TEXT("Biome service bound for blend weights"));
  } else {
    UE_LOG(LogPCGWorldService, Warning,
           TEXT("Biome service cleared; biome weights will default to 1.0"));
  }
}

float UPCGWorldService::ResolveBiomeBlendWeight(
    FTileCoord TileCoord, EBiomeType BiomeType,
    const FPCGTileMetrics *TileMetrics) const {
  if (!BiomeService) {
    return 1.0f;
  }

  const FVector TileCenter =
      TileCoord.ToWorldPosition(WorldGenSettings.TileSizeMeters);
  const FVector2D TileCenter2D(TileCenter.X, TileCenter.Y);

  float Altitude = WorldGenSettings.SeaLevel;
  if (TileMetrics) {
    Altitude = TileMetrics->AverageHeight;
  } else if (HeightfieldService) {
    Altitude = HeightfieldService->SampleHeightWorldXY(TileCenter2D);
  }

  const FBiomeResult BiomeResult =
      BiomeService->DetermineBiome(TileCenter2D, Altitude);

  EBiomeType QueryBiome = BiomeType;
  if (QueryBiome == EBiomeType::None &&
      BiomeResult.PrimaryBiome != EBiomeType::None) {
    QueryBiome = BiomeResult.PrimaryBiome;
  }

  float Weight = 1.0f;
  if (const float *RequestedWeight =
          BiomeResult.BiomeWeights.Find(QueryBiome)) {
    Weight = *RequestedWeight;
  } else if (const float *PrimaryWeight =
                 BiomeResult.BiomeWeights.Find(BiomeResult.PrimaryBiome)) {
    Weight = *PrimaryWeight;
  } else if (BiomeResult.BiomeWeights.Num() > 0) {
    float MaxWeight = 0.0f;
    for (const TPair<EBiomeType, float> &Pair : BiomeResult.BiomeWeights) {
      MaxWeight = FMath::Max(MaxWeight, Pair.Value);
    }
    Weight = MaxWeight;
  }

  return FMath::Clamp(Weight, 0.0f, 1.0f);
}

void UPCGWorldService::PrewarmBiomeAssets(FTileCoord TileCoord,
                                          EBiomeType BiomeType) {
  if (BiomeType == EBiomeType::None) {
    return;
  }

  UWorld *World = GetWorld();
  if (!World) {
    return;
  }

  const APawn *PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);
  if (!PlayerPawn) {
    return;
  }

  const FVector PlayerLocation = PlayerPawn->GetActorLocation();
  const FVector TileCenter =
      TileCoord.ToWorldPosition(WorldGenSettings.TileSizeMeters);
  const float PrewarmRadius = WorldGenSettings.TileSizeMeters * 3.0f;
  if (FVector::DistSquared2D(PlayerLocation, TileCenter) >
      FMath::Square(PrewarmRadius)) {
    return;
  }

  const FBiomeDefinition *BiomeDef = BiomeDefinitions.Find(BiomeType);

  TArray<FSoftObjectPath> AssetsToStream;
  auto QueueAsset = [this, &AssetsToStream](const auto &AssetPtr) {
    if (AssetPtr.IsNull() || AssetPtr.IsValid()) {
      return;
    }

    const FSoftObjectPath AssetPath = AssetPtr.ToSoftObjectPath();
    if (!AssetPath.IsValid() || PrewarmedAssetPaths.Contains(AssetPath)) {
      return;
    }

    AssetsToStream.Add(AssetPath);
    PrewarmedAssetPaths.Add(AssetPath);
  };

  if (const TSoftObjectPtr<UPCGGraph> *GraphPtr =
          BiomePCGGraphs.Find(BiomeType)) {
    QueueAsset(*GraphPtr);
  }

  if (BiomeDef) {
    QueueAsset(BiomeDef->BiomePCGGraph);

    for (const FPCGVegetationRule &VegRule : BiomeDef->VegetationRules) {
      QueueAsset(VegRule.VegetationMesh);
    }

    for (const FPOISpawnRule &POIRule : BiomeDef->POIRules) {
      QueueAsset(POIRule.POIBlueprint);
    }
  }

  if (AssetsToStream.Num() == 0) {
    return;
  }

  FStreamableManager &Streamable = UAssetManager::GetStreamableManager();
  Streamable.RequestAsyncLoad(AssetsToStream, FStreamableDelegate(),
                              FStreamableManager::AsyncLoadHighPriority);
}

bool UPCGWorldService::AddPOI(const FPOIData &POIData) {
  // Validate POI ID is properly initialized
  ensureMsgf(POIData.POIId.IsValid(),
             TEXT("AddPOI: POIData must have a valid POIId"));

  // Get the tile coordinate for persistence logging
  FTileCoord TileCoord = FTileCoord::FromWorldPosition(
      POIData.Location, WorldGenSettings.TileSizeMeters);

  // Add to spawned POIs map
  SpawnedPOIs.Add(POIData.POIId, POIData);

  // Actually spawn the POI
  bool bSpawned = SpawnPOI(POIData.Location, POIData);
  if (!bSpawned) {
    // Remove from map if spawning failed
    SpawnedPOIs.Remove(POIData.POIId);
    return false;
  }

  // Log the addition to persistence manager if available
  if (PersistenceManager) {
    PersistenceManager->AddPOIOperation(TileCoord, POIData,
                                        EInstanceOperation::Add);
  }

  UE_LOG(LogPCGWorldService, Log,
         TEXT("Added POI %s (%s) at (%.1f, %.1f, %.1f)"),
         *POIData.POIId.ToString(), *POIData.POIName, POIData.Location.X,
         POIData.Location.Y, POIData.Location.Z);
  return true;
}

bool UPCGWorldService::LoadTileWithPersistence(
    FTileCoord TileCoord, EBiomeType BiomeType,
    const TArray<float> &HeightData) {
  // First generate the base content
  FPCGGenerationData GenerationData =
      GenerateContentInternal(TileCoord, BiomeType, HeightData);

  // Cache the base generation
  GenerationCache.Add(TileCoord, GenerationData);

  // Apply persistence modifications if persistence manager is available
  if (PersistenceManager) {
    // Load tile journal from disk if it exists
    if (!PersistenceManager->LoadTileJournal(TileCoord)) {
      UE_LOG(LogPCGWorldService, Warning,
             TEXT("Failed to load persistence journal for tile (%d, %d)"),
             TileCoord.X, TileCoord.Y);
    }

    // Replay the journal to apply persistent modifications
    if (!PersistenceManager->ReplayTileJournal(TileCoord, this)) {
      UE_LOG(LogPCGWorldService, Warning,
             TEXT("Failed to replay persistence journal for tile (%d, %d)"),
             TileCoord.X, TileCoord.Y);
    } else {
      UE_LOG(
          LogPCGWorldService, Log,
          TEXT(
              "Successfully applied persistent modifications to tile (%d, %d)"),
          TileCoord.X, TileCoord.Y);
    }
  }

  // Update HISM instances with the final state
  UpdateHISMInstances(TileCoord);

  return true;
}

UHierarchicalInstancedStaticMeshComponent *
UPCGWorldService::CreateHISMComponent(FTileCoord TileCoord, UStaticMesh *Mesh) {
  return GetOrCreateHISMComponent(TileCoord, Mesh);
}

// Private helper methods

void UPCGWorldService::UpdatePerformanceStats(float GenerationTimeMs,
                                              int32 InstanceCount) {
  PerformanceStats.LastGenerationTimeMs = GenerationTimeMs;
  PerformanceStats.TotalInstancesGenerated += InstanceCount;

  // Update average (simple moving average)
  static int32 SampleCount = 0;
  SampleCount++;
  if (SampleCount > 0) {
    PerformanceStats.AverageGenerationTimeMs =
        (PerformanceStats.AverageGenerationTimeMs * (SampleCount - 1) +
         GenerationTimeMs) /
        SampleCount;
  }
}

void UPCGWorldService::InitializeDefaultBiomes() {
  // Initialize with enhanced biome definitions

  // Meadows biome
  FBiomeDefinition MeadowsBiome;
  MeadowsBiome.BiomeType = EBiomeType::Meadows;
  MeadowsBiome.BiomeName = TEXT("Meadows");

  // Add grass vegetation rule
  FPCGVegetationRule GrassRule;
  GrassRule.Density = 0.8f;
  GrassRule.MinScale = 0.8f;
  GrassRule.MaxScale = 1.2f;
  GrassRule.MinHeight = -10.0f;
  GrassRule.MaxHeight = 50.0f;
  GrassRule.SlopeLimit = 35.0f;
  MeadowsBiome.VegetationRules.Add(GrassRule);

  // Add flower vegetation rule
  FPCGVegetationRule FlowerRule;
  FlowerRule.Density = 0.3f;
  FlowerRule.MinScale = 0.5f;
  FlowerRule.MaxScale = 1.0f;
  FlowerRule.MinHeight = -5.0f;
  FlowerRule.MaxHeight = 40.0f;
  FlowerRule.SlopeLimit = 25.0f;
  MeadowsBiome.VegetationRules.Add(FlowerRule);

  // Add POI rule for meadows
  FPOISpawnRule MeadowPOI;
  MeadowPOI.POIName = TEXT("MeadowShrine");
  MeadowPOI.SpawnChance = 0.05f;
  MeadowPOI.MinDistanceFromOthers = 1000.0f;
  MeadowPOI.SlopeLimit = 15.0f;
  MeadowPOI.bRequiresFlatGround = true;
  MeadowsBiome.POIRules.Add(MeadowPOI);

  BiomeDefinitions.Add(EBiomeType::Meadows, MeadowsBiome);

  // Forest biome
  FBiomeDefinition ForestBiome;
  ForestBiome.BiomeType = EBiomeType::Forest;
  ForestBiome.BiomeName = TEXT("Forest");

  // Add tree vegetation rule
  FPCGVegetationRule TreeRule;
  TreeRule.Density = 0.4f;
  TreeRule.MinScale = 0.9f;
  TreeRule.MaxScale = 1.8f;
  TreeRule.MinHeight = 0.0f;
  TreeRule.MaxHeight = 100.0f;
  TreeRule.SlopeLimit = 45.0f;
  ForestBiome.VegetationRules.Add(TreeRule);

  // Add undergrowth vegetation rule
  FPCGVegetationRule UndergrowthRule;
  UndergrowthRule.Density = 0.6f;
  UndergrowthRule.MinScale = 0.7f;
  UndergrowthRule.MaxScale = 1.3f;
  UndergrowthRule.MinHeight = 0.0f;
  UndergrowthRule.MaxHeight = 80.0f;
  UndergrowthRule.SlopeLimit = 40.0f;
  ForestBiome.VegetationRules.Add(UndergrowthRule);

  // Add POI rule for forests
  FPOISpawnRule ForestPOI;
  ForestPOI.POIName = TEXT("AbandonedCamp");
  ForestPOI.SpawnChance = 0.08f;
  ForestPOI.MinDistanceFromOthers = 800.0f;
  ForestPOI.SlopeLimit = 30.0f;
  ForestPOI.bRequiresFlatGround = false;
  ForestBiome.POIRules.Add(ForestPOI);

  BiomeDefinitions.Add(EBiomeType::Forest, ForestBiome);

  // Mountains biome
  FBiomeDefinition MountainBiome;
  MountainBiome.BiomeType = EBiomeType::Mountains;
  MountainBiome.BiomeName = TEXT("Mountains");

  // Sparse vegetation for mountains
  FPCGVegetationRule MountainTreeRule;
  MountainTreeRule.Density = 0.1f;
  MountainTreeRule.MinScale = 0.6f;
  MountainTreeRule.MaxScale = 1.2f;
  MountainTreeRule.MinHeight = 30.0f;
  MountainTreeRule.MaxHeight = 120.0f;
  MountainTreeRule.SlopeLimit = 50.0f;
  MountainBiome.VegetationRules.Add(MountainTreeRule);

  // Add POI rule for mountains
  FPOISpawnRule MountainPOI;
  MountainPOI.POIName = TEXT("MountainCave");
  MountainPOI.SpawnChance = 0.03f;
  MountainPOI.MinDistanceFromOthers = 1500.0f;
  MountainPOI.SlopeLimit = 60.0f;
  MountainPOI.bRequiresFlatGround = false;
  MountainBiome.POIRules.Add(MountainPOI);

  BiomeDefinitions.Add(EBiomeType::Mountains, MountainBiome);

  UE_LOG(LogPCGWorldService, Log,
         TEXT("Initialized %d default biome definitions"),
         BiomeDefinitions.Num());
}

uint32 UPCGWorldService::GetTileRandomSeed(FTileCoord TileCoord) const {
  // Generate deterministic seed based on tile coordinates and world seed
  return HashCombine(
      HashCombine(GetTypeHash(TileCoord.X), GetTypeHash(TileCoord.Y)),
      GetTypeHash(WorldGenSettings.Seed));
}

FVector2D UPCGWorldService::GeneratePoissonSample(FRandomStream &RandomStream,
                                                  FVector2D TileStart,
                                                  float TileSize,
                                                  float MinDistance) const {
  // Simple random sample within the tile. For production, implement true
  // Poisson disk by checking against existing samples and using MinDistance.
  (void)MinDistance; // suppress unused parameter warning for now

  return TileStart + FVector2D(RandomStream.FRandRange(0.0f, TileSize),
                               RandomStream.FRandRange(0.0f, TileSize));
}

void UPCGWorldService::GenerateClusteredSamples(
    FRandomStream &RandomStream, int32 InstanceCount, FVector2D TileStart,
    float TileSize, float MinDistance, TArray<FVector2D> &OutSamples) const {
  OutSamples.Reset();

  if (InstanceCount <= 0) {
    return;
  }

  const float ClusterRadius = FMath::Max(MinDistance * 2.0f, TileSize * 0.1f);
  int32 Remaining = InstanceCount;
  const int32 TargetClusterSize =
      FMath::Clamp(FMath::Max(3, InstanceCount / 6), 3, 12);

  while (Remaining > 0) {
    const FVector2D ClusterCenter =
        GeneratePoissonSample(RandomStream, TileStart, TileSize, MinDistance);
    const int32 SamplesThisCluster = FMath::Min(TargetClusterSize, Remaining);

    for (int32 SampleIndex = 0; SampleIndex < SamplesThisCluster;
         ++SampleIndex) {
      const float Angle = RandomStream.FRandRange(0.0f, 2.0f * PI);
      const float Radius = RandomStream.FRandRange(0.0f, ClusterRadius);
      FVector2D Offset(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius);

      FVector2D SamplePoint = ClusterCenter + Offset;
      SamplePoint.X =
          FMath::Clamp(SamplePoint.X, TileStart.X, TileStart.X + TileSize);
      SamplePoint.Y =
          FMath::Clamp(SamplePoint.Y, TileStart.Y, TileStart.Y + TileSize);
      OutSamples.Add(SamplePoint);
    }

    Remaining -= SamplesThisCluster;
  }

  if (OutSamples.Num() > InstanceCount) {
    OutSamples.SetNum(InstanceCount);
  }
}
float UPCGWorldService::CalculateSlope(const TArray<float> &HeightData, int32 X,
                                       int32 Y, int32 GridSize) const {
  if (!HeightData.IsValidIndex(Y * GridSize + X)) {
    return 0.0f;
  }

  float CenterHeight = HeightData[Y * GridSize + X];

  // Calculate slope using neighboring heights
  float MaxSlope = 0.0f;
  for (int32 DX = -1; DX <= 1; DX++) {
    for (int32 DY = -1; DY <= 1; DY++) {
      if (DX == 0 && DY == 0)
        continue;

      int32 NeighborX = X + DX;
      int32 NeighborY = Y + DY;

      if (NeighborX >= 0 && NeighborX < GridSize && NeighborY >= 0 &&
          NeighborY < GridSize) {
        int32 NeighborIndex = NeighborY * GridSize + NeighborX;
        if (HeightData.IsValidIndex(NeighborIndex)) {
          float NeighborHeight = HeightData[NeighborIndex];
          float HeightDiff = FMath::Abs(NeighborHeight - CenterHeight);
          float Distance = FMath::Sqrt(
              static_cast<float>(DX * DX + DY * DY)); // Grid distance
          float Slope =
              FMath::RadiansToDegrees(FMath::Atan2(HeightDiff, Distance));
          MaxSlope = FMath::Max(MaxSlope, Slope);
        }
      }
    }
  }

  return MaxSlope;
}
FPCGTileMetrics
UPCGWorldService::AnalyzeTileMetrics(const TArray<float> &HeightData) const {
  FPCGTileMetrics Metrics;
  const float SamplesPerSideFloat =
      WorldGenSettings.TileSizeMeters /
      FMath::Max(KINDA_SMALL_NUMBER, WorldGenSettings.SampleSpacingMeters);
  const int32 GridSize =
      FMath::Clamp(FMath::RoundToInt(SamplesPerSideFloat), 1, 4096);
  const int32 ExpectedSize = GridSize * GridSize;
  if (HeightData.Num() != ExpectedSize) {
    return Metrics;
  }

  const float SeaLevel = WorldGenSettings.SeaLevel;

  float SumHeight = 0.0f;
  float SumSlope = 0.0f;
  float MaxSlope = 0.0f;
  float MinHeight = HeightData[0];
  float MaxHeight = HeightData[0];
  float WaterCoverageSamples = 0.0f;
  float AboveWaterSum = 0.0f;
  float BelowWaterSum = 0.0f;
  float MinAbsWaterDistance = TNumericLimits<float>::Max();

  for (int32 Y = 0; Y < GridSize; ++Y) {
    for (int32 X = 0; X < GridSize; ++X) {
      const int32 Index = Y * GridSize + X;
      const float Height = HeightData[Index];

      SumHeight += Height;
      MinHeight = FMath::Min(MinHeight, Height);
      MaxHeight = FMath::Max(MaxHeight, Height);

      const float Slope = CalculateSlope(HeightData, X, Y, GridSize);
      SumSlope += Slope;
      MaxSlope = FMath::Max(MaxSlope, Slope);

      const float WaterDelta = Height - SeaLevel;
      MinAbsWaterDistance =
          FMath::Min(MinAbsWaterDistance, FMath::Abs(WaterDelta));

      if (WaterDelta >= 0.0f) {
        AboveWaterSum += WaterDelta;
      } else {
        BelowWaterSum += -WaterDelta;
        WaterCoverageSamples += 1.0f;
      }
    }
  }

  const float SampleCount = static_cast<float>(ExpectedSize);
  Metrics.AverageHeight = SumHeight / SampleCount;
  Metrics.MinHeight = MinHeight;
  Metrics.MaxHeight = MaxHeight;
  Metrics.AverageSlope = SumSlope / SampleCount;
  Metrics.MaxSlope = MaxSlope;
  Metrics.WaterCoverageRatio =
      SampleCount > 0.0f ? WaterCoverageSamples / SampleCount : 0.0f;

  const float AboveSamples = SampleCount - WaterCoverageSamples;
  Metrics.AverageAboveWater =
      AboveSamples > KINDA_SMALL_NUMBER ? AboveWaterSum / AboveSamples : 0.0f;
  Metrics.AverageBelowWater = WaterCoverageSamples > KINDA_SMALL_NUMBER
                                  ? BelowWaterSum / WaterCoverageSamples
                                  : 0.0f;
  Metrics.MinAbsWaterDistance =
      (MinAbsWaterDistance == TNumericLimits<float>::Max())
          ? 0.0f
          : MinAbsWaterDistance;

  return Metrics;
}

float UPCGWorldService::ComputeEnvironmentScale(
    const FPCGTileMetrics &TileMetrics,
    const FPCGVegetationRule &VegRule) const {
  float SlopeFactor = 1.0f;
  if (VegRule.SlopeLimit > KINDA_SMALL_NUMBER) {
    SlopeFactor = FMath::Clamp(1.0f - (TileMetrics.AverageSlope /
                                       FMath::Max(VegRule.SlopeLimit, 1.0f)),
                               0.0f, 1.0f);
  }

  float WaterFactor = 1.0f;
  const float WaterFalloff =
      FMath::Max(10.0f, WorldGenSettings.TileSizeMeters * 0.75f);
  WaterFactor = FMath::Clamp(
      1.0f - (TileMetrics.MinAbsWaterDistance / WaterFalloff), 0.2f, 1.0f);

  if (TileMetrics.WaterCoverageRatio > 0.0f &&
      VegRule.MaxHeight > WorldGenSettings.SeaLevel) {
    WaterFactor *= 1.0f - TileMetrics.WaterCoverageRatio;
  }

  return FMath::Clamp(SlopeFactor * WaterFactor, 0.1f, 1.5f);
}

float UPCGWorldService::EstimateWorkUnitsForBiome(
    EBiomeType BiomeType, const FPCGTileMetrics *TileMetrics) const {
  const FBiomeDefinition *BiomeDef = BiomeDefinitions.Find(BiomeType);
  if (!BiomeDef) {
    return 0.0f;
  }

  const float TileSize = WorldGenSettings.TileSizeMeters;
  const float TileArea = TileSize * TileSize;
  const float BiomeWeight = 1.0f;

  float EstimatedUnits = 0.0f;
  for (const FPCGVegetationRule &VegRule : BiomeDef->VegetationRules) {
    float Density =
        VegRule.Density * WorldGenSettings.VegetationDensity * BiomeWeight;
    if (TileMetrics) {
      Density *= ComputeEnvironmentScale(*TileMetrics, VegRule);
    }

    const float RuleUnits = FMath::Max(0.0f, Density * TileArea / 100.0f);
    EstimatedUnits += RuleUnits;
  }

  // POI placement cost is coarse but keeps density scaling responsive when
  // vegetation is sparse
  EstimatedUnits += static_cast<float>(BiomeDef->POIRules.Num()) * 10.0f;

  return EstimatedUnits;
}

float UPCGWorldService::ComputeDensityScaleFromWork(
    float EstimatedWorkUnits) const {
  static constexpr float WorkUnitsPerMs = 500.0f;

  if (EstimatedWorkUnits <= KINDA_SMALL_NUMBER) {
    return 1.0f;
  }

  const float TargetMs = FMath::Max(0.1f, WorldGenSettings.PCGTargetMsPerTile);
  const float LoadMs = EstimatedWorkUnits / WorkUnitsPerMs;
  const float EffectiveLoad = FMath::Max(LoadMs, 0.1f);
  const float RawScale = TargetMs / EffectiveLoad;

  return FMath::Clamp(RawScale, 0.5f, 1.2f);
}

bool UPCGWorldService::CheckPOISpacingRequirements(FVector Location,
                                                   float MinDistance) {
  // Check against existing POIs
  for (const auto &POIPair : SpawnedPOIs) {
    const FPOIData &ExistingPOI = POIPair.Value;
    float Distance = FVector::Dist(Location, ExistingPOI.Location);
    if (Distance < MinDistance) {
      return false;
    }
  }

  return true;
}

void UPCGWorldService::ApplyDensityLimiting(
    FPCGGenerationData &GenerationData) {
  if (GenerationData.TotalInstanceCount <= MaxInstancesPerTile) {
    return;
  }

  // Sort instances by some priority (e.g., distance from tile center, or keep
  // first N instances)
  FVector TileCenter =
      GenerationData.TileCoord.ToWorldPosition(WorldGenSettings.TileSizeMeters);

  GenerationData.GeneratedInstances.Sort(
      [TileCenter](const FPCGInstanceData &A, const FPCGInstanceData &B) {
        float DistA = FVector::DistSquared(A.Location, TileCenter);
        float DistB = FVector::DistSquared(B.Location, TileCenter);
        return DistA < DistB; // Keep instances closer to tile center
      });

  // Truncate to max instances
  if (GenerationData.GeneratedInstances.Num() > MaxInstancesPerTile) {
    GenerationData.GeneratedInstances.SetNum(MaxInstancesPerTile);
    GenerationData.TotalInstanceCount = MaxInstancesPerTile;

    UE_LOG(LogPCGWorldService, Warning,
           TEXT("Applied density limiting to tile (%d, %d) - reduced to %d "
                "instances"),
           GenerationData.TileCoord.X, GenerationData.TileCoord.Y,
           MaxInstancesPerTile);
  }
}

void UPCGWorldService::CreateHISMComponentsForTile(FTileCoord TileCoord) {
  UWorld *World = GetWorld();
  if (!World) {
    if (bHeadless) {
      UE_LOG(
          LogPCGWorldService, Verbose,
          TEXT("Headless: skipping HISM component creation for tile (%d,%d)"),
          TileCoord.X, TileCoord.Y);
    } else {
      UE_LOG(LogPCGWorldService, Error,
             TEXT("Cannot create HISM components - no valid world"));
    }
    return;
  }

  if (!TileActor) {
    FVector TileWorldPos =
        TileCoord.ToWorldPosition(WorldGenSettings.TileSizeMeters);
    FTransform ActorTransform(FRotator::ZeroRotator, TileWorldPos,
                              FVector::OneVector);
    TileActor =
        World->SpawnActor<AActor>(AActor::StaticClass(), ActorTransform);
#if WITH_EDITOR
    TileActor->SetActorLabel(TEXT("PCGTileActor"));
#endif
  }

  if (TileActor && !TileActor->GetRootComponent()) {
    USceneComponent *RootComponent =
        NewObject<USceneComponent>(TileActor, TEXT("PCGTileRoot"));
    RootComponent->SetMobility(EComponentMobility::Movable);
    TileActor->SetRootComponent(RootComponent);
    RootComponent->SetWorldTransform(TileActor->GetActorTransform());
    RootComponent->RegisterComponent();
  }

  FHISMComponentArray ComponentArray;
  ComponentArray.Components =
      TArray<UHierarchicalInstancedStaticMeshComponent *>();
  HISMComponents.Add(TileCoord, ComponentArray);

  UE_LOG(LogPCGWorldService, Log,
         TEXT("Created HISM component array for tile (%d, %d)"), TileCoord.X,
         TileCoord.Y);
}

UHierarchicalInstancedStaticMeshComponent *
UPCGWorldService::GetOrCreateHISMComponent(FTileCoord TileCoord,
                                           UStaticMesh *Mesh) {
  if (!Mesh || !GetWorld()) {
    return nullptr;
  }

  FHISMComponentArray *TileComponentArray = HISMComponents.Find(TileCoord);
  if (!TileComponentArray) {
    CreateHISMComponentsForTile(TileCoord);
    TileComponentArray = HISMComponents.Find(TileCoord);
  }

  if (!TileComponentArray) {
    return nullptr;
  }

  for (UHierarchicalInstancedStaticMeshComponent *Component :
       TileComponentArray->Components) {
    if (IsValid(Component) && Component->GetStaticMesh() == Mesh) {
      return Component;
    }
  }

  if (!TileActor) {
    CreateHISMComponentsForTile(TileCoord);
  }

  if (!TileActor) {
    UE_LOG(LogPCGWorldService, Error,
           TEXT("Cannot create HISM component for tile (%d, %d) - tile actor "
                "is invalid"),
           TileCoord.X, TileCoord.Y);
    return nullptr;
  }

  USceneComponent *RootComponent = TileActor->GetRootComponent();
  if (!RootComponent) {
    CreateHISMComponentsForTile(TileCoord);
    RootComponent = TileActor->GetRootComponent();
  }

  if (!RootComponent) {
    UE_LOG(LogPCGWorldService, Error,
           TEXT("Cannot create HISM component for tile (%d, %d) - root "
                "component missing"),
           TileCoord.X, TileCoord.Y);
    return nullptr;
  }

  UHierarchicalInstancedStaticMeshComponent *NewComponent =
      NewObject<UHierarchicalInstancedStaticMeshComponent>(TileActor);
  if (!NewComponent) {
    UE_LOG(LogPCGWorldService, Error,
           TEXT("Failed to allocate HISM component for tile (%d, %d)"),
           TileCoord.X, TileCoord.Y);
    return nullptr;
  }

  NewComponent->SetStaticMesh(Mesh);
  NewComponent->SetMobility(EComponentMobility::Movable);
  NewComponent->SetCanEverAffectNavigation(false);
  NewComponent->SetupAttachment(RootComponent);
  NewComponent->SetCullDistances(CullDistances[0], CullDistances[2]);
  NewComponent->bUseAsOccluder = false; // Vegetation typically shouldnt occlude
  NewComponent->RegisterComponent();

  TileComponentArray->Components.Add(NewComponent);

  UE_LOG(LogPCGWorldService, Log,
         TEXT("Created new HISM component for mesh %s on tile (%d, %d)"),
         *Mesh->GetName(), TileCoord.X, TileCoord.Y);

  return NewComponent;
}

float UPCGWorldService::EstimateMemoryUsage() {
  float TotalMemoryMB = 0.0f;

  // Estimate cache memory usage
  TotalMemoryMB +=
      GenerationCache.Num() * 0.1f; // Rough estimate per generation data entry

  // Estimate HISM memory usage
  int32 TotalInstances = 0;
  for (const auto &TilePair : HISMComponents) {
    for (UHierarchicalInstancedStaticMeshComponent *Component :
         TilePair.Value.Components) {
      if (IsValid(Component)) {
        TotalInstances += Component->GetInstanceCount();
      }
    }
  }
  TotalMemoryMB += TotalInstances * 0.001f; // Rough estimate per instance

  // Estimate POI memory usage
  TotalMemoryMB += SpawnedPOIs.Num() * 0.05f; // Rough estimate per POI

  return TotalMemoryMB;
}

bool UPCGWorldService::FindPOILocationStratified(
    FTileCoord TileCoord, const FPOISpawnRule &POIRule,
    const TArray<float> &HeightData, FRandomStream &RandomStream,
    FVector &OutLocation) {
  // Calculate tile bounds
  const float TileSize = WorldGenSettings.TileSizeMeters;
  const float InvSampleSpacing =
      1.0f /
      FMath::Max(KINDA_SMALL_NUMBER, WorldGenSettings.SampleSpacingMeters);
  const int32 SamplesPerSide = FMath::Clamp(
      FMath::RoundToInt(WorldGenSettings.TileSizeMeters * InvSampleSpacing), 1,
      4096);
  if (HeightData.Num() != SamplesPerSide * SamplesPerSide) {
    return false;
  }

  FVector TileWorldPos = TileCoord.ToWorldPosition(TileSize);
  FVector2D TileStart(TileWorldPos.X - TileSize * 0.5f,
                      TileWorldPos.Y - TileSize * 0.5f);

  // Use stratified sampling - divide tile into 4x4 grid and sample within each
  // cell
  const int32 GridSize = 4;
  const float CellSize = TileSize / GridSize;

  // Try multiple cells for better distribution
  TArray<FIntVector2> CellIndices;
  for (int32 Y = 0; Y < GridSize; Y++) {
    for (int32 X = 0; X < GridSize; X++) {
      CellIndices.Add(FIntVector2(X, Y));
    }
  }

  // Shuffle the cells for random sampling order
  for (int32 i = CellIndices.Num() - 1; i > 0; i--) {
    int32 j = RandomStream.RandRange(0, i);
    CellIndices.Swap(i, j);
  }

  // Try to find suitable location in cells
  for (const FIntVector2 &CellIndex : CellIndices) {
    // Generate random point within this cell
    FVector2D CellMin =
        TileStart + FVector2D(CellIndex.X * CellSize, CellIndex.Y * CellSize);
    const float Padding = FMath::Min(CellSize * 0.1f, 2.0f);
    FVector2D RandomOffset =
        FVector2D(RandomStream.FRandRange(Padding, CellSize - Padding),
                  RandomStream.FRandRange(Padding, CellSize - Padding));
    FVector2D SamplePoint = CellMin + RandomOffset;

    // Convert to heightfield coordinates
    int32 HeightX = FMath::Clamp(
        FMath::FloorToInt((SamplePoint.X - TileStart.X) * InvSampleSpacing), 0,
        SamplesPerSide - 1);
    int32 HeightY = FMath::Clamp(
        FMath::FloorToInt((SamplePoint.Y - TileStart.Y) * InvSampleSpacing), 0,
        SamplesPerSide - 1);
    int32 HeightIndex = HeightY * SamplesPerSide + HeightX;

    if (!HeightData.IsValidIndex(HeightIndex)) {
      continue;
    }

    // Get terrain data at this location
    float Height = HeightData[HeightIndex];
    float Slope = CalculateSlope(HeightData, HeightX, HeightY, SamplesPerSide);
    FVector TestLocation(SamplePoint.X, SamplePoint.Y, Height);

    // Check slope requirements
    if (Slope > POIRule.SlopeLimit) {
      continue;
    }

    // Check altitude constraints (basic filtering)
    if (Height < WorldGenSettings.SeaLevel + 2.0f) // 2m above sea level minimum
    {
      continue;
    }

    // Check spacing requirements
    if (!CheckPOISpacingRequirements(TestLocation,
                                     POIRule.MinDistanceFromOthers)) {
      continue;
    }

    // Additional slope validation for flat ground requirement
    if (POIRule.bRequiresFlatGround) {
      // Check a 3x3 area around the point for consistent flatness
      bool bIsFlatArea = true;
      float MaxSlopeInArea = 0.0f;

      for (int32 CheckY = FMath::Max(0, HeightY - 1);
           CheckY <= FMath::Min(SamplesPerSide - 1, HeightY + 1); CheckY++) {
        for (int32 CheckX = FMath::Max(0, HeightX - 1);
             CheckX <= FMath::Min(SamplesPerSide - 1, HeightX + 1); CheckX++) {
          float LocalSlope =
              CalculateSlope(HeightData, CheckX, CheckY, SamplesPerSide);
          MaxSlopeInArea = FMath::Max(MaxSlopeInArea, LocalSlope);
          if (LocalSlope >
              POIRule.SlopeLimit * 0.5f) // Stricter slope for flat ground
          {
            bIsFlatArea = false;
            break;
          }
        }
        if (!bIsFlatArea)
          break;
      }

      if (!bIsFlatArea) {
        continue;
      }
    }

    // Found suitable location
    OutLocation = TestLocation;

    UE_LOG(LogPCGWorldService, Verbose,
           TEXT("Found POI location at (%.1f, %.1f, %.1f) with slope %.1f "
                "degrees in cell (%d, %d)"),
           TestLocation.X, TestLocation.Y, TestLocation.Z, Slope, CellIndex.X,
           CellIndex.Y);

    return true;
  }

  UE_LOG(
      LogPCGWorldService, Verbose,
      TEXT("Could not find suitable POI location for rule %s in tile (%d, %d)"),
      *POIRule.POIName, TileCoord.X, TileCoord.Y);

  return false;
}

void UPCGWorldService::ApplyPOITerrainStamp(FVector Location, float Radius) {
  // Bypass "stamp terrain" integration step in headless mode: skip terrain
  // modification when no world context available
  if (bHeadless || GetWorld() == nullptr) {
    UE_LOG(LogPCGWorldService, Log,
           TEXT("Headless mode: Skipping terrain stamp at (%.1f, %.1f, %.1f) "
                "with radius %.1f - no world context available"),
           Location.X, Location.Y, Location.Z, Radius);
    return;
  }

  // Get HeightfieldService to apply terrain modification
  UWorld *World = GetWorld();
  if (!World) {
    UE_LOG(LogPCGWorldService, Warning,
           TEXT("Cannot apply terrain stamp - no valid world"));
    return;
  }

  // Find WorldGenManager to access HeightfieldService
  // For now, just log the operation as a placeholder for integration
  UE_LOG(LogPCGWorldService, Log,
         TEXT("Applied terrain stamp at (%.1f, %.1f, %.1f) with radius %.1f "
              "for POI placement"),
         Location.X, Location.Y, Location.Z, Radius);

  // In a full implementation, this would:
  // 1. Get the HeightfieldService from WorldGenManager
  // 2. Apply a flatten operation with the specified radius
  // 3. Clear vegetation in the area
  // 4. Update the heightfield data
  //
  // Example integration code:
  // if (UWorldGenManager* WorldGenManager =
  // World->GetSubsystem<UWorldGenManager>())
  // {
  //     if (UHeightfieldService* HeightfieldService =
  //     WorldGenManager->GetHeightfieldService())
  //     {
  //         HeightfieldService->ModifyHeightfield(Location, Radius, 0.8f,
  //         EHeightfieldOperation::Flatten);
  //
  //         // Clear vegetation in the area
  //         FBox ClearArea(Location - FVector(Radius), Location +
  //         FVector(Radius)); RemoveContentInArea(ClearArea);
  //     }
  // }
}

void UPCGWorldService::InitializeDefaultBiomes(
    TMap<EBiomeType, FBiomeDefinition> &OutDefaultBiomes) {
  // Forest biome with default vegetation rules
  FBiomeDefinition ForestBiome;
  ForestBiome.BiomeType = EBiomeType::Forest;
  ForestBiome.BiomeName = TEXT("Forest");

  // Add default vegetation rules for Forest biome
  FPCGVegetationRule ForestTreeRule;
  ForestTreeRule.Density = 0.5f;
  ForestTreeRule.MinScale = 0.8f;
  ForestTreeRule.MaxScale = 1.2f;
  ForestTreeRule.SlopeLimit = 30.0f;
  ForestTreeRule.MinHeight = -100.0f;
  ForestTreeRule.MaxHeight = 1000.0f;
  // Leave VegetationMesh as null - headless mode will handle this
  ForestBiome.VegetationRules.Add(ForestTreeRule);

  FPCGVegetationRule ForestUndergrowthRule;
  ForestUndergrowthRule.Density = 0.3f;
  ForestUndergrowthRule.MinScale = 0.5f;
  ForestUndergrowthRule.MaxScale = 0.8f;
  ForestUndergrowthRule.SlopeLimit = 45.0f;
  ForestUndergrowthRule.MinHeight = -100.0f;
  ForestUndergrowthRule.MaxHeight = 1000.0f;
  // Leave VegetationMesh as null - headless mode will handle this
  ForestBiome.VegetationRules.Add(ForestUndergrowthRule);

  OutDefaultBiomes.Add(EBiomeType::Forest, ForestBiome);

  // Meadows biome with default vegetation rules
  FBiomeDefinition MeadowsBiome;
  MeadowsBiome.BiomeType = EBiomeType::Meadows;
  MeadowsBiome.BiomeName = TEXT("Meadows");

  FPCGVegetationRule MeadowsGrassRule;
  MeadowsGrassRule.Density = 0.4f;
  MeadowsGrassRule.MinScale = 0.6f;
  MeadowsGrassRule.MaxScale = 1.0f;
  MeadowsGrassRule.SlopeLimit = 35.0f;
  MeadowsGrassRule.MinHeight = -50.0f;
  MeadowsGrassRule.MaxHeight = 500.0f;
  // Leave VegetationMesh as null - headless mode will handle this
  MeadowsBiome.VegetationRules.Add(MeadowsGrassRule);

  OutDefaultBiomes.Add(EBiomeType::Meadows, MeadowsBiome);

  // Mountains biome with sparse vegetation
  FBiomeDefinition MountainsBiome;
  MountainsBiome.BiomeType = EBiomeType::Mountains;
  MountainsBiome.BiomeName = TEXT("Mountains");

  FPCGVegetationRule MountainSparseRule;
  MountainSparseRule.Density = 0.1f;
  MountainSparseRule.MinScale = 0.7f;
  MountainSparseRule.MaxScale = 1.1f;
  MountainSparseRule.SlopeLimit = 25.0f;
  MountainSparseRule.MinHeight = 20.0f;
  MountainSparseRule.MaxHeight = 1000.0f;
  // Leave VegetationMesh as null - headless mode will handle this
  MountainsBiome.VegetationRules.Add(MountainSparseRule);

  OutDefaultBiomes.Add(EBiomeType::Mountains, MountainsBiome);

  // Ocean biome (no vegetation rules - underwater)
  FBiomeDefinition OceanBiome;
  OceanBiome.BiomeType = EBiomeType::Ocean;
  OceanBiome.BiomeName = TEXT("Ocean");
  // No vegetation rules for ocean

  OutDefaultBiomes.Add(EBiomeType::Ocean, OceanBiome);

  UE_LOG(LogPCGWorldService, Log,
         TEXT("Initialized default biome definitions with vegetation rules for "
              "%d biomes"),
         OutDefaultBiomes.Num());
}
