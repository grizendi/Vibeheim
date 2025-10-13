# Requirements Document

## Introduction

Unreal Engine 5.6 replaces the legacy PCG runtime path (pre-5.6 calls like UPCGSubsystem::RunGraph/Wait/GetGraphOutput/Release, FPCGDataCollection, FPCGMetadata) with a scheduler-based execution model (e.g., UPCGSubsystem::ScheduleGraph) and explicit Execution Dependency pins for deterministic ordering on a multithreaded-by-default system. UE 5.6 also adds runtime frustum culling controls and per-data CRC caching to reduce partial re-gen work, which we want to leverage while preserving our HISM fallback.

## Scope & Guardrails

- **Engine:** UE 5.6.x only
- **Plugin:** PCG (engine plugin)
- **Build-time check:** Fail fast if engine < 5.6 or required APIs missing (e.g., UPCGSubsystem::ScheduleGraph, UPCGSettings::HasExecutionDependencyPin)
- **Feature flag:** bEnablePCGGraphs (On in Dev/Editor; gated for Shipping)
- **Build discipline:** Only build when validating a requirement or fixing a failing test (keeps migration edits small & reviewable)

## Important Notes

- **Default multithreading:** Latent data races will surface unless you sequence sensitive segments with the Execution Dependency pin; plan an audit of any custom nodes/side-effectful segments
- **Frustum culling:** Now part of the runtime policy; enable it by default for open-world streaming
- **CRC caching:** Enabled by default; tests should verify iteration speed-ups on partial graph edits
- **Reference:** See Docs\PCG Plugin Update Guide.md for additional update guidance

## Requirements

### Requirement 1: Public API Inventory and Legacy Call-Site Map

**User Story:** As a developer, I need a precise map from the old PCG runtime calls to their 5.6 equivalents.

#### Acceptance Criteria

1. WHEN documenting APIs THEN the system SHALL create /Docs/PCG-5.6.md summarizing public APIs we use: UPCGSubsystem::ScheduleGraph and related scheduling helpers (task handles/queries), plus how to detect the Execution Dependency pin on nodes (UPCGSettings::HasExecutionDependencyPin)
2. WHEN documenting metadata THEN the system SHALL document 5.6 metadata primitives we rely on (UPCGMetadata, PCGMetadataEntryKey, attribute accessors)
3. WHEN creating migration table THEN the system SHALL add migration table mapping Legacy (RunGraph/Wait/GetGraphOutput/Release, FPCGDataCollection, FPCGMetadata) to 5.6 (ScheduleGraph, accessor-based extraction, UPCGMetadata)
4. WHEN inventorying call sites THEN the system SHALL cross-reference every call site in PCGWorldService.cpp, WorldGenManager.cpp, TileStreamingService.cpp, and tests with no unmapped calls remaining
5. IF APIs missing THEN the system SHALL document workaround strategies or alternative approaches

### Requirement 3: Input Assembly - Parameters and Point Data

**User Story:** As a developer, I want inputs built with 5.6 parameter and metadata APIs so graphs receive valid, typed data.

#### Acceptance Criteria

1. WHEN creating parameters THEN the system SHALL update CreateTileParameterData to set parameter overrides using 5.6 patterns (user params/attribute sets)
2. WHEN creating point data THEN the system SHALL update CreateTilePointData to attach attributes (BiomeId, Slope, WaterDistance, Seed, etc.) via UPCGMetadata and attribute accessors with no legacy FPCGMetadata
3. WHEN validating inputs THEN the system SHALL validate attribute types/cardinality pre-schedule and on mismatch emit structured errors and skip execution
4. WHEN documenting attributes THEN the system SHALL document canonical attribute names and types in /Vibeheim/_Assets/Data/PCG/Docs/Attributes.md
5. IF input assembly fails THEN the system SHALL log a detailed validation report and skip execution

### Requirement 2: Execution Model - Scheduler and Explicit Dependency Pins

**User Story:** As a developer, I want all PCG graph execution to be deterministic under 5.6's multithreaded scheduler.

#### Acceptance Criteria

1. WHEN executing graphs THEN the system SHALL use UPCGSubsystem::ScheduleGraph for all runtime graph runs and capture task IDs/handles to track completion and errors
2. WHEN ordering matters THEN the system SHALL wire the Execution-Dependency/Dependency-Only pin in the graph where ordering or side-effects matter and validator must flag nodes that declare an execution dependency but aren't wired
3. WHEN using input-less getters THEN the system SHALL ensure input-less Getters (e.g., Get Landscape Data, Get Actor Data) are context-scoped via the dependency pin so they execute at the intended grid level during hierarchical generation
4. WHEN validating dependencies THEN the system SHALL provide wg.pcg.showdeps <Graph> to list nodes with HasExecutionDependencyPin()==true that are not sequenced
5. WHEN replacing implicit sequencing THEN the system SHALL replace any implicit sequencing with explicit dependency links as default parallelism makes implicit order non-deterministic

### Requirement 4: Output Extraction via 5.6 Accessors

**User Story:** As a developer, I need to extract result points/instances using 5.6 metadata accessors to feed HISM/our spawn path.

#### Acceptance Criteria

1. WHEN extracting outputs THEN the system SHALL update ExtractInstancesFromPointData to iterate points via accessor APIs (transforms, scales, custom attrs) and fill our FPCGGenerationData/HISM payloads
2. WHEN handling non-numeric attributes THEN the system SHALL avoid arithmetic on non-numeric attributes (e.g., never multiply/lerp UStaticMesh*; select by key, keep references separate)
3. WHEN handling empty outputs THEN the system SHALL return 0 instances cleanly with context logging
4. WHEN populating results THEN the system SHALL continue to fill FPCGGenerationData with transforms, scales, and custom attributes
5. IF attribute access errors occur THEN the system SHALL log errors and return empty generation data

### Requirement 5: Streaming Integration - Runtime Policies and Budgets

**User Story:** As a developer, I want runtime PCG that respects streaming budgets and avoids off-camera work.

#### Acceptance Criteria

1. WHEN configuring runtime generation THEN the system SHALL enable Generate At Runtime for partitioned graphs used by tiles and wire a PCG World Actor / Generation Source as needed
2. WHEN optimizing generation THEN the system SHALL adopt 5.6 frustum culling in the runtime scheduling policy, expose toggles in VibeheimSettings and default On for open-world play
3. WHEN respecting budgets THEN the system SHALL expose and enforce a cap on concurrent runtime generation (project setting/CVar) so TileStreamingService stays within frame budget
4. WHEN scheduler unavailable THEN the system SHALL fallback to the HISM path and log graph/biome/tile/budget context if the scheduler is unavailable or times out
5. WHEN leveraging improvements THEN the system SHALL bake 5.6 improvements (reduced scheduling overhead, CRC reuse) into our default runtime policy assumptions

### Requirement 6: Synchronous Helper - Tests and Tools Only

**User Story:** As a developer, I want a minimal "run & wait" façade for tests and editor tools—never used in shipping runtime.

#### Acceptance Criteria

1. WHEN creating helper THEN the system SHALL provide FPCGRunScope RunGraphSync(UWorld*, UPCGGraph*, const FPCGInputDesc&, FTimespan Timeout)
2. WHEN executing synchronously THEN the system SHALL internally ScheduleGraph, then pump game thread in latent test contexts until complete or timeout
3. WHEN returning results THEN the system SHALL return structured result (success, output stats, warnings/errors)
4. WHEN enforcing usage THEN the system SHALL enforce tests/tools-only usage via build guards
5. IF timeout or error occurs THEN the system SHALL emit clear diagnostics

### Requirement 7: Graph Ownership and Component Lifecycle

**User Story:** As a developer, I want safe ownership semantics for per-biome graphs/components.

#### Acceptance Criteria

1. WHEN managing graphs THEN the system SHALL cache graph assets (UPCGGraph*) per biome and create/reuse UPCGComponent instances as execution sources
2. WHEN registering components THEN the system SHALL ensure components are registered/unregistered properly with the world/PCG subsystem with no dangling refs
3. WHEN tearing down THEN the system SHALL destroy/unregister components and release scheduler handles on teardown
4. WHEN initializing service THEN the system SHALL update BiomePCGComponents map to hold the correct type with proper lifecycle management
5. IF lifecycle errors occur THEN the system SHALL trigger HISM fallback on errors

### Requirement 8: Headless and Fallback Behavior

**User Story:** As a developer, I want worldgen to continue when PCG is unavailable or fails.

#### Acceptance Criteria

1. WHEN detecting environment THEN the system SHALL detect headless/no-world contexts and auto-fallback to HISM generation with feature parity
2. WHEN graphs missing THEN the system SHALL issue warnings for missing graphs/components but do not block tile generation
3. WHEN scheduler fails THEN the system SHALL automatically fallback with clear logs on any scheduler failure
4. WHEN maintaining budgets THEN the system SHALL keep budgets enforced in fallback mode
5. IF fallback triggers THEN the system SHALL maintain performance budgets and streaming behavior consistency

### Requirement 9: Validation and Diagnostics

**User Story:** As a developer, I want tooling that surfaces miswired graphs and bad metadata early.

#### Acceptance Criteria

1. WHEN validating graphs THEN the system SHALL implement ValidatePCGGraph to check: PCG subsystem init, required attributes (names/types), dependency-pin wiring where required
2. WHEN extending validation THEN the system SHALL extend wg.pcg.validate to verify scheduler availability, graph/component registration, and frustum-culling policy for runtime graphs
3. WHEN reporting errors THEN the system SHALL include graph name, biome, tile, and failure reason in errors and show expected vs. actual types for attribute issues
4. WHEN validation fails THEN the system SHALL block execution and print remediation steps on validation failure
5. IF scheduler fails THEN the system SHALL provide rich error messages including graph name, biome, and failure reason

### Requirement 10: Tests and Regressions

**User Story:** As a developer, I need tests that prove the new execution path is correct and won't regress.

#### Acceptance Criteria

1. WHEN porting tests THEN the system SHALL port unit/integration tests in Source/Vibeheim/WorldGen/Private/Tests to the scheduler via the sync helper
2. WHEN adding coverage THEN the system SHALL add coverage for: submission, completion, timeout, failure, and HISM fallback on scheduler error
3. WHEN testing performance THEN the system SHALL add perf logging to capture scheduler overhead and budget compliance and confirm CRC caching reduces partial re-gen work
4. WHEN testing regressions THEN the system SHALL add a targeted regression around Difference + GetActorData and if it fails in 5.6 verify your attribute-mask workaround (project cull volumes to a float mask and filter)
5. IF tests fail THEN the system SHALL provide clear failure messages with scheduler state and output data for debugging

### Requirement 11: Staged Rollout and Designer Migration Notes

**User Story:** As a developer, I want a safe rollout and clear authoring guidance.

#### Acceptance Criteria

1. WHEN gating rollout THEN the system SHALL gate the refactor behind bEnablePCGGraphs and enable gradually across maps/biomes
2. WHEN documenting for designers THEN the system SHALL write designer notes covering: when/why to wire the Execution-Dependency pin especially for Getters and timed sequences, canonical attribute names/types we expect on inputs/outputs, and runtime generation expectations (partitioning, frustum culling, concurrency caps)
3. WHEN providing verification THEN the system SHALL provide verification checklist: compile → PIE smoke → wg.pcg.validate → budget/perf
4. WHEN enabling rollback THEN the system SHALL provide rollback plan: toggle flag to HISM-only; keep graphs/components but skip scheduling
5. IF rollout issues occur THEN the system SHALL provide clear remediation steps and fallback behavior documentation
