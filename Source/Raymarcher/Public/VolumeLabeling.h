// (C) Technical University of Munich - Computer Aided Medical Procedures
// Developed by Tomas Bartipan (tomas.bartipan@tum.de)

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "RaymarchRendering.h"

#include "VolumeLabeling.generated.h"

// Enum class for the different possible labels.
//
// If you modify this, also modify the function SurgeryLabelToFloat in the .cpp file
// and the GetColorFromLabelValue function in RaymarchMaterialCommon.usf
// to have your changes consistent.
UENUM(BlueprintType)
enum class FSurgeryLabel : uint8 {
  SL_Clear = 0,   // Clear, will be fully transparent
  SL_Risk = 1,    // Risk area, red with alpha ~0.5
  SL_Target = 2,  // Target area - green with alpha ~0.5
  SL_NotPainting = 3
};

// Class declaring a shader used for writing spheroids into a volume.
class FWriteSphereToVolumeShader : public FGlobalShader {
  DECLARE_SHADER_TYPE(FWriteSphereToVolumeShader, Global)

public:
  static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) {
    return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
  }

  FWriteSphereToVolumeShader(){};

  FWriteSphereToVolumeShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
    : FGlobalShader(Initializer) {
    // Bind Light Volume resource parameters
    MarkedVolume.Bind(Initializer.ParameterMap, TEXT("MarkedVolume"));
    CuboidCenter.Bind(Initializer.ParameterMap, TEXT("CuboidCenter"));
    CuboidSize.Bind(Initializer.ParameterMap, TEXT("CuboidSize"));
    WrittenValue.Bind(Initializer.ParameterMap, TEXT("WrittenValue"));
  }

  void SetMarkedVolumeUAV(FRHICommandListImmediate& RHICmdList, FComputeShaderRHIParamRef ShaderRHI,
                          const FUnorderedAccessViewRHIParamRef pMarkedVolume) {
    // Set the UAV parameter for the labeling volume
    RHICmdList.SetUAVParameter(ShaderRHI, MarkedVolume.GetUAVIndex(), pMarkedVolume);
  }

  void SetParameters(FRHICommandListImmediate& RHICmdList, FComputeShaderRHIParamRef ShaderRHI,
                     FIntVector pCuboidCenter, FIntVector pCuboidSize, uint8 pWrittenValue) {
    SetShaderValue(RHICmdList, ShaderRHI, CuboidCenter, pCuboidCenter);
    SetShaderValue(RHICmdList, ShaderRHI, CuboidSize, pCuboidSize);
    SetShaderValue(RHICmdList, ShaderRHI, WrittenValue, pWrittenValue);
  }

  void UnbindMarkedVolumeUAV(FRHICommandListImmediate& RHICmdList,
                             FComputeShaderRHIParamRef ShaderRHI) {
    // Unbind the UAVs.
    RHICmdList.SetUAVParameter(ShaderRHI, MarkedVolume.GetUAVIndex(),
                               FUnorderedAccessViewRHIParamRef());
  }

  bool Serialize(FArchive& Ar) {
    bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
    Ar << MarkedVolume << CuboidCenter << WrittenValue << CuboidSize;
    return bShaderHasOutdatedParameters;
  }

protected:
  // The modified volume.
  FRWShaderParameter MarkedVolume;
  // Written cuboid center and size.
  FShaderParameter CuboidCenter;
  FShaderParameter CuboidSize;
  // Value to be written to the area.
  FShaderParameter WrittenValue;
};

/**
  Render-thread side of writing a sphere to a volume. Takes care of binding the resources
  to the compute shader and dispatching it.
*/
void WriteSphereToVolume_RenderThread(FRHICommandListImmediate& RHICmdList,
                                      FRHITexture3D* MarkedVolume, const float SphereRadiusWorld,
                                      const FVector BrushWorldCenter,
                                      const FRaymarchWorldParameters WorldParameters,
                                      FSurgeryLabel WrittenValue);

/**
  Render-thread side of writing a sphere to a volume. Takes care of binding the resources
  to the compute shader and dispatching it.
*/
void WriteSphereToVolumeLocal_RenderThread(FRHICommandListImmediate& RHICmdList,
                                           FRHITexture3D* MarkedVolume,
                                           const FVector BrushLocalCenter,
                                           const float SphereRadiusLocal,
                                           FSurgeryLabel WrittenValue);

// Volume labeling blueprint library follows.
UCLASS()
class ULabelVolumeLibrary : public UBlueprintFunctionLibrary {
  GENERATED_BODY()
public:
  /** Creates labeling volume with the provided name and dimensions. Returns the newly created
   * texture. */
  UFUNCTION(BlueprintCallable, Category = "Label Volume")
  static void CreateNewLabelingVolumeAsset(FString AssetName, FIntVector Dimensions,
                                           UVolumeTexture*& OutTexture);

  /** Recreates the labeling volume in the provided volume texture. */
  UFUNCTION(BlueprintCallable, Category = "Label Volume")
  static void InitLabelingVolume(UPARAM(ref) UVolumeTexture*& LabelVolumeAsset,
                                 FIntVector Dimensions);

  /**
    Writes the specified label into a volume at a sphere specified by world coordinates and size.
    If using an anisotropic volume, the sphere written will be a sphere in world coordinates, not
    in local.
  */
  UFUNCTION(BlueprintCallable, Category = "Label Volume")
  static void LabelSphereInVolumeWorld(UVolumeTexture* MarkedVolume, FVector BrushWorldCenter,
                                       const float SphereRadiusWorld,
                                       const FRaymarchWorldParameters WorldParameters,
                                       const FSurgeryLabel Label);

  /**
    Writes the specified label into a volume at a sphere specified by local coordinates and size.
    If using an anisotropic volume, the sphere written will be a sphere in local coordinates, not
    in world.
  */
  UFUNCTION(BlueprintCallable, Category = "Label Volume")
  static void LabelSphereInVolumeLocal(UVolumeTexture* MarkedVolume, FVector BrushLocalCenter,
                                       const float SphereLocalRadius, const FSurgeryLabel Label);
};
