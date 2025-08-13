// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Vibeheim : ModuleRules
{
	public Vibeheim(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG", "Slate", "SlateCore" });

		PrivateDependencyModuleNames.AddRange(new string[] { 
			"Voxel", 
			"VoxelGraph", 
			"VoxelHelpers",
			"Json"
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("AutomationTest");
		}

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] {
				"UnrealEd",
				"ToolMenus",
				"EditorStyle",
				"EditorWidgets"
			});
		}

		// Slate UI modules are now included in PublicDependencyModuleNames
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
