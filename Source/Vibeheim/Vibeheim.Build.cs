using System.IO;
using UnrealBuildTool;

public class Vibeheim : ModuleRules
{
    public Vibeheim(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // Add these two lines so “WorldGen/Public” and “WorldGen/Private” become include roots
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "WorldGen", "Public"));
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "WorldGen", "Private"));

        // Public because types from these modules appear in your PUBLIC headers.
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "EnhancedInput",
            "PCG"
        });

        // Private-only usage
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Json",
            "JsonUtilities",
            "Projects" // FPaths, IPluginManager, config helpers, etc.
        });

        // Compile dev automation tests in non-shipping configs
        if (Target.Configuration != UnrealTargetConfiguration.Shipping)
        {
            PrivateDependencyModuleNames.Add("AutomationTest");
        }

        // No extra include paths: Source/Vibeheim/Public and /Private are already roots.
        // No engine-version checks: UE 5.6 ships PCG; your code requires it publicly.
    }
}