// (C) Technical University of Munich - Computer Aided Medical Procedures
// Developed by Tomas Bartipan (tomas.bartipan@tum.de)

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "RaymarchRendering.h"

#include "VolumeLabeling.generated.h"

// Enum class for the different possible labels

// Enumeration for how to handle parts of Transfer Function which are cut off.
UENUM(BlueprintType)
enum class FSurgeryLabel : uint8 {
  SL_Clear = 0,   // Clear, will be fully transparent
  SL_Risk = 1,    // Risk area, red with alpha ~0.5
  SL_Target = 2,  // Target area - green with alpha ~0.5
  SL_NotPainting = 3
};

// Parent shader for any shader that works on Light Volumes. Provides a way to bind and unbind the 4
// light volumes.
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
    // Set the UAV parameter for the light volume
    SetUAVParameter(RHICmdList, ShaderRHI, MarkedVolume, pMarkedVolume);
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
    SetUAVParameter(RHICmdList, ShaderRHI, MarkedVolume, FUnorderedAccessViewRHIParamRef());
  }

  bool Serialize(FArchive& Ar) {
    bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
    Ar << MarkedVolume << CuboidCenter << WrittenValue << CuboidSize;
    return bShaderHasOutdatedParameters;
  }

protected:
  // The modified volume.
  FShaderResourceParameter MarkedVolume;
  // Written cuboid center and size.
  FShaderParameter CuboidCenter;
  FShaderParameter CuboidSize;
  // Value to be written to the area.
  FShaderParameter WrittenValue;
};

void WriteSphereToVolume_RenderThread(FRHICommandListImmediate& RHICmdList,
                                      FRHITexture3D* MarkedVolume, const float SphereRadiusWorld,
                                      const FVector BrushWorldCenter,
                                      const FRaymarchWorldParameters WorldParameters,
                                      FSurgeryLabel WrittenValue);

void WriteSphereToVolumeLocal_RenderThread(FRHICommandListImmediate& RHICmdList,
                                           FRHITexture3D* MarkedVolume,
                                           const FVector BrushLocalCenter,
                                           const float SphereRadiusLocal,
                                           FSurgeryLabel WrittenValue);

// Actual blueprint library follows
UCLASS()
class ULabelVolumeLibrary : public UBlueprintFunctionLibrary {
  GENERATED_BODY()
public:
  /** Creates labeling volume with the provided name and dimensions. */
  UFUNCTION(BlueprintCallable, Category = "Label Volume")
  static void CreateNewLabelingVolumeAsset(FString AssetName, FIntVector Dimensions,
                                           UVolumeTexture*& OutTexture);

  /** Recreates the labeling volume in the provided volume texture. */
  UFUNCTION(BlueprintCallable, Category = "Label Volume")
  static void InitLabelingVolume(UVolumeTexture* LabelVolumeAsset, FIntVector Dimensions);

  /** Writes specified value into a volume at a sphere specified in world coordinates. */
  UFUNCTION(BlueprintCallable, Category = "Label Volume")
  static void LabelSphereInVolumeWorld(UVolumeTexture* MarkedVolume, FVector BrushWorldCenter,
                                       const float SphereRadiusWorld,
                                       const FRaymarchWorldParameters WorldParameters,
                                       const FSurgeryLabel Label);

  /** Writes specified value into a volume at a sphere specified in local coordinates. */
  UFUNCTION(BlueprintCallable, Category = "Label Volume")
  static void LabelSphereInVolumeLocal(UVolumeTexture* MarkedVolume, FVector BrushLocalCenter,
                                       const float SphereLocalRadius, const FSurgeryLabel Label);
};