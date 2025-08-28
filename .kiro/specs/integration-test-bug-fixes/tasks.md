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

- [ ] 3. Fix terrain persistence - verify and test complete fix
  - Run terrain persistence integration test to verify checksums now match
  - Ensure both "Modified Checksum" and "Reloaded Checksum" are identical
  - Validate that terrain modifications persist correctly across save/load cycles
  - Confirm fix doesn't break existing editor terrain editing functionality
  - _Requirements: 1.1, 1.5_

- [ ] 4. Fix PCG content generation - ensure headless mode generates instances
  - Modify PCG instance generation logic in PCGWorldService.cpp to work in headless mode
  - Ensure `GenerateInstancesForTile()` produces non-zero instances even without UWorld
  - Separate instance data generation from HISM component creation
  - Add logging to show instance counts and generation success in headless mode
  - _Requirements: 2.1, 2.2, 2.5_

- [ ] 5. Fix PCG content generation - debug biome content rules
  - Add diagnostic logging to PCG generation to show why 0 instances are generated for biome 2
  - Verify biome-specific spawning rules are being applied correctly
  - Check if biome content parameters are configured properly for test scenarios
  - Ensure content generation respects biome rules and produces appropriate instance counts
  - _Requirements: 2.3, 2.6_

- [ ] 6. Fix PCG content generation - verify and test complete fix
  - Run PCG content generation integration test to verify non-zero instances are generated
  - Ensure headless mode returns success while skipping HISM component creation
  - Validate that biome-specific content spawning works correctly
  - Confirm fix maintains existing editor PCG functionality
  - _Requirements: 2.4_

- [ ] 7. Implement missing POI placement validation methods
  - Implement `ValidatePlacementConstraints()` method in POIService.cpp to check slope and altitude constraints
  - Add detailed diagnostic logging to POI constraint validation
  - Implement slope calculation and threshold comparison logic
  - Add coordinate validation to ensure test locations match terrain modifications
  - _Requirements: 3.2, 3.4, 3.5_

- [ ] 8. Fix POI placement validation - align test coordinates if needed
  - Check if integration test coordinates match the steep terrain location created for testing
  - Update test coordinates to use tile center position if misaligned
  - Ensure test creates steep terrain and tests placement at the same location
  - Verify coordinate system alignment between terrain creation and placement testing
  - _Requirements: 3.1, 3.3_

- [ ] 9. Fix POI placement validation - verify and test complete fix
  - Run POI placement integration test to verify constraint validation works correctly
  - Ensure valid placement locations are accepted and invalid locations are rejected
  - Validate that slope and altitude constraints are properly enforced
  - Confirm fix doesn't break existing POI placement functionality in editor
  - _Requirements: 3.1, 3.2_

- [ ] 10. Run complete integration test suite and verify all fixes
  - Execute full `wg.IntegrationTest` command to verify all three bugs are resolved
  - Ensure all 7 integration tests now pass consistently
  - Validate that fixes maintain backward compatibility with existing functionality
  - Confirm integration test displays "✓ ALL INTEGRATION TESTS PASSED" message
  - _Requirements: 4.1, 4.2, 4.3, 4.4, 5.1, 5.2, 5.3, 5.4, 5.5_