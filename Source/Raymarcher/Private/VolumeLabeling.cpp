// (C) Technical University of Munich - Computer Aided Medical Procedures
// Developed by Tomas Bartipan (tomas.bartipan@tum.de)

#include "../Public/VolumeLabeling.h"
#include "AssetRegistryModule.h"
#include "RenderCore/Public/RenderUtils.h"
#include "Renderer/Public/VolumeRendering.h"
#include "TextureHelperFunctions.h"

#define LOCTEXT_NAMESPACE "RaymarchPlugin"

IMPLEMENT_SHADER_TYPE(, FWriteSphereToVolumeShader,
                      TEXT("/Plugin/VolumeRaymarching/Private/WriteCuboidShader.usf"),
                      TEXT("MainComputeShader"), SF_Compute)

void WriteSphereToVolume_RenderThread(FRHICommandListImmediate& RHICmdList,
                                      FRHITexture3D* MarkedVolume, const FVector BrushWorldCenter,
                                      const float SphereRadiusWorld,
                                      const FRaymarchWorldParameters WorldParameters,
                                      FSurgeryLabel WrittenValue) {
  // Get local center.
  FVector localCenter = WorldParameters.VolumeTransform.InverseTransformPosition(BrushWorldCenter) + 0.5;
  // Get local center to UVW coords

  float localSphereDiameter = WorldParameters.VolumeTransform.InverseTransformVector(FVector(SphereRadiusWorld, 0, 0)).Size() * 2;

  /**
  This would be for making the sphere actually round in world space. Currently, let's prefer the sphere being round in the 
  square window for displaying the labeled data.

  FVector VolumeDimensions(MarkedVolume->GetSizeXYZ());
  FVector VoxelSize = WorldParameters.VolumeTransform.GetScale3D() / FVector(VolumeDimensions);
  FVector BrushSize = ((FVector(SphereRadiusWorld) / VoxelSize) * 2) + 1;

  FIntVector BrushSizeInt(FMath::RoundToInt(BrushSize.X), FMath::RoundToInt(BrushSize.Y),
                          FMath::RoundToInt(BrushSize.Z));

  FIntVector LocalCenterIntCoords = FIntVector(LocalCenterUVW * VolumeDimensions);

  */

  // Get local center in integer space
  FIntVector localCenterIntCoords = FIntVector(localCenter * FVector(MarkedVolume->GetSizeXYZ()));

  // Get shader ref from GlobalShaderMap
  TShaderMap<FGlobalShaderType>* GlobalShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);
  TShaderMapRef<FWriteSphereToVolumeShader> ComputeShader(GlobalShaderMap);

  RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
  // RHICmdList.TransitionResource(EResourceTransitionAccess::ERWNoBarrier, LightVolumeResource);
  FUnorderedAccessViewRHIRef MarkedVolumeUAV = RHICreateUnorderedAccessView(MarkedVolume);

  // Transfer from gfx to compute, otherwise the renderer might touch our textures while we're
  // writing them.
  RHICmdList.TransitionResource(EResourceTransitionAccess::ERWNoBarrier,
                                EResourceTransitionPipeline::EGfxToCompute, MarkedVolumeUAV);

  // Get brush size in texture space.
  FIntVector brush = FIntVector(localSphereDiameter * MarkedVolume->GetSizeX() + 1,
                                localSphereDiameter * MarkedVolume->GetSizeY() + 1,
                                localSphereDiameter * MarkedVolume->GetSizeZ() + 1);

  FString kkt = "Written texture size = " + MarkedVolume->GetSizeXYZ().ToString() + "\nLocalCenter =" + localCenter.ToString() + ", in ints = " + localCenterIntCoords.ToString() +
                "\nLocal Sphere diameter = " + FString::SanitizeFloat(localSphereDiameter) + ", brush size in ints = " + brush.ToString();

//  GEngine->AddOnScreenDebugMessage(0, 20, FColor::Yellow, kkt);

  FComputeShaderRHIParamRef cs = ComputeShader->GetComputeShader();
  ComputeShader->SetMarkedVolumeUAV(RHICmdList, cs, MarkedVolumeUAV);
  // As the label enum class is based on uint8 anyways, the cast is probably redundant.
  ComputeShader->SetParameters(RHICmdList, cs, localCenterIntCoords, brush, (uint8)WrittenValue);

  DispatchComputeShader(RHICmdList, *ComputeShader, brush.X, brush.Y, brush.Z);

  ComputeShader->UnbindMarkedVolumeUAV(RHICmdList, cs);

  RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable,
                                EResourceTransitionPipeline::EComputeToGfx, MarkedVolumeUAV);
}

void WriteSphereToVolumeLocal_RenderThread(FRHICommandListImmediate& RHICmdList,
                                           FRHITexture3D* MarkedVolume,
                                           const FVector BrushLocalCenter,
                                           const float SphereRadiusLocal,
                                           FSurgeryLabel WrittenValue) {
  // Get local center in integer space
  FIntVector localCenterIntCoords = FIntVector(BrushLocalCenter * FVector(MarkedVolume->GetSizeXYZ()));

  // Get shader ref from GlobalShaderMap
  TShaderMap<FGlobalShaderType>* GlobalShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);
  TShaderMapRef<FWriteSphereToVolumeShader> ComputeShader(GlobalShaderMap);

  RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
  // RHICmdList.TransitionResource(EResourceTransitionAccess::ERWNoBarrier, LightVolumeResource);
  FUnorderedAccessViewRHIRef MarkedVolumeUAV = RHICreateUnorderedAccessView(MarkedVolume);

  // Transfer from gfx to compute, otherwise the renderer might touch our textures while we're
  // writing them.
  RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable,
                                EResourceTransitionPipeline::EGfxToCompute, MarkedVolumeUAV);
    
  check((uint8)WrittenValue < 6);

  // Get brush size in texture space.
  FIntVector brush = FIntVector(SphereRadiusLocal * 2 * MarkedVolume->GetSizeX() + 1,
                                SphereRadiusLocal * 2 * MarkedVolume->GetSizeY() + 1,
                                SphereRadiusLocal * 2 * MarkedVolume->GetSizeZ() + 1);

  FString kkt = "Written texture size = " + MarkedVolume->GetSizeXYZ().ToString() +
                "\nLocalCenter =" + BrushLocalCenter.ToString() +
                ", in ints = " + localCenterIntCoords.ToString() +
                "\nLocal Sphere diameter = " + FString::SanitizeFloat(SphereRadiusLocal * 2) +
                ", brush size in ints = " + brush.ToString();

//  GEngine->AddOnScreenDebugMessage(0, 20, FColor::Yellow, kkt);

  FComputeShaderRHIParamRef cs = ComputeShader->GetComputeShader();
  ComputeShader->SetMarkedVolumeUAV(RHICmdList, cs, MarkedVolumeUAV);
  // As the label enum class is based on uint8 anyways, the cast is probably redundant.
  ComputeShader->SetParameters(RHICmdList, cs, localCenterIntCoords, brush, (uint8)WrittenValue);

  DispatchComputeShader(RHICmdList, *ComputeShader, brush.X, brush.Y, brush.Z);

  ComputeShader->UnbindMarkedVolumeUAV(RHICmdList, cs);

  RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable,
                                EResourceTransitionPipeline::EComputeToGfx, MarkedVolumeUAV);
}

void ULabelVolumeLibrary::CreateNewLabelingVolumeAsset(FString AssetName, FIntVector Dimensions,
                                                       UVolumeTexture*& OutTexture) {
  if (!CreateVolumeTextureAsset(AssetName, PF_G8, Dimensions, OutTexture, nullptr, false, false,
                                true)) {
    GEngine->AddOnScreenDebugMessage(0, 10, FColor::Yellow, "Failed creating the labeling volume.");
  }
}

void ULabelVolumeLibrary::InitLabelingVolume(UVolumeTexture* LabelVolumeAsset,
                                             FIntVector Dimensions) {


  if (!UpdateVolumeTextureAsset(LabelVolumeAsset, PF_G8, Dimensions, nullptr, false, false, true)) {
    GEngine->AddOnScreenDebugMessage(0, 10, FColor::Yellow,
                                     "Failed initializing the labeling volume.");
  }
  FlushRenderingCommands();
}

float SurgeryLabelToFloat(const FSurgeryLabel label) {
  switch (label) {
    case FSurgeryLabel::SL_NotPainting:
    case FSurgeryLabel::SL_Clear: return 0.0;
    case FSurgeryLabel::SL_Risk: return (1.0 / 255.0);
    case FSurgeryLabel::SL_Target: return (2.0 / 255.0);
//    case FSurgeryLabel::SL_PotentialRisk: return (3.0 / 255.0);
//    case FSurgeryLabel::SL_PotentialTarget: return (4.0 / 255.0);
    // This shouldn't happen -> assert
    default: check(0); return 0.0;
  }
}

void ULabelVolumeLibrary::LabelSphereInVolumeWorld(UVolumeTexture* MarkedVolume,
                                                   const FVector BrushWorldCenter,
                                                   const float SphereRadiusWorld,
                                                   const FRaymarchWorldParameters WorldParameters,
                                                   const FSurgeryLabel WrittenValue) {
  if (MarkedVolume->Resource == NULL) return;
  // Call the actual rendering code on RenderThread.

  ENQUEUE_RENDER_COMMAND(CaptureCommand)
  ([=](FRHICommandListImmediate& RHICmdList) {
    WriteSphereToVolume_RenderThread(RHICmdList, MarkedVolume->Resource->TextureRHI->GetTexture3D(),
                                     BrushWorldCenter, SphereRadiusWorld, WorldParameters,
                                     WrittenValue);
  });
}

void ULabelVolumeLibrary::LabelSphereInVolumeLocal(UVolumeTexture* MarkedVolume,
                                                   FVector BrushLocalCenter,
                                                   const float SphereLocalRadius,
                                                   const FSurgeryLabel WrittenValue) {
if (MarkedVolume->Resource == NULL) return;
  // Call the actual rendering code on RenderThread.

  ENQUEUE_RENDER_COMMAND(CaptureCommand)
  ([=](FRHICommandListImmediate& RHICmdList) {
    WriteSphereToVolumeLocal_RenderThread(RHICmdList, MarkedVolume->Resource->TextureRHI->GetTexture3D(),
                                     BrushLocalCenter, SphereLocalRadius, WrittenValue);
  });}

#undef LOCTEXT_NAMESPACE
