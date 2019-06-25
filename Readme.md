

# Volume Rendering Plugin for Unreal Engine
Allows volume rendering with Unreal Engine.

# Features
 * Volume raycasting for arbitrary `UVolumeTexture` textures
 * Volume illumination implementing concepts from [Efficient Volume Illumination with Multiple Light Sources through Selective Light Updates](https://ieeexplore.ieee.org/document/7156382) (2015) by Sundén and Ropinski
 * Uses color curves for transfer function definition

# Limitations
 * Needs a custom build of the engine to enable UAV access to `UVolumeTexture` resources for the interactive illumination computation
 * You can find the branch here: https://github.com/TheHugeManatee/UnrealEngine/tree/raymarching
 * Raymarched volume only self-shadows and doesn't cast shadows into the scene

# Example
Have a look at [our example project](https://github.com/TheHugeManatee/UE4_PluginDemos) for an example map of how to use various parts of this plugin.

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

# Getting started
Most of the functionality is implemented as C++ calls wrapped in static blueprints. 
We also provide some blueprint classes out-of-the-box, which use the static blueprint calls to provide fully functional examples.

Logically, the project can be separated into:
 - Actual raymarching materials (shaders)
 - Illumination computation compute shaders (and C++ code wrapping them)
 - Labeling shaders  (and C++ code wrapping them)
 - A blueprint library wrapping the C++ calls into static functions
 - Blueprints using the raymarching blueprint library
 
 We will now describe these individually.
 
## Actual raymarching materials
We created raymarching materials that repeatedly sample a volume texture along a ray, transfer the read value by a transfer function, take lighting into account and then accumulate the color samples into a final pixel color.

We based our implementation on [Ryan Brucks' raymarching](https://shaderbits.com/blog/creating-volumetric-ray-marcher) materials from way back when Volume Textures weren't a thing in Unreal. We made some modifications, most notably, added support for a cutting plane to cut away parts of the volume. 

Also, because using Custom nodes in material editor is a pain, we implemented our material functions in .usf files inside our plugin and then include and call these functions from within Custom nodes.

Another modification is that we use unit meshes (a cube centered at 0, going from -0.5 to 0.5, so we don't need to care about mesh bounds of the used cube.

We also improved the depth calculations, so that depth-ordering works with arbitrary transforms of the raymarched volume (notably anisotropic scaling didn't work before).


### Actual raymarching

We tried to be generous with comments, so see `VolumeRaymarching/Shaders/Private/RaymarchMaterials.usf` for the actual code and explanation.

The actual raymarching is a 2-step process. First part is getting the entry point and thickness at the current pixel. See `PerformRaymarchCubeSetup()`to see how that's done.
Second part is performing the actual raymarch. See `PerformLitRaymarch()` for the simplest raymarcher.

We implemented 4 raymarch materials in this plugin.
`PerformLitRaymarch()` - standard raymarching using Transfer Functions and an Illumination volume.
`PerformIntensityRaymarch()` - isn't true raymarching, instead when the volume is first hit, the intensity of the volume is directly transformed into a grayscale value and returned. We used this to be able to show the underlying volumes' data. 

For both of these, there is also a "labeled" version, which also samples a label volume and mixes the colors from the label volume with the colors sampled from the Data volume.
The labels aren't lit, don't cast a shadow and they're not affected by the cutting plane.
See `PerformLitRaymarchLabeled()` and `PerformIntensityRaymarchLabeled()`. We discuss labeling in detail in the Labeling section below.

As we were targetting specific medical data, we only needed support for uint_8 typed volumes combined with a transfer function that transforms this data into a RGBA tuple. We don't provide functionality to raymarch implicitly colored volume textures (such as PF_FloatRGBA format).

### Shared functions
As you can see in `RaymarchMaterials.usf`, we include common functions from `RaymarcherCommon.usf`. 

This file contains functions that are used both in material shaders and in compute shaders computing the illumination. Using the same functions assures consistency between opacity perceived when looking at the volume and the strength of shadows cast by the volume.  

A notable function here is the `SampleDataVolume()`function. This samples the volume at the given UVW position. Then it transforms the value according to the provided intensity domain. The intensity domain is provided as a range within [0, 1]. This is used to filter away very high or very low frequencies.
 Afterwards, the Transfer function texture is sampled at a position corresponding to the sampled intensity. The alpha of the color sampled from the Transfer Function is then modified by an extinction formula to accomodate for the size of the raymarching step. (Longer step means more opacity, same as in `CorrectForStepSize()` function) 

## Illumination computation compute shaders (and C++ code wrapping them)
As mentioned above, we implement a method for precomputed illumination as in the paper s from the paper "Efficient Volume Illumination with Multiple Light Sources through Selective Light Updates". Unlike them, we contended with a single channel illumination volume (not a RGB light volume to accomodate for colored lights).
We also didn't implement some of the optimizations mentioned in the paper - notably joining passes of different lights that go in the same axis into one pass (this would require a rewrite of the compute shaders and the whole blueprint/C++ hierarchy).

The shaders are declared in `RaymarchRendering.h` and defined in  `RaymarchRendering.cpp`. You can notice that the shaders have an inheritence hierarchy and only the non-abstract shaders are declared and implemented with the UE macro `DECLARE_SHADER_TYPE() / IMPLEMENT_SHADER_TYPE()`

The two shaders that actually calculate the illumination and modify the illumination volume are `FAddDirLightShader` and `FChangeDirLightShader`. 

The .usf files containing the actual HLSL code of the shaders is in `/Shaders/Private/AddDirLightShader.usf` and `ChangeDirLightShader.usf`

It's important to note that a single invocation of the shader only affects one slice of the light volume. The shaders both need to be invoke as many times as there are layers in the current propagation axis. This is because we didn't find a way to synchronize within the whole propagation layer (as compute shaders can only be synchronized within one thread group and the whole slice of the texture doesn't fit in one group). Read the paper for more info on how we use the read/write buffers and propagate the light per-layer.

We implemented an AddDirLight shader and a ChangeDirLight shader. 

A (kind of a) sequence diagram here shows the interplay of blueprints, game thread, render thread and RHI thread.

![
](https://github.com/TheHugeManatee/UE4_VolumeRaymarching/Documents/sequence.png)

# Platforms
Currently only Windows is natively supported. We experimetnted with building on Linux and illumination compute shaders didn't work properly under OpenGL. They did, however work under the Vulkan backend, but significantly slower than under DX. 

**Because Unreal blueprints and maps don't transform very well to older engine version, we can only  guarantee this works in 4.22. (but since you need a custom build from us anyways, just build from the "release" branch of our custom engine repo.* <br/>

However, other versions might still work.
