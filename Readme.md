# Volume Rendering Plugin for Unreal Engine
Allows volume rendering with Unreal Engine.

# Features
 * Volume raycasting for arbitrary `UVolumeTexture` textures
 * Volume illumination implementing concepts from [Efficient Volume Illumination with Multiple Light Sources through Selective Light Updates](https://ieeexplore.ieee.org/document/7156382) (2015) by Sundén and Ropinski
 * Uses color curves for transfer function definition

# Limitations
 * Needs a custom build of the engine to enable UAV access to `UVolumeTexture` resources for the interactive illumination computation

# Example
Have a look at https://github.com/TheHugeManatee/UE4_PluginDemos for an example map of how to use various parts of this plugin.

# Using this plugin in your own project
If you want to use the functinality in your own project, you will need to
 * Copy/clone the Plugin to your own project in the `Plugins/VolumeRaymarching` folder
   * Consider using `git submodule` to directly add a reference to the git project
   * Make sure that git LFS has checked out and created all large files, i.e. check that `.uasset`, `.dll` and `.lib` files have more than 1 KiB file size.
 * You will need a C++ project. If you don't have one already, convert your BP-based project by simply creating some new C++ class with `Actor` as parent (there should be a menu option under `File`)
 * Edit your Build configuration of your project to reference the module by adapting your `Module.Build.cs` similar to the following:
    ```CSharp
    PublicDependencyModuleNames.AddRange(new string[] { 
        "Core", "CoreUObject", "Engine", "InputCore", // These are default ones you probably already have
        "VolumeRaymarching" // <-- This is the important one ;)
        // ... other dependencies you might have
    });

    PrivateIncludePaths.AddRange(
        new string[] {
           "VolumeRaymarching/Private", // Make the header files available so you can use OpenCV headers in your C++ code
           // ... other include directories you might have
        }
    );
    ```
 * Re-Generate your C++ project from the `.uproject` file and rebuild the project
    * if the linker about `.lib` files not being readable/having unexpected content, double-check that git lfs has done its job

# Platforms
Currently only Windows is supported. This is mainly limited by my (non-existent) understanding of how building and including with external libraries works on different platforms.

**Tested on Unreal Engine 4.19 and 4.21** <br/>
However, other versions might still work.