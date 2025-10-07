# Runtime PCG Contract

This file captures the expectations between runtime code and designer-authored PCG graphs.

## Canonical inputs
- **Tile** – Spatial input representing the tile volume. All graphs should consume this pin.
- **TileParameters** – Parameter data that exposes tile metrics (seed, slope, biome weight, etc.).

> ✅ Keep pin names and tags exactly as shown so runtime scheduling can wire graphs automatically.

## Cookbook: minimal scatter graph
1. **Input (Tile)** → feed into `Distribute Points in Volume` configured to the tile extents.
2. **Input (TileParameters)** → use `Get Attribute` nodes to read:
   - `TileSeed`
   - `AverageSlope`
   - `MinWaterDistance`
   - `TileSize`
   - `DensityScale`
   - `BiomeWeight`
3. Use `TileSeed` for any stochastic node to keep generation deterministic.
4. Optionally derive per-point data (e.g., via a custom *GetTerrainHeight* node) and filter against slope or water proximity.
5. Before output, write attributes with `Set Attribute`:
   - `Mesh` (**Soft Object Path**, never an object pointer)
   - `InstanceScale` (`Vector`)
   - `InstanceRotation` (`Rotator`)
   - `RespectGraphZ` (`bool`, set to `false` if C++ should project onto the terrain)
6. Emit the result through the **Output → Points** pin.

### Soft reference policy
Runtime extraction only honours mesh references authored as Soft Object Paths. Pointer types (`UStaticMesh*`, `Object`) do not participate in streaming or blends.
