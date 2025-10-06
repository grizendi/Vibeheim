# PCG UE 5.6 API Reference & Migration Guide

## Overview

This document provides a comprehensive reference for the UE 5.6 PCG (Procedural Content Generation) APIs used in Vibeheim, along with migration guidance from legacy pre-5.6 patterns.

**Engine Version Policy:** Vibeheim requires **UE 5.6.x only**. This is enforced at compile time via `PCGVersionGuard.h`.

## Version Enforcement

### Compile-Time Guards

All PCG-related code must include the version guard header:

```cpp
#include "PCGVersionGuard.h"
```

This header provides:
- Static assertion for UE 5.6.x requirement
- PCG module availability check
- `VHM_PCG_ENABLED` flag for conditional compilation
- Scheduler API signature verification

### Build Configuration

**Client/Editor Builds:**
- `VHM_PCG_ENABLED=1` (default)
- Full PCG scheduler path active
- HISM fallback available on errors

**Server Builds:**
- `VHM_PCG_ENABLED=0` (set in Build.cs)
- HISM-only path, no PCG dependencies
- Logical spawn data output only

## UE 5.6 PCG APIs

### Core Subsystem

**UPCGSubsystem** - Main entry point for PCG operations

Key methods:
```cpp
// Schedule graph execution with component context (UE 5.6 scheduler)
FPCGTaskId ScheduleGraph(
    UPCGComponent* SourceComponent,
    FPCGTaskId TaskId,
    const FPCGStackContext& StackContext
);

// Query task status
bool IsTaskComplete(FPCGTaskId TaskId) const;

// Cancel/release tasks
void CancelTask(FPCGTaskId TaskId);
void ReleaseTask(FPCGTaskId TaskId);
```

### Metadata System

**UPCGMetadata** - Attribute storage for PCG data

Key methods:
```cpp
// Create typed attributes
template<typename T>
FPCGMetadataAttributeBase* CreateAttribute(FName AttributeName, T DefaultValue);

// Set attribute values
template<typename T>
void SetValue(PCGMetadataEntryKey EntryKey, FName AttributeName, const T& Value);

// Get attribute values
template<typename T>
bool GetValue(PCGMetadataEntryKey EntryKey, FName AttributeName, T& OutValue) const;
```

**PCGMetadataEntryKey** - Handle for metadata entries
- Opaque key type for accessing point/param metadata
- Created when adding points or parameters

### Data Types

**UPCGParamData** - Parameter/attribute data container
```cpp
UPCGParamData* ParamData = NewObject<UPCGParamData>(Outer);
UPCGMetadata* Metadata = ParamData->MutableMetadata();
// Set attributes via Metadata->CreateAttribute<T>() and SetValue<T>()
```

**UPCGPointData** - Point cloud data container
```cpp
UPCGPointData* PointData = NewObject<UPCGPointData>(Outer);
TArray<FPCGPoint>& Points = PointData->GetMutablePoints();
// Add points, then attach metadata
UPCGMetadata* Metadata = PointData->MutableMetadata();
```

### Execution Dependency Pins

**UPCGSettings::HasExecutionDependencyPin()** - Check if node requires explicit ordering
```cpp
if (Settings->HasExecutionDependencyPin())
{
    // This node needs explicit dependency wiring for deterministic execution
    // Especially important for: Get Landscape Data, Get Actor Data, custom nodes
}
```


### Vibeheim Wrapper Types

- `FPCGInputSet`, `FPCGOutputSet`, and `FPCGScheduleResult` live in `Source/Vibeheim/WorldGen/Public/Services/PCGWorldServiceTypes.h`. They gate all PCG inputs/outputs behind a stable map-based API and keep the transient data alive with `TStrongObjectPtr` until the scheduler releases the task.
- `EPCGTaskState` and `FPCGTaskContext` track the scheduler lifecycle (Scheduled -> Completing -> Completed -> Released, or Abandoned). Context stores the owning world/tile plus strong refs so GC does not collect parameter data mid-run.
- `VHMPCGAttr` namespace centralises attribute `FName` constants (`TileSeed`, `BiomeWeight`, `StaticMesh`, etc.) so call-sites never hardcode literal strings.

### FPCGSchedulerExecutor helper

- Implemented in `Source/Vibeheim/WorldGen/Private/Services/PCGSchedulerExecutor.{h,cpp}`. Wraps `UPCGSubsystem::ScheduleGraph` with validation, task tracking, and synchronous convenience helpers used by `UPCGWorldService`.
- Validates pin names and data types against the graph input node. If a pin is unmatched it falls back to insertion order and emits a warning so designers know pin renames break determinism.
- Builds `FPCGScheduleGraphParams` with the biome component as execution source and injects data through a lightweight `FVibeheimPCGInputElement`.
- Tracks each task in a `TMap<FPCGTaskId, FScheduledTask>` with start time, cached outputs, and abandon/release state guarantees (`ReleaseTask` is idempotent, `AbandonTask` cancels and clears backpressure immediately).
- Synchronous helper `RunGraphSync` polls with frame-friendly cadence controlled by `vhm.pcg.poll_ms` (default 12 ms) and fails fast after `vhm.pcg.timeout_ms` (default 5000 ms). Both CVars are documented for tuning and logged on timeout.
- Extraction wraps `Subsystem.GetOutputData` and converts the result into `FPCGOutputSet`, summing point counts and preserving warnings/errors for the caller. CPU profiler markers (`PCG_Schedule`, `PCG_Extract`) bracket scheduler calls for tracing.
- `UPCGWorldService` anchors execution through a hidden `PCGAnchor` actor and per-biome `UPCGComponent` instances to supply component context to the scheduler and keep hierarchical generation intact.

## Migration Table: Legacy → UE 5.6

| Legacy API (Pre-5.6) | UE 5.6 Equivalent | Notes |
|---------------------|-------------------|-------|
| `UPCGSubsystem::RunGraph()` | `UPCGSubsystem::ScheduleGraph()` | Async scheduler, requires component context |
| `UPCGSubsystem::Wait()` | Poll `IsTaskComplete()` | Frame-paced polling, never busy-spin |
| `UPCGSubsystem::GetGraphOutput()` | `GetTaskOutput()` via executor | Extract via accessor APIs |
| `UPCGSubsystem::Release()` | `ReleaseTask()` | Explicit cleanup |
| `FPCGDataCollection` | `FPCGInputSet`/`FPCGOutputSet` | Wrapper types in `PCGWorldServiceTypes.h` |
| `FPCGMetadata` | `UPCGMetadata` | UObject-based, GC-managed |
| Direct attribute access | Accessor APIs (`GetValue<T>`, `SetValue<T>`) | Type-safe templates |
| Implicit execution order | Execution Dependency pins | Wire explicitly for determinism |

## Call Site Inventory

### Files Using PCG APIs

1. **PCGWorldService.cpp** - Primary PCG integration
   - `CreateTileParameterData()` - Parameter assembly
   - `CreateTilePointData()` - Point data assembly
   - `ExtractInstancesFromPointData()` - Output extraction
   - `GeneratePCGContent()` - Main execution path

2. **WorldGenManager.cpp** - High-level coordination
   - Biome graph management
   - Component lifecycle
   - Feature flag checks

3. **TileStreamingService.cpp** - Streaming integration
   - Budget enforcement
   - Concurrent task tracking
   - Tile load/unload coordination

4. **Tests/** - Validation and regression tests
   - Scheduler behavior tests
   - Input/output validation tests
   - Determinism tests

### Legacy API Cleanup

All legacy API usage has been removed or wrapped. CI enforces this via automated checks.

**CI Workflow:** `.github/workflows/pcg-legacy-api-detection.yml`

The CI workflow performs three checks:

1. **Legacy API Detection** - Fails if legacy APIs appear outside wrapper implementation
2. **PCGVersionGuard.h Existence** - Verifies version guard header exists
3. **PCGVersionGuard.h Includes** - Warns if PCG files don't include the guard

**Legacy API Patterns Detected:**
- `RunGraph` - Legacy synchronous execution
- `GetGraphOutput` - Legacy output retrieval
- `FPCGDataCollection` - Legacy data container
- `FPCGMetadata` - Legacy metadata (replaced by `UPCGMetadata`)

**Exclusions:**
- `PCGSchedulerExecutor.cpp` - Internal wrapper may use legacy types for scheduler interop
- `Intermediate/` and `Binaries/` directories - Build artifacts
- Third-party/engine code - Not in scope

**Local Validation:**

Run the validation script locally before committing:

```powershell
# Windows PowerShell
.\Scripts\validate_pcg_apis.ps1

# Fail on detection (for CI simulation)
.\Scripts\validate_pcg_apis.ps1 -FailOnLegacyAPI

# Generate report file
.\Scripts\validate_pcg_apis.ps1 -FailOnLegacyAPI -OutputFile pcg_validation.txt
```

```bash
# Linux/Mac
pwsh Scripts/validate_pcg_apis.ps1
```

**CI Behavior:**
- **On Push/PR:** Automatically runs on changes to WorldGen module files
- **On Failure:** Blocks merge, displays detected legacy API usage with file/line numbers
- **On Success:** Confirms all code uses UE 5.6 scheduler APIs

## Scheduler Execution Model

### Task Lifecycle

```
Scheduled → Completing → Completed → Released
         ↘ Abandoned → Released
```

**States:**
- `Scheduled` - Task submitted to scheduler
- `Completing` - Execution in progress
- `Completed` - Execution finished successfully
- `Abandoned` - Cancelled before completion (tile unload)
- `Released` - Resources freed, task handle invalid

### Component Context Requirement

**Critical:** Always pass `UPCGComponent*` to `ScheduleGraph()`:

```cpp
UPCGComponent* Component = GetOrCreateBiomeComponent(BiomeType);
FPCGTaskId TaskId = PCGSubsystem->ScheduleGraph(
    Component,  // Required for hierarchical generation and getter context
    TaskId,
    StackContext
);
```

Without component context:
- Hierarchical generation breaks
- Input-less getters (Get Landscape Data, Get Actor Data) fail
- Grid-level scoping incorrect

### Object Lifetime Management

**Input Data GC Safety:**

```cpp
// Create with persistent Outer (PCGAnchor actor, not transient)
UPCGParamData* ParamData = NewObject<UPCGParamData>(PCGAnchorActor);
ParamData->SetFlags(RF_Transient);

// Keep alive until task completes
FPCGTaskContext Context;
Context.InputDataRefs.Add(TStrongObjectPtr<UPCGData>(ParamData));
ActiveTasks.Add(TaskId, Context);

// Release in ReleaseTask()
Context.InputDataRefs.Empty();
```

**Never:**
- Use transient Outer for input data (GC will collect mid-execution)
- Use `AddToRoot()` unless absolutely necessary
- Forget to release `TStrongObjectPtr` refs

## Runtime Policies

### Frustum Culling

UE 5.6 adds runtime frustum culling to the scheduler:

```cpp
// Enable in project settings or CVars
vhm.pcg.frustum.enable = 1
vhm.pcg.frustum.margin = 500.0  // Units beyond frustum
```

**Benefits:**
- Skip off-camera generation
- Reduce CPU/memory pressure
- Improve streaming performance

**Configuration:**
- Per-biome margin overrides via `FrustumCullingMarginByLOD`
- Fallback order: exact key → biome → global

### CRC Caching

UE 5.6 caches per-data CRCs to skip redundant work:

**Telemetry:**
- `NodesExecuted` - Fresh computation
- `NodesCached` - CRC hit, skipped

**Validation:**
- Confirm `NodesCached > 0` on partial graph edits
- Measure iteration speed-up (target: 2-5x)

### Concurrency Control

```cpp
// Limit concurrent tasks to stay within frame budget
vhm.pcg.max_concurrent = 4  // Default

// Check before scheduling
if (ActiveTasks.Num() >= MaxConcurrentPCGTasks)
{
    // Defer or fallback
}
```

**Abandoned tasks don't count toward limit** - immediate backpressure relief on tile unload.

## Validation & Diagnostics

### Console Commands

**wg.pcg.validate <Biome|GraphPath>**
- Validates graph structure
- Checks required attributes
- Detects unwired dependency pins
- Prints remediation steps

**wg.pcg.showdeps <GraphPath>**
- Lists nodes with execution dependency pins
- Highlights unwired pins
- Provides wiring guidance

### Validation Checks

1. **PCG Subsystem Init** - Verify subsystem available
2. **Graph Asset Valid** - Check graph loaded
3. **Required Attributes** - Validate input/output attributes
4. **Dependency Pin Wiring** - Ensure deterministic ordering
5. **Scheduler Availability** - Confirm scheduler ready
6. **Frustum Policy Coherence** - Check runtime settings

### Error Messages

Structured errors include:
- Graph name and asset path
- Biome and tile context
- Expected vs actual types (for attribute mismatches)
- Remediation steps

Example:
```
[PCG] Validation failed for graph '/Game/PCG/Biomes/Forest_PCG'
  Biome: Forest, Tile: (10, 5)
  Error: Node 'Get Landscape Data' has unwired execution dependency pin
  Fix: Wire Execution Dependency pin to enforce deterministic ordering
  Expected attribute 'BiomeWeight' (float) not found in output
  Fix: Add 'BiomeWeight' attribute to point data in graph
```

## Fallback Behavior

### Automatic Fallback Triggers

1. **Headless/No World** - `GetWorld() == nullptr`
2. **Dedicated Server** - `WITH_SERVER_CODE && !IsRunningClient()`
3. **Missing Subsystem** - `GetSubsystem<UPCGSubsystem>() == nullptr`
4. **Scheduler Timeout** - Task exceeds `vhm.pcg.timeout_ms`
5. **Scheduler Error** - Any scheduler failure

### Fallback Path

```cpp
#if VHM_PCG_ENABLED
if (!bEnablePCGGraphs || !PCGSubsystem)
{
    return GenerateFallbackContent(TileCoord, BiomeType, HeightData, TileMetrics);
}
// PCG scheduler path...
#else
// HISM-only path (server builds)
return GenerateFallbackContent(TileCoord, BiomeType, HeightData, TileMetrics);
#endif
```

**Fallback maintains:**
- Feature parity with PCG path
- Performance budgets
- Streaming behavior
- Deterministic output

## Testing & Validation

### Test Coverage

1. **Scheduler Tests** (`PCGSchedulerTests.cpp`)
   - Schedule/complete/timeout/error paths
   - GT marshalling
   - Task cancellation

2. **Input/Output Tests**
   - Attribute validation
   - Type safety
   - Empty output handling

3. **Integration Tests**
   - End-to-end tile generation
   - Fallback triggers
   - Concurrent task limits

4. **Performance Tests**
   - CRC caching validation
   - Scheduler overhead measurement
   - Budget compliance

5. **Determinism Tests**
   - Seed control
   - Dependency pin wiring
   - Transform hash matching

### Validation Workflow

1. **Compile** - No legacy API usage outside wrapper
2. **PIE Smoke** - Generate one tile per biome
3. **wg.pcg.validate** - Run on all biome graphs
4. **Budget/Perf** - Confirm CRC caching reduces re-gen time
5. **Determinism** - Same seed → same output (hash match)
6. **Fallback** - Disable PCG → HISM still works
7. **Unload** - Tile eviction doesn't leak tasks
8. **Dedicated Server** - No PCG code paths touched

## Common Pitfalls

1. **Scheduling without component context** → Hierarchical generation breaks
2. **No task cancellation** → Memory leaks on tile unload
3. **Wrong Outer for input data** → GC collects mid-execution
4. **Missing GT assertions** → Race conditions in ActiveTasks
5. **Grep blocks wrapper** → CI fails on legitimate internal usage
6. **Ignoring CRC counters** → Can't validate performance wins
7. **No seed control** → Non-deterministic tests
8. **Arithmetic on pointers** → Compile errors or UB

## References

- **Requirements:** `.kiro/specs/pcg-ue56-migration/requirements.md`
- **Design:** `.kiro/specs/pcg-ue56-migration/design.md`
- **Tasks:** `.kiro/specs/pcg-ue56-migration/tasks.md`
- **Checklist:** `.kiro/specs/pcg-ue56-migration/MIGRATION_CHECKLIST.md`
- **Epic Docs:** https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-framework-in-unreal-engine

## Version History

- **2025-01-05:** Initial UE 5.6 migration documentation
- Engine version: UE 5.6.x
- PCG plugin: Engine-bundled version




