# Requirements Document

## Introduction

This feature implements an intelligent NPC system for Vibeheim where NPCs (animals, monsters, and humans) have basic survival needs that drive behavior. NPCs continuously evaluate needs (Hunger, Thirst, Energy, Health), find resources (Food, Water, Shelter), navigate using NavMesh, and consume/rest to recover. The system is data-driven, StateTree-controlled, and performance-aware to create emergent, lifelike behavior that makes the world feel alive and immersive.

## Scope

**MVP:** Hunger, Thirst, Energy, Health needs; Food/Water/Shelter resources; StateTree decision loop; NavMesh travel; reservations; simple failure recovery; performance LOD; basic debugging.

**Stretch:** EQS/AI Perception, Smart Objects, Save/Load of needs, multiplayer, mood/taming/additional needs.

## Shared Definitions

**Gameplay Tags:** Resource.Food, Resource.Water, Resource.Shelter, Need.Hunger, Need.Thirst, Need.Energy, Need.Health

**Data Assets:** DA_SpeciesTuning per-species Need decay/recovery, thresholds, search radii, wander params

**Reservation:** A lightweight claim token so multiple NPCs don't stampede one finite resource

**Hysteresis:** Pair of thresholds (Enter/Exit) that prevents rapid state flipping

**Blacklist:** Short-lived cache of recently failed targets excluded from selection

**NavQueryFilter:** UE filter class used to include/exclude NavAreas (e.g., water, steep)

## Requirements

### Requirement 1 — Needs Model & Decay (MVP)

**User Story:** As a developer, I want a consistent needs model so NPCs behave predictably.

#### Acceptance Criteria

1. NPCs SHALL track Hunger, Thirst, Energy, and Health with Current, Max, DecayPerSec, RecoverPerSec, LowThreshold, CriticalThreshold
2. Needs SHALL decay every tick (or timer) using species-specific rates from a data asset
3. WHEN Hunger or Thirst reach 0 for ≥ N seconds THEN Health SHALL decay at HealthPenaltyPerSec
4. All rates/thresholds SHALL be editable in data assets with sane defaults if none provided
5. Needs SHALL use hysteresis: each need defines EnterLowThreshold and ExitLowThreshold (Exit ≥ Enter) to prevent oscillation
6. The decay/update loop SHALL support a deterministic mode (fixed time step, seeded RNG) toggled via cvar

### Requirement 2 — Decision Arbitration (MVP)

**User Story:** As a player, I want NPCs to choose the most urgent need intelligently.

#### Acceptance Criteria

1. WHEN ≥2 needs are below LowThreshold THEN the system SHALL compute an urgency score (minimum deficit-based with criticality multiplier)
2. The highest urgency need SHALL be selected and pushed into the StateTree context
3. Re-evaluation SHALL occur on a fixed cadence (e.g., every 0.5–2.0s, species-tunable) and may be suppressed by a per-need post-satiation cooldown
4. WHEN a need reaches critical threshold THEN it SHALL receive higher priority weighting
5. Ties in urgency SHALL break by (a) nearest reachable satisfier, then (b) lowest current load (reservations)

### Requirement 3 — Food Behavior (MVP)

**User Story:** As a player, I want hungry NPCs to seek, reach, and consume food realistically.

#### Acceptance Criteria

1. WHEN Hunger < LowThreshold THEN NPC SHALL search for Resource.Food within Species.SearchRadius
2. IF ≥1 is reachable THEN NPC SHALL select the closest valid candidate and navigate via NavMesh
3. WHEN within UseRadius THEN NPC SHALL consume and recover Hunger over time until satisfied or resource depleted
4. On depletion, resource SHALL become unusable for new claimants
5. On success, the addressed need enters a cooldown window during which it won't be re-selected unless it falls below CriticalThreshold
6. IF the selected target becomes invalid (depleted, destroyed, reservation lost) THEN the NPC SHALL blacklist it for a short duration before re-searching

### Requirement 4 — Water Behavior (MVP)

**User Story:** As a player, I want thirsty NPCs to drink from nearby water sources.

#### Acceptance Criteria

1. WHEN Thirst < LowThreshold THEN NPC SHALL search for Resource.Water
2. Selection SHALL prefer closest reachable water source
3. Infinite sources SHALL support concurrent users (no depletion); finite sources follow reservation rules
4. Drinking SHALL recover Thirst at RecoverPerSec until LowThreshold is cleared (or Max)
5. On success, the addressed need enters a cooldown window during which it won't be re-selected unless it falls below CriticalThreshold
6. IF the selected target becomes invalid (depleted, destroyed, reservation lost) THEN the NPC SHALL blacklist it for a short duration before re-searching

### Requirement 5 — Shelter / Rest (MVP)

**User Story:** As a player, I want tired NPCs to rest in shelter.

#### Acceptance Criteria

1. WHEN Energy < LowThreshold THEN NPC SHALL search for Resource.Shelter
2. On arrival, NPC SHALL enter Rest state and Energy recovers each second at species rate
3. WHEN Energy ≥ RestExitThreshold THEN NPC exits and re-evaluates needs
4. IF shelter is lost (destroyed/unavailable) THEN NPC SHALL gracefully replan
5. On success, the addressed need enters a cooldown window during which it won't be re-selected unless it falls below CriticalThreshold
6. IF the selected target becomes invalid (depleted, destroyed, reservation lost) THEN the NPC SHALL blacklist it for a short duration before re-searching
7. Rest can be gated by animation alignment; Energy recovers only while aligned/inside UseRadius

### Requirement 6 — Resource Search & Selection (MVP)

**User Story:** As a developer, I want predictable and performant resource finding.

#### Acceptance Criteria

1. Search SHALL use a spatial query (sphere overlap or pre-placed registries) with escalating search radius on retries and a hard cap; no full world iteration
2. Candidates MUST match the target Resource.* tag and pass a reachability check (NavMesh path exists)
3. Selection policy (MVP): nearest reachable by straight-line distance; tie-break by lowest current load (active reservations)
4. Failed search SHALL trigger a short Explore/Wander state before re-try (cooldown tunable)
5. Reachability SHALL respect species nav filters (e.g., avoid water/steep slopes) in the path query

### Requirement 7 — Travel & Pathfinding (MVP)

**User Story:** As a player, I want realistic movement that handles obstacles gracefully.

#### Acceptance Criteria

1. Travel SHALL use UE NavMesh pathfinding; movement cancels when target invalidates
2. IF path request fails or path is repeatedly blocked K times THEN NPC SHALL clear target and re-search
3. IF no reachable candidates after M retries THEN NPC SHALL enter Explore for T seconds, then re-evaluate needs
4. Movement states SHALL expose success/failure to the StateTree debugger
5. Stuck detection: if distance to goal decreases by < X meters over S seconds, treat as failure and replan
6. On repeated failures, the target is blacklisted for a cooldown period before selection can consider it again

### Requirement 8 — Interaction & Consumption (MVP)

**User Story:** As a developer, I need a clean consumption API for resources.

#### Acceptance Criteria

1. Resource actors SHALL expose CanUse(Need), BeginUse(NPC), TickUse(NPC, dt), EndUse(NPC) interface methods that return a use handle/token to couple with reservations and allow heartbeat updates
2. Finite resources SHALL decrease Quantity as they're used and broadcast depletion
3. Use SHALL respect UseRadius and optional alignment time (e.g., sit/lie animations)
4. Recovery SHALL be continuous over time, not an instant fill, unless configured
5. BeginUse/EndUse MAY be gated by montage notifies or simple timers for align animations

### Requirement 9 — Concurrency & Reservations (MVP)

**User Story:** As a player, I don't want NPCs to dog-pile finite resources.

#### Acceptance Criteria

1. On selection, NPCs SHALL attempt to reserve the resource; if reservation fails, pick next candidate
2. Finite resources SHALL limit concurrent users to MaxConcurrentUsers (default 1)
3. Infinite water sources SHALL ignore Quantity and allow ≥ MaxConcurrentUsers (configurable)
4. Reservations SHALL expire unless heartbeated (e.g., every 1–2s) while the NPC is en route; expiry returns the slot to the pool

### Requirement 10 — StateTree Integration & Debugging (MVP)

**User Story:** As a developer, I want visual, debuggable logic for NPC behavior.

#### Acceptance Criteria

1. Each NPC SHALL have a UStateTreeComponent running a graph with states: Evaluate → Find → Travel → Use → Evaluate, plus Idle/Explore
2. The StateTree debugger SHALL show current state, transitions, and external data bindings (Needs, Context)
3. External data (Needs, Owner, Context struct) SHALL be bound and visible in the debugger
4. Tasks and transitions SHALL fail fast with explicit reasons (for logs)

### Requirement 11 — Performance & LOD (MVP)

**User Story:** As a player, I want smooth FPS with many NPCs.

#### Acceptance Criteria

1. Systems SHALL avoid per-tick heavy work: search throttled (≥0.5s), re-eval cadence tunable per species
2. Distance LOD: NPCs beyond PlayerDistanceLOD SHALL reduce evaluation/search frequency (e.g., 2–5s) and skip fine-grained checks; LOD also clamps tick rates for decay and disables expensive searches; agents beyond LOD may batch updates at ≥2s intervals
3. All spatial searches SHALL be capped by radius and object channels; EQS disabled in MVP (optional later)
4. Performance budget target: ≤0.3 ms per 50 nearby NPCs on a mid-range CPU (tunable; measure with Unreal insights)
5. Provide cvars for search throttle, LOD distance, retry caps, and fast-decay to enable perf tuning in PIE

### Requirement 12 — Resource Authoring (MVP)

**User Story:** As a developer, I want easy placement and tuning of resources.

#### Acceptance Criteria

1. Resources SHALL be placeable actors with a Resource.* tag, UseRadius, Quantity, MaxConcurrentUsers
2. All fields SHALL be editable in the Details panel and support Blueprint children
3. Optionally, Water may be represented by trigger volumes or actors adjacent to WaterBody
4. WHEN placing a resource in the world THEN it SHALL be tagged with appropriate gameplay tags

### Requirement 13 — Data-Driven Species Tuning (MVP)

**User Story:** As a developer, I want per-species behavior without code changes.

#### Acceptance Criteria

1. Species defaults (decay/recover/thresholds/search radii/wander speeds/LODs) SHALL live in a UPrimaryDataAsset
2. On spawn, NPCs SHALL load species data and initialize Needs accordingly
3. Missing data assets SHALL fall back to sane defaults (logged once, not spammy)
4. WHEN defining a new NPC species THEN decay rates for all needs SHALL be configurable via data assets

### Requirement 14 — Debuggability & Tools (MVP)

**User Story:** As a developer, I need quick visibility into NPC behavior.

#### Acceptance Criteria

1. A debug toggle (cvar or blueprint) SHALL draw current need bars and active target over each NPC
2. Logging categories LogVHMNeeds, LogVHMNPC, LogVHMResource SHALL be available with Verbose gates
3. Console hooks: vhm.npc.FastDecay, vhm.npc.ShowTargets, vhm.npc.LODDistance, etc.
4. WHEN debugging NPC behavior THEN StateTree debugger SHALL show current state and transitions
5. Deterministic test mode cvars: vhm.npc.deterministic 1, vhm.npc.fastdecay 1, vhm.npc.search_throttle 1.0, vhm.npc.retry_max 3

### Requirement 15 — Integration & Compliance (MVP)

**User Story:** As a developer, I want the NPC system to integrate cleanly with existing Vibeheim architecture.

#### Acceptance Criteria

1. The system SHALL compile with existing Vibeheim module dependencies (Core, Engine, AIModule, NavigationSystem, GameplayTasks, GameplayTags, StateTreeModule, StructUtils)
2. NPCs SHALL operate on existing terrain/PCG content without modification (NavMesh present)
3. Naming/conventions SHALL follow Vibeheim standards (prefix VHM)
4. WHEN integrating with world generation THEN NPCs SHALL work with existing terrain and PCG systems
5. Reachability checks SHALL use NavQueryFilter per species (configurable in species data asset)

## Stretch Requirements (Not part of MVP)

- **S1 EQS & AI Perception:** Perception-first candidate discovery; EQS scoring
- **S2 Smart Objects:** Convert Shelter/Bed/Fire to claimable SmartObjects with task slots
- **S3 Save/Load:** Persist needs and reservations for long-running worlds
- **S4 Multiplayer:** Server-auth needs & reservations; client-side prediction for movement
- **S5 Additional Needs:** Mood, Social, Temperature, Taming meter (for animals)
## Test
 Checklist (MVP Validation)

- **Oscillation test:** With Hysteresis enabled, agent does not alternate between Food/Water every <2s under near-equal deficits
- **Stuck test:** Place blocking volume mid-path; after S seconds of no progress, agent replans to an alternative target
- **Reservation test:** Two agents, one finite Food (MaxConcurrentUsers=1) → one reserves, the other picks a different node or waits; when the first leaves, the second can claim
- **LOD test:** At > LOD distance, eval cadence and search frequency drop (verify via cvars/logs) with stable needs decay