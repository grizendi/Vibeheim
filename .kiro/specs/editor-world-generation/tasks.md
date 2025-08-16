# Implementation Plan

- [ ] 1. Set up editor module structure and dependencies
  - Create EditorWorldGen module directory under Source/VibeheimEditor/EditorWorldGen/
  - Update VibeheimEditor.Build.cs to include ToolMenus, EditorStyle, EditorWidgets, UnrealEd dependencies
  - Add module dependencies for existing WorldGen module and VoxelPluginLegacy
  - Create EditorWorldGen.Build.cs with proper editor-only module configuration
  - _Requirements: 6.1, 6.2_

- [ ] 2. Implement editor-friendly settings system
  - Create UWorldGenDeveloperSettings class extending UDeveloperSettings
  - Move FWorldGenSettings to developer settings with config=Game, defaultconfig
  - Implement PostEditChangeProperty to trigger world rebuilds on setting changes
  - Add settings validation with user-friendly error messages in editor UI
  - Create settings migration utility to convert existing WorldGenSettings.json files
  - _Requirements: 2.1, 2.2, 2.4, 2.5_

- [ ] 3. Extend WorldGenManager with editor-time generation
  - Add OnConstruction override to AWorldGenManager for editor-time world building
  - Implement editor context detection using GetWorld()->WorldType == EWorldType::Editor
  - Add CallInEditor UFUNCTIONs for RebuildWorldInEditor and ClearWorldInEditor
  - Create editor-specific initialization path that respects preview radius settings
  - Add PostEditChangeProperty override to handle automatic rebuilds on property changes
  - _Requirements: 1.1, 1.2, 6.1, 6.4_

- [ ] 4. Create enhanced biome data structure with materials
  - Extend FBiomeData struct with TSoftObjectPtr<UMaterialInterface> BiomeMaterial property
  - Add FLinearColor BiomeColor and float MaterialBlendSharpness properties to FBiomeData
  - Update BiomeSystem to use new material properties during terrain generation
  - Implement material parameter collection integration for dynamic biome blending
  - Create default biome materials and ensure fallback behavior for missing materials
  - _Requirements: 4.1, 4.2, 4.4, 4.5_

- [ ] 5. Implement voxel generator selection system
  - Add TSubclassOf<UVoxelGenerator> GeneratorClass property to FWorldGenSettings
  - Create UVoxelGenerator* GeneratorInstance property with EditInlineNew meta tag
  - Implement automatic detection and dropdown population of available UVoxelGenerator classes
  - Add generator parameter editing support through details panel customization
  - Create fallback to default generator when selected generator fails to load
  - _Requirements: 3.1, 3.2, 3.4, 3.5_

- [ ] 6. Create WorldGenManager details panel customization
  - Implement FWorldGenManagerDetails class extending IDetailCustomization
  - Create custom details panel with "Rebuild World" and "Clear World" buttons
  - Add generator selection dropdown widget with live preview updates
  - Implement biome material assignment grid for easy material swapping
  - Register details customization in EditorWorldGen module startup
  - _Requirements: 1.2, 3.1, 4.2, 5.2_

- [ ] 7. Build Editor Utility Widget for world generation tools
  - Create UWorldGenEditorUtility class extending UEditorUtilityWidget
  - Design widget UI with generation controls (Build/Rebuild/Clear buttons)
  - Add real-time performance metrics display (generation time, memory usage, triangle count)
  - Implement progress indicators with cancellation support for long operations
  - Create settings property panel within the utility widget for quick access
  - _Requirements: 5.1, 5.2, 5.3, 5.4_

- [ ] 8. Implement material parameter collection integration
  - Create BiomeMaterialCollection asset with parameters for each biome (color, blend settings)
  - Update BiomeSystem to set material collection parameters during generation
  - Implement UpdateBiomeMaterialParameters method to sync biome data with material system
  - Create master material that samples biome masks and blends materials using MPC parameters
  - Add editor preview support to maintain visual consistency between editor and runtime
  - _Requirements: 4.2, 4.4, 6.3_

- [ ] 9. Add editor-specific performance optimizations
  - Implement EditorPreviewRadius setting to limit generation scope in editor
  - Create simplified LOD system for editor preview (reduced triangle counts)
  - Add async generation with progress callbacks to maintain editor responsiveness
  - Implement memory management for editor-only generation data cleanup
  - Add cancellation support for long-running editor generation operations
  - _Requirements: 1.3, 5.4, 6.4_

- [ ] 10. Create comprehensive error handling for editor workflows
  - Implement user-friendly error message display in editor UI using FNotificationInfo
  - Add graceful degradation to simplified generation when full generation fails
  - Create validation feedback system for invalid settings with immediate UI updates
  - Implement recovery options (reset to defaults, retry with fallback settings)
  - Add detailed logging for editor-specific operations with LogWorldGenEditor category
  - _Requirements: 1.5, 2.4, 2.5, 3.5, 4.5_

- [ ] 11. Implement settings migration and backward compatibility
  - Create JSON to UDeveloperSettings migration utility function
  - Add automatic detection and conversion of existing WorldGenSettings.json files
  - Implement backward compatibility layer to support existing world save files
  - Create validation system to ensure migrated settings maintain world generation consistency
  - Add user prompts and guidance for settings migration process
  - _Requirements: 2.1, 2.2, 6.2, 6.4_

- [ ] 12. Add editor integration testing and validation
  - Create automated tests for OnConstruction world generation in editor context
  - Implement tests for PostEditChangeProperty triggering appropriate rebuilds
  - Add visual consistency tests comparing editor preview with runtime generation
  - Create performance tests to ensure editor generation meets responsiveness targets
  - Implement integration tests for settings migration and backward compatibility
  - _Requirements: 1.4, 2.3, 6.3, 6.4_

- [ ] 13. Create documentation and user guides
  - Write comprehensive documentation for new editor workflows and tools
  - Create step-by-step guides for migrating from JSON-based to editor-based configuration
  - Document best practices for biome material setup and customization
  - Create troubleshooting guide for common editor integration issues
  - Add inline help text and tooltips for all new editor UI elements
  - _Requirements: 2.1, 4.1, 5.1_

- [ ] 14. Implement final integration and polish
  - Wire all systems together in EditorWorldGen module initialization
  - Add proper cleanup and shutdown handling for editor-only systems
  - Implement final UI polish and user experience improvements
  - Add keyboard shortcuts and context menu integration for common operations
  - Create final validation tests for complete editor workflow integration
  - _Requirements: 5.1, 5.5, 6.1, 6.4_