Vibeheim Simple Test World – Commands and Gates

Commands
- wg.launch: initialize systems in /Game/Maps/WG_TestMap
- wg.seed <value>: set authoritative world seed
- wg.radii <gen> <load> <active>: set streaming radii
- wg.reset: reset settings and caches
- wg.validate: validate VHM integration
- wg.gateA: orbit seam validation at two LOD thresholds
- wg.test.determinism <seed> [tiles] [-writebaseline]: determinism checksums
- wg.perf.export [filename.csv]: export per‑tile perf CSV to Saved/Vibeheim/WorldGen/Perf/
- wg.status: print seed, radii, tile counts, cache efficiency, avg/peak gen ms

CSV Columns
- TileX,TileY,GenMs,PCGMs,StreamInMs,GTOverheadMs,ThreadSpikesMs
  - GenMs: heightfield+biome generation time
  - PCGMs: PCG generation time
  - StreamInMs: VHM mesh creation time for the tile
  - GTOverheadMs: call time minus StreamInMs
  - ThreadSpikesMs: max frame‑time minus baseline over ~3s window

Gates (DoD)
- Gate A: no gaps/lighting steps while orbiting LOD thresholds
- Gate B: edits raise/lower/smooth/noise; update ≤100ms; PCG cleared; reload persists
- Gate C: determinism checksums match across launches for first N tiles
- Gate D: perf CSV for ~30 tiles; worker p50 ≤10ms, p95 ≤20ms; spikes ≤+8ms during activation

Test Map
- All commands operate only on /Game/Maps/WG_TestMap

Notes
- Seed and radii are read from WorldGenSettings and can be adjusted via commands.
- Perf CSV uses ERR:<code> in cells for tiles that failed to generate (no empty rows).
