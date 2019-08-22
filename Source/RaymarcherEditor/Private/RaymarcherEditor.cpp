// (C) Technical University of Munich - Computer Aided Medical Procedures
// Jakob Weiss <jakob.weiss@tum.de>

#include "RaymarcherEditor.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "FRaymarcherEditorModule"

void FRaymarcherEditorModule::StartupModule() {
  // This code will execute after your module is loaded into memory; the exact timing is specified
  // in the .uplugin file per-module
}

void FRaymarcherEditorModule::ShutdownModule() {
  // This function may be called during shutdown to clean up your module.  For modules that support
  // dynamic reloading, we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRaymarcherEditorModule, RaymarcherEditor)
