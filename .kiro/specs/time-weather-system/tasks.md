# Implementation Plan

Convert the feature design into a series of prompts for a code-generation LLM that will implement each step in a test-driven manner. Prioritize best practices, incremental progress, and early testing, ensuring no big jumps in complexity at any stage. Make sure that each prompt builds on the previous prompts, and ends with wiring things together. There should be no hanging or orphaned code that isn't integrated into a previous step. Focus ONLY on tasks that involve writing, modifying, or testing code.

- [ ] 1. Set up module structure and core data types
  - Create Environment folder structure under Source/Vibeheim/
  - Define core enums (EVHMEnvSegment, EVHMWeather) in VHMEnvironmentTypes.h (keep UENUM tight, avoid ENUM_CLASS_FLAGS)
  - Implement core structs (FVHMTimeState, FVHMWeatherState) with proper UE5.6 reflection
  - Write unit tests: reflection sanity (UHT metadata present), default values, WITH_EDITORONLY_DATA fields don't leak into runtime builds
  - _Requirements: 8.1, 8.2_

- [ ] 2. Create configuration data assets and settings
  - Implement FVHMTimeSettings struct with sun path parameters (PeakElevationDeg, NightLowElevationDeg, BaseAzimuthDeg)
  - Implement FVHMWeatherDef struct with transition weights and parameter targets
  - Create UVHMTimeSettingsDataAsset and UVHMWeatherTableDataAsset classes
  - Create UVHMEnvironmentSettings developer settings class with MPC path and FGameplayTag discovery tags (not FName)
  - Write unit tests: clamp rules (Min<=Max), zero-sum weights → normalized fallback, TransitionSec==0 snap flag, per-state weight guard (all weights 0/NaN → stay in current state)
  - _Requirements: 6.1, 6.2, 6.4_

- [ ] 3. Implement environmental actor registry system
  - Create FEnvActorRegistry struct with weak pointers to lighting actors
  - Implement actor discovery by both GameplayTags and Actor::Tags fallback
  - Add validation for actor mobility, atmosphere linkage, and base value caching (cache BaseSunIntensity/Color once after discovery)
  - Listen for late actors (FWorldDelegates::LevelAddedToWorld, OnActorSpawned) and re-scan only added level/actor
  - Add dedicated server guard—skip all component pokes (lights/sky/MPC) on NM_DedicatedServer
  - Implement graceful degradation for missing optional actors
  - Write unit/integration test (PIE): place fake lights with tags, one Static → warn once; SkyAtmosphere unlinked → auto-link index 0; spawn Sun after BeginPlay → system picks it up
  - _Requirements: 4.1, 4.2, 4.4_

- [ ] 4. Create core subsystem class with initialization
  - Implement UVHMEnvironmentSubsystem inheriting from UTickableWorldSubsystem
  - Override ShouldCreateSubsystem(UObject* Outer) to not create in non-game/editor preview worlds
  - Add global kill-switch CVar vhm.env.enabled (default 1). If 0 → subsystem early-outs everything
  - Use Initialize/Deinitialize + world begin/cleanup bindings (Subsystem semantics)
  - Add bReady flag and initialization order handling for async assets
  - Store absolute day clock as double accumulator (TotalDaySecondsD) to avoid float drift in long sessions
  - Implement IsTickableWhenPaused() gated by vhm.env.tick_when_paused CVar
  - Don't tick in non-game worlds: GetWorld()->IsGameWorld() && !GetWorld()->IsPreviewWorld()
  - Implement InitFromStartClock() algorithm to set initial time from StartClockHours
  - Write unit test: StartClockHours → correct segment & SegT across boundaries (0.0, 12.0, 23.9), paused ticking on/off via CVar
  - _Requirements: 8.1, 8.2, 8.5_

- [ ] 5. Implement time progression system
  - Create TickTime() method with asymmetric day/night progression
  - Implement deterministic fixed-step accumulator with max substeps cap (8 steps/frame) and record stat/counter when clamped
  - Add AdvanceTime() with segment transitions and OnSunrise/OnSunset events (use epsilon when comparing SegT >= 1.f - KINDA_SMALL_NUMBER)
  - Protect against pathological config: clamp PeakElevationDeg ∈ [5, 89], NightLowElevationDeg ∈ [5, 89]
  - Don't evaluate color/intensity curves outside domain—pre-clip elevation to curve keys' min/max before sampling
  - Calculate TimeOfDay01 across full day+night cycle
  - Add SetClockHours(float) canonical solver for Segment/SegT; reuse in console and tests
  - Establish per-tick order: time math → sun transform → weather blend → MPC → skylight recapture (document and unit-assert)
  - For deterministic tests, lock easing to time-normalized curve (FAlphaBlend with Alpha from normalized timeline), never integrate with variable Dt
  - Write unit tests: asymmetric lengths math; deterministic reproducibility; big skip/negative rewind across multiple flips triggers each event once; runtime length change preserves wall-clock hours
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.7, 1.8_

- [ ] 6. Implement weather state machine
  - Create TickWeather() method with deterministic FRandomStream-based transitions
  - Implement SelectNextWeather() using weighted selection from current state
  - Add weather parameter blending with SmoothStep easing function (clamp all blended outputs, prevent overshoot)
  - Ensure Wetness01 obeys monotonic rise during rain even if TransitionSec is tiny and target oscillates due to quick Sunny↔Rain flips
  - Implement wetness tail-off system with configurable half-life decay
  - Handle TransitionSec==0 → atomic target snap (Wetness/params in same frame)
  - If blend in progress and duration expires, queue next transition to start after current completes
  - Add optional hook for AWindDirectionalSource (guarded by tag) mapping Wind01 → strength/gust; skip on dedicated
  - Write unit tests: same seed → same sequence; TransitionSec==0 → snap; duration end during blend handled correctly; frame-rate independence test for wetness half-life (30 vs 120 Hz)
  - _Requirements: 2.1, 2.2, 2.3, 2.4, 2.6, 2.7_

- [ ] 7. Create Material Parameter Collection integration
  - Implement MPC_Environment parameter updates with configurable epsilon change detection (vhm.env.mpc.eps CVar)
  - Add parameter clamping and validation for all 7 MPC parameters
  - Cache UMaterialParameterCollectionInstance for efficient updates
  - Add "first frame flush" flag and pending-writes buffer until UMaterialParameterCollectionInstance resolves; never push default zeros
  - Provide material param namespace prefix (Env_) in code and docs to prevent conflicts
  - Skip MPC writes on dedicated server
  - Update MPC parameters only when values change to optimize performance
  - Write unit test: setting identical values does not call SetScalarParameterValue; make bound configurable via vhm.env.mpc.maxWritesPerSec (assert in test using current defaults); add STAT_VHM_MPCWritesSkipped counter
  - _Requirements: 3.1, 3.2, 3.3, 3.5_

- [ ] 8. Implement lighting actor integration
  - Create UpdateSolarAnglesAndLight() to apply sun rotation and intensity/color from curves
  - Cache base sun intensity/color values and apply multipliers (not absolutes)
  - Implement curve fallback system with smoothstep intensity and lerped day/night colors
  - On discovery, if SkyLight->SourceType == SLS_SpecifiedCubemap, skip recapture entirely and warn once
  - Add skylight recapture with throttling: only recapture if change metric > threshold AND interval passed
  - Add hysteresis band to change metric (5–10% of threshold) to avoid oscillation at sunrise/sunset
  - Throttle recapture with randomized jitter (±0.25s) to avoid client synchronization spikes
  - If using Lumen, expose vhm.env.recapture.allow 0/1 to disable globally
  - Force SkyLight->SetRealTimeCapture(false) if Mobility != Static; warn once if left on
  - Add Mobile/SM5 guard: only call recapture if component exists & platform supports it
  - Add hot-swap handling: if tagged actor destroyed and re-spawned (level streamed), registry should rebind without logging duplicates
  - Write integration test: drive elevation through sweep → recapture fires ≤ N times given thresholds; tiny elevation jiggle below threshold → no recapture; curve-missing fallback produces continuous output (no jumps)
  - _Requirements: 1.5, 1.6, 4.1_

- [ ] 9. Create networking replicator actor
  - Implement AVHMEnvironmentReplicator as AInfo with FVHMEnvironmentReplicationData
  - Add SetReplicates(true) in ctor; set bAlwaysRelevant=true, bOnlyRelevantToOwner=false, bNetLoadOnClient=true, NetDormancy=DORM_Awake, NetUpdateFrequency from vhm.env.net_hz CVar
  - Include server anchor in payload: ServerAbsDaySec (seconds into current 24h) for client drift reconciliation
  - When day/night lengths change at runtime, replicate the change timestamp (server time) so clients can rebase smoothly
  - Add proper replication setup with GetLifetimeReplicatedProps and OnRep_State
  - Implement EnsureReplicator() with seamless travel guard—check existing instance via TActorIterator and static weak ref; never spawn duplicates
  - PIE with multiple clients: ensure EnsureReplicator() chooses persistent world, not PIE preview worlds (log world name on spawn)
  - Create SetReplicatedTarget() for client interpolation in subsystem Tick
  - Late join: On first OnRep_State, snap (no blend) to server anchor once; subsequent updates blend
  - Add GameMode hook: in PostLogin (server), call Replicator->ForceNetUpdate() for late join handling
  - Write net tests (2 clients): late join convergence within K frames; frequency honored; seamless A→B travel doesn't duplicate replicator; include net payload size stat (bytes/frame)
  - _Requirements: 7.3, 7.4, 7.5_

- [ ] 10. Implement console commands system
  - Create VHMEnvironmentConsole.cpp with all required CVars and exec functions
  - Implement time control commands (pause, set_time, skip_minutes, daylen, nightlen, scale_day, scale_night)
  - Add weather control commands (set_weather, next_weather, force_weather, det, seed)
  - Add vhm.env.net_hz CVar and wire to Replicator->NetUpdateFrequency
  - Make vhm.env.help print auth requirements, current seed/state, values, ranges (QA can paste single screenshot in bugs)
  - Add time dilation mode: optional CVar to use dilated vs undilated delta for TOD
  - Implement runtime day/night length changes with SegmentProgress rescaling
  - For client-side commands that RPC to server, echo goal state back to client immediately in UI, but keep visuals driven by replication only
  - Add client → server path: either reject clearly or send reliable Server RPC on Replicator to apply
  - In comments and CVars, be consistent: seconds vs hours; always include units in help text
  - Write unit tests: client-only exec blocked; runtime day/night len change rescale SegT test near segment end; test both listen & dedicated server
  - _Requirements: 5.1_

- [ ] 11. Create debug overlay widget
  - Implement UVHMEnvironmentDebugWidget UMG class with required UI components
  - Add HH:MM clock display, segment progress bars, and weather state indicators
  - Create time scrubbing slider and weather control buttons (Sunny/Cloudy/Rain)
  - Add wetness readout, skylight recapture indicator, and snap/blend toggles
  - Guard overlay compile with #if !UE_BUILD_SHIPPING && WITH_EDITORONLY_DATA
  - Guard runtime registration with FSlateApplication::IsInitialized() to avoid headless server crashes
  - Wire widget updates to subsystem state changes
  - Write unit test: overlay factory returns nullptr in Shipping (guard works); shipping build never binds tick delegates
  - _Requirements: 5.2_

- [ ] 12. Add performance statistics and logging
  - Implement STATGROUP_VibeheimEnv with QUICK_SCOPE_CYCLE_COUNTER macros
  - Add LogVHMEnvTime and LogVHMEnvWeather categories with appropriate log levels
  - Wrap heavy operations (time update, weather update, skylight recapture, MPC update) in stats
  - Add one-time warnings for missing actors, curves, and configuration issues
  - Add one-liner startup summary log: "EnvSubsys: Sun=OK, Sky=OK, Clouds=Missing, Fog=OK, MPC=OK, Recapture=min=3s thr=0.02"
  - Write perf microbench: must run in commandlet (-nullrhi -unattended) to be stable; assert budget there
  - _Requirements: 5.3, 5.4, 7.1_

- [ ] 13. Implement public Blueprint API
  - Add BlueprintCallable getter functions (GetClockHours, IsDay, GetWeather, GetRain01, GetWetness01, GetSunDirection)
  - Expose time and weather control functions for Blueprint scripting
  - Mark events BlueprintAssignable (OnSunrise, OnSunset, OnRainStart, OnRainStop) for Niagara integration
  - Ensure all public API functions are properly documented and categorized
  - Write unit test: all getters return sane outputs with no Sun (MPC-only mode)
  - _Requirements: 2.5_

- [ ] 14. Add build configuration and module dependencies
  - Update Vibeheim.Build.cs with required dependencies (GameplayTags, UMG, Projects, DeveloperSettings, Engine)
  - Ensure module doesn't pull editor deps in runtime build (no UMG in server-only target if not needed)
  - Create gameplay tags for actor discovery (Env.Sun, Env.SkyLight, Env.SkyAtmosphere, Env.Clouds, Env.Fog)
  - Set up MPC_Environment asset with default parameter values
  - Configure developer settings with default curves and asset references
  - Add PrimaryAssetLabel that hard-refs MPC + default Time/Weather assets; cook test runs as commandlet
  - Write cook test: packaged build loads curves/MPC; no nulls
  - _Requirements: 4.1, 6.x_

- [ ] 15. Implement large time jump handling with visual easing
  - Add vhm.env.visual_ease CVar for smooth transitions vs instant snapping
  - Implement cross-fade system using single FAlphaBlend with SmoothStep; avoid dt-dependent easing
  - Explicitly state the blend clock is unscaled real time so time dilation changes don't affect visual ease unless desired
  - Handle multiple segment flips in single time skip (fire events at most once per segment)
  - Add console command force modes for weather changes during transitions
  - Write unit test: blend vs snap sets produce no >1-frame spikes in MPC deltas
  - _Requirements: 1.9_

- [ ] 16. Create comprehensive test suite
  - Write unit tests for determinism, edge cases, authority gating, rescale near boundary
  - Test weather weights zero/NaN guards, Min>Max duration, TransitionSec==0 snap path
  - Validate networking: client lerp location, late-join test verifying SegT/BlendAlpha/Wetness converge within N frames
  - Test SkyLight RealTimeCapture disabled, throttled recapture obeys interval and brightness/color threshold
  - Add automation perf test (simulate fast TOD) and MPC spam epsilon validation
  - Add fuzz test: random console spam (set_time, next_weather) over 5s—no asserts, no double-fired events
  - Add AutomationSpec that runs with -nullrhi -unattended and repeats determinism tests 10× with mixed FPS caps (30/60/120) to catch flakiness
  - Add soak test that toggles vhm.env.enabled at runtime—no leaks, no crashes, state resets cleanly
  - Verify determinism across different FPS caps (30/60/120) when deterministic mode on
  - _Requirements: 1.7, 2.7, 7.1_

- [ ] 17. Integration testing and final wiring
  - Create one minimal test map with tagged actors, default assets in DevSettings
  - Test complete system integration with all components working together
  - Verify MPC parameter updates drive materials correctly across day/night and weather changes
  - Run multi-client PIE test with server authority and client interpolation
  - Add dedicated server + two clients soak test (3–5 minutes): CPU, net payload size (bytes/sec), stable visuals
  - Add World Partition map test: stream sublevel with optional Fog actor after start → registry picks it up gracefully
  - Test sublevel unload removing the Sun (temporary) → registry must not null-deref
  - Consider running visual updates in PostUpdateWork equivalent frame phase to avoid mid-frame lighting pops
  - Validate skylight recapture throttling and visual quality impact
  - Ensure debug overlay and console commands work correctly in PIE and packaged builds
  - Remove stale references to LatitudeDeg from top-level requirements (not using it) or reintroduce simple axial tilt multiplier
  - _Requirements: 7.1, 7.2, 7.5_