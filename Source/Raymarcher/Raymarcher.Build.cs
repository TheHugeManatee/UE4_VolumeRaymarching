// (C) Technical University of Munich - Computer Aided Medical Procedures
// Developed by Tomas Bartipan (tomas.bartipan@tum.de)

using System.IO;
using UnrealBuildTool;

public class Raymarcher : ModuleRules
{
	public Raymarcher(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
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
                "ShaderCore",
                "RHI",
				"AssetRegistry" // ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine"
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
			);
	}
}
