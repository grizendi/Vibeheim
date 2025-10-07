# PCG 5.6 Verification Checklist

This checklist captures the verification flow required before enabling the
scheduler-driven runtime path on new builds or branches. Run every step in
sequence; mark failures with owner + follow-up issue before rolling forward.

## 1. Compile & Automation Smoke
- **Action:** Build `VibeheimEditor Win64 Development`.
- **Expected:** Compilation succeeds with no warnings from `VHM_PCG_ENABLED`
  guards; scheduler helper symbols resolve. If build fails, stop and address
  compilation errors before testing.
- **Notes:** Trigger Live Coding after hot reloads to confirm headers are
  picked up when iterating on metadata structs.

## 2. PIE Sanity Pass
- **Action:** Launch PIE in the primary streaming map for five minutes of
  traversal.
- **Expected:**
  - `LogPCGWorldService` only reports scheduled/completed telemetry (no fallback
    spam).
  - Streaming tiles populate with PCG-driven foliage; fallback HISM path stays
    dormant unless manually triggered.
- **Failure Handling:** Any fallback warning → capture callstack, graph,
  biome, tile coordinate, and repro steps. Do **not** advance to validation
  until resolved.

## 3. Graph Validation
- **Action:** Run `wg.pcg.validate <Biome>` for each biome targeted for release.
- **Expected:**
  - Validation reports zero errors.
  - Warnings only occur for intentionally unwired optional pins; document the
    rationale.
  - Missing attributes list is empty.
- **Failure Handling:** Wire missing execution dependency pins immediately;
  attribute mismatches require updating the graph or the canonical attribute
  contract before proceeding.

## 4. Budget & Performance Validation
- **Action:** Enable telemetry CSV logging with `vhm.pcg.telemetry.csv 1`,
  regenerate a 5×5 tile region per biome, then analyze `Saved/PCG/pcg_tasks.csv`.
- **Expected:**
  - Median tile execution ≤ **1.0 ms** with `MaxConcurrentPCGTasks` at default.
  - Timeout count remains `0`.
  - Fallback flag remains `0`.
- **Failure Handling:** If medians exceed the budget, lower concurrency and
  profile bottleneck nodes. Persist CSV artifact to the performance share and
  open a regression ticket.

## 5. Sign-Off
- **Action:** Record outcomes (pass/fail per step) in the release tracking
  sheet. Flag any known limitations (e.g., intentionally disabled biomes).
- **Expected:** Release owner initials final approval before enabling the
  feature flag for the target map or platform.

Following this checklist keeps the migration controlled and creates an audit
trail for every rollout iteration.
