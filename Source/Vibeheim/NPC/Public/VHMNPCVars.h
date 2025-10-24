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

