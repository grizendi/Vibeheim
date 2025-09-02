# WorldGenTestSubsystem Gate Check Verification

This document outlines how to verify the gate check requirements for Task 1 of the Simple Test World implementation.

## Gate Check Requirements

- **No crashes**: System should not crash during initialization or hot reload
- **No stale pointers**: Hot reload should not leave dangling references
- **Commands ignored with log line outside WG_TestMap**: Console commands should only work in the test map

## Verification Steps

### 1. Basic Functionality Test

1. **Load the Test Map**
   - Create `/Game/Maps/WG_TestMap` in the editor
   - Add an `AWorldGenManager` actor to the scene
   - Load the map in PIE (Play in Editor)

2. **Test Console Commands**
   ```
   wg.launch          // Should initialize systems
   wg.seed 42         // Should set seed to 42
   wg.radii 9 5 3     // Should set streaming radii
   wg.reset           // Should reset to defaults
   ```

3. **Verify Logging**
   - Commands should log successful execution
   - Check that authoritative seed is properly managed

### 2. Map Validation Test

1. **Load Different Map**
   - Load any map other than `WG_TestMap`
   - Try running console commands

2. **Expected Behavior**
   - Commands should be ignored
   - Error log should appear: `"Command only works in /Game/Maps/WG_TestMap (current: [MapName])"`

### 3. Hot Reload Test

1. **Make Code Changes**
   - Modify a comment in `WorldGenTestSubsystem.cpp`
   - Use Live Coding (Ctrl+Alt+F11) or compile in editor

2. **Test After Reload**
   - Console commands should still work
   - No crashes should occur
   - No stale pointer warnings in log

### 4. Subsystem Integration Test

1. **Verify Seed Propagation**
   - Use `wg.seed 123`
   - Check that `UWorldGenSeedSubsystem` receives the update
   - Verify `UWorldGenSettings` is updated

2. **Test Radii Validation**
   - Try invalid radii: `wg.radii 3 5 9` (Active > Load > Generate)
   - Should log error and reject invalid values

## Expected Log Output

### Successful Command Execution
```
LogWorldGenTest: Set authoritative seed to 42
LogWorldGenSeed: Authoritative seed changed to 42
LogWorldGenSeed: Propagated seed 42 to WorldGenSettings
```

### Map Validation Error
```
LogWorldGenTest: Error: wg.seed: Command only works in /Game/Maps/WG_TestMap (current: /Game/ThirdPerson/Maps/ThirdPersonMap)
```

### Radii Validation Error
```
LogWorldGenTest: Error: ActiveRadius (9) cannot be greater than LoadRadius (5)
```

## Success Criteria

✅ All console commands work in WG_TestMap
✅ Commands are rejected in other maps with appropriate error messages
✅ Hot reload works without crashes or stale pointers
✅ Authoritative seed system properly propagates changes
✅ Input validation works for streaming radii
✅ No category errors/warnings during normal operation

## Troubleshooting

### Commands Not Registering
- Check that `WITH_EDITOR` is defined
- Verify subsystem is being created (check initialization log)
- Ensure world is a game world (not editor preview)

### Map Detection Issues
- Verify map name contains "WG_TestMap"
- Check both world name and package name
- PIE maps may have prefixes like "UEDPIE_0_"

### Seed Propagation Issues
- Verify `UWorldGenSeedSubsystem` is created
- Check that `UWorldGenSettings` singleton is accessible
- Ensure game instance subsystem is properly initialized