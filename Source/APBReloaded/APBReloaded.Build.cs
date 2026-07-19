// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class APBReloaded : ModuleRules
{
	public APBReloaded(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

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
			"APBReloaded/Domain",
			"APBReloaded/Variant_Platforming",
			"APBReloaded/Variant_Platforming/Animation",
			"APBReloaded/Variant_Combat",
			"APBReloaded/Variant_Combat/AI",
			"APBReloaded/Variant_Combat/Animation",
			"APBReloaded/Variant_Combat/Gameplay",
			"APBReloaded/Variant_Combat/Interfaces",
			"APBReloaded/Variant_Combat/UI",
			"APBReloaded/Variant_SideScrolling",
			"APBReloaded/Variant_SideScrolling/AI",
			"APBReloaded/Variant_SideScrolling/Gameplay",
			"APBReloaded/Variant_SideScrolling/Interfaces",
			"APBReloaded/Variant_SideScrolling/UI"
		});
	}
}
