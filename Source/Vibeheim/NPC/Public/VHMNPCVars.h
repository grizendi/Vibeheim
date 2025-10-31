#pragma once

#include "CoreMinimal.h"

// Console variables used by NPC systems (Needs, Tasks, Debugging)

// LOD distance override (cm). If > 0, overrides species LODDistance.
extern VIBEHEIM_API TAutoConsoleVariable<float> CVarVHM_LODDistance;

// Evaluation interval override (seconds). If > 0, overrides species EvaluationInterval.
extern VIBEHEIM_API TAutoConsoleVariable<float> CVarVHM_EvalInterval;

// Alternate alias for evaluation interval (kept for compatibility with docs/specs).
extern VIBEHEIM_API TAutoConsoleVariable<float> CVarVHM_EvaluationInterval;

// Deterministic mode toggle (1 = fixed timestep updates using eval interval; 0 = real DeltaTime)
extern VIBEHEIM_API TAutoConsoleVariable<int32> CVarVHM_Deterministic;

// Fast decay/recovery toggle to accelerate testing (1 = faster updates)
extern VIBEHEIM_API TAutoConsoleVariable<int32> CVarVHM_FastDecay;

// Seconds between resource searches. If > 0, used to throttle FindTarget.
extern VIBEHEIM_API TAutoConsoleVariable<float> CVarVHM_SearchThrottle;

// Maximum retries for FindTarget before giving up/escalating. If <= 0, uses species MaxRetries.
extern VIBEHEIM_API TAutoConsoleVariable<int32> CVarVHM_RetryMax;

// Debug draws: needs bar text above NPCs
extern VIBEHEIM_API TAutoConsoleVariable<int32> CVarVHM_DrawNeeds;

// Debug draws: target indicators (line to target, sphere at target/use radius)
extern VIBEHEIM_API TAutoConsoleVariable<int32> CVarVHM_DrawTarget;

// Debug draws: search radius sphere from NPC
extern VIBEHEIM_API TAutoConsoleVariable<int32> CVarVHM_DrawSearch;

// Convenience toggle that enables common target visuals (equivalent to draw_needs+draw_target)
extern VIBEHEIM_API TAutoConsoleVariable<int32> CVarVHM_ShowTargets;
