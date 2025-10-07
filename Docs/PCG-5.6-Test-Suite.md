# PCG 5.6 Test Suite Overview

This document catalogues the automation coverage for the UE 5.6 PCG migration
and explains how to execute and interpret the tests. Use it as the reference
when triaging regressions or onboarding new tests.

## Directory Layout
All tests live in `Source/Vibeheim/WorldGen/Private/Tests/`.

- **Scheduler Unit Tests:** `PCGSchedulerTests.cpp`
  - Validates synchronous helper success, timeout, invalid graph handling,
    subsystem availability, task lifecycle, and GT marshalling.
- **Integration Tests:**
  - `PCGWorldServiceIntegrationTests.cpp` – end-to-end tile generation,
    fallback triggers, concurrency caps, and frustum policy.
  - `VHMIntegrationTest.cpp`, `WorldGenIntegrationTest.cpp` – streaming
    orchestration and telemetry checks.
- **Regression Tests:**
  - `DeterminismTest.cpp` – dependency pin determinism and transform hashing.
  - `ComprehensiveDiagnostic.cpp`, `DiagnosticFailureTests.cpp` – validation
    error surfacing and remediation messaging.
  - `PCGWorldServiceIntegrationTests.cpp` Difference + GetActorData regression.
- **Fallback/Headless Tests:**
  - `VHMCompatibilityRuntimeTests.cpp`, `VHMComponentTests.cpp` – ensure
    dedicated server/headless modes stay on HISM path.

## Running the Tests
1. Build the **VibeheimEditor** target.
2. Launch with `UnrealEditor-Cmd.exe Vibeheim.uproject -ExecCmds="Automation RunTests PCG" -nop4 -unattended`.
3. Alternatively, run individual suites in-editor via the Automation window
   under the `PCG` category.

## Expected Results & Interpretation
- **Pass Criteria:** All scheduler, integration, and regression suites return
  `Success`. Telemetry logs should contain scheduler submission/completion for
  PCG-enabled runs.
- **Timeout Failures:** Indicate either regression in `MaxConcurrentPCGTasks`
  handling or graph complexity spikes. Investigate telemetry CSV and adjust
  budgets.
- **Validation Failures:** Provide explicit attribute/type mismatches. Update
  the offending graph or metadata contract before re-running tests.
- **Fallback Assertions:** Any unexpected fallback in scheduler-enabled tests
  must be triaged immediately; consult `LogPCGWorldService` output captured in
  the automation logs.

## Adding New Tests
1. Derive new fixtures from the existing scheduler harness (`FPCGTestBase`).
2. Reuse the lightweight test graphs located in
   `Vibeheim/_Assets/Data/PCG/Tests` to avoid loading production content.
3. Document the new test scenario and expected results in this file with a
   cross-reference to the source file.

Maintaining this suite is critical to guaranteeing regression-free rollouts as
we enable the scheduler across more biomes and maps.
