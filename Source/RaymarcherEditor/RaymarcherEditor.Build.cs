// (C) Technical University of Munich - Computer Aided Medical Procedures
// Jakob Weiss <jakob.weiss@tum.de>

using System.IO;
using UnrealBuildTool;

public class RaymarcherEditor : ModuleRules
{
	public RaymarcherEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
                Path.Combine(ModuleDirectory, "Public"),
            }
            );
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				Path.Combine(ModuleDirectory, "Private"),
            }
            );
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
                "RenderCore",
                "RHI",
				"AssetRegistry",
                "Raymarcher"
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
                "UnrealEd"
            }
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
			);
	}
}
