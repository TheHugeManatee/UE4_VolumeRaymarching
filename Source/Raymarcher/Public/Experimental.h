// (C) Technical University of Munich - Computer Aided Medical Procedures
// Developed by Tomas Bartipan (tomas.bartipan@tum.de)

#pragma once

#include "CoreMinimal.h"

#include "Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/VolumeTexture.h"
#include "Engine/World.h"
#include "GlobalShader.h"
#include "Internationalization/Internationalization.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Logging/MessageLog.h"
#include "PipelineStateCache.h"
#include "RHIStaticStates.h"
#include "SceneInterface.h"
#include "SceneUtils.h"
#include "ShaderParameterUtils.h"
#include "UObject/ObjectMacros.h"

#include <algorithm>  // std::sort
#include <utility>    // std::pair, std::make_pair
#include <vector>     // std::pair, std::make_pair
//#include "Experimental.generated.h"

/** Experimental - testing writing a single value into a Volume Texture using draw calls */
void WriteTo3DTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FIntVector Size,
	UVolumeTexture* inTexture);

/** Writes a single layer (along X axis) of a volume texture to a 2D texture.*/
void WriteVolumeTextureSlice_RenderThread(FRHICommandListImmediate& RHICmdList,
	UVolumeTexture* VolumeTexture,
	UTexture2D* WrittenSliceTexture, int Layer);

  /** 
  A class for a compute shader that creates higher-level mips on a volume texture.
  In our case, the higher mip will always just take the max of lower mips.
 */

class FMakeMaxMipsShader : public FGlobalShader {
  DECLARE_SHADER_TYPE(FMakeMaxMipsShader, Global)
public:
  static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) {
    return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
  }

  FMakeMaxMipsShader() : FGlobalShader() {}

  FMakeMaxMipsShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
    : FGlobalShader(Initializer) {
    // Volume texture + Transfer function uniforms
    VolumeLowMip.Bind(Initializer.ParameterMap, TEXT("VolumeLowMip"), SPF_Mandatory);
    VolumeHighMip.Bind(Initializer.ParameterMap, TEXT("VolumeHighMip"), SPF_Mandatory);
    TransferFunc.Bind(Initializer.ParameterMap, TEXT("TransferFunc"), SPF_Optional);
    UsingTF.Bind(Initializer.ParameterMap, TEXT("UsingTF"), SPF_Mandatory);
  }

  void SetResources(FRHICommandListImmediate& RHICmdList, FComputeShaderRHIParamRef ShaderRHI,
                    FUnorderedAccessViewRHIRef pVolumeLowMip,
                    FUnorderedAccessViewRHIRef pVolumeHighMip,
                    const FTexture2DRHIRef pTransferFunc = nullptr) {
    // Set the multiplier to -1 if we're removing the light. Set to 1 if adding it.
    SetUAVParameter(RHICmdList, ShaderRHI, VolumeLowMip, pVolumeLowMip);
    SetUAVParameter(RHICmdList, ShaderRHI, VolumeHighMip, pVolumeHighMip);

    SetShaderValue(RHICmdList, ShaderRHI, UsingTF, pTransferFunc != nullptr);
    if (pTransferFunc) {
      SetTextureParameter(RHICmdList, ShaderRHI, TransferFunc, pTransferFunc);
    }
  }

  void UnbindResources(FRHICommandListImmediate& RHICmdList, FComputeShaderRHIParamRef ShaderRHI) {
    // Unbind parent and also our added parameter.
    SetUAVParameter(RHICmdList, ShaderRHI, VolumeLowMip, FUnorderedAccessViewRHIParamRef());
    SetUAVParameter(RHICmdList, ShaderRHI, VolumeHighMip, FUnorderedAccessViewRHIParamRef());
    SetTextureParameter(RHICmdList, ShaderRHI, TransferFunc, FTexture2DRHIRef());
  }

  virtual bool Serialize(FArchive& Ar) override {
    bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
    Ar << VolumeLowMip << VolumeHighMip << TransferFunc << UsingTF;
    return bShaderHasOutdatedParameters;
  }

protected:
  // Tells the shader the pixel offset for reading from the previous loop's buffer
  FShaderResourceParameter VolumeLowMip;
  FShaderResourceParameter VolumeHighMip;

  FShaderResourceParameter TransferFunc;
  FShaderParameter UsingTF;
};

/**
	-- Experimental --
	A class for a shader that generates a distance field of a volume texture.
	This is used to accelerate raymarching, by having a separate volume containing distances
	to the nearest non-zero voxel.

	Implementation is primitive and takes ages.
*/
class FMakeDistanceFieldShader : public FGlobalShader {
  DECLARE_SHADER_TYPE(FMakeDistanceFieldShader, Global)
public:
  static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) {
    return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
  }

  FMakeDistanceFieldShader() : FGlobalShader() {}

  FMakeDistanceFieldShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
    : FGlobalShader(Initializer) {
    // Volume texture + Transfer function uniforms
    Volume.Bind(Initializer.ParameterMap, TEXT("Volume"), SPF_Mandatory);
    VolumeSampler.Bind(Initializer.ParameterMap, TEXT("VolumeSampler"), SPF_Mandatory);
    TransferFunc.Bind(Initializer.ParameterMap, TEXT("TransferFunc"), SPF_Mandatory);
    TransferFuncSampler.Bind(Initializer.ParameterMap, TEXT("TransferFuncSampler"), SPF_Mandatory);
    DistanceFieldVolume.Bind(Initializer.ParameterMap, TEXT("DistanceFieldVolume"), SPF_Mandatory);
    CuboidSize.Bind(Initializer.ParameterMap, TEXT("CuboidSize"), SPF_Mandatory);
    Threshold.Bind(Initializer.ParameterMap, TEXT("Threshold"), SPF_Mandatory);
    MaxDistance.Bind(Initializer.ParameterMap, TEXT("MaxDistance"), SPF_Mandatory);
    ZOffset.Bind(Initializer.ParameterMap, TEXT("ZOffset"), SPF_Mandatory);
  }

  void SetResources(FRHICommandListImmediate& RHICmdList, FComputeShaderRHIParamRef ShaderRHI,
                    FUnorderedAccessViewRHIRef pDistanceFieldVolume, FTexture3DRHIRef pVolume,
                    FTexture2DRHIRef pTransferFunc, FIntVector pCuboidSize, float pThreshold,
                    float pMaxDistance) {
    // Set the multiplier to -1 if we're removing the light. Set to 1 if adding it.
    SetUAVParameter(RHICmdList, ShaderRHI, DistanceFieldVolume, pDistanceFieldVolume);

    // Read the Volume and TF @ the closest point, do not interpolate
    FSamplerStateRHIParamRef SamplerRef =
        TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
    SetTextureParameter(RHICmdList, ShaderRHI, Volume, VolumeSampler, SamplerRef, pVolume);
    SetTextureParameter(RHICmdList, ShaderRHI, TransferFunc, TransferFuncSampler, SamplerRef,
                        pTransferFunc);

    SetShaderValue(RHICmdList, ShaderRHI, CuboidSize, pCuboidSize);
    SetShaderValue(RHICmdList, ShaderRHI, Threshold, pThreshold);
    SetShaderValue(RHICmdList, ShaderRHI, MaxDistance, pMaxDistance);
  }

  void SetZOffset(FRHICommandListImmediate& RHICmdList, FComputeShaderRHIParamRef ShaderRHI,
                  unsigned pZOffset) {
    SetShaderValue(RHICmdList, ShaderRHI, ZOffset, pZOffset);
  }

  void UnbindResources(FRHICommandListImmediate& RHICmdList, FComputeShaderRHIParamRef ShaderRHI) {
    // Unbind parent and also our added parameter.
    SetUAVParameter(RHICmdList, ShaderRHI, DistanceFieldVolume, FUnorderedAccessViewRHIParamRef());
    SetTextureParameter(RHICmdList, ShaderRHI, Volume, FTexture2DRHIRef());
    SetTextureParameter(RHICmdList, ShaderRHI, TransferFunc, FTexture2DRHIRef());
  }

  virtual bool Serialize(FArchive& Ar) override {
    bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
    Ar << Volume << VolumeSampler << TransferFunc << TransferFuncSampler << DistanceFieldVolume
       << CuboidSize << Threshold << MaxDistance << ZOffset;
    return bShaderHasOutdatedParameters;
  }

protected:
  FShaderResourceParameter Volume;
  FShaderResourceParameter VolumeSampler;

  FShaderResourceParameter TransferFunc;
  FShaderResourceParameter TransferFuncSampler;

  FShaderResourceParameter DistanceFieldVolume;

  FShaderParameter CuboidSize;
  FShaderParameter Threshold;
  FShaderParameter MaxDistance;
  FShaderParameter ZOffset;
};

// Shader used for fast drawing of a single layer from a volume texture to a 2D texture
class FWriteSliceToTextureShader : public FGlobalShader {
	DECLARE_SHADER_TYPE(FWriteSliceToTextureShader, Global)

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) {
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	FWriteSliceToTextureShader() {};

	FWriteSliceToTextureShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer) {
		Volume.Bind(Initializer.ParameterMap, TEXT("Volume"), SPF_Mandatory);
		TextureUAV.Bind(Initializer.ParameterMap, TEXT("TextureUAV"), SPF_Mandatory);
		Layer.Bind(Initializer.ParameterMap, TEXT("Layer"), SPF_Mandatory);
	}

	virtual void SetParameters(FRHICommandListImmediate& RHICmdList, FTexture3DRHIRef VolumeRef,
		FUnorderedAccessViewRHIParamRef TextureUAVRef, int pLayer) {
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		SetUAVParameter(RHICmdList, ShaderRHI, TextureUAV, TextureUAVRef);
		SetTextureParameter(RHICmdList, ShaderRHI, Volume, VolumeRef);
		SetShaderValue(RHICmdList, ShaderRHI, Layer, pLayer);
	}

	void UnbindUAV(FRHICommandList& RHICmdList) {
		SetUAVParameter(RHICmdList, GetComputeShader(), TextureUAV, FUnorderedAccessViewRHIParamRef());
	}

	virtual bool Serialize(FArchive& Ar) override {
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << Volume << TextureUAV << Layer;
		return bShaderHasOutdatedParameters;
	}

protected:
	// Float values to be set to the alpha volume.
	FShaderResourceParameter Volume;
	FShaderResourceParameter TextureUAV;
	FShaderParameter Layer;
};
