// (C) Technical University of Munich - Computer Aided Medical Procedures
// Developed by Tomas Bartipan (tomas.bartipan@tum.de)

#include "Raymarcher.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "FRaymarcherModule"

void FRaymarcherModule::StartupModule() {
  // This code will execute after your module is loaded into memory; the exact timing is specified
  // in the .uplugin file per-module

  FString PluginShaderDir = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("VolumeRaymarching"));
  PluginShaderDir = FPaths::Combine(PluginShaderDir, TEXT("Shaders"));
  AddShaderSourceDirectoryMapping(TEXT("/Plugin/VolumeRaymarching"), PluginShaderDir);
}

void FRaymarcherModule::ShutdownModule() {
  // This function may be called during shutdown to clean up your module.  For modules that support
  // dynamic reloading, we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRaymarcherModule, Raymarcher)
