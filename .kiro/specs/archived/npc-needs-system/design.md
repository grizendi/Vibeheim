# Design Document

## Overview

The NPC Needs System implements intelligent survival behavior for NPCs in Vibeheim using a data-driven, StateTree-controlled architecture. NPCs continuously evaluate their basic needs (Hunger, Thirst, Energy, Health), find appropriate resources, and navigate to consume them. The system is designed for performance with many NPCs while maintaining emergent, lifelike behavior.

The core design follows a simple loop: **Evaluate → Find → Travel → Use → Evaluate**, with robust failure handling, reservation systems, and performance optimizations through LOD and spatial queries.
## Arc
hitecture

### High-Level Component Relationships

```
AVHMNPCCharacter
├── UVHMNeedsComponent (needs tracking & decay)
├── UStateTreeComponent (decision making)
└── AVHMNPCAIController (movement & pathfinding)

AVHMResourceActor (world resources)
├── IVHMUsableResource (consumption interface)
├── Reservation system (actor-local, token-based)
└── GameplayTag system (Resource.Food/Water/Shelter)

UVHMSpeciesDataAsset (configuration)
├── Need decay/recovery rates
├── Search parameters
└── Performance LOD settings
```

### Module Integration

The system extends the existing Vibeheim module structure:

```
Source/Vibeheim/
├── NPC/                    (new folder)
│   ├── Public/
│   │   ├── VHMNPCCharacter.h
│   │   ├── VHMNPCAIController.h
│   │   ├── VHMNeedsComponent.h
│   │   ├── VHMResourceActor.h
│   │   ├── VHMSpeciesDataAsset.h
│   │   └── VHMNPCTypes.h
│   └── Private/
│       ├── VHMNPCCharacter.cpp
│       ├── VHMNPCAIController.cpp
│       ├── VHMNeedsComponent.cpp
│       ├── VHMResourceActor.cpp
│       ├── VHMSpeciesDataAsset.cpp
│       └── StateTree/
│           ├── STT_EvaluateNeed.cpp
│           ├── STT_FindTarget.cpp
│           ├── STT_MoveTo.cpp
│           ├── STT_UseResource.cpp
│           └── STT_Explore.cpp
```## Compone
nts and Interfaces

### Core Data Structures

#### EVHMNeed Enumeration
```cpp
UENUM(BlueprintType)
enum class EVHMNeed : uint8
{
    Hunger,
    Thirst, 
    Energy,
    Health
};
```

#### FNeedStat Structure
```cpp
USTRUCT(BlueprintType)
struct VIBEHEIM_API FNeedStat
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EVHMNeed Type = EVHMNeed::Hunger;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Current = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Max = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DecayPerSec = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RecoverPerSec = 30.0f;

    // Hysteresis thresholds to prevent oscillation
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EnterLowThreshold = 60.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ExitLowThreshold = 70.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CriticalThreshold = 25.0f;

    // Utility functions
    float Deficit() const { return Max - Current; }
    bool IsLow() const { return Current <= EnterLowThreshold; }
    bool IsCritical() const { return Current <= CriticalThreshold; }
    bool ShouldExitLow() const { return Current >= ExitLowThreshold; }
};
```

#### FVHMUseHandle (Reservation Token)
```cpp
USTRUCT(BlueprintType)
struct VIBEHEIM_API FVHMUseHandle
{
    GENERATED_BODY()

    UPROPERTY()
    TWeakObjectPtr<AActor> Resource;

    UPROPERTY()
    TWeakObjectPtr<APawn> User;

    UPROPERTY()
    FGuid ReservationId; // Default invalid

    UPROPERTY()
    float LastHeartbeatTime = 0.0f;

    // Default constructor leaves ReservationId invalid

    bool IsValid() const { return Resource.IsValid() && User.IsValid() && ReservationId.IsValid(); }
};
```

#### FAgentNeedContext (StateTree Context)
```cpp
USTRUCT(BlueprintType)
struct VIBEHEIM_API FAgentNeedContext
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EVHMNeed CurrentNeed = EVHMNeed::Hunger;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    AActor* TargetActor = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector TargetLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float NeedScore = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LastEvaluationTime = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<AActor*> BlacklistedTargets;
};
```

### UVHMNeedsComponent

**Responsibilities:**
- Track all four needs with species-specific parameters
- Handle continuous decay with deterministic mode support
- Implement hysteresis to prevent oscillation
- Manage post-satiation cooldowns
- Provide urgency scoring for decision arbitration

**Key Features:**
- Configurable tick rates for performance LOD
- Deterministic mode for testing (fixed timestep, seeded RNG)
- Health penalty when Hunger/Thirst reach zero
- Lock-free need queries on game thread for StateTree tasks

### IVHMUsableResource Interface

```cpp
UINTERFACE(BlueprintType)
class VIBEHEIM_API UVHMUsableResource : public UInterface
{
    GENERATED_BODY()
};

class VIBEHEIM_API IVHMUsableResource
{
    GENERATED_BODY()

public:
    virtual bool CanUse(EVHMNeed Need) const = 0;
    virtual FVHMUseHandle BeginUse(APawn* User) = 0;
    virtual bool TickUse(const FVHMUseHandle& Handle, float DeltaTime) = 0;
    virtual void EndUse(const FVHMUseHandle& Handle) = 0;
};
```

### AVHMResourceActor & IVHMUsableResource

**Resource Actor Design:**
```cpp
UCLASS(BlueprintType, Blueprintable)
class VIBEHEIM_API AVHMResourceActor : public AActor, public IVHMUsableResource
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
    FGameplayTag ResourceTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
    float UseRadius = 180.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
    float Quantity = -1.0f; // -1 = infinite

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
    int32 MaxConcurrentUsers = 1;

    // IVHMUsableResource interface
    virtual bool CanUse(EVHMNeed Need) const override;
    virtual FVHMUseHandle BeginUse(APawn* User) override;
    virtual bool TickUse(const FVHMUseHandle& Handle, float DeltaTime) override;
    virtual void EndUse(const FVHMUseHandle& Handle) override;
};
```

**Reservation System:**
- Lightweight token-based reservations prevent resource conflicts
- Heartbeat mechanism (1-2s intervals) prevents abandoned reservations
- Automatic expiry when NPCs are too far or make no progress
- Configurable MaxConcurrentUsers per resource###
 StateTree Integration

#### Task Architecture
The StateTree uses five core tasks in a simple decision loop:

1. **STT_EvaluateNeed**: Analyzes all needs, selects most urgent, applies cooldowns
2. **STT_FindTarget**: Spatial search with escalating radius, reachability checks, reservations
3. **STT_MoveTo**: NavMesh pathfinding with stuck detection and failure handling
4. **STT_UseResource**: Consumption loop with alignment, recovery, and completion detection
5. **STT_Explore**: Fallback wandering behavior when no resources are found

#### External Data Bindings
```cpp
// Available to all StateTree tasks
TStateTreeExternalData<UVHMNeedsComponent> NeedsComponent;
TStateTreeExternalData<AActor> OwnerActor;
TStateTreeExternalData<APawn> OwnerPawn;
TStateTreeInstanceDataPropertyRef<FAgentNeedContext> Context; // UE 5.6 API

// In task Link() method:
// Linker.LinkExternalData(NeedsComponent);
// Linker.LinkExternalData(OwnerActor);  
// Linker.LinkExternalData(OwnerPawn);
// Linker.LinkInstanceData(Context); // Required for instance data
```

## Data Models

### Species Configuration (UVHMSpeciesDataAsset)

```cpp
USTRUCT(BlueprintType)
struct VIBEHEIM_API FVHMSpeciesConfig
{
    GENERATED_BODY()

    // Need configuration per species
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Needs")
    TMap<EVHMNeed, FNeedStat> DefaultNeeds;

    // Search behavior
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Search")
    float SearchRadius = 2500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Search")
    float SearchRadiusMax = 5000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Search")
    float SearchCooldown = 1.0f;

    // Performance LOD
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    float EvaluationInterval = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    float LODEvaluationInterval = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    float LODDistance = 3000.0f;

    // Navigation
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation")
    TSubclassOf<UNavigationQueryFilter> NavQueryFilter;

    // Failure handling
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Failure")
    int32 MaxRetries = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Failure")
    float BlacklistDuration = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Failure")
    float StuckThreshold = 50.0f; // cm

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Failure")
    float StuckTimeout = 5.0f; // seconds
};

UCLASS(BlueprintType)
class VIBEHEIM_API UVHMSpeciesDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, Category = "Species")
    FVHMSpeciesConfig Config;
};
```#
## Gameplay Tags Hierarchy

```
Resource
├── Resource.Food
├── Resource.Water  
└── Resource.Shelter

Need
├── Need.Hunger
├── Need.Thirst
├── Need.Energy
└── Need.Health

NPC
├── NPC.State.Evaluating
├── NPC.State.Searching
├── NPC.State.Traveling
├── NPC.State.Using
└── NPC.State.Exploring
```

## Error Handling

### Failure Recovery Strategies

**Search Failures:**
- Escalate search radius on each retry (up to species max)
- Apply exponential backoff with jitter
- Enter Explore state after max retries
- Clear blacklist periodically to allow retry of previously failed resources

**Pathfinding Failures:**
- Detect stuck conditions (distance progress < threshold over time)
- Blacklist unreachable targets temporarily
- Fall back to alternative resources
- Use species-specific NavQueryFilters for terrain constraints

**Resource Conflicts:**
- Reservation system prevents most conflicts
- Graceful fallback when reservations fail
- Queue system for high-demand finite resources
- Heartbeat mechanism prevents abandoned reservations

**State Recovery:**
- StateTree tasks fail fast with explicit error codes
- Automatic transition to Evaluate state on critical failures
- Logging with appropriate verbosity levels
- Debug visualization for troubleshooting## T
esting Strategy

### Unit Testing Approach

**Component Tests:**
- UVHMNeedsComponent: Decay rates, hysteresis, cooldowns, deterministic mode
- Resource reservation system: Concurrency, expiry, heartbeat
- Species data asset loading and fallback behavior

**Integration Tests:**
- Full need evaluation → resource finding → pathfinding → consumption cycle
- Multi-NPC scenarios with resource contention
- Performance tests with 50+ NPCs under various LOD conditions
- Failure recovery scenarios (blocked paths, depleted resources)

**Validation Tests (from Requirements):**
- Oscillation prevention with hysteresis
- Stuck detection and recovery
- Reservation conflicts with finite resources
- LOD performance scaling

### Debug Tools Integration

**Visual Debugging:**
- Need bars and current target displayed above NPCs
- StateTree debugger integration showing current state and transitions
- Resource reservation visualization
- Search radius and blacklist visualization

**Console Commands:**
```cpp
// Performance tuning
vhm.npc.lod_distance 3000
vhm.npc.search_throttle 1.0
vhm.npc.evaluation_interval 0.5

// Testing aids  
vhm.npc.deterministic 1
vhm.npc.fast_decay 1
vhm.npc.show_targets 1
vhm.npc.retry_max 3

// Debug visualization
vhm.npc.draw_needs 1
vhm.npc.draw_reservations 1
vhm.npc.draw_search_radius 1
```

**Logging Categories:**
- LogVHMNeeds: Need evaluation, decay, recovery
- LogVHMNPC: High-level NPC behavior and state transitions  
- LogVHMResource: Resource usage, reservations, conflicts
- LogVHMStateTree: StateTree task execution and failures#
# Performance Considerations

### Optimization Strategies

**Spatial Queries:**
- Use collision channel overlap (ECC_GameTraceChannel2 = Resource) for resource discovery
- Limit search radius and object channels to dedicated Resource collision channel
- Cache nearby resources per NPC with invalidation
- Batch queries when possible

**LOD System:**
- Distance-based evaluation frequency scaling
- Reduced tick rates for needs decay when far from players
- Disable expensive searches beyond LOD distance
- Batch updates for distant NPCs (2-5s intervals)

**Memory Management:**
- Pool StateTree context structs
- Limit blacklist sizes with LRU eviction
- Use object pooling for reservation tokens
- Minimize allocations in hot paths

**Threading Considerations:**
- All gameplay logic runs on game thread (no additional threading complexity)
- StateTree tasks avoid shared mutable state
- Resource reservations use simple game thread operations
- Spatial queries executed on game thread with appropriate throttling

### Performance Targets

- **Budget:** ≤0.3ms per 50 nearby NPCs on mid-range CPU
- **Scalability:** Support 200+ NPCs with appropriate LOD
- **Memory:** <1MB additional memory per 100 NPCs
- **Network:** Minimal bandwidth impact (server-authoritative needs only)## Int
egration Points

### Existing Vibeheim Systems

**World Generation Integration:**
- NPCs spawn with terrain-appropriate species configurations
- Resources placed via PCG with proper gameplay tags
- NavMesh generation includes NPC accessibility requirements

**Module Dependencies:**
```cpp
// Required additions to Vibeheim.Build.cs
PublicDependencyModuleNames.AddRange(new string[]
{
    "Core",
    "CoreUObject", 
    "Engine",
    "AIModule",
    "NavigationSystem", 
    "GameplayTasks",
    "GameplayTags",
    "StateTreeModule",
    "StructUtils"
});
```

**Blueprint Integration:**
- Resource actors fully Blueprint-configurable
- Species data assets editable in editor
- Debug visualization toggleable via Blueprint
- Event hooks for custom behavior extensions

### Future Extension Points

**Smart Objects Integration:**
- Resource actors can be converted to Smart Objects
- Task slots for complex multi-step interactions
- Claim/reserve system integration

**AI Perception Integration:**
- Resources as stimuli for perception system
- Memory system for discovered resources
- Social communication about resource locations

**Save/Load System:**
- Persist NPC needs across sessions
- Save reservation states for long-running worlds
- Species configuration versioning

This design provides a solid foundation for the MVP while maintaining clear extension points for future enhancements. The architecture emphasizes simplicity, performance, and debuggability while delivering the emergent survival behaviors that will make Vibeheim's world feel alive.#
# Implementation Details

### Reservation System Implementation

```cpp
// In AVHMResourceActor - actor-local reservations
USTRUCT()
struct FReservationSlot
{
    GENERATED_BODY()
    
    UPROPERTY()
    FGuid Id; // Default invalid, set in TryReserve
    
    UPROPERTY()
    TWeakObjectPtr<APawn> User;
    
    UPROPERTY()
    float LastHeartbeat = 0.0f;
};

UPROPERTY()
TArray<FReservationSlot> ActiveReservations;

bool HasFreeSlot() const { return ActiveReservations.Num() < MaxConcurrentUsers; }

FGuid TryReserve(APawn* User)
{
    // Expire dead reservations
    const float Now = GetWorld()->GetTimeSeconds();
    ActiveReservations.RemoveAll([&](const FReservationSlot& S) {
        return !S.User.IsValid() || (Now - S.LastHeartbeat) > 2.0f;
    });

    if (!HasFreeSlot()) return FGuid();

    FReservationSlot Slot;
    Slot.Id = FGuid::NewGuid();
    Slot.User = User;
    Slot.LastHeartbeat = Now;
    ActiveReservations.Add(Slot);
    return Slot.Id;
}

bool HeartbeatReservation(const FGuid& Id)
{
    const float Now = GetWorld()->GetTimeSeconds();
    for (auto& S : ActiveReservations)
    {
        if (S.Id == Id) { S.LastHeartbeat = Now; return true; }
    }
    return false;
}

void ReleaseReservation(const FGuid& Id)
{
    ActiveReservations.RemoveAll([&](const FReservationSlot& S) { return S.Id == Id; });
}
```

### Resource Search Implementation

```cpp
// In STT_FindTarget - collision channel overlap approach
void FindNearbyResources(const FVector& Origin, float SearchRadius, EVHMNeed Need, TArray<AVHMResourceActor*>& OutResources)
{
    // Setup collision query for Resource channel (ECC_GameTraceChannel2)
    FCollisionObjectQueryParams ObjParams;
    ObjParams.AddObjectTypesToQuery(ECC_GameTraceChannel2); // Resource channel

    TArray<FOverlapResult> Hits;
    GetWorld()->OverlapMultiByObjectType(
        Hits, Origin, FQuat::Identity, ObjParams, 
        FCollisionShape::MakeSphere(SearchRadius)
    );

    for (const auto& Hit : Hits)
    {
        if (auto* Resource = Cast<AVHMResourceActor>(Hit.GetActor()))
        {
            if (Resource->CanUse(Need) && !IsBlacklisted(Resource))
            {
                OutResources.Add(Resource);
            }
        }
    }

    // Sort by distance
    OutResources.Sort([&](const AVHMResourceActor* A, const AVHMResourceActor* B) {
        const float DistA = FVector::DistSquared(Origin, A->GetActorLocation());
        const float DistB = FVector::DistSquared(Origin, B->GetActorLocation());
        return DistA < DistB;
    });
}
```

### Movement Progress Tracking

```cpp
// In STT_MoveTo - stuck detection implementation
struct FMovementProgressTracker
{
    float LastGoalDistance = 0.0f;
    float TimeSinceProgress = 0.0f;
    static constexpr float PROGRESS_THRESHOLD = 75.0f; // cm
    static constexpr float STUCK_TIMEOUT = 3.0f; // seconds

    bool UpdateProgress(const FVector& CurrentPos, const FVector& Goal, float DeltaTime)
    {
        const float CurrentDistance = FVector::Dist(CurrentPos, Goal);
        
        if (LastGoalDistance - CurrentDistance > PROGRESS_THRESHOLD)
        {
            // Made progress
            LastGoalDistance = CurrentDistance;
            TimeSinceProgress = 0.0f;
            return true;
        }
        else
        {
            // No significant progress
            TimeSinceProgress += DeltaTime;
            return TimeSinceProgress < STUCK_TIMEOUT;
        }
    }
};
```

### Console Variables Registration

```cpp
// VHMNPCVars.h
extern TAutoConsoleVariable<float> CVarVHM_LODDistance;
extern TAutoConsoleVariable<float> CVarVHM_SearchThrottle;
extern TAutoConsoleVariable<int32> CVarVHM_Deterministic;
extern TAutoConsoleVariable<int32> CVarVHM_FastDecay;
extern TAutoConsoleVariable<int32> CVarVHM_ShowTargets;

// VHMNPCVars.cpp
TAutoConsoleVariable<float> CVarVHM_LODDistance(
    TEXT("vhm.npc.lod_distance"), 3000.0f, 
    TEXT("LOD distance for NPC AI evaluation"), ECVF_Default);

TAutoConsoleVariable<float> CVarVHM_SearchThrottle(
    TEXT("vhm.npc.search_throttle"), 1.0f, 
    TEXT("Seconds between resource searches"), ECVF_Default);

TAutoConsoleVariable<int32> CVarVHM_Deterministic(
    TEXT("vhm.npc.deterministic"), 0, 
    TEXT("Enable deterministic mode for testing"), ECVF_Default);

TAutoConsoleVariable<int32> CVarVHM_FastDecay(
    TEXT("vhm.npc.fast_decay"), 0, 
    TEXT("Enable fast need decay for testing"), ECVF_Default);

TAutoConsoleVariable<int32> CVarVHM_ShowTargets(
    TEXT("vhm.npc.show_targets"), 0, 
    TEXT("Show NPC targets and needs above characters"), ECVF_Default);
```

### Logging Categories

```cpp
// VHMNPCLogging.h
DECLARE_LOG_CATEGORY_EXTERN(LogVHMNeeds, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogVHMNPC, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogVHMResource, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogVHMStateTree, Log, All);

// VHMNPCLogging.cpp
DEFINE_LOG_CATEGORY(LogVHMNeeds);
DEFINE_LOG_CATEGORY(LogVHMNPC);
DEFINE_LOG_CATEGORY(LogVHMResource);
DEFINE_LOG_CATEGORY(LogVHMStateTree);
```

This design is now implementation-ready with all the critical fixes applied. The architecture emphasizes simplicity, performance, and debuggability while delivering the emergent survival behaviors that will make Vibeheim's world feel alive.### 
BeginUse Implementation

```cpp
FVHMUseHandle AVHMResourceActor::BeginUse(APawn* User)
{
    FVHMUseHandle Handle;
    if (!CanUse(/*Need param from context*/)) return Handle;
    
    const FGuid ReservationId = TryReserve(User);
    if (!ReservationId.IsValid()) return Handle;
    
    Handle.Resource = this;
    Handle.User = User;
    Handle.ReservationId = ReservationId;
    Handle.LastHeartbeatTime = GetWorld()->GetTimeSeconds();
    return Handle;
}
```

### Collision Channel Setup

**Project Settings Configuration:**
- Add new collision channel: ECC_GameTraceChannel2 = "Resource"
- Set resource actors to use this collision channel
- Configure collision responses appropriately for spatial queries

### Movement Task with Navigation Filter

```cpp
// In STT_MoveTo task
FAIMoveRequest MoveReq(TargetLocation);
MoveReq.SetAcceptanceRadius(AcceptanceRadius);
if (NavQueryFilter) 
{
    MoveReq.SetNavigationFilter(NavQueryFilter);
}
const auto Result = AIController->MoveTo(MoveReq);
```