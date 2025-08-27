# USTRUCT Initialization Standards for UE5.6

## Critical UE5.6 Compatibility Requirements

**NEVER use in-class FGuid initializers with NewGuid()** - This causes "StructProperty ... is not initialized properly" errors in UE5.6's reflection system.

## Required Patterns

### Pattern 1: Constructor Member Initializer List (for ID-type structs)

```cpp
USTRUCT(BlueprintType)
struct VIBEHEIM_API FMyIdStruct
{
    GENERATED_BODY()

    UPROPERTY()
    FGuid MyId;  // No in-class initializer

    FMyIdStruct() 
        : MyId(FGuid::NewGuid())  // Initialize in member initializer list
    {
        ensureMsgf(MyId.IsValid(), TEXT("MyId must be valid"));
    }
};

template<>
struct TStructOpsTypeTraits<FMyIdStruct> : public TStructOpsTypeTraitsBase2<FMyIdStruct>
{
    enum { WithZeroConstructor = false };  // Uses NewGuid(), not zero-init
};
```

### Pattern 2: In-Class Zero Initialization (for config/template structs)

```cpp
USTRUCT(BlueprintType)
struct VIBEHEIM_API FMyConfigStruct
{
    GENERATED_BODY()

    UPROPERTY()
    FGuid TemplateId = FGuid();  // Zero-init, assigned later

    // No constructor needed
};
```

## Decision Tree

```
Does struct contain FGuid members that need unique values immediately?
├─ YES → Use Pattern 1 (Constructor Member Initializer List)
└─ NO → Use Pattern 2 (In-Class Zero Initialization)

Is this struct used for runtime entities that need immediate IDs?
├─ YES → Pattern 1
└─ NO → Pattern 2
```

## Vibeheim Examples

- `FHeightfieldModification::ModificationId` → Pattern 1
- `FInstanceJournalEntry::InstanceId` → Pattern 1  
- `FPOIData::POIId` → Pattern 1
- `FPCGInstanceData::InstanceId` → Pattern 1

## Forbidden Patterns

```cpp
// ❌ NEVER - Causes UE5.6 reflection errors
FGuid MyId = FGuid::NewGuid();

// ❌ NEVER - Late initialization
FMyStruct() { MyId = FGuid::NewGuid(); }

// ❌ NEVER - Uninitialized
FGuid MyId;  // without constructor
```

## Required Validation

For every struct fix, ensure:
- [ ] No "StructProperty ... not initialized" errors in engine log
- [ ] Uses Pattern 1 OR Pattern 2 consistently
- [ ] TStructOpsTypeTraits configured correctly
- [ ] ensureMsgf() validation added (Pattern 1 only)

## Blueprint Behavior Warning

Switching from `= FGuid::NewGuid()` to constructor `NewGuid()` changes Blueprint behavior:
- **Before:** Same ID for all Blueprint instances (compile-time)
- **After:** Unique ID per Blueprint instance (runtime)

This is usually the desired behavior for ID-type structs.