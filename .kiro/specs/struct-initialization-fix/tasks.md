# Implementation Plan

- [ ] 1. Audit existing struct initialization patterns
  - Scan all USTRUCT definitions in WorldGen module for FGuid UPROPERTY members and custom ID wrappers
  - Document current initialization patterns and identify all problematic structs
  - Record whether each struct is used in assets/DataTables/Blueprints vs runtime-only
  - Classify each struct by semantic policy with owner/reviewer assignment:
    - **Persistent IDs**: Must be valid immediately (runtime journal entries) → use NewGuid() in constructor
    - **Optional/Asset IDs**: Used in BP/DT defaults, assigned later → use zero GUID (FGuid()) in-class initializer
    - **Mixed**: Requires case-by-case analysis
  - Create audit table: Struct | Policy | Owner/Reviewer | Usage Context | Current Pattern | Target Pattern
  - _Requirements: 4.6_

- [ ] 2. Fix FHeightfieldModification struct initialization
  - Classify as Persistent ID (runtime terrain modifications need immediate unique IDs)
  - Remove in-class initializer `= FGuid()` from ModificationId member
  - Update constructor to use member initialization list: `ModificationId(FGuid::NewGuid())`
  - Ensure all constructor overloads initialize ModificationId in member init list
  - Update TStructOpsTypeTraits: WithZeroConstructor = false (uses NewGuid)
  - Add ensureMsgf(ModificationId.IsValid(), ...) in key mutation paths for validation
  - Test that struct constructs with valid GUID and no reflection errors
  - Verify no asset defaults become dirty after open → save → reopen cycle
  - _Requirements: 2.1, 3.2_

- [ ] 3. Fix FInstanceJournalEntry struct initialization
  - Classify as Persistent ID (runtime journal entries need immediate unique IDs)
  - Remove in-class initializer `= FGuid()` from InstanceId member
  - Update default constructor to use member initialization list: `InstanceId(FGuid::NewGuid())`
  - Update parameterized constructors to properly initialize InstanceId in member init list
  - Ensure all constructor paths initialize InstanceId before constructor body
  - Update TStructOpsTypeTraits: WithZeroConstructor = false (uses NewGuid)
  - Add ensureMsgf(InstanceId.IsValid(), ...) in key mutation paths for validation
  - Verify no asset defaults become dirty after open → save → reopen cycle
  - _Requirements: 2.2, 3.2_

- [ ] 4. Fix FPOIData struct initialization
  - Classify as Optional/Asset ID (used in Blueprint/DataTable defaults, assigned at spawn time)
  - Keep in-class initializer `= FGuid()` for deterministic zero default
  - Remove or simplify default constructor that assigns NewGuid() in body
  - Ensure POI ID generation happens at registration/spawn time, not construction
  - Update TStructOpsTypeTraits: WithZeroConstructor = true (zero-guid default, safe memzero)
  - Add ensureMsgf(!POIId.IsValid(), ...) at CDO/PostInit for early warning validation
  - Test that struct constructs with zero GUID and no reflection errors
  - Add asset dirtiness test: verify Blueprint/DataTable assets don't become dirty after load
  - _Requirements: 2.3, 3.2_

- [ ] 5. Fix FPCGInstanceData struct initialization
  - Classify as Persistent ID (runtime PCG instances need immediate unique IDs for tracking)
  - Remove in-class initializer `= FGuid()` from InstanceId member
  - Update default constructor to use member initialization list: `InstanceId(FGuid::NewGuid())`
  - Verify custom serialization methods handle GUID correctly
  - Update TStructOpsTypeTraits: WithZeroConstructor = false (uses NewGuid)
  - Add ensureMsgf(InstanceId.IsValid(), ...) in key mutation paths for validation
  - Test that PCG instance creation generates unique IDs without reflection errors
  - Verify no asset defaults become dirty after open → save → reopen cycle
  - _Requirements: 2.4, 3.2_

- [ ] 6. Validate TStructOpsTypeTraits consistency
  - Review TStructOpsTypeTraits declarations for all fixed structs
  - Apply policy-based trait settings:
    - Persistent ID structs: WithZeroConstructor = false (uses NewGuid)
    - Optional/Asset ID structs: WithZeroConstructor = true (zero-guid default, safe memzero)
  - Ensure WithNoInitConstructor is only set if truly implemented
  - Verify all reflected members are zero-constructible when WithZeroConstructor = true
  - Add inline comments documenting the trait choices and reasoning for each struct
  - Document traits truthfulness: one lying trait = flaky editor boot
  - _Requirements: 3.5_

- [ ] 7. Create struct initialization validation tests
  - Implement FStructInitializationTest automation test class with policy-aware validation
  - Add shared helper: IsZeroOrValid() to check either zero-guid or valid guid (no garbage bits)
  - Add policy-specific tests:
    - Persistent ID structs: TestTrue("InstanceId must be valid", Entry.InstanceId.IsValid())
    - Optional/Asset ID structs: TestTrue("POIId must be zero by default", !POI.POIId.IsValid())
  - Add equality/hash stability test: verify TMap/TSet lookups work after save/load cycles
  - Test both zero-initialized and NewGuid-initialized patterns
  - Verify tests pass after all struct fixes are applied
  - _Requirements: 4.1, 4.3_

- [ ] 8. Create reflection-based determinism validation test
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
## Defi
nition of Done (Per Struct Fix)

Each struct fix must meet these criteria:

- [ ] **Editor Boot Clean**: No "StructProperty ... not initialized" lines in engine startup log
- [ ] **Policy Tests Pass**: Struct-specific validation tests pass (valid vs zero GUID as appropriate)
- [ ] **Reflection Sweep Green**: Comprehensive reflection validation test passes
- [ ] **Asset Stability**: No asset defaults marked dirty after open → save → reopen cycle
- [ ] **Save-Load Roundtrip**: IDs unchanged after serialization, TMap/TSet lookups still succeed
- [ ] **Traits Documented**: TStructOpsTypeTraits has inline comment explaining "why" for each setting
- [ ] **Validation Guards**: ensureMsgf() added in key mutation paths for runtime validation
- [ ] **Backward Compatibility**: Existing save files load correctly without data corruption