# Implementation Plan

## Overview

This implementation plan breaks down the PCG UE 5.6 migration into discrete, manageable coding tasks. Each task builds incrementally on previous steps, prioritizing core functionality and marking optional testing tasks. The plan follows the 5-phase migration roadmap defined in the design document.

## Task List

- [x] 0. Engine/Plugin Guards & CI Fail-Fast






  - Add engine version check and CI validation
  - _Requirements: All (prerequisite)_

- [x] 0.1 Add engine version static assertion


  - Create `Source/Vibeheim/WorldGen/Public/PCGVersionGuard.h` with static assertions (included by every TU touching PCG)
  - Add `static_assert(ENGINE_MAJOR_VERSION==5 && ENGINE_MINOR_VERSION==6, "Requires UE 5.6.x only");`
  - Add compile-time API existence check: `#if __has_include("PCGSubsystem.h") && defined(WITH_PCG)`
  - Verify PCG module is present in `*.Build.cs` (runtime builds too)
  - For server targets in `*.Build.cs`: `PublicDefinitions.Add("VHM_PCG_ENABLED=0");` to ensure HISM-only path compiles without PCG module
  - Add compile-time gate `#if !VHM_PCG_ENABLED` with `#else` stub for HISM path (keeps codepaths symmetrical for lint/static-analysis)
  - Document version policy explicitly in `/Docs/PCG-5.6.md`
  - _Requirements: All (prerequisite)_

- [x] 0.2 Add CI legacy API detection


  - Add CI step: `grep` fail if `RunGraph|GetGraphOutput|FPCGDataCollection|FPCGMetadata` appear in `Source/Vibeheim/WorldGen` (exclude `PCGSchedulerExecutor.cpp` wrapper)
  - Scope greps to exclude third-party/engine and the scheduler wrapper implementation
  - Add automated check for legacy includes/usages
  - Document CI setup and exclusion rules in `/Docs/PCG-5.6.md`
  - _Requirements: 1.4_

- [x] 1. Phase 1: Foundation - API Inventory & Scheduler Executor
  - Create API documentation and implement core scheduler helper
  - _Requirements: 1, 2_

- [x] 1.1 Create PCG 5.6 API inventory document
  - Create `/Docs/PCG-5.6.md` documenting public APIs: `UPCGSubsystem::ScheduleGraph`, task handle management, `UPCGMetadata`, `PCGMetadataEntryKey`, attribute accessors
  - Document migration table mapping legacy APIs (`RunGraph`/`Wait`/`GetGraphOutput`/`Release`, `FPCGMetadata`, `FPCGDataCollection`) to 5.6 equivalents
  - Cross-reference all call sites in `PCGWorldService.cpp`, `WorldGenManager.cpp`, `TileStreamingService.cpp`
  - Document Execution Dependency pin detection via `UPCGSettings::HasExecutionDependencyPin()`
  - Add "grep cleanup" subtask removing all legacy includes/usages (auto-check in CI)
  - Note: document only public fields you set on `FPCGScheduleGraphParams` (don't enumerate engine internals)
  - _Requirements: 1.1, 1.2, 1.3, 1.4_

- [x] 1.2 Create shared types header
  - Create `Source/Vibeheim/WorldGen/Public/Services/PCGWorldServiceTypes.h`
  - Move `FPCGTileMetrics` struct from private `.cpp` to shared header
  - Define `FPCGInputSet` struct with optional pin-name mapping: `TMap<FName, TObjectPtr<UPCGData>> Inputs` (prefer map over array for stability)
  - Define `FPCGOutputSet` struct with `TArray<TObjectPtr<UPCGData>> Outputs`
  - Define `FPCGScheduleResult` struct with success flag, point count, warnings, errors, execution time, and output set
  - Define `EPCGTaskState` enum: `{Scheduled, Completing, Completed, Released, Abandoned}` for state machine
  - Define `FPCGTaskContext` struct: `{UWorld*, FTileCoord, FPCGTaskId, EPCGTaskState State, TArray<TStrongObjectPtr<UPCGData>> InputDataRefs}` for lifecycle + GC safety
  - Add `VHMPCGAttr` namespace with all attribute name constants
  - Document `TileSeed` as `int32` with mixing formula: `Hash(TileX, TileY, BiomeId, GlobalSeed)`
  - _Requirements: 1.2, 3.4_

- [x] 1.3 Implement FPCGSchedulerExecutor helper class
  - Create `Source/Vibeheim/WorldGen/Private/Services/PCGSchedulerExecutor.h` and `.cpp`
  - Document exact `ScheduleGraph` overload used: `UPCGSubsystem::ScheduleGraph(UPCGComponent*, FPCGTaskId, ...)` with source component for hierarchical/getter behavior
  - Build `FPCGScheduleGraphParams` with frustum culling policy and runtime scheduler settings
  - Implement `ScheduleGraphAsync`: validate pin names exist on graph, check type compatibility, fail fast with clear log if missing
  - Emit warning if falling back to index ordering (designers may reorder pins)
  - Implement `IsTaskComplete` method: check task completion status via subsystem
  - Implement `GetTaskOutput` method: fill `FPCGOutputSet`; if no public nodes executed/cached counters, use proxy (elapsed vs historical median) and label clearly
  - Implement `ReleaseTask` method: guard with state check (no-op unless `Completed || Abandoned`), never runs twice, transitions to `Released`
  - Implement `AbandonTask` method: transition `Scheduled → Abandoned`, remove from backpressure counts immediately
  - On completion callback: if `Abandoned`, skip extraction, only release handles, log terse "abandoned" line
  - Implement `RunGraphSync` method (editor/tests only): schedule, poll with frame-paced cadence (never busy-spin), timeout, return `FPCGScheduleResult`
  - Add CVars: `vhm.pcg.poll_ms` (default 8-16ms), `vhm.pcg.timeout_ms` (default 5000ms); log values used
  - Add build guards: `#if WITH_EDITOR || WITH_AUTOMATION_TESTS` for `RunGraphSync`
  - Create `FGTCall` helper utility to funnel all completions through `AsyncTask(ENamedThreads::GameThread, ...)`
  - Wrap schedule/extract with `TRACE_CPUPROFILER_EVENT_SCOPE(PCG_Schedule)` / `TRACE_CPUPROFILER_EVENT_SCOPE(PCG_Extract)`
  - Add structured error logging with task ID, graph name, graph asset path, elapsed time
  - _Requirements: 2.1, 2.2, 6.1, 6.2_

- [x] 1.4 Write unit tests for FPCGSchedulerExecutor (NON-OPTIONAL - safety net)
  - Create `Source/Vibeheim/WorldGen/Private/Tests/PCGSchedulerTests.cpp`
  - Use `FPCGInputSet`/`FPCGOutputSet` (not `FPCGDataCollection`)
  - Test `RunGraphSync` with valid graph (success case)
  - Test `RunGraphSync` with timeout scenario
  - Test `RunGraphSync` with invalid graph
  - Test `RunGraphSync` with subsystem unavailable
  - Test `ScheduleGraphAsync` returns valid task handle
  - Test `IsTaskComplete` polling behavior
  - Test `GetTaskOutput` retrieves correct data
  - Test `ReleaseTask` cleanup
  - Test `CancelTask` or abandon-on-arrival behavior
  - Test GT marshalling for completion callbacks
  - _Requirements: 10.1_

- [x]* 1.5 Setup log category and editor commands scaffold
  - Add `UE_DECLARE_LOG_CATEGORY_EXTERN(LogPCGWorldService, Log, All);` in header
  - Add `UE_DEFINE_LOG_CATEGORY(LogPCGWorldService);` in cpp
  - Register console commands in `#if WITH_EDITOR` blocks
  - _Requirements: 9.2_

- [x] 2. Phase 2: Data Contract - Input Assembly & Output Extraction
  - Update input/output methods to use UE 5.6 metadata APIs
  - _Requirements: 3, 4_

- [x] 2.1 Update CreateTileParameterData to UPCGMetadata
  - Modify `PCGWorldService::Private::CreateTileParameterData` signature to use `FPCGTileMetrics` from shared header
  - Replace `FPCGMetadata` usage with `UPCGMetadata`
  - Use `PCGMetadataEntryKey` for entry management
  - Use template attribute helpers: `Metadata->CreateAttribute<T>`, `Metadata->SetValue<T>`
  - Create with safe Outer (persistent anchor actor or subsystem), mark `RF_Transient`
  - Keep alive until task completes (root or `TStrongObjectPtr`)
  - Add `TileSeed` attribute to parameter data for determinism
  - Validate attribute creation success; log errors if creation fails
  - Add attributes (final list with type map):
    - **Floats**: `AverageHeight`, `MinHeight`, `MaxHeight`, `AverageSlope`, `MaxSlope`, `WaterCoverageRatio`, `AverageAboveWater`, `AverageBelowWater`, `MinAbsWaterDistance`, `SeaLevel`, `TileSize`
    - **Ints**: `TileX`, `TileY`, `TileSeed`, `BiomeId`
    - **Float [0..1]**: `BiomeWeight` (if param-level; else move to point-level)
  - _Requirements: 3.1, 3.2_

- [x] 2.2 Update CreateTilePointData to UPCGMetadata
  - Modify `PCGWorldService::Private::CreateTilePointData` to use `UPCGMetadata` accessor APIs
  - Use `PCGMetadataEntryKey` for point metadata entry creation
  - Create with safe Outer (persistent anchor actor or subsystem), mark `RF_Transient`
  - Keep alive via `TStrongObjectPtr` in `FPCGTaskContext` until task completes (don't use `AddToRoot` unless necessary)
  - Validate point metadata entry creation success
  - Call `RecomputeBounds()` once after all points/transforms are finalized (defer if multiple builders touch same point set)
  - Add `AverageSlope` attribute to point metadata
  - If `BiomeWeight` is per-point, set it here (not in param data)
  - _Requirements: 3.1, 3.2_

- [x] 2.3 Implement input validation helper
  - Create `ValidateInputAttributes` method in `PCGWorldService`
  - Check attribute names match expected canonical names
  - Check attribute types match expected types (float, int32, FVector, etc.)
  - Verify each canonical attribute is in the expected scope (ParamData vs PointData)
  - Log expected vs actual UE type names for mismatches (use `Property->GetCPPType()` or similar)
  - Return structured validation result with errors and warnings
  - Log detailed validation report on failure
  - _Requirements: 3.3, 3.5_

- [x] 2.4 Update ExtractInstancesFromPointData to 5.6 accessors
  - Modify `PCGWorldService::Private::ExtractInstancesFromPointData` to use accessor-based attribute retrieval
  - Use `Metadata->GetAttribute<T>(AttributeName, EntryKey, OutValue)` pattern
  - Add type safety checks: never perform arithmetic on ANY non-numeric attributes (`UObject*`, `FName`, `FString`, `FSoftObjectPath`, etc.)
  - Decide and document: `StaticMesh`/`Mesh` as `UStaticMesh*` or `FSoftObjectPath` for runtime (prefer soft paths for packaging/streaming)
  - Accept both `StaticMesh` and `Mesh`; prefer `UStaticMesh*` if both appear; never treat pointers as numeric
  - Handle missing attributes gracefully: log warning and use default value
  - Handle empty output: return 0 instances with context logging
  - Log attribute access errors with UE type name + friendly label (e.g., "float (double ok)", "int32", "UObject ptr") + first offending node name
  - Sort instances by stable key (`InstanceId`, else position) before hashing to avoid nondeterministic ordering false-fails
  - Compute a 64-bit hash of output transforms with `QuantizeTransformForHash()` (mm precision for pos, 1e-4 for quats)
  - Extract attributes: `StaticMesh`/`Mesh`, `InstanceScale`, `InstanceRotation`, `IsActive`, `InstanceId`
  - _Requirements: 4.1, 4.2, 4.3, 4.5_

- [x] 2.5 Document canonical attributes
  - Create `/Vibeheim/_Assets/Data/PCG/Docs/Attributes.md`
  - Document all canonical attribute names used by the system
  - Document expected types for each attribute (float, int32, FVector, FRotator, FSoftObjectPath, etc.)
  - Document which attributes are required vs optional
  - Document attribute semantics and valid value ranges
  - Provide examples of correct attribute usage in PCG graphs
  - _Requirements: 3.4_

- [ ]* 2.6 Write unit tests for input assembly and output extraction
  - Test `CreateTileParameterData` creates all required attributes
  - Test `CreateTileParameterData` attribute types are correct
  - Test `CreateTilePointData` initializes metadata correctly
  - Test `CreateTilePointData` bounds are initialized
  - Test `ValidateInputAttributes` detects missing attributes
  - Test `ValidateInputAttributes` detects type mismatches
  - Test `ValidateInputAttributes` detects scope mismatches (param vs point)
  - Test `ExtractInstancesFromPointData` with valid data
  - Test `ExtractInstancesFromPointData` with empty output
  - Test `ExtractInstancesFromPointData` with missing attributes
  - Test `ExtractInstancesFromPointData` with type mismatches
  - _Requirements: 10.1_

- [x] 2.7 Attribute scope audit
  - Confirm which attributes are Param vs Point in `/Vibeheim/_Assets/Data/PCG/Docs/Attributes.md`
  - Update builders (`CreateTileParameterData`, `CreateTilePointData`) accordingly
  - Document scope rationale for each attribute
  - _Requirements: 3.4_

- [x] 3. Phase 3: Runtime Integration - Policies & Component Lifecycle
  - Integrate runtime policies and fix component lifecycle
  - _Requirements: 5, 6, 7_

- [x] 3.1 Add frustum culling settings
  - Add `bEnableFrustumCulling` property to `FWorldGenConfig` or `UVibeheimSettings`
  - Add `FrustumCullingMargin` property (default 500.0f)
  - Add `FrustumCullingMarginByLOD` as `TMap<FName, float>` keyed by layer/partition name; fallback order: exact key → biome → global
  - Add `MaxConcurrentPCGTasks` property (default 4)
  - Create CVars: `vhm.pcg.max_concurrent`, `vhm.pcg.frustum.enable`, `vhm.pcg.frustum.margin`
  - Mirror CVars into settings on world start and add `OnChanged` handlers to update live
  - Explicitly reference Runtime Gen Scheduler as owner of frustum policy (document in comments/tooltips)
  - Expose settings in editor UI with appropriate categories and tooltips
  - _Requirements: 5.2, 5.3_

- [x] 3.2 Update GeneratePCGContent to use scheduler
  - Early-out if `!bEnablePCGGraphs` → HISM fallback: `if (!WorldGenSettings.bEnablePCGGraphs) { return GenerateFallbackContent(...); }`
  - Compile-time gate for HISM-only builds: `#if !VHM_PCG_ENABLED`
  - Replace `PCGSubsystem->RunGraph` with `FPCGSchedulerExecutor::ScheduleGraphAsync`
  - Build `FPCGInputSet` from `CreateTileParameterData` and `CreateTilePointData`
  - Validate inputs before scheduling using `ValidateInputAttributes`
  - Configure `FPCGScheduleGraphParams` with frustum culling settings (populate only public fields)
  - Track task handle in `ActiveTasks` set
  - Poll for completion using `FPCGSchedulerExecutor::IsTaskComplete`
  - On completion, call `FPCGSchedulerExecutor::GetTaskOutput`
  - Extract instances from output using updated `ExtractInstancesFromPointData`
  - Release task handle using `FPCGSchedulerExecutor::ReleaseTask`
  - Remove task from `ActiveTasks` set (ensure all ops on Game Thread)
  - On timeout: log error with elapsed time, release handle, trigger HISM fallback
  - On scheduler failure: log error with graph/biome/tile context, trigger HISM fallback
  - _Requirements: 2.1, 2.2, 2.3, 5.1, 5.4_

- [x] 3.3 Implement concurrent task tracking with cancellation
  - Add `TMap<FPCGTaskId, FPCGTaskContext> ActiveTasks` member to `PCGWorldService` (tracks world/tile/state/input refs)
  - Implement `CanScheduleNewTask` method: check `ActiveTasks.Num() < MaxConcurrentPCGTasks` (abandoned tasks don't count toward backpressure)
  - Implement `TrackTask` method: add task context with `TStrongObjectPtr` refs to input data, transition to `Scheduled`, `check(IsInGameThread());`
  - Implement `ReleaseTrackedTask` method: drop input data refs, remove from `ActiveTasks`, transition to `Released`, `check(IsInGameThread());`
  - Implement `AbandonTasksForTile` method: transition `Scheduled → Abandoned`, remove from backpressure immediately
  - Implement `AbandonTasksForWorld` method: mark all tasks for world as abandoned on world cleanup
  - Hook `AbandonTasksForWorld` into `FWorldDelegates::OnWorldCleanup`
  - Update `GeneratePCGContent` to check `CanScheduleNewTask` before scheduling
  - Update `GeneratePCGContent` to call `TrackTask` after successful schedule
  - Update `GeneratePCGContent` to call `ReleaseTrackedTask` after completion/error, skip extraction if `Abandoned`
  - Assert GT for `RegisterComponent()`/`UnregisterComponent()` and any actor/HISM touching
  - _Requirements: 5.3_

- [x] 3.4 Fix BiomePCGComponents map type
  - Change `BiomePCGComponents` type from `TMap<EBiomeType, TObjectPtr<UObject>>` to `TMap<EBiomeType, TObjectPtr<UPCGComponent>>`
  - Store `BiomePCGGraphs` as `TSoftObjectPtr<UPCGGraph>`
  - Add "ensure loaded" helper with time budget so async loads don't stall streaming thread
  - Log when exceeding budget and falling back to sync load (Editor only): include asset path and time taken
  - Update all references to use correct type
  - _Requirements: 7.4_

- [x] 3.5 Implement component lifecycle methods
  - Spawn dedicated hidden, never-streamed "PCGAnchor" actor in persistent level if `AWorldGenManager` isn't guaranteed persistent
  - Implement `GetOrCreateBiomeComponent` method: use PCGAnchor actor as Outer for `UPCGComponent`, set graph, register with world, cache in map
  - Pass `UPCGComponent*` to scheduler as source component context (required for hierarchical generation, grid level scoping, getters)
  - Implement `DestroyBiomeComponent` method: unregister component, null map entry to avoid dangling refs
  - Implement `CleanupAllComponents` method: iterate all components, unregister, clear maps
  - Bind `FWorldDelegates::OnWorldCleanup` to `CleanupAllComponents()`
  - Call `CleanupAllComponents` in `PCGWorldService` destructor or shutdown method
  - Update `GeneratePCGContent` to use `GetOrCreateBiomeComponent` when component context is needed (treat as hard requirement)
  - _Requirements: 7.1, 7.2, 7.3_

- [ ]* 3.6 Write unit tests for component lifecycle
  - Test `GetOrCreateBiomeComponent` creates and registers component
  - Test `GetOrCreateBiomeComponent` reuses existing component
  - Test `DestroyBiomeComponent` unregisters and removes component
  - Test `CleanupAllComponents` cleans up all components
  - Test multiple biome components can coexist
  - _Requirements: 10.1_

- [x] 4. Phase 4: Validation & Diagnostics
  - Enhance validation and add diagnostic tooling
  - _Requirements: 8, 9_

- [x] 4.1 Enhance ValidatePCGGraph method
  - Gate editor-only checks with `#if WITH_EDITOR` (graph traversal is editor-facing)
  - Check PCG subsystem is initialized
  - Check graph asset is valid and loaded
  - Check required attributes are present in graph outputs
  - Check attribute types match expected types
  - Check nodes with `HasExecutionDependencyPin() == true` are wired (especially getters: Get Landscape Data, Get Actor Data)
  - **ERROR** (not warning) if node advertises execution-dependency pin AND has side effects (custom nodes) but isn't wired
  - Check scheduler is available
  - Check frustum culling policy is coherent with runtime settings
  - Only rely on public `UPCGSettings::HasExecutionDependencyPin()` and graph wiring—resist sniffing private node internals
  - Don't introspect private engine fields—use only public APIs
  - Provide "auto-repair" suggestions:
    - "Wire Execution Dependency pin for Get Landscape Data/Get Actor Data to enforce deterministic ordering"
    - "Move BiomeWeight to point scope; param scope is ignored by node X/Y"
  - Return `FPCGGraphValidationResult` with errors, warnings, missing attributes, unwired dependency nodes
  - _Requirements: 9.1_

- [x] 4.2 Implement wg.pcg.showdeps console command
  - Wrap in `#if WITH_EDITOR`
  - Register console command `wg.pcg.showdeps <GraphPath>` (accept loaded object path or asset path)
  - Print resolved graph name and asset path at top (screenshots are self-contained)
  - Load graph from path
  - Iterate all nodes in graph
  - For each node, check if `Settings->HasExecutionDependencyPin()` is true
  - Check if dependency pin is wired to another node
  - Log warning for nodes with unwired dependency pins
  - Print one-line summary footer: "Total Nodes: N, Dep Pins: D, Unwired: U"
  - _Requirements: 2.4, 9.1_

- [x] 4.3 Implement wg.pcg.validate console command
  - Wrap in `#if WITH_EDITOR`
  - Register console command `wg.pcg.validate <Biome|GraphPath>`
  - If biome specified, resolve graph for that biome
  - Call `ValidatePCGGraph` with resolved graph
  - Print validation result: errors, warnings, missing attributes, unwired nodes
  - Print remediation steps for each error/warning
  - _Requirements: 9.2_

- [x] 4.4 Add structured telemetry logging
  - Add telemetry struct: `{Biome, GraphAssetPath, Tile, TaskId, SubmitTs, StartTs, DoneTs, Status, PointsOut, NodesExecuted, NodesCached, FallbackUsed}`
  - Add `TRACE_CPUPROFILER_EVENT_SCOPE(PCG_TileGenerate)` around schedule/extract for Insights workflows
  - Emit `{Biome, Graph, Tile, TaskId}` counters and a latency histogram
  - Expose "nodes executed vs cached" counters from scheduler stats; if not publicly available, use proxy (elapsed vs per-graph historical median) and label clearly as proxy
  - Log telemetry on task submission: biome, graph asset path, tile, task ID, submit timestamp
  - Log telemetry on task start: start timestamp (if available from scheduler)
  - Log telemetry on task completion: done timestamp, status (success/timeout/error), output point count, nodes executed/cached
  - Log telemetry on fallback trigger: fallback used flag, reason
  - Optional: write CSV rows to `Saved/PCG/pcg_tasks.csv` using `IFileManager::CreateFileWriter` with `FILEWRITE_Append`, buffer in memory, flush on timer or `OnWorldCleanup`, add header row once
  - Gate CSV writes behind DevOnly CVar `vhm.pcg.telemetry.csv` to avoid SSD thrash in perf tests
  - Add per-biome rolling 50th/95th/99th percentile in memory for regression spotting
  - Wrap `ensure()`-heavy logs in `#if !UE_BUILD_SHIPPING`
  - Aggregate telemetry per biome/graph for performance analysis
  - _Requirements: 9.5_

- [x] 4.5 Implement headless/fallback detection
  - Detect headless context: `GetWorld() == nullptr`
  - Detect dedicated server: `WITH_SERVER_CODE && !IsRunningClient()`
  - Detect missing PCG subsystem: `World->GetSubsystem<UPCGSubsystem>() == nullptr`
  - Auto-trigger HISM fallback in headless/dedicated server mode with feature parity
  - In Dedicated Server, output logical spawn data only (IDs/transforms) and skip renderer/HISM paths entirely
  - Log warnings for missing graphs/components but don't block tile generation
  - Maintain performance budgets in fallback mode
  - Maintain streaming behavior consistency in fallback mode
  - _Requirements: 8.1, 8.2, 8.5_

- [ ]* 4.6 Write integration tests for validation and diagnostics
  - Test `ValidatePCGGraph` with valid graph
  - Test `ValidatePCGGraph` with missing attributes
  - Test `ValidatePCGGraph` with unwired dependency pins
  - Test `wg.pcg.showdeps` command output
  - Test `wg.pcg.validate` command output
  - Test telemetry logging captures all required fields
  - Test headless mode triggers fallback
  - Test missing subsystem triggers fallback
  - _Requirements: 10.1_

- [ ] 5. Phase 5: Testing & Rollout
  - Port tests, add performance validation, write migration notes
  - _Requirements: 10, 11_

- [ ] 5.1 Port existing PCG tests to scheduler
  - Identify all existing PCG tests in `Source/Vibeheim/WorldGen/Private/Tests`
  - Update tests to use `FPCGSchedulerExecutor::RunGraphSync` instead of legacy APIs
  - Replace any `FPCGDataCollection` usage with `FPCGInputSet`/`FPCGOutputSet`
  - Update tests to validate against 5.6 metadata APIs
  - Ensure all tests pass with new scheduler path
  - _Requirements: 10.1_

- [ ] 5.2 Add integration tests for end-to-end generation
  - Test full tile generation with scheduler path
  - Test fallback trigger on forced scheduler failure
  - Test fallback trigger on forced timeout
  - Test concurrent task cap enforcement under load
  - Test frustum culling integration (on vs off comparison)
  - _Requirements: 10.2_

- [ ] 5.3 Add performance validation tests
  - Warm-up once before measuring to avoid shader compile skew
  - Test CRC caching reduces re-gen time on partial graph edits
  - Measure and log scheduling overhead vs legacy baseline
  - Measure and log memory usage (CPU and VRAM if telemetry available)
  - Validate scheduler overhead is within acceptable bounds
  - Validate CRC caching provides measurable speed-up
  - Save perf artifacts to CSV per run (map/biome columns) for trend tracking
  - _Requirements: 10.3_

- [ ] 5.4 Add regression test for Difference + GetActorData
  - Create test graph using Difference node with GetActorData input
  - Test if Difference node correctly culls points in 5.6
  - If regression detected, implement attribute-mask workaround: project cull volumes to float mask, filter via attribute
  - Validate workaround produces correct results
  - _Requirements: 10.4_

- [ ] 5.5 Add determinism test for dependency pin wiring
  - Create test graph with input-less getters (Get Landscape Data, Get Actor Data)
  - Test without dependency pin wiring: verify non-deterministic or incorrect results
  - Test with dependency pin wiring: verify deterministic, correct results
  - Seed both PCG component seed AND `TileSeed` metadata to avoid accidental seed drift between runs
  - Use transform hash helper with quantization: assert identical `(Biome, Tile, TileSeed)` → identical count and hash when dep pins wired
  - Validate `wg.pcg.showdeps` detects unwired pins
  - _Requirements: 2.2, 2.4_

- [ ] 5.6 Write designer migration notes
  - Create `/Docs/PCG-5.6-Designer-Guide.md`
  - Document when/why to wire Execution Dependency pin (especially for getters and timed sequences)
  - Include "before/after" graph wiring diagram for Execution-Dependency pins
  - Document canonical attribute names and types expected on inputs/outputs
  - Document runtime generation expectations: partitioning, frustum culling, concurrency caps
  - Provide examples of correct graph setup for common scenarios
  - Document common pitfalls and how to avoid them
  - _Requirements: 11.2_

- [ ] 5.7 Create verification checklist
  - Create checklist document: compile → PIE smoke test → `wg.pcg.validate` → budget/perf validation
  - Document expected results for each verification step
  - Document how to interpret validation errors and warnings
  - Document performance benchmarks and acceptable ranges
  - _Requirements: 11.3_

- [ ] 5.8 Create rollback plan documentation
  - Document rollback procedure: set `bEnablePCGGraphs = false`, restart editor/game
  - Document verification steps: confirm tile generation continues with HISM, confirm no scheduler errors in logs
  - Document that graphs/components remain loadable but inactive
  - Document that no data loss or corruption occurs during rollback
  - _Requirements: 11.4, 11.5_

- [ ] 5.9 Stage rollout behind feature flag
  - Ensure `bEnablePCGGraphs` flag is properly wired to all scheduler code paths
  - Test with flag enabled: scheduler path active
  - Test with flag disabled: HISM fallback active, no scheduler calls
  - Document flag location and how to toggle it
  - Plan gradual rollout: enable per map/biome, monitor for issues
  - _Requirements: 11.1_

- [ ]* 5.10 Write comprehensive test suite documentation
  - Document all test files and their coverage
  - Document how to run tests (unit, integration, performance)
  - Document expected test results and how to interpret failures
  - Document test data requirements and setup
  - Document how to add new tests for custom graphs/biomes
  - _Requirements: 10.5_

## Notes

- Tasks marked with `*` are optional testing tasks that can be skipped if time is limited
- Each task references the requirements it addresses for traceability
- Tasks are ordered to minimize dependencies and enable incremental progress
- Core functionality tasks are never marked as optional
- Testing tasks are marked as optional to prioritize shipping core features
- The implementation can proceed linearly through phases, or tasks within a phase can be parallelized if multiple developers are available


- [ ] 5.11 Document CVars and settings
  - Add CVar/Settings documentation to `/Docs/PCG-5.6-Designer-Guide.md`
  - List all CVars: `vhm.pcg.max_concurrent`, `vhm.pcg.frustum.enable`, `vhm.pcg.frustum.margin`
  - Document defaults and how they mirror project settings
  - Document live tuning workflow
  - _Requirements: 11.2_

- [ ] 5.12 Create test assets pack
  - Create one minimal partitioned PCG graph per biome
  - Create tiny heightmap/actor set for testing
  - Ensure tests don't depend on big content
  - Document test asset setup in `/Docs/PCG-5.6.md`
  - _Requirements: 10.1_

## Helper Code Snippets

### Transform Hash Helper (for determinism tests)
```cpp
static uint64 HashTransform(const FTransform& T)
{
    const FVector P = T.GetLocation();
    const FQuat   Q = T.GetRotation();
    const FVector S = T.GetScale3D();
    uint64 H = 1469598103934665603ull; // FNV offset
    auto Mix = [&](const void* Ptr, size_t Size){
        const uint8* B = static_cast<const uint8*>(Ptr);
        for (size_t i=0;i<Size;i++){ H ^= B[i]; H *= 1099511628211ull; }
    };
    Mix(&P, sizeof(P)); Mix(&Q, sizeof(Q)); Mix(&S, sizeof(S));
    return H;
}
```

### Feature Flag Early-Out Pattern
```cpp
if (!WorldGenSettings.bEnablePCGGraphs)
{
    return GenerateFallbackContent(TileCoord, BiomeType, HeightData, /*bLog*/false, TileMetrics);
}
```

### Game Thread Guard Pattern
```cpp
check(IsInGameThread()); // Use in TrackTask/ReleaseTrackedTask
```


- [ ] 5.13 Add headless/dedicated server build test
  - Assert server builds never touch PCG plugin code paths guarded by compile-time define
  - Test that `WITH_SERVER_CODE && !IsRunningClient()` triggers HISM fallback
  - Verify logical spawn data output (IDs/transforms) without renderer/HISM
  - _Requirements: 8.1_

## Additional Helper Code Snippets

### QuantizeTransformForHash (avoid float jitter)
```cpp
static FTransform QuantizeTransformForHash(const FTransform& T)
{
    // mm precision for position, 1e-4 for quaternion
    FVector P = T.GetLocation();
    P.X = FMath::RoundToFloat(P.X * 1000.0f) / 1000.0f;
    P.Y = FMath::RoundToFloat(P.Y * 1000.0f) / 1000.0f;
    P.Z = FMath::RoundToFloat(P.Z * 1000.0f) / 1000.0f;
    
    FQuat Q = T.GetRotation();
    Q.X = FMath::RoundToFloat(Q.X * 10000.0f) / 10000.0f;
    Q.Y = FMath::RoundToFloat(Q.Y * 10000.0f) / 10000.0f;
    Q.Z = FMath::RoundToFloat(Q.Z * 10000.0f) / 10000.0f;
    Q.W = FMath::RoundToFloat(Q.W * 10000.0f) / 10000.0f;
    Q.Normalize();
    
    FVector S = T.GetScale3D();
    S.X = FMath::RoundToFloat(S.X * 10000.0f) / 10000.0f;
    S.Y = FMath::RoundToFloat(S.Y * 10000.0f) / 10000.0f;
    S.Z = FMath::RoundToFloat(S.Z * 10000.0f) / 10000.0f;
    
    return FTransform(Q, P, S);
}
```

### FGTCall Helper Utility
```cpp
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
            AsyncTask(ENamedThreads::GameThread, [Callable = Forward<Func>(Callable)]()
            {
                Callable();
            });
        }
    }
};
```


