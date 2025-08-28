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

- [x] 4. Fix terrain persistence test - verify checksums match







  - Run terrain persistence integration test to verify checksums now match
  - Ensure both "Modified Checksum" and "Reloaded Checksum" are identical
  - Validate that terrain modifications persist correctly across save/load cycles
  - Debug any remaining checksum mismatch issues
  - _Requirements: 1.1, 1.5_

- [ ] 5. Fix PCG content generation test - debug biome content rules
  - Add diagnostic logging to PCG generation to show why 0 instances are generated for biome 2
  - Verify biome-specific spawning rules are being applied correctly in headless mode
  - Check if biome content parameters are configured properly for test scenarios
  - Ensure content generation respects biome rules and produces appropriate instance counts
  - _Requirements: 2.1, 2.2, 2.3, 2.5, 2.6_

- [ ] 6. Fix POI placement validation - implement missing constraint validation
  - Implement `ValidatePlacementConstraints()` method in POIService.cpp to check slope and altitude constraints
  - Add detailed diagnostic logging to POI constraint validation
  - Implement slope calculation and threshold comparison logic
  - Ensure test coordinates match the steep terrain location created for testing
  - _Requirements: 3.1, 3.2, 3.4, 3.5_

- [ ] 7. Run complete integration test suite and verify all fixes
  - Execute full `wg.IntegrationTest` command to verify all three bugs are resolved
  - Ensure all 7 integration tests now pass consistently
  - Validate that fixes maintain backward compatibility with existing functionality
  - Confirm integration test displays "✓ ALL INTEGRATION TESTS PASSED" message
  - _Requirements: 4.1, 4.2, 4.3, 4.4, 5.1, 5.2, 5.3, 5.4, 5.5_