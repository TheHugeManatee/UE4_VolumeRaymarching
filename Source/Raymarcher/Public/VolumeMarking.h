// (C) Technical University of Munich - Computer Aided Medical Procedures
// Developed by Tomas Bartipan (tomas.bartipan@tum.de)

#pragma once

#include "RaymarchRendering.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "VolumeMarking.generated.h"


// Parent shader for any shader that works on Light Volumes. Provides a way to bind and unbind the 4
// light volumes.
class FWriteCuboidToVolumeShader : public FGlobalShader {
	DECLARE_SHADER_TYPE(FWriteCuboidToVolumeShader, Global)

public:
  static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) {
    return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
  }

  FWriteCuboidToVolumeShader(){};

  FWriteCuboidToVolumeShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
    : FGlobalShader(Initializer) {
    // Bind Light Volume resource parameters
    MarkedVolume.Bind(Initializer.ParameterMap, TEXT("MarkedVolume"));
    CuboidCenter.Bind(Initializer.ParameterMap, TEXT("CuboidCenter"));
    CuboidSize.Bind(Initializer.ParameterMap, TEXT("CuboidSize"));
    WrittenValue.Bind(Initializer.ParameterMap, TEXT("WrittenValue"));
  }

  void SetMarkedVolumeUAV(FRHICommandListImmediate& RHICmdList,
                                  FComputeShaderRHIParamRef ShaderRHI,
                                  const FUnorderedAccessViewRHIParamRef pMarkedVolume) {
    // Set the UAV parameter for the light volume
    SetUAVParameter(RHICmdList, ShaderRHI, MarkedVolume, pMarkedVolume);
  }

  void SetParameters(FRHICommandListImmediate& RHICmdList, FComputeShaderRHIParamRef ShaderRHI, FIntVector pCuboidCenter, FIntVector pCuboidSize, float pWrittenValue)
  {
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
    Ar << MarkedVolume << CuboidCenter << CuboidSize << WrittenValue;
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

void WriteCuboidToVolume_RenderThread(FRHICommandListImmediate & RHICmdList, FRHITexture3D * MarkedVolume, const FVector BrushWorldCenter, const FVector BrushWorldSize, const FRaymarchWorldParameters WorldParameters, float WrittenValue);


// Actual blueprint library follows
UCLASS()
class UVolumeMarkingLibrary : public UBlueprintFunctionLibrary {
	GENERATED_BODY()
public:

	///** Writes specified value into a volume at a cuboid specified in world coordinates. */
	UFUNCTION(BlueprintCallable, Category = "RGBRaymarcher",
		meta = (WorldContext = "WorldContextObject"))
		static void CreateMarkingVolume(FIntVector Dimensions, FString AssetName, UVolumeTexture*& OutTexture);

	/** Writes specified value into a volume at a cuboid specified in world coordinates. */
	UFUNCTION(BlueprintCallable, Category = "RGBRaymarcher",
		meta = (WorldContext = "WorldContextObject"))
	static void MarkCuboidInVolumeWorld(UVolumeTexture* MarkedVolume, FVector BrushWorldCenter, FVector BrushWorldSize, const FRaymarchWorldParameters WorldParameters, float WrittenValue);
};