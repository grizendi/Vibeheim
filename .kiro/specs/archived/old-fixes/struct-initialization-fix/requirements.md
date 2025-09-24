# Requirements Document

## Introduction

The Struct Initialization Fix addresses critical UE5.6 reflection system errors where UPROPERTY struct members are not properly initialized during default construction. The Unreal reflection system expects all reflected struct members to have deterministic values after default construction, but several ID-type structs in the WorldGen system are causing initialization errors. This fix ensures all reflected structs comply with UE5.6's stricter initialization requirements while maintaining existing functionality.

### Definition

"ID-type structs" are all USTRUCTs whose primary purpose is to hold unique identifiers (e.g., FWorldGenGuid, FInstanceId). These are used as UPROPERTY members inside WorldGen data structures and must follow proper initialization patterns to work correctly with UE5.6's reflection system.

## Non-Functional Requirements

- Fixes SHALL NOT introduce runtime overhead beyond standard in-class initialization
- Fixes SHALL NOT increase serialized data size beyond current alignment/padding  
- Fixes SHALL preserve deterministic construction across all platforms supported by the project

## Requirements

### Requirement 1

**User Story:** As a developer, I want the UE5.6 reflection system to initialize all struct properties correctly, so that the engine doesn't log initialization errors during startup.

#### Acceptance Criteria

1. WHEN the engine starts THEN the system SHALL NOT log "StructProperty ... is not initialized properly" errors for any WorldGen structs
2. WHEN a USTRUCT with UPROPERTY members is default-constructed THEN all reflected members SHALL have deterministic, initialized values
3. WHEN the reflection system validates struct initialization THEN all ID-type structs SHALL pass validation without errors
4. IF a struct has custom default constructors THEN they SHALL properly initialize all reflected members

### Requirement 2

**User Story:** As a developer, I want ID-type structs to have proper default initialization, so that they work correctly with UE5.6's reflection system requirements.

#### Acceptance Criteria

1. WHEN FHeightfieldModification::ModificationId is default-constructed THEN it SHALL have initialized values for all UPROPERTY members
2. WHEN FInstanceJournalEntry::InstanceId is default-constructed THEN it SHALL have initialized values for all UPROPERTY members  
3. WHEN FPOIData::POIId is default-constructed THEN it SHALL have initialized values for all UPROPERTY members
4. WHEN FPCGInstanceData::InstanceId is default-constructed THEN it SHALL have initialized values for all UPROPERTY members
5. WHEN deserializing old save data with uninitialized IDs THEN deserialization SHALL succeed and assign valid default values without crashing
6. WHEN loading old save data THEN all ID values SHALL be equal to what they were before the fixes

### Requirement 3

**User Story:** As a developer, I want struct initialization fixes to follow UE5.6 best practices, so that the code is maintainable and follows engine conventions.

#### Acceptance Criteria

1. WHEN fixing struct initialization THEN the system SHALL use in-class member initializers where possible
2. WHEN custom constructors are needed THEN they SHALL explicitly initialize all reflected members
3. WHEN structs wrap FGuid or similar types THEN they SHALL properly initialize the wrapped values
4. IF a struct doesn't need custom construction logic THEN it SHALL rely on in-class initializers instead of custom constructors
5. IF a struct declares custom TStructOpsTypeTraits THEN the traits SHALL truthfully reflect its construction semantics
6. WHEN fixing initialization THEN the system SHALL NOT alter the struct's external API (constructor signatures, serialization functions, operator==) so that dependent code compiles unchanged

### Requirement 4

**User Story:** As a developer, I want to validate that struct initialization fixes work correctly, so that I can ensure the errors are resolved and won't reoccur.

#### Acceptance Criteria

1. WHEN the fixes are applied THEN the engine startup SHALL complete without struct initialization errors
2. WHEN running integration tests THEN all WorldGen functionality SHALL work identically to before the fixes
3. WHEN creating new instances of fixed structs THEN they SHALL have proper default values
4. WHEN serializing and deserializing fixed structs THEN data integrity SHALL be maintained
5. IF new ID-type structs are added in the future THEN they SHALL follow the established initialization patterns
6. A static analysis or unit test SHALL validate that all new USTRUCTs with UPROPERTY members either have in-class initializers for every reflected member or have a custom default constructor that initializes all members