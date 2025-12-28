using System.IO;
using UnrealBuildTool;

public class Vibeheim : ModuleRules
{
    public Vibeheim(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        bUseUnity = true;

        // Add these two lines so �WorldGen/Public� and �WorldGen/Private� become include roots
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "WorldGen", "Public"));
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "WorldGen", "Private"));
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "NPC", "Public"));
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "NPC", "Private"));

        // Public because types from these modules appear in your PUBLIC headers.
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "AIModule",
            "InputCore",
            "EnhancedInput",
            "GameplayTags",
            "GameplayTasks",
            "NavigationSystem",
            "StateTreeModule",
            "PCG",
            "PCGCompute",
            
            "StructUtils",
            "VirtualHeightfieldMesh",
            "RenderCore",
            "ProceduralMeshComponent"
        });

        // Private-only usage
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Json",
            "JsonUtilities",
            "Projects", // FPaths, IPluginManager, config helpers, etc.
            "RHI", // Needed for PixelFormat.h
            "AssetRegistry",
            "ImageWrapper"
        });

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "UnrealEd",
                "EditorFramework",
                "LevelEditor",
                "ToolMenus",
                "Blutility",
                "UMG",
                "UMGEditor",
                "Slate",
                "SlateCore",
                "ApplicationCore",
                "AppFramework"
            });
        }

        // Compile dev automation tests in non-shipping configs
        if (Target.Configuration != UnrealTargetConfiguration.Shipping)
        {
            PrivateDependencyModuleNames.Add("AutomationTest");
        }

        // PCG feature flag for server builds
        // Server targets should disable PCG to use HISM-only path
        if (Target.Type == TargetType.Server)
        {
            PublicDefinitions.Add("VHM_PCG_ENABLED=0");
        }
        else
        {
            PublicDefinitions.Add("VHM_PCG_ENABLED=1");
        }

        // Engine version policy: UE 5.7.x only
        // PCGVersionGuard.h enforces this at compile time with static_assert
        // See: Source/Vibeheim/WorldGen/Public/PCGVersionGuard.h
    }
}


