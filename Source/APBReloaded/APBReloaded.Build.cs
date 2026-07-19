// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class APBReloaded : ModuleRules
{
	public APBReloaded(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Domain .cpp files define same-named anonymous-namespace helpers; unity
		// blobs merge them into one TU and collide. Compile each file separately.
		bUseUnity = false;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"SlateCore",
			"HTTP",
			"Json",
			"JsonUtilities",
			"NetCore",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"MediaAssets",
			"MediaUtils"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"APBReloaded",
			"APBReloaded/Systems",
			"APBReloaded/Domain"
		});
	}
}
