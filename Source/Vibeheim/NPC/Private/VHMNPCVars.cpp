#include "VHMNPCVars.h"

#include "HAL/IConsoleManager.h"

TAutoConsoleVariable<float> CVarVHM_LODDistance(
    TEXT("vhm.npc.lod_distance"),
    3000.0f,
    TEXT("LOD distance (cm) for NPC AI evaluation cadence"),
    ECVF_Default);

TAutoConsoleVariable<float> CVarVHM_EvalInterval(
    TEXT("vhm.npc.eval_interval"),
    0.5f,
    TEXT("Global NPC evaluation interval override (seconds). If <=0 uses species settings."),
    ECVF_Default);

TAutoConsoleVariable<float> CVarVHM_EvaluationInterval(
    TEXT("vhm.npc.evaluation_interval"),
    0.5f,
    TEXT("Alternate name for eval interval (seconds). If <=0 uses species settings."),
    ECVF_Default);

TAutoConsoleVariable<int32> CVarVHM_Deterministic(
    TEXT("vhm.npc.deterministic"),
    0,
    TEXT("Enable deterministic mode (fixed timestep using eval interval) for NPC systems"),
    ECVF_Default);

TAutoConsoleVariable<int32> CVarVHM_FastDecay(
    TEXT("vhm.npc.fast_decay"),
    0,
    TEXT("Accelerate need decay/recovery for testing"),
    ECVF_Default);

