# Requirements Document

## Introduction

The Integration Test Bug Fixes feature addresses three specific failures discovered during world generation integration testing. These bugs prevent the integration test suite from passing and indicate real issues in the world generation system that would affect gameplay. The fixes ensure terrain editing persistence works correctly, PCG content generation handles headless mode properly, and POI placement validation uses correct coordinates.

## Requirements

### Requirement 1

**User Story:** As a developer, I want terrain editing and persistence to work correctly, so that player modifications to the world are properly saved and restored.

#### Acceptance Criteria

1. WHEN terrain deltas are saved to disk THEN the system SHALL ensure they can be loaded back correctly (currently saves 4 deltas but loads 0)
2. WHEN `LoadTileTerrainDeltas()` is called THEN the system SHALL properly read and parse the saved terrain delta files
3. WHEN a tile is generated with existing modifications THEN the system SHALL call `ApplyModificationsToTile()` after base generation in `GenerateHeightfield()`
4. WHEN modifications are applied to a heightfield THEN the system SHALL recalculate normals and slopes using `CalculateNormalsAndSlopes()`
5. WHEN terrain is saved and reloaded THEN the system SHALL produce identical checksums for modified and reloaded heightfields with deterministic ordering
6. WHEN multiple modifications have identical timestamps THEN the system SHALL use ModificationId as a deterministic tie-breaker for consistent ordering
7. WHEN terrain deltas are serialized to disk THEN the system SHALL use high-resolution timestamps (ticks) to minimize timestamp collisions
8. WHEN terrain modifications are applied THEN the system SHALL use the same processing pipeline for both cached and regenerated heightfields
6. IF the terrain delta file format or loading logic has issues THEN the system SHALL fix the file I/O to ensure proper persistence

### Requirement 2

**User Story:** As a developer, I want PCG content generation to work properly in both headless and normal modes, so that integration tests can validate content generation logic.

#### Acceptance Criteria

1. WHEN PCG service runs in headless mode THEN the system SHALL still generate instance data even without creating HISM components
2. WHEN `GenerateInstancesForTile()` is called THEN the system SHALL produce non-zero instances for appropriate biomes (currently generating 0 instances for biome 2)
3. WHEN PCG content generation runs THEN the system SHALL respect biome-specific spawning rules and generate appropriate content
4. WHEN `UpdateHISMInstances()` is called in headless mode THEN the system SHALL return success without attempting component creation
5. WHEN PCG generation completes THEN the system SHALL report accurate instance counts even in headless mode
6. WHEN `GenerateBiomeContent()` is called for area removal testing THEN the system SHALL cache the generated content in GenerationCache so it can be removed later
7. WHEN `RemoveContentInArea()` is called THEN the system SHALL find and remove content from the cached generation data
8. IF biome content rules are not being applied correctly THEN the system SHALL fix the content generation logic to produce instances

### Requirement 3

**User Story:** As a developer, I want POI placement validation to work correctly, so that slope and altitude constraints are properly enforced.

#### Acceptance Criteria

1. WHEN POI placement validation tests a valid location THEN the system SHALL correctly accept the placement (currently rejecting valid locations)
2. WHEN POI placement validation tests an invalid location with steep slopes THEN the system SHALL correctly reject the placement
3. WHEN the integration test creates steep terrain for testing THEN the system SHALL ensure the test coordinates match the steep terrain location
4. WHEN POI constraint validation logic runs THEN the system SHALL properly calculate slope values and compare against thresholds
5. IF the POI validation logic has bugs THEN the system SHALL fix the constraint checking to properly validate placement locations

### Requirement 4

**User Story:** As a developer, I want all integration tests to pass consistently, so that I can validate the world generation system is working correctly before gameplay testing.

#### Acceptance Criteria

1. WHEN I run `wg.IntegrationTest` THEN the system SHALL pass all terrain editing and persistence tests
2. WHEN I run `wg.IntegrationTest` THEN the system SHALL pass all PCG content generation tests in headless mode
3. WHEN I run `wg.IntegrationTest` THEN the system SHALL pass all POI generation and placement tests with correct coordinate validation
4. WHEN all integration tests complete THEN the system SHALL display "✓ ALL INTEGRATION TESTS PASSED" with correct test counts
5. IF any test still fails after fixes THEN the system SHALL provide clear error messages for remaining issues

### Requirement 5

**User Story:** As a developer, I want the fixes to maintain backward compatibility, so that existing editor and gameplay functionality continues to work unchanged.

#### Acceptance Criteria

1. WHEN terrain modifications are applied in the editor THEN the system SHALL continue to work exactly as before
2. WHEN PCG content generates in the editor with a valid UWorld THEN the system SHALL continue to create HISM components normally
3. WHEN POI placement occurs during normal gameplay THEN the system SHALL continue to use existing placement logic
4. WHEN the fixes are applied THEN the system SHALL not change any existing editor or gameplay behavior
5. IF headless mode detection is added THEN the system SHALL only affect behavior when no valid UWorld is available