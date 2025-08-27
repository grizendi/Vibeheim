# Vibeheim Documentation

This directory contains technical documentation for the Vibeheim project.

## Documentation Index

### Struct Initialization (UE5.6 Compatibility)

- **[Struct Initialization Implementation Summary](StructInitializationImplementationSummary.md)** - Executive summary of the complete struct initialization fix
- **[Struct Initialization Behavior Changes](StructInitializationBehaviorChanges.md)** - Documentation of behavior changes from the fixes

**Note:** Coding standards for struct initialization are located in `.kiro/steering/struct-initialization-standards.md` and are automatically included in Kiro's context when working with structs.

### Testing & Validation

- **[WorldGen Integration Testing](WorldGenIntegrationTesting.md)** - Integration testing documentation for WorldGen systems
- **[Scripts/README.md](../Scripts/README.md)** - Static analysis validation tools for struct initialization

## Quick Links

### For Developers Adding New Structs
1. Kiro will automatically include struct initialization standards from `.kiro/steering/`
2. Run validation scripts from [Scripts/README.md](../Scripts/README.md)
3. Check the implementation summary for complete context

### For Code Reviews
1. Verify patterns match the steering standards (automatically included in Kiro context)
2. Run validation scripts from [Scripts/README.md](../Scripts/README.md)
3. Check against the implementation summary

### For Troubleshooting UE5.6 Reflection Errors
1. Check [Behavior Changes](StructInitializationBehaviorChanges.md) for known issues
2. Check [Implementation Summary](StructInitializationImplementationSummary.md) for complete context
3. Run validation tools to identify problematic patterns

## Contributing to Documentation

When adding new documentation:
1. Follow the existing structure and naming conventions
2. Update this README.md index
3. Cross-reference related documents
4. Include practical examples and code snippets
5. Add validation/testing information where applicable