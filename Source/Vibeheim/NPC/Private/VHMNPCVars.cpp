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

TAutoConsoleVariable<float> CVarVHM_SearchThrottle(
    TEXT("vhm.npc.search_throttle"),
    1.0f,
    TEXT("Seconds between resource searches (FindTarget throttle). If <=0 uses species SearchCooldown."),
    ECVF_Default);

TAutoConsoleVariable<int32> CVarVHM_RetryMax(
    TEXT("vhm.npc.retry_max"),
    3,
    TEXT("Max retries for FindTarget before escalation. If <=0 uses species MaxRetries."),
    ECVF_Default);

TAutoConsoleVariable<int32> CVarVHM_DrawNeeds(
    TEXT("vhm.npc.draw_needs"),
    0,
    TEXT("Draw need values above NPCs (debug text)."),
    ECVF_Default);

TAutoConsoleVariable<int32> CVarVHM_DrawTarget(
    TEXT("vhm.npc.draw_target"),
    0,
    TEXT("Draw target line/sphere for current target (if any)."),
    ECVF_Default);

TAutoConsoleVariable<int32> CVarVHM_DrawSearch(
    TEXT("vhm.npc.draw_search"),
    0,
    TEXT("Draw search radius sphere for the NPC."),
    ECVF_Default);

TAutoConsoleVariable<int32> CVarVHM_ShowTargets(
    TEXT("vhm.npc.show_targets"),
    0,
    TEXT("Convenience toggle to enable common target visuals (needs + target)."),
    ECVF_Default);
