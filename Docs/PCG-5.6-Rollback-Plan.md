# PCG 5.6 Rollback Plan

This document describes the steps required to disable the scheduler-driven
runtime generation path and return to the deterministic HISM fallback without
losing authored data or breaking designer workflows.

## 1. Triggering the Rollback
1. Open **Project Settings → Vibeheim → World Generation**.
2. Set **Enable PCG Graphs** (`bEnablePCGGraphs`) to **false**.
3. For live sessions, execute the console command `wg.settings set pcggraphs 0`
   to apply the change without restarting the editor.
4. Propagate the setting to configuration control (`WorldGenSettings.json` and
   build configs) before packaging.

## 2. Verification Steps
- **PIE Check:** Launch PIE and confirm `LogPCGWorldService` reports
  "PCG graphs disabled" and falls back to `GenerateFallbackContent`.
- **Tile Generation:** Ensure streaming tiles continue to spawn foliage using
  the HISM path with parity to pre-migration builds.
- **Scheduler Logs:** Confirm no scheduler telemetry or task submissions are
  emitted while the flag is disabled.
- **Automation:** Run the headless regression test suite to ensure HISM-only
  code paths remain green.

## 3. Data Integrity
- PCG graphs, components, and metadata assets remain on disk and loadable.
- Active PCG tasks are abandoned automatically; tracked contexts release their
  `TStrongObjectPtr` references to metadata objects.
- No save data is mutated by the rollback—the fallback path persists the same
  logical instance payload used before the migration.

## 4. Communication Checklist
- Notify designers that runtime graph authoring can continue; graphs simply
  will not execute until the flag is re-enabled.
- Capture telemetry snapshots (if available) to compare scheduler timings
  before/after the rollback for postmortem analysis.
- Record the rollback in the release log with timestamp, operator, and reason.

## 5. Re-Enabling the Scheduler
1. Re-enable **Enable PCG Graphs** in settings or via console command.
2. Follow the [Verification Checklist](PCG-5.6-Verification-Checklist.md) before
   shipping the re-enabled build.
3. Monitor telemetry for at least one full play session after reenabling to
   ensure no regressions remain.

This plan ensures the project can revert to the known-good HISM path within
minutes while preserving all PCG content for later reactivation.
