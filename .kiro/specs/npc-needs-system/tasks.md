# Implementation Plan

- [x] 1. Project setup (fail fast)
  - Update Vibeheim.Build.cs with: Core, CoreUObject, Engine, AIModule, NavigationSystem, GameplayTasks, GameplayTags, StateTreeModule, StructUtils
  - Project Settings → Collision: add channel Resource (ECC_GameTraceChannel2), set Resource actors to Block on this channel
  - Add Gameplay Tags in Config/DefaultGameplayTags.ini: Resource.Food, Resource.Water, Resource.Shelter, Need.Hunger/Thirst/Energy/Health
  - Create VHMNPCTags.h/.cpp using UE_DEFINE_GAMEPLAY_TAG_STATIC for the above
  - Add logging categories (LogVHMNeeds, LogVHMNPC, LogVHMResource, LogVHMStateTree)
  - _Requirements: 15.1, 12.1_

- [x] 2. Core types & data
  - VHMNPCTypes.h: EVHMNeed, FNeedStat (with enter/exit low, critical), no side-effects
  - FVHMUseHandle (starts invalid; no GUID in constructor)
  - FAgentNeedContext (instance data for StateTree; target, score, blacklist)
  - FVHMSpeciesConfig + UVHMSpeciesDataAsset : UPrimaryDataAsset (nav filter, search radii, retry caps, LOD, stuck thresholds)
  - Default assets for Human, Wolf species (to be authored in Editor as `DA_Species_Human` / `DA_Species_Wolf` under `Content/NPC/Species`)
  - _Requirements: 1.1, 13.1, 13.2, 13.3_

- [x] 3. Needs component (single source of truth)
  - Decay loop (supports deterministic fixed timestep via cvar)
  - Health penalty when Hunger/Thirst == 0 for N seconds
  - GetMostPressingNeed(OutScore) implements scoring + post-satiation cooldown + hysteresis
  - LOD: distance-based cadence; clamp decay update when far
  - CVars: vhm.npc.deterministic, vhm.npc.fast_decay, vhm.npc.eval_interval, vhm.npc.lod_distance
  - Build & fix (fast compile checkpoint)
  - _Requirements: 1.1, 1.2, 1.3, 2.1, 2.2, 2.3, 2.4, 11.1, 11.2_

- [x] 4. Resource system (actor-local, no tick)
  - IVHMUsableResource UInterface: CanUse, BeginUse, TickUse, EndUse
  - AVHMResourceActor: FGameplayTag ResourceTag, UseRadius, Quantity (<0 infinite), MaxConcurrentUsers
  - Reservations: TryReserve(User), HeartbeatReservation(Id), ReleaseReservation(Id); expire on heartbeat lapse
  - Expire stale slots in TryReserve(); release on EndUse() and EndPlay()
  - Set resource primitive CollisionEnabled = QueryOnly, ObjectType = Resource (ECC_GameTraceChannel2); leave other responses ignored
  - On OnEndPlay (resource): release all active reservations
  - Provide OnUserDestroyed helper so tasks can EndUse() if the pawn dies/unpossesses mid-use
  - No per-frame tick; logic only on calls
  - Blueprint variants: Food, Water, Shelter (tagged, defaults set)
  - Build & fix (fast compile checkpoint)
  - _Requirements: 8.1, 8.2, 8.3, 8.4, 9.1, 9.2, 9.3, 9.4, 12.1, 12.2_

- [x] 5. Pawn & controller
  - AVHMNPCCharacter (ACharacter): add UVHMNeedsComponent, UStateTreeComponent; AutoPossess AI; load species data on BeginPlay and seed needs
  - AVHMNPCAIController: standard possession; expose helper to set nav filter per species
  - _Requirements: 13.2, 15.1_

- [x] 6. StateTree tasks (thin, robust)
  - [x] 6.1 STT_EvaluateNeed
    - Read from NeedsComponent->GetMostPressingNeed()
    - Write to FAgentNeedContext; do not redo hysteresis/cooldowns here
    - _Requirements: 2.1, 2.2, 2.3, 10.4_

  - [x] 6.2 STT_FindTarget
    - Overlap on Resource channel within species SearchRadius; escalate radius with retry up to SearchRadiusMax
    - Filter by tag + temporary blacklist; optional NavSys->TestPathSync() for reachability
    - Attempt reservation; on failure, try next candidate
    - Reuse preallocated overlap/candidate arrays to avoid per-tick allocations
    - For water candidates, project the target point to NavMesh within UseRadius; discard if projection fails
    - _Requirements: 6.1, 6.2, 6.3, 6.4_

  - [x] 6.3 STT_MoveTo
    - FAIMoveRequest to target (actor or location), SetNavigationFilter(Species.NavQueryFilter) (fallback to controller default if null)
    - Set MoveTo acceptance radius = Resource.UseRadius (from context)
    - If HeartbeatReservation(Id) returns false at any tick → abort, blacklist target, transition to FindTarget
    - Stuck detection: <75 cm progress over 3 s → fail; blacklist target
    - While Running, heartbeat reservation every ~1–2 s
    - _Requirements: 7.1, 7.2, 7.4, 9.4_

  - [x] 6.4 STT_UseResource
    - On enter: BeginUse(). While running: TickUse(); concurrently call NeedsComponent->Recover(CurrentNeed, dt)
    - Exit when ShouldExitLow() OR resource depletes/invalid
    - Always EndUse() in ExitState and on external invalidation (OnEndPlay)
    - _Requirements: 3.3, 4.4, 5.3, 8.1, 8.3, 8.4_

  - [x] 6.5 STT_Explore
    - Small random NavMesh wander for T seconds; then back to Evaluate
    - _Requirements: 7.3_

- [x] 7. StateTree asset
  - Graph: Evaluate → (need) Find → Travel → Use → Evaluate; else Idle/Explore → Evaluate
  - Bind external data: NeedsComponent, OwnerActor, OwnerPawn; instance data: FAgentNeedContext
  - Bind a resource "use radius" float into instance data (or read from actor in MoveTo) to keep acceptance radius consistent
  - Verify in debugger: current state, transitions, instance data visible
  - _Requirements: 10.1, 10.2, 10.3, 14.4_
  - Implemented: AVHMNPCCharacter now loads a default UStateTree (soft reference or fallback path `/Game/NPC/StateTrees/ST_NPC_Needs`) and starts UStateTreeComponent at BeginPlay. Tasks read Needs/Owner at runtime; FAgentNeedContext on Needs carries UseAcceptanceRadius.

- [ ] 8. Debug, CVars, logs
  - Overhead draws: need bars, current need/target, search radius, reservation count; toggle via vhm.npc.draw_*
  - CVars: search throttle (vhm.npc.search_throttle), retry caps (vhm.npc.retry_max), LOD distance, deterministic, fast_decay, show_targets
  - Verbose logs under categories; guard hot paths
  - _Requirements: 14.1, 14.2, 14.3_

- [ ] 9. Test env & validation
  - Test map: NavMeshBoundsVolume; three resource actors; lighting; player start
  - Place 1–4 AVHMNPCCharacter with species assets; assign StateTree
  - [ ] 9.1 Scenarios:
    - Cycle: decay→evaluate→find→travel→use→exit (visually confirm + logs)
    - Oscillation: verify hysteresis (no ping-pong within 2 s)
    - Contention: finite Food MaxConcurrentUsers=1; two NPCs → one reserves, one replans/waits
    - Stuck: add blocking volume; see replan + blacklist
    - LOD: far away → cadence increases; perf stable
    - Destroy a target mid-travel/use → NPC blacklists & replans; reservation is released
  - _Requirements: 15.2, 13.2, Test Checklist items_

- [ ] 10. Perf & polish
  - Tune eval cadence / search throttle per species; confirm ≤0.3 ms/50 nearby NPCs target
  - Remove any accidental ticks from resources/tasks; profile with Unreal Insights
  - Final pass on memory (no per-tick allocs; reuse arrays; LRU blacklist)
  - _Requirements: 11.1, 11.2, 11.4_

## Danger Spots (Watch These)

- **Destroyed targets:** Subscribe to OnEndPlay on TargetActor; clear/blacklist if it dies mid-travel/use
- **Water bodies:** Choose a nav-reachable edge point (project to NavMesh near the water) instead of targeting inside a WaterBody volume
- **Acceptance radius:** Tie MoveTo acceptance to resource UseRadius to prevent "orbiting"
- **Spam logs:** Guard Verbose with CVars; keep hot paths silent by default
- **Species data missing:** Warn once, apply defaults, don't spam
- **Arrays:** Reuse temp arrays in FindTarget to avoid per-tick allocs
- **EQS temptation:** Resist for MVP; overlap + reachability is enough
