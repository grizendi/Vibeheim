# Implementation Plan

## Hard Gates (No Proceed Until Green)
- **Gate A** (after Task 2): First ring renders in PIE, no visible cracks when orbiting LOD boundaries
- **Gate B** (after Task 3): 3 edits (raise/lower/smooth) apply with ≤100ms visible update; nearby PCG cleared; restart and edits persist
- **Gate C** (after Task 4): Same seed, two launches → identical height/biome checksums for first 10 tiles
- **Gate D** (after Task 5): CSV shows worker p50 ≤10ms, p95 ≤20ms; max spike ≤+8ms over baseline during activation burst

---

- [x] 1. Subsystem + Core Commands

  - Create UWorldGenTestSubsystem with runtime console command registration inside Initialize() (no statics)
  - Establish one authoritative seed source (UGameInstanceSubsystem or config asset) that all systems read from
  - Add wg.launch, wg.seed <n>, wg.radii <gen> <load> <active>, wg.reset commands with WITH_EDITOR guards
  - Ensure commands only act on /Game/Maps/WG_TestMap - print single-line error and bail on wrong map
  - Update Vibeheim.Build.cs: set bUseUnity = true, add VirtualHeightfieldMesh, RenderCore, RHI, PCG dependencies
  - Gate check: No crashes, no stale pointers after hot reload, commands ignored with log line outside WG_TestMap
  - _Requirements: 1.1, 5.1, 5.3_

- [x] 2. Create Test Map + VHM Integration





  - Create /Game/Maps/WG_TestMap.umap with single AWorldGenManager (spawn if missing)
  - Integrate VHMTerrainRenderer with WorldGenManager initialization sequence
  - Implement fixed streaming ring (Generate=9, Load=5, Active=3) with tiny hysteresis (keep tiles alive 1 ring beyond Active)
  - Ensure VHM components are created for tiles within Active radius with proper heightfield textures
  - Add flat meadow fallback for VHM creation failures with one error code line
  - Implement basic seam prevention: either small skirt or index stitching (not both)
  - Normalize normals/tangents across tile borders to prevent lighting seams
  - **Gate A**: Orbit seam at two LOD thresholds - no gaps, no lighting steps, no foliage popping at blend bands
  - _Requirements: 1.1, 1.2, 1.3, 1.6_

- [ ] 3. Terrain Editing System
  - Implement 4 terrain editing brushes: raise/lower/smooth/noise with radius & falloff parameters
  - Create console commands: wg.edit.raise <x> <y> <radius> <strength>, wg.edit.lower, wg.edit.smooth, wg.edit.noise
  - Ensure VHM visible + collision update ≤100ms on edited area (gate measurement to first collision update received)
  - Clear PCG by deleting/pooling HISM/HISMC instances within R = 1.25×brushRadius (no component leaks)
  - Implement terrain edit persistence to Saved/Vibeheim/WorldGen/Mods/ with versioned journal
  - Verify terrain edits reload correctly after restart with version mismatch handling
  - **Gate B**: No orphaned HISM components after 20 edit cycles; reloaded session shows identical instance counts within brush region
  - _Requirements: 2.1, 2.2, 2.3, 2.4_

- [ ] 4. Determinism Testing System
  - Implement wg.test.determinism <seed> [tiles] [-writebaseline] command (default 10 tiles)
  - Compute checksums in spiral tile order from (0,0) for stable ordering
  - Create checksum calculation for heightfield + biome ID + climate data only (exclude timestamps, GUIDs, counters)
  - Add optional -writebaseline flag to persist checksums to Saved/.../Determinism/ for comparison
  - Add logging that prints only first 3 diffs on mismatch (don't spam)
  - Implement tile data extraction and checksum generation utilities
  - **Gate C**: Two independent launches match 10/10 checksums
  - _Requirements: 1.5_

- [ ] 5. Performance Monitoring + CSV Export
  - Use FPlatformTime::Seconds() and store durations in ms (not absolute times)
  - Create CSV headers: TileX,TileY,GenMs,PCGMs,StreamInMs,GTOverheadMs,ThreadSpikesMs
  - Define spike as max(frameTime - baseline) over last N seconds during activation (single number)
  - Add wg.perf.export command that writes CSV to Saved/Vibeheim/WorldGen/Perf/
  - Write error codes for failed tiles instead of empty rows
  - Implement performance data collection during tile generation and streaming
  - **Gate D**: CSV with ≥30 tiles; p50/p95 within targets; zero empty rows
  - _Requirements: 4.2, 4.4, 4.5_

- [ ] 6. Final Polish & Validation
  - Add console command parameter validation and single-line help per command
  - Create tiny README: commands, gates, DoD, CSV/export locations, seed & radii configuration
  - Add optional wg.status command that prints seed, radii, active rings, and tile counts
  - Ensure no category errors/warnings from LogWorldGen, LogVHM, LogPCGWorld (one concise line per failure with error code)
  - Validate 5-minute traversal across ≥30 tiles with no category errors/warnings
  - Verify all gates pass: Gate A (no seams), Gate B (edits work), Gate C (determinism), Gate D (performance)
  - _Requirements: All requirements validation_