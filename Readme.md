
# Deprecation notice
This project is deprecated, find the new and improved version here:

https://github.com/tommybazar/TBRaymarchProject

The new one works without the need for a custom UE build, is way cleaner and more reliable and works with 4.25+

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

# Plugin Structure
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

All materials used for raymarching are in `Content/Materials` with `M_Raymarch` and `M_Raymarch_Labeled` being the most interesting. Also, the `M_PureIntensityRaymarch` and `M_PureIntensityRaymarch_Labeled` are there to perform a "raymarch" that terminates immediately after hitting the first non-cut-away voxel (you can see that in the demo when the cube changes into a greyscale cube).

The materials aren't very complex, as we do all our calculations in code, so the materials are pretty much just wrappers for our custom HLSL code and to pass parameters into the code.

Also, because using Custom nodes in material editor is a pain, we implemented our material functions in .usf files inside our plugin and then include and call these functions from within Custom nodes.

You can see the way we hack the functions we need included into the compiled shader in the `GlobalIncludes` custom node. Credit for this beautiful hack goes here
[http://blog.kiteandlightning.la/ue4-hlsl-shader-development-guide-notes-tips/](http://blog.kiteandlightning.la/ue4-hlsl-shader-development-guide-notes-tips/).

We based our implementation on [Ryan Brucks' raymarching](https://shaderbits.com/blog/creating-volumetric-ray-marcher) materials from way back when Volume Textures weren't a thing in Unreal. We made some modifications, most notably, added support for a cutting plane to cut away parts of the volume. 


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

It's important to note that a single invocation of the shader only affects one slice of the light volume. The shaders both need to be invoked as many times as there are layers in the current propagation axis. This is because we didn't find a way to synchronize within the whole propagation layer (as compute shaders can only be synchronized within one thread group and the whole slice of the texture doesn't fit in one group). Read the paper for more info on how we use the read/write buffers and propagate the light per-layer.

We implemented the AddDirLight shader and a ChangeDirLight shader. The difference being that the ChangeDirLight shader takes `OldLightParameters` and `NewLightParameters` to change a light in a single pass.

A (kind of a) sequence diagram here shows the interplay of blueprints, game thread, render thread and RHI thread.

![
](https://github.com/TheHugeManatee/UE4_VolumeRaymarching/blob/master/Documents/sequence.png)

Unlike in the paper, we don't perform the optimization of joining all light sources coming from the same axis into one propagation pass. It would be possible to do this by changing the shaders to use Structured Buffers with an arbitrary number of lights as inputs, but a rewrite of the whole plugin structure would be required for that (instead of static blueprints working on a single light each, C++ code that checks for all the changed lights, queues them up and then invokes the shader passes for the changed axes). I didn't have the time to do this rewrite and the way it is now is a bit more obvious in how everything works.

## Labeling Shaders, blueprints & C++ code
For creating the labeling volumes, we use blueprints from the blueprint library located at 
`Source/Raymarcher/Public/VolumeLabeling.h`

I tried to be generous with comments in both the header and source files, as well as in the shader used for labeling located at `Shaders/Private/WriteEllipsoidShader.usf`

You can use the blueprints `LabelSphereInVolumeWorld` and `LabelSphereInVolumeLocal` to actually perform the writing of ellipsoids into the volume. The difference between these two is that the `Local` version will write an ellipsoid that will be spherical in local coordinates of the volume (and thus possibly not actually spherical in a stretched volume when displaying it) and the `World` version will make a sphere that is spherical in World coordinates (and thus possibly non-spherical in local coords).

These blueprints are called by the `LabelSphere` function in the `RaymarchVolumeLabeled` blueprint actor.

We use a uint8 volume texture to write up to 255 labels (+ 0 for no label) and then these labels get converted into a RGBA value in the function `GetColorFromLabelValue()` located in `RaymarchMaterialCommon.usf`. Because we could only get this to work properly with compute shaders using float operations, it's necessary to convert the uint to float in the shaders using `CharToFloat()` and `FloatToChar()` located in `RaymarcherCommon.usf` in the shader.

If you want to change the labels used, you need to change the `GetColorFromLabelValue()` function as well as updating the enum `FSurgeryLabel` to accomodate for your labels. 

## C++ static blueprint wrappers
Most of the functionality can be accessed by calling static blueprint functions declared in `Source/Public/RaymarchBlueprintLibrary.h`. Looking back, this was not the ideal way to implement the plugin, as you must pass a massive amount of parameters to some of the functions, it would make more sense to create C++ actors and make them have non-static BP functions, but this would need a full rewrite of the whole plugin.

The functions are mostly well-documented with self-explanatory names, so I won't get into explaining what which one does.

## Blueprints using the static functions
Now we're getting to the highest level, actual UE4 blueprints using the static blueprint functions.

To see how we use the functions, inspect the functions implemented in the `Content/Blueprints/BP_RaymarchedVolume.uasset` and `Content/Blueprints/BP_RaymarchedVolume_Labeled.uasset` blueprint assets.

# Getting started

 1. Compile our custom version of the engine and create a project with this plugin in the `Plugins` folder of the project.
2. Make sure you have `Show Plugin Content` enabled in the content browser, then you will see `Raymarcher Content` in the root of the content tree. 
3. In the `Blueprints` folder, drag and drop a `BP_RaymarchedVolume` into the scene, along with 2-3 of `BP_RaymarchingLight` and a single `BP_ClippingPlane`.
4. Place the clipping plane within the volume and then go to the properties of the`BP_RaymarchedVolume` and add your lights into the `Lights Array` of the raymarched volume. Then set the `Clipping Plane` property of the volume to the clipping plane you inserted into the map.  
5. You can see other parameters of the volume that are interesting and that is the `Size in mm` and `Volume Scale`. These two can be changed to scale the volume. 
6. You can also change the transfer function texture and ColorCurve. The texture content used for the transfer function gets created in the `ConstructionScript` of the volume and the values get taken from the ColorCurve provided. See the BP function `ColorCurveToTexture` for more.
7. You can also change the `Raymarching Steps` parameter to change the amount of steps taken when raymarching. More steps leads to nicer results at the cost of performance.

Now, if you press Simulate, the light volume will get updated and the raymarched volume will be lit and cut away on one side of the cutting plane. You can rotate the lights to see the lighting update and move/rotate the clipping plane to see it being cut away differently.

I recommend going through the `ConstructionScript` and `BeginPlay` of the blueprint to see the necessary setup performed at game start.

In the `Construction script` it is notable that we create dynamic material instances, so we can set the parameters of the raymarching materials during runtime. What gets changed often is the clipping plane parameters in the `SetClippingPlaneParams` BP function.

Notice the `Switch Renderer` BP Function which toggles between Lit Raymarching and Pure Intensity raymarching. Try binding a button to call this function to see the effect.

## Loading MHDs

If you want to load a MHD file containing some other data, there is the `MHD File Name` text field parameter in the . You can put in the absolute path to the file, or a path relative to the `Content` directory of your project. See `LoadMHDIntoVolumeTextureAsset` static BP function for more. 
Beware, because construction scripts don't support adding a flag that would be persistent, the file will be loaded every time the construction script is invoked (which is very often if you e.g. move the volume). It's better to create an BLutility script for loading the volume into an empty Volume Texture asset and then set the `Data Volume` parameter to that Volume Texture. The `LoadMHDIntoVolumeTextureAsset` will overwrite the data in the existing asset provided to it, while there is also a function `LoadMhdIntoNewVolumeTextureAsset` which will create the asset for you and place it into `Generated Textures` folder in your project `Content` folder.
 Both of these have a `Persistent` flag. If that is set to `true`, then the texture source will be updated and the loaded volume texture can be saved as an .uasset to be used in later runs of the engine. Note you actually have to go to the asset and press the save button for it to save.
 
 The loading and parsing of MHD files is done in the `MHDInfo.cpp` file and can be debugged there.
 
## Other formats
If you make a different data reader which will give you your Volume Texture dimensions and raw data as a uint8* array, you can use functions from `TextureHelperFunctions.h` to create Volume Texture assets from them from within your C++ code. 
The functions are `CreateVolumeTextureAsset` and `UpdateVolumeTextureAsset`.

If you have raw Volume Texture data saved on disk and you already know the dimensions,  use `LoadRawFileIntoArray` to create a uint8* array from them and then use the above mentioned functions to create Volume Texture assets from them.

## Tick
 On each tick of the `BP_RaymarchedVolume`, we check if the light volume needs to be modified because our lights changed. First we check if the BP function `Needs World Update` returns true. That is the case if the whole light volume needs to be recalculated either because the volume rotated or because the clipping plane moved.
 If that is the case, the `Reset All Lights` BP function gets called and the whole light volume gets cleared and all lights are then added again.
 
 If that is not the case, we check each individual light to see if it changed rotation or intensity. If more than half of the lights did, we also `Reset All Lights`, if only a few did, we call the `Update Light` BP function on them, which gets the lights' previous and current parameters and then calls the `ChangeDirLightInSingleLightVolume` function on all the affected lights.

You can see that there is a separate branch for the first "draw" of the lights into the lighting volume texture. This is because we cannot guarantee that all resources are created when BeginPlay finishes initializing the light volume, so we check for the validity of all resources and if they are all valid, perform the first adding of the lights.
(TODO - this might be possible with `FlushRenderingCommands`...)

# Platforms
Currently only Windows is natively supported. We experimetnted with building on Linux and illumination compute shaders didn't work properly under OpenGL. They did, however work under the Vulkan backend, but significantly slower than under DX. 

**Because Unreal blueprints and maps don't transform very well to older engine version, we can only  guarantee this works in 4.22. (but since you need a custom build from us anyways, just build from the "release" branch of our custom engine repo.* <br/>
