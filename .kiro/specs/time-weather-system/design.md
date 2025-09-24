# Design Document

## Overview

The Time & Weather System is a UTickableWorldSubsystem that drives an asymmetric, deterministic day/night cycle and a simple weather model (Sunny / Cloudy / Rain) with smooth transitions. It:

- Updates UE lighting/atmosphere actors discovered by Gameplay Tags,
- Feeds materials through a single MPC_Environment,
- Exposes robust debug controls (CVars, overlay),
- Is server-authoritative with compact replication and client-side interpolation,
- Targets ≤ 0.1 ms per tick on mid-range hardware.

## Architecture

### High-Level System Design

```
UVHMEnvironmentSubsystem : UTickableWorldSubsystem
├─ Time
│  ├─ FVHMTimeState (segment, progress, lengths, sun angles)
│  ├─ Independent day/night time scales
│  └─ Deterministic fixed-step accumulator
├─ Weather
│  ├─ FVHMWeatherState (current, next, BlendAlpha, timers)
│  ├─ Seeded FRandomStream (deterministic)
│  └─ Wetness tail-off (half-life)
├─ Actor Integration
│  ├─ FEnvActorRegistry (Sun, SkyLight, SkyAtmosphere, Clouds, Fog)
│  ├─ Auto-discovery by Gameplay Tags (on world start)
│  └─ Manual registration API (overrides)
├─ MPC
│  ├─ MPC_Environment: TimeOfDay01, SunElevationDeg, SunAzimuthDeg
│  ├─ Cloudiness01, Rain01, Wetness01, Wind01
│  └─ Per-frame clamped updates
├─ Debug
│  ├─ CVars/exec (pause/set/skip/set_weather/det/seed/recapture/show)
│  ├─ UMG overlay (non-shipping)
│  └─ Logs (LogVHMEnvTime, LogVHMEnvWeather) + stats (VibeheimEnv)
└─ Net
   ├─ Server ticks; replicates compact payload
   └─ Clients interpolate visuals locally
```

### Module Structure

```
Source/Vibeheim/Environment/
├─ Public/
│  ├─ VHMEnvironmentSubsystem.h
│  ├─ VHMEnvironmentTypes.h
│  ├─ Data/
│  │  ├─ VHMTimeSettingsDataAsset.h
│  │  └─ VHMWeatherTableDataAsset.h
│  └─ Debug/
│     └─ VHMEnvironmentDebugWidget.h
└─ Private/
   ├─ VHMEnvironmentSubsystem.cpp
   ├─ VHMEnvironmentConsole.cpp
   └─ Debug/
      └─ VHMEnvironmentDebugWidget.cpp
```

## Components & Interfaces

### Core Data Structures

#### Time Types

```cpp
UENUM(BlueprintType)
enum class EVHMEnvSegment : uint8
{
    Day,
    Night
};

USTRUCT(BlueprintType)
struct VIBEHEIM_API FVHMTimeState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) EVHMEnvSegment CurrentSegment = EVHMEnvSegment::Day;
    UPROPERTY(BlueprintReadOnly) float SegmentProgress = 0.f;         // [0..1] in current segment
    UPROPERTY(BlueprintReadOnly) float CurrentSegmentDuration = 600.f;// sec
    UPROPERTY(BlueprintReadOnly) float DayTimeScale   = 1.f;
    UPROPERTY(BlueprintReadOnly) float NightTimeScale = 1.f;
    UPROPERTY(BlueprintReadOnly) float SunElevation   = 0.f;          // deg
    UPROPERTY(BlueprintReadOnly) float SunAzimuth     = 0.f;          // deg
    UPROPERTY(BlueprintReadOnly) float TimeOfDay01    = 0.f;          // [0..1] across whole cycle
    UPROPERTY(BlueprintReadOnly) bool  bDeterministicMode = false;
    UPROPERTY(BlueprintReadOnly) bool  bPaused = false;

    float FixedAccumulator = 0.f;  // internal (not exposed to BP)
};
```

#### Weather Types

```cpp
UENUM(BlueprintType)
enum class EVHMWeather : uint8
{
    Sunny,
    Cloudy,
    Rain
};

USTRUCT(BlueprintType)
struct VIBEHEIM_API FVHMWeatherState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) EVHMWeather CurrentWeather = EVHMWeather::Sunny;
    UPROPERTY(BlueprintReadOnly) EVHMWeather NextWeather    = EVHMWeather::Sunny;
    UPROPERTY(BlueprintReadOnly) float BlendAlpha = 0.f;   // [0..1] transition alpha
    UPROPERTY(BlueprintReadOnly) float StateTimeRemaining = 0.f;    // sec for current state
    UPROPERTY(BlueprintReadOnly) float TransitionTimeRemaining = 0.f;// sec during blend

    // Effective parameters (post-blend)
    UPROPERTY(BlueprintReadOnly) float Cloudiness01 = 0.f;
    UPROPERTY(BlueprintReadOnly) float Rain01       = 0.f;
    UPROPERTY(BlueprintReadOnly) float Wetness01    = 0.f; // rises in rain, decays w/ half-life
    UPROPERTY(BlueprintReadOnly) float Wind01       = 0.2f;
    UPROPERTY(BlueprintReadOnly) float LightIntensityMul = 1.f;
    UPROPERTY(BlueprintReadOnly) float FogDensityMul     = 1.f;

    // Determinism
    UPROPERTY(BlueprintReadOnly) int32 WeatherSeed = 1337;
};
```

#### Data Assets

```cpp
USTRUCT(BlueprintType)
struct VIBEHEIM_API FVHMTimeSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere) float DayLengthSec   = 600.f;
    UPROPERTY(EditAnywhere) float NightLengthSec = 300.f;
    UPROPERTY(EditAnywhere, meta=(ClampMin="0", ClampMax="24")) float StartClockHours = 9.f;
    UPROPERTY(EditAnywhere, meta=(ClampMin="-90", ClampMax="90")) float LatitudeDeg = 45.f;

    // Explicit sun path parameters (exposed to designers)
    UPROPERTY(EditAnywhere, meta=(ClampMin="0", ClampMax="90")) float PeakElevationDeg     = 55.f;
    UPROPERTY(EditAnywhere, meta=(ClampMin="0", ClampMax="90")) float NightLowElevationDeg = 25.f; // used as -NightLowElevationDeg
    UPROPERTY(EditAnywhere, meta=(ClampMin="0", ClampMax="360")) float BaseAzimuthDeg      = 180.f;

    UPROPERTY(EditAnywhere) TSoftObjectPtr<UCurveFloat>       LightIntensityByElev;
    UPROPERTY(EditAnywhere) TSoftObjectPtr<UCurveLinearColor> LightColorByElev;
};

USTRUCT(BlueprintType)
struct VIBEHEIM_API FVHMWeatherDef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere) EVHMWeather State = EVHMWeather::Sunny;
    UPROPERTY(EditAnywhere, meta=(ClampMin="1"))   float MinDurationSec   = 120.f;
    UPROPERTY(EditAnywhere, meta=(ClampMin="1"))   float MaxDurationSec   = 480.f;
    UPROPERTY(EditAnywhere, meta=(ClampMin="0.1")) float TransitionSec    = 8.f;

    // Targets [0..1] (multipliers where noted)
    UPROPERTY(EditAnywhere, meta=(ClampMin="0", ClampMax="1")) float Cloudiness01 = 0.1f;
    UPROPERTY(EditAnywhere, meta=(ClampMin="0", ClampMax="1")) float Rain01       = 0.0f;
    UPROPERTY(EditAnywhere, meta=(ClampMin="0", ClampMax="1")) float Wind01       = 0.2f;
    UPROPERTY(EditAnywhere, meta=(ClampMin="0", ClampMax="2")) float LightIntensityMul = 1.0f;
    UPROPERTY(EditAnywhere, meta=(ClampMin="0", ClampMax="2")) float FogDensityMul     = 1.0f;

    // Weighted transitions
    UPROPERTY(EditAnywhere, meta=(ClampMin="0")) float ToSunnyWeight  = 1.f;
    UPROPERTY(EditAnywhere, meta=(ClampMin="0")) float ToCloudyWeight = 1.f;
    UPROPERTY(EditAnywhere, meta=(ClampMin="0")) float ToRainWeight   = 1.f;
};

UCLASS(BlueprintType)
class VIBEHEIM_API UVHMTimeSettingsDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere) FVHMTimeSettings Settings;
};

UCLASS(BlueprintType)
class VIBEHEIM_API UVHMWeatherTableDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere) TArray<FVHMWeatherDef> States; // Sunny/Cloudy/Rain
    UPROPERTY(EditAnywhere) int32 Seed = 1337;
};
```

#### Environmental Actor Registry

```cpp
USTRUCT()
struct VIBEHEIM_API FEnvActorRegistry
{
    GENERATED_BODY()

    UPROPERTY() TWeakObjectPtr<ADirectionalLight>       SunLight;        // required
    UPROPERTY() TWeakObjectPtr<ASkyLight>               SkyLight;        // optional
    UPROPERTY() TWeakObjectPtr<ASkyAtmosphere>          SkyAtmosphere;   // optional
    UPROPERTY() TWeakObjectPtr<AVolumetricCloud>        VolumetricCloud; // optional
    UPROPERTY() TWeakObjectPtr<AExponentialHeightFog>   HeightFog;       // optional

    // Cached base values to apply multipliers (not absolutes)
    float BaseSunIntensity = 1.0f;
    FLinearColor BaseSunColor = FLinearColor::White;

    bool IsValid() const { return SunLight.IsValid(); }
    void DiscoverActors(UWorld* World);   // find by tags Env.Sun / Env.SkyLight / ...
    void ValidateActorSetup();            // check mobility, atmosphere linkage
    void ClearReferences();
};
```

#### Actor Integration Validation

- **Sun linkage**: If SkyAtmosphere exists, ensure Sun's AtmosphereSunLightIndex is valid (usually 0). If unset, apply at init and log once.
- **Mobility checks**: Warn once if SunLight or SkyLight are Static; dynamic cycling requires Stationary/Movable.
- **Skylight recapture**: Only works on USkyLightComponent::RecaptureSky() when mobility is Stationary/Movable.
- **Disable RealTimeCapture**: If manually recapturing, set `SkyLight->GetLightComponent()->SetRealTimeCapture(false)` to avoid double work.
- **Actor Tags fallback**: Engine lights don't implement IGameplayTagAssetInterface by default. Support both GameplayTags and AActor::Tags ("Env.Sun" in Details panel).

### Material Parameter Collection (MPC_Environment)

| Name | Type | Range | Notes |
|------|------|-------|-------|
| TimeOfDay01 | float | [0..1] | Normalized over full day+night cycle: `(CurrentSegment==Day ? (SegT * DayLen) : (DayLen + SegT * NightLen)) / (DayLen + NightLen)` |
| SunElevationDeg | float | [-90..90] | Used for intensity/color curves |
| SunAzimuthDeg | float | [0..360] | Visual interest |
| Cloudiness01 | float | [0..1] | |
| Rain01 | float | [0..1] | |
| Wetness01 | float | [0..1] | Decays post-rain with half-life |
| Wind01 | float | [0..1] | |

All parameters are clamped and written once per frame.

## Algorithms

### Time Progression (asymmetric, deterministic)

```cpp
void UVHMEnvironmentSubsystem::TickTime(float Dt)
{
    if (TimeState.bPaused) return;
    const float scale = (TimeState.CurrentSegment==EVHMEnvSegment::Day) ? TimeState.DayTimeScale : TimeState.NightTimeScale;
    const float scaledDt = Dt * scale;
    
    if (TimeState.bDeterministicMode) {
        TimeState.FixedAccumulator += scaledDt;
        constexpr float FixedStep = 1.f / 60.f; // fixed 60 Hz step
        while (TimeState.FixedAccumulator >= FixedStep) {
            AdvanceTime(FixedStep);
            TimeState.FixedAccumulator -= FixedStep;
        }
    } else {
        AdvanceTime(scaledDt);
    }
}

void UVHMEnvironmentSubsystem::AdvanceTime(float Dt)
{
    TimeState.SegmentProgress += Dt / TimeState.CurrentSegmentDuration;
    if (TimeState.SegmentProgress >= 1.f) {
        TimeState.SegmentProgress = 0.f;
        TimeState.CurrentSegment = (TimeState.CurrentSegment==EVHMEnvSegment::Day) ? EVHMEnvSegment::Night : EVHMEnvSegment::Day;
        TimeState.CurrentSegmentDuration = (TimeState.CurrentSegment==EVHMEnvSegment::Day) ? TimeCfg.DayLengthSec : TimeCfg.NightLengthSec;
        BroadcastSunriseSunset(); // exactly once per flip
    }
    UpdateSolarAnglesAndLight(); // rotation + intensity/color from curves (with defaults if missing)
    UpdateTimeOfDay01();         // normalized over full day+night
}
```

#### Large time jumps (set/skip)

- Internally set segment/durations immediately.
- **Provide bVisualEaseOnTimeSet CVar** (default true). If true, blend MPC/light over 0.25–0.5s; if false, snap instantly (useful for automated tests).
- **Sunrise/sunset events**: Skip time across multiple flips in a single call (e.g., +10 hours) — events should fire at most once per skipped segment.

#### Solar Angles (simple arc)

- **Day**: Elevation = sin(π * SegmentProgress) * PeakElevationDeg
- **Night**: Elevation = -sin(π * SegmentProgress) * NightLowElevationDegAbs
- **Azimuth**: (BaseAzimuth + TimeOfDay01 * 360) % 360

Curves IntensityByElev and ColorByElev sample by elevation (if null, use sane defaults and warn once).

### Weather State Machine (deterministic)

```cpp
void UVHMEnvironmentSubsystem::TickWeather(float Dt)
{
    // Transition handling
    if (WeatherState.TransitionTimeRemaining > 0.f) {
        WeatherState.TransitionTimeRemaining = FMath::Max(0.f, WeatherState.TransitionTimeRemaining - Dt);
        const float total = FMath::Max(0.001f, CurrentDef(WeatherState.NextWeather).TransitionSec);
        const float t = 1.f - (WeatherState.TransitionTimeRemaining / total);
        WeatherState.BlendAlpha = FMath::Clamp(t, 0.f, 1.f);
        if (WeatherState.TransitionTimeRemaining == 0.f) {
            WeatherState.CurrentWeather = WeatherState.NextWeather;
            WeatherState.StateTimeRemaining = RandomDuration(WeatherState.CurrentWeather);
            WeatherState.BlendAlpha = 0.f;
        }
    } else {
        // Steady state countdown
        WeatherState.StateTimeRemaining -= Dt;
        if (WeatherState.StateTimeRemaining <= 0.f) {
            WeatherState.NextWeather = SelectNextWeather();             // weighted by current state's weights
            WeatherState.TransitionTimeRemaining = CurrentDef(WeatherState.NextWeather).TransitionSec;
            WeatherState.BlendAlpha = 0.f;
        }
    }
    
    // Blend effective parameters from Current->Next by BlendAlpha (ease in/out)
    UpdateBlendedWeatherParams();
    
    // Wetness tail-off: rise during rain toward target; decay with half-life when no rain
    UpdateWetness(Dt);
}
```

SelectNextWeather() uses FRandomStream WeatherRNG(Seed) and normalized weights (ToSunnyWeight, ToCloudyWeight, ToRainWeight).

#### Console Command Weather Forcing

- **For CmdSetWeather**: set NextWeather = Target, TransitionTimeRemaining = TargetDef.TransitionSec, reset BlendAlpha=0.
- **If vhm.env.force_weather 1**: skip blend and set CurrentWeather=Target with StateTimeRemaining = RandomDuration(Target).
- **Transition edge case**: Weather duration ends exactly when a blend is already in progress — force a new transition only after the current completes.

#### Wetness Tail-Off

- **If Rain01 > 0**: Wetness01 = FMath::FInterpTo(Wetness01, RainTarget, Dt, RiseSpeed)
- **Else** decay exponentially: Wetness01 *= exp2(-Dt / HalfLifeSec)

#### Skylight Recapture (throttled)

- Maintain LastRecaptureTime and LastBrightness/Color.
- Compute a cheap change metric (e.g., |ΔElevation| + |ΔIntensity| + |ΔColor|) each tick.
- Recapture only if:
  - Now - LastRecaptureTime >= MinRecaptureIntervalSec **and**
  - ChangeMetric >= RecaptureThreshold.
- Expose CVars for both. Default interval 2–5s.
#
# Debug and Development Tools

### Console Commands Implementation

The system provides comprehensive console commands following the existing project patterns:

```cpp
// Time control commands
static FAutoConsoleCommand CmdPause(
    TEXT("vhm.env.pause"),
    TEXT("Pause/unpause time progression. Usage: vhm.env.pause <0|1>"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args) {
        // Implementation
    })
);

static FAutoConsoleCommand CmdSetTime(
    TEXT("vhm.env.set_time"),
    TEXT("Set absolute time in hours. Usage: vhm.env.set_time <hours>"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args) {
        // Implementation with smooth visual transition
    })
);

// Weather control commands
static FAutoConsoleCommand CmdSetWeather(
    TEXT("vhm.env.set_weather"),
    TEXT("Force weather state. Usage: vhm.env.set_weather <sunny|cloudy|rain>"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args) {
        // Implementation with transition blending
    })
);
```

### Debug Overlay Widget

UMG-based debug overlay with real-time controls:

```cpp
UCLASS(BlueprintType)
class VIBEHEIM_API UVHMEnvironmentDebugWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // Widget components
    UPROPERTY(meta = (BindWidget))
    class UTextBlock* ClockDisplay;

    UPROPERTY(meta = (BindWidget))
    class UProgressBar* DayProgressBar;

    UPROPERTY(meta = (BindWidget))
    class UProgressBar* NightProgressBar;

    UPROPERTY(meta = (BindWidget))
    class USlider* TimeScrubbingSlider;

    UPROPERTY(meta = (BindWidget))
    class UButton* SunnyButton;

    UPROPERTY(meta = (BindWidget))
    class UButton* CloudyButton;

    UPROPERTY(meta = (BindWidget))
    class UButton* RainButton;

    // Update functions called by subsystem
    UFUNCTION(BlueprintImplementableEvent)
    void UpdateTimeDisplay(const FString& TimeString, float DayProgress, float NightProgress);

    UFUNCTION(BlueprintImplementableEvent)
    void UpdateWeatherDisplay(const FString& CurrentWeather, const FString& NextWeather, float TimeToNext);
};
```

### Performance Statistics

```cpp
DECLARE_STATS_GROUP(TEXT("VibeheimEnv"), STATGROUP_VibeheimEnv, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("Time Update"), STAT_VHM_TimeUpdate, STATGROUP_VibeheimEnv);
DECLARE_CYCLE_STAT(TEXT("Weather Update"), STAT_VHM_WeatherUpdate, STATGROUP_VibeheimEnv);
DECLARE_CYCLE_STAT(TEXT("Skylight Recapture"), STAT_VHM_SkylightRecapture, STATGROUP_VibeheimEnv);
DECLARE_CYCLE_STAT(TEXT("MPC Update"), STAT_VHM_MPCUpdate, STATGROUP_VibeheimEnv);
```

## Networking Architecture

### Server-Authoritative Design

The server maintains the authoritative time and weather state, while clients receive compact updates and perform local interpolation for smooth visuals.

#### Replication Structure

```cpp
USTRUCT()
struct VIBEHEIM_API FVHMEnvironmentReplicationData
{
    GENERATED_BODY()

    // Time state
    UPROPERTY()
    EVHMEnvSegment Segment = EVHMEnvSegment::Day;

    UPROPERTY()
    float SegmentProgress = 0.0f;

    UPROPERTY()
    float DayLengthSec = 600.0f;

    UPROPERTY()
    float NightLengthSec = 300.0f;

    // Weather state
    UPROPERTY()
    EVHMWeather CurrentWeather = EVHMWeather::Sunny;

    UPROPERTY()
    EVHMWeather NextWeather = EVHMWeather::Sunny;

    UPROPERTY()
    float WeatherBlendAlpha = 0.0f;

    UPROPERTY()
    float Wetness01 = 0.0f;

    // Verification
    UPROPERTY()
    int32 WeatherSeed = 1337;
};
```

#### Client Interpolation

```cpp
void InterpolateToReplicatedState(const FVHMEnvironmentReplicationData& NewState, float DeltaTime)
{
    // Smooth time interpolation
    float TargetProgress = NewState.SegmentProgress;
    float CurrentProgress = TimeState.SegmentProgress;
    
    // Handle segment transitions
    if (NewState.Segment != TimeState.CurrentSegment)
    {
        TimeState.CurrentSegment = NewState.Segment;
        OnSegmentTransition();
    }
    
    // Interpolate progress smoothly
    TimeState.SegmentProgress = FMath::FInterpTo(CurrentProgress, TargetProgress, DeltaTime, 2.0f);
    
    // Weather interpolation
    WeatherState.WeatherBlendAlpha = FMath::FInterpTo(
        WeatherState.WeatherBlendAlpha, 
        NewState.WeatherBlendAlpha, 
        DeltaTime, 
        4.0f
    );
}
```

## Implementation Considerations

### Build Configuration Updates

The system requires additional module dependencies in `Vibeheim.Build.cs`:

```csharp
PublicDependencyModuleNames.AddRange(new string[]
{
    // Existing dependencies...
    "GameplayTags",      // For actor discovery
    "UMG",              // For debug overlay
    "Niagara"           // For weather VFX hooks (optional)
});

PrivateDependencyModuleNames.AddRange(new string[]
{
    // Existing dependencies...
    "DeveloperSettings", // For console commands
    "ToolMenus"         // For editor integration (optional)
});
```

### Gameplay Tags Setup

Required gameplay tags for actor discovery:

```
Env.Sun          - DirectionalLight (required)
Env.SkyLight     - SkyLight (optional)
Env.SkyAtmosphere - SkyAtmosphere (optional)  
Env.Clouds       - VolumetricCloud (optional)
Env.Fog          - ExponentialHeightFog (optional)
```

### Material Parameter Collection Setup

The MPC_Environment asset should be created in the project content with the following scalar parameters:

- TimeOfDay01 (default: 0.5)
- SunElevationDeg (default: 45.0)
- SunAzimuthDeg (default: 180.0)
- Cloudiness01 (default: 0.1)
- Rain01 (default: 0.0)
- Wetness01 (default: 0.0)
- Wind01 (default: 0.2)

### Performance Optimization Strategies

1. **Skylight Recapture Throttling**: Only recapture when brightness/color delta exceeds threshold
2. **MPC Update Batching**: Update all parameters in single call per frame
3. **Actor Reference Caching**: Cache weak pointers to avoid repeated world searches
4. **Deterministic Mode Optimization**: Use fixed-step accumulator only when needed
5. **Debug Overlay Conditional**: Only update UI when visible (non-shipping builds)

### Integration with Existing Systems

The environment system is designed to integrate cleanly with existing Vibeheim systems:

1. **WorldGen Integration**: Weather can influence procedural generation parameters
2. **VHM Terrain**: Wetness affects terrain material blending
3. **Debug Systems**: Follows existing debug overlay patterns from VHMDebugSystem
4. **Console Commands**: Uses same patterns as WorldGenConsoleCommands

### Future Extensibility

The design supports future enhancements while maintaining the MVP scope:

1. **Seasonal System**: Time settings can be extended with seasonal multipliers
2. **Regional Weather**: Weather states can be extended with spatial components
3. **Advanced Weather**: Additional weather types can be added to the enum and data assets
4. **Save/Load**: State structures are designed for easy serialization
5. **Calendar System**: Time progression can be extended with date tracking

This design provides a solid foundation for the MVP requirements while maintaining clean architecture for future expansion.## Debug
 & Tools

### CVars / Exec (examples)

- `vhm.env.pause 0/1`
- `vhm.env.set_time <hours>` / `vhm.env.skip_minutes <m>`
- `vhm.env.daylen <sec>` / `vhm.env.nightlen <sec>` (rescales SegmentProgress to preserve clock position)
- `vhm.env.scale_day <x>` / `vhm.env.scale_night <x>`
- `vhm.env.set_weather sunny|cloudy|rain` / `vhm.env.next_weather`
- `vhm.env.force_weather 0/1` (skip transitions when setting weather)
- `vhm.env.det 0/1` / `vhm.env.seed <int>`
- `vhm.env.recapture_skylight`
- `vhm.env.show 0/1`

**Authority gating**: Exec commands typed on clients should route to server or be rejected. Use `GetWorld()->GetAuthGameMode<AGameModeBase>()` check.

### Overlay (non-shipping)

UMG widget showing:

- HH:MM, segment (Day/Night) and progress bars,
- Weather (current/next), time to next, Rain/Cloud sliders,
- **Wetness readout (0..1)** and **recapture indicator** (grey/green),
- Scrub slider, Sunny/Cloudy/Rain buttons, Pause toggle,
- **"Snap" vs "Blend" toggle** for set_time and set_weather, mirroring the CVars.

### Stats & Logs

```cpp
DECLARE_STATS_GROUP(TEXT("VibeheimEnv"), STATGROUP_VibeheimEnv, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("Time Update"),         STAT_VHM_TimeUpdate,         STATGROUP_VibeheimEnv);
DECLARE_CYCLE_STAT(TEXT("Weather Update"),      STAT_VHM_WeatherUpdate,      STATGROUP_VibeheimEnv);
DECLARE_CYCLE_STAT(TEXT("Skylight Recapture"),  STAT_VHM_SkylightRecapture,  STATGROUP_VibeheimEnv);
DECLARE_CYCLE_STAT(TEXT("MPC Update"),          STAT_VHM_MPCUpdate,          STATGROUP_VibeheimEnv);
```

Logs: LogVHMEnvTime, LogVHMEnvWeather (warn once for missing actors/curves).

## Networking

### Server-Authoritative with Replicator Actor

- Server ticks time & weather, holds RNG seed and durations.
- **UTickableWorldSubsystem cannot replicate directly** - requires AVHMEnvironmentReplicator actor.
- Replicates compact payload at 2–4 Hz (configurable via vhm.env.net_hz CVar).

```cpp
USTRUCT()
struct VIBEHEIM_API FVHMEnvironmentReplicationData
{
    GENERATED_BODY()

    // Time
    UPROPERTY() EVHMEnvSegment Segment = EVHMEnvSegment::Day;
    UPROPERTY() float SegT = 0.f;                 // normalized segment progress
    UPROPERTY() float DayLen = 600.f;
    UPROPERTY() float NightLen = 300.f;

    // Weather
    UPROPERTY() EVHMWeather Current = EVHMWeather::Sunny;
    UPROPERTY() EVHMWeather Next    = EVHMWeather::Sunny;
    UPROPERTY() float BlendAlpha = 0.f;
    UPROPERTY() float Wetness01  = 0.f;

    // Verification only
    UPROPERTY() int32 Seed = 1337;
};

// Replicator actor (required for subsystem networking)
UCLASS(NotPlaceable, Transient)
class VIBEHEIM_API AVHMEnvironmentReplicator : public AInfo
{
    GENERATED_BODY()
public:
    AVHMEnvironmentReplicator();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

    UPROPERTY(ReplicatedUsing=OnRep_State)
    FVHMEnvironmentReplicationData NetState;

    UFUNCTION() void OnRep_State();

    // Server-side API
    void SetState(const FVHMEnvironmentReplicationData& In);

    // Tuning
    UPROPERTY(EditDefaultsOnly) float NetHz = 3.f; // 2-4 Hz
};
```

### Client Interpolation (Proper Pattern)

```cpp
// Replicator: store target, don't interpolate in OnRep
void AVHMEnvironmentReplicator::SetState(const FVHMEnvironmentReplicationData& In)
{
    NetState = In;
    ForceNetUpdate(); // correct runtime method
}

void AVHMEnvironmentReplicator::OnRep_State()
{
    if (auto* Subsys = GetWorld()->GetSubsystem<UVHMEnvironmentSubsystem>())
        Subsys->SetReplicatedTarget(NetState); // store target; no lerp here
}

// Subsystem: interpolate in Tick, not OnRep
FVHMEnvironmentReplicationData TargetNetState;
bool bHasTarget = false;

void UVHMEnvironmentSubsystem::SetReplicatedTarget(const FVHMEnvironmentReplicationData& S)
{
    TargetNetState = S;
    bHasTarget = true;
}

void UVHMEnvironmentSubsystem::Tick(float Dt)
{
    if (!bReady) return;

    if (IsNetModeClient() && bHasTarget)
    {
        // Time interpolation
        if (TimeState.CurrentSegment != TargetNetState.Segment) {
            TimeState.CurrentSegment = TargetNetState.Segment;
            OnSunFlipClient();
        }
        TimeState.SegmentProgress = FMath::FInterpTo(TimeState.SegmentProgress, TargetNetState.SegT, Dt, 2.f);

        // Weather interpolation
        WeatherState.BlendAlpha = FMath::FInterpTo(WeatherState.BlendAlpha, TargetNetState.BlendAlpha, Dt, 4.f);
        WeatherState.CurrentWeather = TargetNetState.Current;
        WeatherState.NextWeather    = TargetNetState.Next;
        WeatherState.Wetness01      = TargetNetState.Wetness01; // authoritative
    }

    TickTime(Dt);
    TickWeather(Dt);
    UpdateMPCAndActors(Dt);
}
```

## Error Handling & Validation

- **Missing Sun (Env.Sun)**: warn once; continue with MPC updates.
- **Missing curves**: use defaults (linear intensity; white color); warn once.
- **Invalid durations**: clamp to sane mins; log error.
- **MPC missing**: create at runtime if possible, else run without material updates; log error.
- **Graceful degradation**: all optional actors are optional.

## Performance Considerations

- **≤ 0.1 ms per tick target**: batch MPC writes; avoid repeated lookups; cache actor refs.
- **Skylight recapture throttled** with interval + threshold CVars.
- **Deterministic mode**: only enable fixed-step if the cvar is set.
- **Overlay/UI**: update only when visible.

## Build & Integration

### Build.cs

Add (if not already present):

- **Public**: GameplayTags, UMG, Niagara (optional), Engine
- **Private**: DeveloperSettings, Projects (for GConfig/settings)

### Developer Settings Asset

Create **UVHMEnvironmentSettings : UDeveloperSettings** with:

- SoftObjectPtr to MPC_Environment (authoritative path)
- Tag names (override defaults: Env.Sun, Env.SkyLight, etc.)
- Skylight thresholds & min recapture interval
- Default curves & time/weather assets
- Base day/night colors for curve fallbacks

### Initialization Order & Async Assets

- World subsystems may tick before SoftObjectPtr curves and MPC are loaded.
- Add **bReady flag** and early-out in Tick() until InitializeEnvironment() returns true:
  - Actor discovery completed
  - Curves resolved (load sync once or async then block before enabling tick)
  - MPC pointer valid

### Gameplay Tags (actor discovery)

```
Env.Sun          → ADirectionalLight (required)
Env.SkyLight     → ASkyLight (optional)
Env.SkyAtmosphere→ ASkyAtmosphere (optional)
Env.Clouds       → AVolumetricCloud (optional)
Env.Fog          → AExponentialHeightFog (optional)
```

### MPC_Environment (defaults)

Scalars: TimeOfDay01=0.5, SunElevationDeg=45, SunAzimuthDeg=180, Cloudiness01=0.1, Rain01=0, Wetness01=0, Wind01=0.2.

## Future Extensibility (kept clean)

Seasons (multipliers, curve sets), regional weather, storms/lightning, calendar & moon, save/load (serialize FVHMTimeState + FVHMWeatherState).

## Public API Extensions

### Read-Only Getters (for gameplay scripting)

```cpp
// Expose on UVHMEnvironmentSubsystem for Blueprint/gameplay use
UFUNCTION(BlueprintCallable, Category = "Environment")
float GetClockHours() const;          // 0..24

UFUNCTION(BlueprintCallable, Category = "Environment")
bool IsDay() const;

UFUNCTION(BlueprintCallable, Category = "Environment")
EVHMWeather GetWeather() const;

UFUNCTION(BlueprintCallable, Category = "Environment")
float GetRain01() const;

UFUNCTION(BlueprintCallable, Category = "Environment")
float GetWetness01() const;

UFUNCTION(BlueprintCallable, Category = "Environment")
FVector GetSunDirection() const;        // normalized
```

## Critical Implementation Notes

### Curve Fallbacks

If either curve is null:

- **Intensity**: remap elevation to [0..1] using smoothstep, multiply by BaseIntensity.
- **Color**: use lerped day/night colors (two editable FLinearColor defaults in settings).

### Easing Function

Standardize to **SmoothStep(t) = t*t*(3-2*t)** for cross-fades (weather & big time jumps). Avoid overshoot without depending on frame dt.

### Performance Guards

- Keep RNG (FRandomStream) and small floats in the subsystem; do not rebuild RNG every transition.
- Wrap heavy operations in **QUICK_SCOPE_CYCLE_COUNTER(STAT_VHM_XXXX)** to make perf budget explicit.

### Test Cases (Critical Edge Cases)

1. **Transition edge**: Weather duration ends exactly when a blend is already in progress.
2. **Sunrise/sunset once**: Skip time across multiple flips in a single call — events should fire at most once per skipped segment.
3. **Skylight throttle**: Set very small time scale, ensure recapture still respects min interval & threshold.
4. **Late join**: Connect a client mid-rain; verify Wetness01, BlendAlpha, SegT match within one replicated frame.
5. **RealTimeCapture accidentally left on**: Verify throttle still works (it won't—hence the explicit disable).
6. **Force weather mid-transition**: Test both "blend" and "snap" modes.
7. **StartClockHours at boundaries**: 0.0, 12.0, 23.99 — no off-by-one into wrong segment.
8. **Runtime length change while near segment end**: Rescale logic doesn't skip or double-fire sunrise/sunset.#
# Critical Implementation Algorithms

### Initialize from StartClockHours

```cpp
void UVHMEnvironmentSubsystem::InitFromStartClock()
{
    const float day = TimeCfg.DayLengthSec, night = TimeCfg.NightLengthSec;
    const float total = day + night;
    const float seconds = FMath::Clamp(TimeCfg.StartClockHours, 0.f, 24.f) / 24.f * total;

    if (seconds < day) {
        TimeState.CurrentSegment = EVHMEnvSegment::Day;
        TimeState.CurrentSegmentDuration = day;
        TimeState.SegmentProgress = seconds / day;
    } else {
        const float s = seconds - day;
        TimeState.CurrentSegment = EVHMEnvSegment::Night;
        TimeState.CurrentSegmentDuration = night;
        TimeState.SegmentProgress = s / night;
    }
}
```

### Runtime Day/Night Length Changes

```cpp
void UVHMEnvironmentSubsystem::SetDayLength(float NewLen)
{
    if (TimeState.CurrentSegment == EVHMEnvSegment::Day) {
        const float abs = TimeState.SegmentProgress * TimeState.CurrentSegmentDuration;
        TimeState.CurrentSegmentDuration = FMath::Max(1.f, NewLen);
        TimeState.SegmentProgress = abs / TimeState.CurrentSegmentDuration;
    } else {
        TimeCfg.DayLengthSec = FMath::Max(1.f, NewLen);
    }
}
```

### Actor Discovery with Fallback

```cpp
static bool HasEnvTag(const AActor* A, const FName& Tag, const FGameplayTag& GT)
{
    if (const IGameplayTagAssetInterface* IFace = Cast<IGameplayTagAssetInterface>(A))
    {
        FGameplayTagContainer C;
        IFace->GetOwnedGameplayTags(C);
        if (C.HasTag(GT)) return true;
    }
    return A->Tags.Contains(Tag);
}
```

### Ensure Single Replicator Per World

```cpp
void UVHMEnvironmentSubsystem::EnsureReplicator()
{
    if (!IsNetModeServer()) return;
    for (TActorIterator<AVHMEnvironmentReplicator> It(GetWorld()); It; ++It) {
        Replicator = *It;
        break;
    }
    if (!Replicator.IsValid())
        Replicator = GetWorld()->SpawnActor<AVHMEnvironmentReplicator>();
}
```

### Performance Optimizations

- **MPC Change Detection**: Set parameters only when they changed since last tick (cheap float comparison).
- **Base Value Caching**: Store original sun intensity/color at init, apply multipliers (not absolutes).
- **Ready Gating**: Gate tick until actors found, curves `LoadSynchronous()`, MPC instance acquired.
- **Skylight Setup**: Disable RealTimeCapture if manually recapturing to avoid double work.