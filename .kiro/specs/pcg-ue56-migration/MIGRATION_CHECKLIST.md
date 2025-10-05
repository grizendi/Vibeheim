# PCG UE 5.6 Migration - Critical Checklist

## Pre-Implementation Verification

- [ ] Confirm UE 5.6.x engine version (strict requirement)
- [ ] Verify PCG plugin is enabled in project
- [ ] Review `/Docs/PCG Plugin Update Guide.md`
- [ ] Backup current PCG integration code

## Critical Implementation Points

### 1. Scheduler API Usage
- **Use**: `UPCGSubsystem::ScheduleGraph(UPCGComponent*, FPCGTaskId, ...)` overload with source component
- **Avoid**: Direct scheduling without component context (breaks hierarchical generation)
- **Note**: `FPCGDataCollection` still exists internally—wrapper can use it, but hide from public API

### 2. Object Lifetime Management
- **Input Data**: Create `UPCGParamData`/`UPCGPointData` with persistent Outer (PCGAnchor actor)
- **Flags**: Mark `RF_Transient`
- **Lifetime**: Store `TStrongObjectPtr` refs in `FPCGTaskContext`, drop in `ReleaseTask`
- **GC Risk**: Without proper rooting, GC can collect mid-execution
- **Don't**: Use `AddToRoot` unless absolutely necessary

### 3. Task Cancellation & State Machine
- **Required**: Implement abandon-on-arrival for tile unload scenarios
- **State Machine**: `EPCGTaskState {Scheduled, Completing, Completed, Released, Abandoned}`
- **Transitions**: `Scheduled → Completing → Completed → Released` OR `Scheduled → Abandoned → Released`
- **Tracking**: Use `TMap<FPCGTaskId, FPCGTaskContext>` with world/tile/state/input refs
- **Backpressure**: Abandoned tasks don't count toward `MaxConcurrentPCGTasks`
- **Cleanup**: Hook `FWorldDelegates::OnWorldCleanup` to abandon all tasks
- **Guard**: `ReleaseTask` is no-op unless `Completed || Abandoned`, never runs twice

### 4. Component Lifecycle
- **Outer**: Use dedicated "PCGAnchor" actor in persistent level (never streamed)
- **Context**: Always pass `UPCGComponent*` to scheduler (hard requirement)
- **Registration**: Assert GT for `RegisterComponent()`/`UnregisterComponent()`
- **Cleanup**: Bind to `OnWorldCleanup`, null map entries to avoid dangling refs

### 5. Game Thread Discipline
- **All Mutations**: `check(IsInGameThread())` for `ActiveTasks`, component registration, HISM
- **Completions**: Use `FGTCall` helper to marshal off-GT callbacks
- **Pattern**: Never assume scheduler callbacks arrive on GT

### 6. CI & Build Guards
- **Grep Scope**: Exclude `PCGSchedulerExecutor.cpp` from legacy API checks
- **Version**: `static_assert(ENGINE_MINOR_VERSION==6)` in `PCGVersionGuard.h` (included by all TUs)
- **Compile Gates**: `#if !VHM_PCG_ENABLED` with `#else` stub for HISM path
- **Server Builds**: `PublicDefinitions.Add("VHM_PCG_ENABLED=0");` in `*.Build.cs` for server targets

### 7. Attribute Scope
- **Lock Now**: Finalize canonical attribute list (Param vs Point)
- **Validation**: Enforce in `ValidateInputAttributes` with auto-repair suggestions
- **Type Safety**: Never arithmetic on `UObject*`, `FName`, `FString`, `FSoftObjectPath`

### 8. Telemetry
- **Nodes Executed/Cached**: Expose from scheduler stats to validate CRC wins
- **CSV**: Buffer on background thread, gate behind DevOnly CVar
- **Percentiles**: Track 50th/95th/99th in-memory per biome

### 9. Determinism
- **Seeds**: Set both PCG component seed AND `TileSeed` metadata
- **Formula**: `TileSeed = Hash(TileX, TileY, BiomeId, GlobalSeed)` (document to prevent silent changes)
- **Quantization**: Use `QuantizeTransformForHash()` to avoid float jitter false-fails
- **Sorting**: Sort instances by stable key (`InstanceId`, else position) before hashing
- **Validation**: Assert identical `(Biome, Tile, TileSeed)` → identical count/hash

### 10. Testing Priority
- **NON-OPTIONAL**: Task 1.4 (scheduler tests) - safety net for timeouts, GT marshalling
- **Optional**: Tasks 2.6, 3.6 (input/component tests) - can defer if time-constrained
- **Required**: Headless/dedicated server build test (5.13)

## Common Pitfalls

1. **Scheduling without component context** → Hierarchical generation breaks
2. **No task cancellation** → Memory leaks on tile unload
3. **Wrong Outer for input data** → GC collects mid-execution
4. **Missing GT assertions** → Race conditions in ActiveTasks
5. **Grep blocks wrapper** → CI fails on legitimate internal usage
6. **Ignoring CRC counters** → Can't validate performance wins
7. **No seed control** → Non-deterministic tests
8. **Arithmetic on pointers** → Compile errors or UB

## Validation Steps

1. **Compile**: No legacy API usage outside wrapper
2. **PIE Smoke**: Generate one tile per biome
3. **wg.pcg.validate**: Run on all biome graphs
4. **Budget/Perf**: Confirm CRC caching reduces re-gen time
5. **Determinism**: Same seed → same output (hash match)
6. **Fallback**: Disable PCG → HISM still works
7. **Unload**: Tile eviction doesn't leak tasks
8. **Dedicated Server**: No PCG code paths touched

## Rollback Plan

1. Set `bEnablePCGGraphs = false` in project settings
2. Restart editor/game
3. Verify tiles generate with HISM
4. Confirm no scheduler errors in logs
5. Graphs/components remain loadable but inactive

## Success Criteria

- ✅ All tiles generate with scheduler path
- ✅ No legacy API usage (CI passes)
- ✅ Task cancellation on unload (no leaks)
- ✅ Deterministic output (hash matches)
- ✅ CRC caching measurable (nodes cached > 0)
- ✅ Fallback works (HISM-only mode)
- ✅ Dedicated server builds (no PCG paths)
- ✅ Performance within budget (< 16ms @ 60 FPS)

## References

- Requirements: `.kiro/specs/pcg-ue56-migration/requirements.md`
- Design: `.kiro/specs/pcg-ue56-migration/design.md`
- Tasks: `.kiro/specs/pcg-ue56-migration/tasks.md`
- UE Docs: `Docs/PCG Plugin Update Guide.md`
- Epic Docs: https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-framework-in-unreal-engine


## Additional Critical Details

### Pin Validation
- **Pre-Schedule**: Validate all pin names exist on graph, check type compatibility
- **Fail Fast**: Clear log if pin missing
- **Warning**: Emit if falling back to index ordering (designers may reorder)

### Polling & Timeouts
- **CVars**: `vhm.pcg.poll_ms` (8-16ms), `vhm.pcg.timeout_ms` (5000ms)
- **Never**: Busy-spin; use frame-paced polling
- **Logging**: Print CVar values used

### Telemetry Counters
- **If Available**: Expose nodes executed/cached from scheduler stats
- **Fallback**: Use proxy (elapsed vs historical median), label clearly as proxy
- **CSV**: Buffer in memory, flush on timer/cleanup, use `IFileManager::CreateFileWriter`

### Asset References
- **Decision**: `UStaticMesh*` vs `FSoftObjectPath` for runtime
- **Recommendation**: Prefer soft paths for packaging/streaming
- **Resolve**: At extraction time or later in HISM step

### Bounds Computation
- **Call Once**: `RecomputeBounds()` after all points/transforms finalized
- **Defer**: If multiple builders touch same point set

### Validator Logging
- **Type Names**: UE type + friendly label (e.g., "float (double ok)")
- **Node Names**: First offending node for quick fixups
- **Auto-Repair**: Specific suggestions with node/attribute names
