# PCG Test Asset Pack

This folder contains lightweight PCG graph definitions used by automation
tests. Each JSON file mirrors a minimal `UPCGGraph` configuration that can be
imported via the PCG editor's **Import Graph** command or recreated manually in
the editor following the listed nodes.

## Graph Summary

| File | Purpose |
| ---- | ------- |
| `PG_Test_Forest.pcg.json` | Scatter four conifer instances with canonical attributes |
| `PG_Test_Meadows.pcg.json` | Scatter grass tufts using BiomeWeight modulation |
| `PG_Test_Mountains.pcg.json` | Place sparse rock meshes with slope filtering |
| `PG_Test_Ocean.pcg.json` | Spawn shoreline props gated on water coverage |

All graphs share the same contract:

- `TileParameters` input pins expose parameter metadata (`TileSeed`, `BiomeId`,
  `AverageSlope`, etc.).
- `TilePoints` input provides the base point set for deterministic scattering.
- Output points emit the canonical attributes required by
  `ExtractInstancesFromPointData`.

## Import Instructions

1. In the PCG editor, create a new `PCGGraph` asset in the destination folder.
2. Open the graph and use **File → Import Graph** (or copy/paste JSON into the
   developer utility) to populate nodes and edges.
3. Re-link mesh asset references to project-local meshes as needed. Use test
   meshes (simple cube/plane) to keep the package lightweight.
4. Save the asset and repeat for each biome.

Automation tests reference these assets under the path:
```
/Vibeheim/_Assets/Data/PCG/Tests/PG_Test_<Biome>
```
Ensure the graphs remain partition-compatible (single tile input, runtime
execution enabled) so synchronous helpers can run them without editor-specific
context.
