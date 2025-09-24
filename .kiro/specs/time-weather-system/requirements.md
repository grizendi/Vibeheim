# Requirements Document

## Introduction

This system provides an asymmetric day/night cycle and a deterministic weather model (Sunny / Cloudy / Rain) with smooth visual transitions, MPC-driven material control, robust debugging, and server-authoritative state with lightweight replication. It integrates with Unreal Engine lighting/atmospheric actors and must be performant.

## Requirements

### Requirement 1 — Asymmetric Day/Night Cycle

**User Story:** As a player, I want believable time of day with different day/night lengths.

#### Acceptance Criteria

1. WHEN the system is configured THEN designers SHALL set DayLengthSec and NightLengthSec independently
2. WHEN time controls are used THEN time SHALL run continuously in game time and support pause, set absolute time (hours), and skip by minutes/hours
3. WHEN time scaling is configured THEN the system SHALL support independent time scales for Day and Night (e.g., Day ×1.0, Night ×1.5)
4. WHEN the sun updates THEN the DirectionalLight (Sun) rotation SHALL update every frame to reflect solar azimuth/elevation for the current segment
5. WHEN solar elevation changes THEN light intensity and color SHALL follow designer curves (by elevation) without discontinuities; IF curves are missing, sane defaults SHALL be applied and a warning logged once
6. WHEN atmospheric components update THEN SkyAtmosphere, SkyLight, Fog, and Clouds SHALL respond smoothly with eased interpolation; no visible popping on transitions or time jumps
7. WHEN deterministic mode is enabled THEN time progression SHALL use a fixed-step accumulator independent of frame rate and produce identical results given the same inputs
8. WHEN segment transitions occur THEN the system SHALL broadcast OnSunrise and OnSunset events exactly once per flip
9. WHEN time is set or skipped by a large amount THEN visuals SHALL ease to the new state (e.g., brief blend) while internal time updates immediately

### Requirement 2 — Dynamic Weather System

**User Story:** As a player, I want simple, occasional weather changes that fit the sky.

#### Acceptance Criteria

1. WHEN weather is active THEN supported states SHALL be Sunny, Cloudy, and Rain
2. WHEN a weather state is defined THEN it SHALL define targets: Cloudiness01, Rain01, Wind01, LightIntensityMul, FogDensityMul
3. WHEN entering a state THEN the duration SHALL be drawn uniformly from [MinDurationSec, MaxDurationSec] using a seeded deterministic RNG
4. WHEN weather transitions THEN transitions SHALL blend over TransitionSec with eased interpolation and SHALL not overshoot target values
5. WHEN Rain starts or stops THEN the system SHALL trigger hooks for Niagara VFX and looping SFX; stopping Rain SHALL fade out gracefully
6. WHEN Rain ends THEN Wetness01 SHALL decay toward 0 with a configurable half-life; during Rain it SHALL rise toward the state target
7. WHEN deterministic mode is enabled THEN, given a seed, the sequence of weather states and durations SHALL be reproducible
8. WHEN running in multiplayer THEN weather state SHALL be server-authoritative; clients SHALL replicate compact state and SHALL NOT run RNG locally

### Requirement 3 — Material Parameter Control (MPC)

**User Story:** As a developer, I want consistent material response to time & weather.

#### Acceptance Criteria

1. WHEN the system initializes THEN it SHALL create or use MPC_Environment with parameters: TimeOfDay01, SunElevationDeg, SunAzimuthDeg, Cloudiness01, Rain01, Wetness01, Wind01
2. WHEN the system ticks THEN it SHALL update these MPC parameters once per frame
3. WHEN parameters are updated THEN values SHALL be clamped to valid ranges
4. WHEN materials use the MPC THEN they SHALL update automatically without per-actor code
5. WHEN calculating TimeOfDay01 THEN it SHALL represent normalized progress across the entire day-night cycle [0..1] (not just the current segment)

### Requirement 4 — Environmental Actor Integration

**User Story:** As a level designer, I want easy integration with existing actors.

#### Acceptance Criteria

1. WHEN actors are tagged THEN the system SHALL discover by Gameplay Tags: Env.Sun → DirectionalLight (required, warn once if missing), Env.SkyLight (optional), Env.SkyAtmosphere (optional), Env.Clouds (optional), Env.Fog (optional)
2. WHEN actors are discovered THEN the system SHALL cache references for efficient updates
3. WHEN manual registration is needed THEN Blueprint actors SHALL be able to register themselves via API as overrides
4. IF optional actors are missing THEN the system SHALL degrade gracefully (no hard failures)

### Requirement 5 — Debug & Development Tools

**User Story:** As a developer, I need comprehensive controls and visibility.

#### Acceptance Criteria

1. WHEN using console commands THEN the system SHALL provide: vhm.env.pause, vhm.env.set_time <hours>, vhm.env.skip_minutes <m>, vhm.env.daylen <sec>, vhm.env.nightlen <sec>, vhm.env.scale_day <x>, vhm.env.scale_night <x>, vhm.env.set_weather <sunny|cloudy|rain>, vhm.env.next_weather, vhm.env.det <0|1>, vhm.env.seed <int>, vhm.env.recapture_skylight, vhm.env.show <0|1>
2. WHEN the debug overlay is enabled (non-shipping) THEN it SHALL display: HH:MM clock, segment (Day/Night) with progress bars, current weather and time to next change, a time scrub slider, and quick buttons for Sunny/Cloudy/Rain
3. WHEN logging is enabled THEN categories LogVHMEnvTime and LogVHMEnvWeather SHALL report state changes and control actions at Verbose levels
4. WHEN performance monitoring is needed THEN the VibeheimEnv stat group SHALL expose per-frame timings for time update, weather update, and Skylight recapture

### Requirement 6 — Data Configuration

**User Story:** As a designer, I want to edit settings via data assets.

#### Acceptance Criteria

1. WHEN configuring time THEN designers SHALL use UVHMTimeSettingsDataAsset containing: DayLengthSec, NightLengthSec, StartClockHours, LatitudeDeg (tilt/look), and curve references IntensityByElev & ColorByElev
2. WHEN configuring weather THEN designers SHALL use UVHMWeatherTableDataAsset with one entry per state (Sunny/Cloudy/Rain), each defining duration ranges, TransitionSec, and the targets in Req-2.2
3. WHEN defining weather transitions THEN states SHALL support weighted transitions to other states; unspecified weights default to 1.0
4. WHEN using deterministic mode THEN the weather table SHALL include a seed used for deterministic RNG
5. WHEN editing data assets THEN changes SHALL take effect on next world start (hot-reload optional, not required)

### Requirement 7 — Performance & Networking

**User Story:** As a developer, I need low cost and correct multiplayer behavior.

#### Acceptance Criteria

1. WHEN the system ticks THEN combined time + weather update cost SHALL be ≤ 0.1 ms per frame on mid-range hardware at default settings (measured via stats)
2. WHEN Skylight recapture occurs THEN it SHALL be throttled to no more than once every 2–5 seconds and only when brightness/color delta exceeds a small threshold; a cvar SHALL control the minimum interval
3. WHEN running on server THEN the subsystem SHALL tick and maintain authoritative time & weather state
4. WHEN running on clients THEN they SHALL receive a compact replicated payload and interpolate visuals locally. The payload SHALL include: Segment (Day/Night), SegT (normalized), DayLengthSec, NightLengthSec, CurrentWeather, NextWeather, BlendAlpha, Wetness01, and Seed (verification only)
5. WHEN clients receive updates THEN they SHALL smoothly interpolate to the new state without popping or time jumps; replication cadence SHALL be no faster than a configurable rate (e.g., 2–4 Hz)

### Requirement 8 — System Architecture & Lifecycle

**User Story:** As a developer, I want a clean, UE-native integration.

#### Acceptance Criteria

1. WHEN the system initializes THEN it SHALL be implemented as a UTickableWorldSubsystem
2. WHEN the world starts THEN the subsystem SHALL automatically discover tagged actors (Req-4) and cache references; manual registration SHALL override discovery
3. WHEN the system updates THEN all time, weather, and MPC updates SHALL occur inside the subsystem; actors remain passive
4. WHEN the system shuts down THEN the subsystem SHALL clear references, stop timers, stop/cleanup VFX/SFX, and unregister overlays cleanly
5. WHEN using the public API THEN the subsystem SHALL expose minimal controls for time, weather, deterministic mode, and actor registration

## Out of Scope (MVP)

Seasons, temperature, thunderstorms/lightning, region-based weather, calendar/moon phases, save/load persistence (may be added later)