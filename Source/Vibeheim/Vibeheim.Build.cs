// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class Vibeheim : ModuleRules
{
	public Vibeheim(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { 
			"Core", 
			"CoreUObject", 
			"Engine", 
			"InputCore", 
			"EnhancedInput"
		});

		// Add PCG modules if available (UE5.1+)
		if (Target.Version.MajorVersion >= 5 && Target.Version.MinorVersion >= 1)
		{
			PublicDependencyModuleNames.AddRange(new string[] {
				"PCG"
			});
			PublicDefinitions.Add("WITH_PCG=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_PCG=0");
		}

		PrivateDependencyModuleNames.AddRange(new string[] { 
			"Json",
			"JsonUtilities"
		});

        // Make nested WorldGen folders visible as include roots:
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "WorldGen", "Public"));
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "WorldGen", "Private"));

        // Add AutomationTest module only in development builds
        if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrivateDependencyModuleNames.Add("AutomationTest");
		}

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
