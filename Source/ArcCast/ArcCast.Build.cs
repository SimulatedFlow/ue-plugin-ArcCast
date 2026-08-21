// Copyright 2026 Silvan Teufel. All Rights Reserved.

using UnrealBuildTool;

public class ArcCast : ModuleRules
{
	public ArcCast(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// One runtime module and nothing else.
		//
		// Deliberately NOT here:
		//   UMG                      - the arc is drawn on UCanvas through AHUD, so it survives a cooked
		//                              Shipping build. The demo HUD in Content/ is example material, not
		//                              the product, and a project that never touches UMG still gets the
		//                              full preview.
		//   UnrealEd                 - everything ships. The editor-viewport draw is a UDebugDrawService
		//                              second path behind WITH_EDITOR, not an editor module.
		//   Niagara / ProceduralMesh - the whole point is that the arc needs no asset. A ribbon system or
		//                              a spline mesh would make the plugin depend on content the customer
		//                              has to import, set up and cook.
		//
		// RenderCore is here for GWhiteTexture, which the statistics box background is drawn with.
		// SlateCore comes along for the canvas item / font plumbing the stats box uses.
		// NavigationSystem is private and optional at runtime: it is only touched when a profile asks for
		// bRequireNavMesh, and a project with no navigation data still loads and answers honestly.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"SlateCore",
			"RenderCore",
			"NavigationSystem",
		});
	}
}
