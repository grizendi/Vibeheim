# Implementation Plan

- [x] 1. Audit existing struct initialization patterns
  - ✅ Scanned all USTRUCT definitions in WorldGen module for FGuid UPROPERTY members
  - ✅ Identified four problematic structs: FHeightfieldModification, FInstanceJournalEntry, FPOIData, FPCGInstanceData
  - ✅ Classified structs by semantic policy:
    - **Persistent IDs**: FHeightfieldModification, FInstanceJournalEntry, FPOIData → use NewGuid() in constructor
    - **Mixed/Problematic**: FPCGInstanceData → has conflicting patterns (in-class initializer + constructor assignment)
  - _Requirements: 4.6_

- [x] 2. Fix FHeightfieldModification struct initialization
  - ✅ Already properly implemented with FGuid::NewGuid() in constructor body
  - ✅ Has proper TStructOpsTypeTraits: WithZeroConstructor = false
  - ✅ Test file exists: HeightfieldModificationTest.cpp
  - _Requirements: 2.1, 3.2_

- [x] 3. Fix FInstanceJournalEntry struct initialization
  - ✅ Already properly implemented with FGuid::NewGuid() in constructor body
  - ✅ Has proper TStructOpsTypeTraits: WithZeroConstructor = false
  - ✅ Has validation with ensureMsgf in all constructors
  - _Requirements: 2.2, 3.2_

- [x] 4. Fix FPOIData struct initialization
  - ✅ Already properly implemented with FGuid::NewGuid() in constructor body
  - ✅ Has proper TStructOpsTypeTraits: WithZeroConstructor = false, WithSerializer = true
  - ✅ Has validation with ensureMsgf in constructor
  - _Requirements: 2.3, 3.2_

- [x] 5. Fix FPCGInstanceData struct initialization



  - Remove problematic in-class initializer `= FGuid()` from InstanceId member
  - Update constructor to use member initialization list: `InstanceId(FGuid::NewGuid())`
  - Add missing TStructOpsTypeTraits declaration: WithZeroConstructor = false (uses NewGuid)
  - Add ensureMsgf(InstanceId.IsValid(), ...) in constructor for validation
  - Test that struct constructs with valid GUID and no reflection errors
  - _Requirements: 2.4, 3.2_

- [x] 6. Validate TStructOpsTypeTraits consistency
  - ✅ All fixed structs have proper TStructOpsTypeTraits declarations
  - ✅ All use WithZeroConstructor = false (uses NewGuid pattern)
  - ✅ Inline comments document the trait choices and reasoning
  - _Requirements: 3.5_

- [x] 7. Create struct initialization validation tests

  - ✅ FHeightfieldModificationInitializationTest exists and validates proper initialization
  - ✅ Instance persistence tests validate FInstanceJournalEntry and related structs
  - ✅ Tests verify structs construct with valid GUIDs and no reflection errors
  - _Requirements: 4.1, 4.3_

- [x] 8. Create reflection-based determinism validation test





  - Implement fast AutomationTest that scans USTRUCTs without launching full editor
  - Use UE reflection to find all structs in Vibeheim/WorldGen package
  - Default-construct each struct and validate FGuid members using IsZeroOrValid() helper
  - Skip RF_Transient/test modules and support allow-list/deny-list for CI stability
  - Test specifically checks for the four known problematic struct members
  - Add test to ProductFilter set for pre-merge CI validation
  - _Requirements: 1.1, 4.1_

- [ ] 9. Validate serialization compatibility and behavior changes
  - Create test that saves structs with old initialization pattern
  - Load saved data after applying fixes and verify data integrity
  - Test that old save files with zero GUIDs load correctly and don't auto-convert
  - Verify that new save files maintain expected GUID values per policy
  - Test both binary serialization and custom Serialize() methods
  - Document Blueprint behavior changes: switching to NewGuid() may dirty BP defaults
  - Test that TMap/TSet lookups by ID still work after save/load cycles
  - Verify GetTypeHash(Struct) uses GUID, not address or transient fields
  - Test key stability: IDs remain stable across save/load for hash-based containers
  - _Requirements: 2.5, 4.4_

- [ ] 10. Create comprehensive integration test
  - Implement test that exercises all fixed structs in realistic usage scenarios
  - Test WorldGen system functionality with fixed struct initialization
  - Verify POI creation, instance tracking, and heightfield modifications work identically
  - Validate that all WorldGen features function correctly after fixes
  - _Requirements: 4.2_

- [ ] 11. Add static analysis validation
  - Create CI script that scans header files for UPROPERTY FGuid patterns using regex
  - Implement validation that ensures FGuid members have either in-class initializer OR constructor member-init list
  - Add suppression file for legitimate exceptions to the pattern
  - Consider fallback C++ header parser (clang-tidy check) for refactor safety
  - Generate report of any structs that don't follow established patterns
  - Simple first pass: regex `UPROPERTY.*FGuid` and verify initialization in same header/impl
  - _Requirements: 4.6_

- [ ] 12. Create comprehensive reflection validation automation test
  - Implement FWorldGenStructDeterminismTest that scans all USTRUCTs in Vibeheim package
  - Use TObjectRange<UScriptStruct> and TFieldIterator to find FGuid properties
  - Default-construct each struct and verify FGuid members are deterministic (zero or valid)
  - Skip transient/test structs by name or module filtering
  - Add this test to prevent future initialization issues with new structs
  - _Requirements: 4.6_

- [ ] 13. Update documentation and coding standards
  - Document the two initialization patterns with decision matrix:
    - Runtime-only structs: NewGuid() in constructor → Persistent ID pattern
    - Asset/Blueprint structs: FGuid() zero default → Optional/Asset ID pattern
  - Create guidelines for when to use each pattern with examples
  - Add TStructOpsTypeTraits documentation: WithZeroConstructor based on initialization policy
  - Include warnings about Blueprint behavior changes when switching patterns
  - Document Hot Reload gotcha: never NewGuid() in types that appear in asset defaults
  - Document cooking determinism considerations for asset-based structs
  - Add Definition of Done checklist for each struct fix
  - _Requirements: 3.1, 3.4_

- [ ] 14. Performance validation and regression testing
  - Measure struct construction performance before and after fixes
  - Verify no runtime overhead introduced by initialization changes
  - Test that serialized data size remains unchanged
  - Validate that memory usage patterns are identical
  - Run performance regression tests on WorldGen system
  - _Requirements: Non-functional requirements_

## Definition of Done (Per Struct Fix)

Each struct fix must meet these criteria:

- [ ] **Editor Boot Clean**: No "StructProperty ... not initialized" lines in engine startup log
- [ ] **Policy Tests Pass**: Struct-specific validation tests pass (valid vs zero GUID as appropriate)
- [ ] **Reflection Sweep Green**: Comprehensive reflection validation test passes
- [ ] **Asset Stability**: No asset defaults marked dirty after open → save → reopen cycle
- [ ] **Save-Load Roundtrip**: IDs unchanged after serialization, TMap/TSet lookups still succeed
- [ ] **Traits Documented**: TStructOpsTypeTraits has inline comment explaining "why" for each setting
- [ ] **Validation Guards**: ensureMsgf() added in key mutation paths for runtime validation
- [ ] **Backward Compatibility**: Existing save files load correctly without data corruption

## Current Status Summary

**✅ COMPLETED FIXES:**
- FHeightfieldModification: Properly uses NewGuid() in constructor, has TStructOpsTypeTraits
- FInstanceJournalEntry: Properly uses NewGuid() in constructor, has TStructOpsTypeTraits  
- FPOIData: Properly uses NewGuid() in constructor, has TStructOpsTypeTraits

**⚠️ REMAINING WORK:**
- FPCGInstanceData: Has conflicting initialization patterns (in-class initializer + constructor assignment)
- Missing comprehensive reflection validation test
- Missing serialization compatibility validation
- Missing integration and performance tests
- Missing documentation updates

**🎯 PRIORITY TASKS:**
1. Fix FPCGInstanceData struct initialization (Task 5)
2. Create reflection-based validation test (Task 8)
3. Validate serialization compatibility (Task 9)