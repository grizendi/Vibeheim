# Requirements Document

## Introduction

The Editor World Generation feature enhances the existing voxel world generation system by providing editor-time tools and workflows that allow designers to preview, configure, and iterate on world generation without entering Play-in-Editor (PIE) mode. This system builds upon the existing WorldGenManager and VoxelPluginLegacy integration to provide immediate visual feedback and streamlined configuration management within the Unreal Editor.

## Requirements

### Requirement 1

**User Story:** As a designer, I want to preview generated terrain in the editor viewport, so that I can iterate on world generation settings without entering PIE mode.

#### Acceptance Criteria

1. WHEN a designer places a WorldGenManager in a level THEN the system SHALL generate voxel terrain visible in the editor viewport
2. WHEN a designer modifies world generation settings THEN the system SHALL provide a way to rebuild the terrain immediately in the editor
3. WHEN terrain is generated in editor THEN the system SHALL maintain the same quality and appearance as runtime generation
4. WHEN the editor is closed and reopened THEN the system SHALL preserve the generated terrain state
5. IF terrain generation fails in editor THEN the system SHALL display clear error messages in the editor UI

### Requirement 2

**User Story:** As a designer, I want to configure world generation settings through the editor interface, so that I can avoid editing JSON files manually.

#### Acceptance Criteria

1. WHEN a designer opens world generation settings THEN the system SHALL provide an editor-friendly interface for all FWorldGenSettings parameters
2. WHEN settings are modified in the editor THEN the system SHALL save changes automatically without requiring JSON file editing
3. WHEN settings are changed THEN the system SHALL provide immediate feedback on how changes affect generation
4. WHEN invalid settings are entered THEN the system SHALL provide validation feedback and prevent invalid configurations
5. IF settings fail to save THEN the system SHALL display error messages and maintain the previous valid configuration

### Requirement 3

**User Story:** As a designer, I want to swap noise generators and materials in the editor, so that I can quickly compare different generation approaches.

#### Acceptance Criteria

1. WHEN a designer accesses generator settings THEN the system SHALL provide a dropdown to select from available UVoxelGenerator classes
2. WHEN a generator is changed THEN the system SHALL allow editing of generator-specific parameters inline
3. WHEN a designer changes the voxel material THEN the system SHALL update the preview immediately
4. WHEN custom generators are created THEN the system SHALL automatically detect and include them in the selection list
5. IF a generator fails to load THEN the system SHALL fall back to a default generator and log the error

### Requirement 4

**User Story:** As a designer, I want to customize biome materials and appearance, so that I can create distinct visual styles for different biomes.

#### Acceptance Criteria

1. WHEN a designer configures biomes THEN the system SHALL allow setting unique materials for each biome type
2. WHEN biome materials are changed THEN the system SHALL update the terrain preview to show the new materials
3. WHEN a designer adjusts biome colors THEN the system SHALL provide color picker interfaces for easy modification
4. WHEN biomes blend THEN the system SHALL properly blend the assigned materials at biome boundaries
5. IF a biome material is missing THEN the system SHALL use a default material and display a warning

### Requirement 5

**User Story:** As a designer, I want access to editor utility tools for world generation, so that I can streamline my workflow with dedicated UI panels.

#### Acceptance Criteria

1. WHEN a designer opens the world generation tools THEN the system SHALL provide an Editor Utility Widget with generation controls
2. WHEN using the utility widget THEN the system SHALL provide buttons for build, rebuild, and clear operations
3. WHEN the utility widget is open THEN the system SHALL display real-time generation statistics and performance metrics
4. WHEN generation is in progress THEN the system SHALL show progress indicators and allow cancellation
5. IF the utility widget encounters errors THEN the system SHALL display detailed error information for debugging

### Requirement 6

**User Story:** As a developer, I want the editor tools to integrate with existing world generation systems, so that editor and runtime behavior remain consistent.

#### Acceptance Criteria

1. WHEN editor tools generate terrain THEN the system SHALL use the same WorldGenManager and VoxelPluginAdapter classes as runtime
2. WHEN settings are modified in editor THEN the system SHALL maintain compatibility with existing save/load systems
3. WHEN editor generation occurs THEN the system SHALL respect all existing biome rules, POI placement, and streaming logic
4. WHEN switching between editor and PIE modes THEN the system SHALL maintain consistent world state
5. IF editor integration conflicts with runtime systems THEN the system SHALL prioritize runtime compatibility and log warnings