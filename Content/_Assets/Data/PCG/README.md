# PCG Graph Authoring Notes

* Mesh references **must** be authored as Soft Object Path attributes. Use the `Set Attribute` node, set the type to `Soft Object Path`, and provide the asset reference (e.g. `/Game/Foo/Bar.Bar`).
* Supported attribute keys for meshes are `StaticMesh` or `Mesh`.
* Avoid wiring `UStaticMesh*` or other pointer types. Pointer metadata cannot be blended and will fail validation.
* Optional attribute `RespectGraphZ` (bool) controls whether runtime generation preserves the Z value authored in the graph. Leave unset or `false` to project instances onto the terrain automatically.
