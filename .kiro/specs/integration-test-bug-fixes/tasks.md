# Implementation Plan

- [x] 1. Implement missing terrain persistence methods







  - Implement `SaveTileTerrainDeltas()` method in HeightfieldService.cpp to serialize modifications to .terra files
  - Implement `LoadTileTerrainDeltas()` method in HeightfieldService.cpp to deserialize modifications from .terra files
  - Implement `ApplyModificationsToTile()` method to apply loaded modifications to heightfield data
  - Add detailed diagnostic logging to track file I/O operations and modification counts
  - _Requirements: 1.2, 1.6_

- [x] 2. Fix terrain persistence - add missing ApplyModifications call





  - Modify `GenerateHeightfield()` method in HeightfieldService.cpp to call `ApplyModificationsToTile()` after base generation
  - Add call to `CalculateNormalsAndSlopes()` after applying modifications
  - Ensure modifications are applied for both cached and fresh generation
  - Add logging to confirm modifications are being applied during generation
  - _Requirements: 1.3, 1.4_

- [x] 3. Implement missing integration test methods









  - Implement `RunPersistenceTest()` method to test terrain editing and persistence
  - Implement `RunPCGIntegrationTest()` method to test PCG content generation in headless mode
  - Implement `RunPOIIntegrationTest()` method to test POI placement validation
  - Add `ExecuteTestCategory()` method to route test execution to specific test methods
  - _Requirements: 1.1, 1.5, 2.4, 3.1, 3.2_

- [x] 4. Fix checksum determinism - implement deterministic ordering and timestamps


  - Implement deterministic sorting in `ApplyModificationsToTile()` using ModificationId as tie-breaker for identical timestamps
  - Implement deterministic sorting in `SaveTileTerrainDeltas()` before serialization to ensure stable file order
  - Update terrain delta serialization format to use high-resolution timestamps (ticks) instead of Unix seconds
  - Add backward compatibility for reading legacy Unix timestamp format (version 1) while writing new ticks format (version 2)
  - Modify integration test Step 3 to use same pipeline as Step 5 (GenerateHeightfield + ApplyModifications) instead of cached data
  - _Requirements: 1.5, 1.6, 1.7, 1.8_

- [x] 4.1 Implement deterministic GUID comparison for tie-breaking


  - Create helper function for deterministic GUID comparison (A, B, C, D components)
  - Apply deterministic sort in both ApplyModificationsToTile and SaveTileTerrainDeltas
  - _Requirements: 1.6_

- [x] 4.2 Update terrain delta serialization format with version bump


  - Modify SerializeTerrainDeltas to write version 2 format with high-resolution ticks
  - Modify DeserializeTerrainDeltas to handle both version 1 (Unix seconds) and version 2 (ticks) formats
  - Ensure backward compatibility for existing .terra files
  - _Requirements: 1.7_

- [x] 4.3 Fix integration test to use consistent pipeline


  - Modify UltimateTerrainPersistenceTest Step 3 to regenerate heightfield instead of using cached data
  - Ensure both "Modified" and "Reloaded" checksums use identical GenerateHeightfield + ApplyModifications pipeline
  - _Requirements: 1.8_

- [ ] 5. Fix PCG content generation test - implement headless mode support


  - Implement headless mode detection in PCGWorldService (check if GetWorld() returns nullptr)
  - Modify instance generation logic to work without UWorld/HISM components in headless mode
  - Add diagnostic logging to PCG generation to show why 0 instances are generated for biome 2
  - Ensure PCG generates instance data even when no valid UWorld is available for component creation
  - Verify biome-specific spawning rules are being applied correctly in headless mode
  - _Requirements: 2.1, 2.2, 2.3, 2.5, 2.6_

- [ ] 6. Fix POI placement validation - implement missing constraint validation
  - Implement `ValidatePlacementConstraints()` method in POIService.cpp to check slope and altitude constraints
  - Add detailed diagnostic logging to POI constraint validation
  - Implement slope calculation and threshold comparison logic
  - Fix test coordinate alignment in integration test to match steep terrain location
  - Ensure test coordinates match the steep terrain location created for testing
  - _Requirements: 3.1, 3.2, 3.4, 3.5_

- [ ] 7. Run complete integration test suite and verify all fixes
  - Execute full `wg.IntegrationTest` command to verify all three bugs are resolved
  - Ensure all 7 integration tests now pass consistently
  - Validate that fixes maintain backward compatibility with existing functionality
  - Confirm integration test displays "✓ ALL INTEGRATION TESTS PASSED" message
  - _Requirements: 4.1, 4.2, 4.3, 4.4, 5.1, 5.2, 5.3, 5.4, 5.5_