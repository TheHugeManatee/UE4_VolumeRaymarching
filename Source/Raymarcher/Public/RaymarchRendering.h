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
#include "RaymarchRendering.generated.h"

#define MY_LOG(x)                                                        \
  if (GEngine) {                                                         \
    GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TEXT(x)); \
  }

// USTRUCT for Directional light parameters.
USTRUCT(BlueprintType) struct FDirLightParameters {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadWrite, Category = "DirLightParameters") FVector LightDirection;
  UPROPERTY(BlueprintReadWrite, Category = "DirLightParameters") float LightIntensity;

  FDirLightParameters(FVector LightDir, float LightInt)
    : LightDirection(LightDir), LightIntensity(LightInt){};
  FDirLightParameters() : LightDirection(FVector(0, 0, 0)), LightIntensity(0){};
};

// USTRUCT for Clipping plane parameters.
USTRUCT(BlueprintType) struct FClippingPlaneParameters {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadWrite, Category = "ClippingPlaneParameters") FVector Center;
  UPROPERTY(BlueprintReadWrite, Category = "ClippingPlaneParameters") FVector Direction;

  FClippingPlaneParameters(FVector ClipCenter, FVector ClipDirection)
    : Center(ClipCenter), Direction(ClipDirection){};
  FClippingPlaneParameters() : Center(FVector(0, 0, 0)), Direction(FVector(0, 0, 0)){};
};

// Enumeration for how to handle parts of Transfer Function which are cut off.
UENUM(BlueprintType)
enum class FTransferFunctionCutoffMode : uint8 {
  TF_Clamp = 0,  // Fill with nearest value
  TF_Clear = 1   // Leave fully transparent
};

// Struct for transfer function range modification
// USTRUCT for Clipping plane parameters.
USTRUCT(BlueprintType) struct FTransferFunctionRangeParameters {
  GENERATED_BODY()

  // The range from which the TF will be sampled.
  UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "TransferFunctionRange")
  FVector2D IntensityDomain;
  // The range thet will be fully transparent - Relative to Intensity domain!
  // ( 0 to Cutoffs.X ) and ( Cutoffs.Y to 1) will be zero
  UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "TransferFunctionRange") FVector2D Cutoffs;
  UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "TransferFunctionRange")
  FTransferFunctionCutoffMode LowCutMode;
  UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "TransferFunctionRange")
  FTransferFunctionCutoffMode HighCutMode;

  FTransferFunctionRangeParameters(FVector2D IntensityDomain, FVector2D Cutoffs,
                                   FTransferFunctionCutoffMode LowCutMode,
                                   FTransferFunctionCutoffMode HighCutMode)
    : IntensityDomain(IntensityDomain)
    , Cutoffs(Cutoffs)
    , LowCutMode(LowCutMode)
    , HighCutMode(HighCutMode){};
  FTransferFunctionRangeParameters()
    : IntensityDomain(FVector2D(0, 1))
    , Cutoffs(FVector2D(0, 1))
    , LowCutMode(FTransferFunctionCutoffMode::TF_Clamp)
    , HighCutMode(FTransferFunctionCutoffMode::TF_Clamp){};
};

// A structure for 4 switchable read-write buffers. Used for one axis. Need 2 pairs for change-light
// shader.
struct OneAxisReadWriteBufferResources {
  FTexture2DRHIRef Buffers[4];
  FUnorderedAccessViewRHIRef UAVs[4];
};

/** A structure holding all resources related to a single raymarchable volume - its texture ref, the
   TF texture ref and TF Range parameters,
    light volume texture ref, and read-write buffers used for propagating along all axes. */
USTRUCT(BlueprintType) struct FBasicRaymarchRenderingResources {
  GENERATED_BODY()

  // Flag that these Rendering Resources have been initialized and can be used.
  UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Basic Raymarch Rendering Resources")
  bool isInitialized;

  UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Basic Raymarch Rendering Resources")
  UVolumeTexture* VolumeTextureRef;
  UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Basic Raymarch Rendering Resources")
  UTexture2D* TFTextureRef;
  UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Basic Raymarch Rendering Resources")
  UVolumeTexture* ALightVolumeRef;
  UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Basic Raymarch Rendering Resources")
  FTransferFunctionRangeParameters TFRangeParameters;
  UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Basic Raymarch Rendering Resources")
  bool LightVolumeHalfResolution;

  // Following is not visible in BPs.
  // Unordered access view to the Light Volume.
  FUnorderedAccessViewRHIRef ALightVolumeUAVRef;
  // Read-write buffers for all 3 major axes.
  OneAxisReadWriteBufferResources XYZReadWriteBuffers[3];
};

/** Structure containing the world parameters required for light propagation shaders - these include
  the volume's world transform and clipping plane parameters.
*/
USTRUCT(BlueprintType) struct FRaymarchWorldParameters {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadWrite, Category = "Raymarch Rendering World Parameters")
  FTransform VolumeTransform;
  UPROPERTY(BlueprintReadWrite, Category = "Raymarch Rendering World Parameters")
  FClippingPlaneParameters ClippingPlaneParameters;
};

// Enum for indexes for cube faces - used to discern axes for light propagation shader.
// Also used for deciding vectors provided into cutting plane material.
// The axis convention is - you are looking at the cube along positive Y axis in UE.
UENUM(BlueprintType)
enum class FCubeFace : uint8 {
  XPositive = 0,  // +X
  XNegative = 1,  // -X
  YPositive = 2,  // +Y
  YNegative = 3,  // -Y
  ZPositive = 4,  // +Z
  ZNegative = 5   // -Z
};

// Utility function to get sensible names from FCubeFace
static FString GetDirectionName(FCubeFace face) {
  switch (face) {
    case FCubeFace::XPositive: return FString("+X");
    case FCubeFace::XNegative: return FString("-X");
    case FCubeFace::YPositive: return FString("+Y");
    case FCubeFace::YNegative: return FString("-Y");
    case FCubeFace::ZPositive: return FString("+Z");
    case FCubeFace::ZNegative: return FString("-Z");
    default: return FString("Something went wrong here!");
  }
}

// Normals of corresponding cube faces in object-space.
const FVector FCubeFaceNormals[6] = {
    {1.0, 0.0, 0.0},   // +x
    {-1.0, 0.0, 0.0},  // -x
    {0.0, 1.0, 0.0},   // +y
    {0.0, -1.0, 0.0},  // -y
    {0.0, 0.0, 1.0},   // +z
    {0.0, 0.0, -1.0}   // -z
};

/** Structure corresponding to the 3 major axes to propagate a light-source along with their
   respective weights. */
struct FMajorAxes {
  // The 3 major axes indexes
  std::vector<std::pair<FCubeFace, float>> FaceWeight;
};

// Comparison for a Face-weight pair, to sort in Descending order.
static bool SortDescendingWeights(const std::pair<FCubeFace, float>& a,
                                  const std::pair<FCubeFace, float>& b) {
  return (a.second > b.second);
};

/* Returns the weighted major axes to propagate along to add/remove a light to/from a lightvolume
   (LightPos must be converted to object-space). */
static FMajorAxes GetMajorAxes(FVector LightPos) {
  FMajorAxes RetVal;
  std::vector<std::pair<FCubeFace, float>> faceVector;

  for (int i = 0; i < 6; i++) {
    // Dot of position and face normal yields cos(angle)
    float weight = FVector::DotProduct(FCubeFaceNormals[i], LightPos);

    // Dot^2 for non-negative faces will always sum up to 1.
    weight = (weight > 0 ? weight * weight : 0);
    RetVal.FaceWeight.push_back(std::make_pair(FCubeFace(i), weight));
  }
  // Sort so that the 3 major axes are the first.
  std::sort(RetVal.FaceWeight.begin(), RetVal.FaceWeight.end(), SortDescendingWeights);
  return RetVal;
};

// Pixel shader for writing to a Volume Texture.
class FVolumePS : public FGlobalShader {
  DECLARE_SHADER_TYPE(FVolumePS, Global);

public:
  static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) {
    return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
  }

  FVolumePS() {}

  FVolumePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
    : FGlobalShader(Initializer) {}

  virtual bool Serialize(FArchive& Ar) override {
    bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
    return bShaderHasOutdatedParameters;
  }
};

FSamplerStateRHIRef GetBufferSamplerRef(uint32 BorderColorInt);

uint32 GetBorderColorIntSingle(FDirLightParameters LightParams, FMajorAxes MajorAxes,
                               unsigned index);

void AddDirLightToSingleLightVolume_RenderThread(FRHICommandListImmediate& RHICmdList,
                                                 FBasicRaymarchRenderingResources Resources,
                                                 const FDirLightParameters LightParameters,
                                                 const bool Added,
                                                 const FRaymarchWorldParameters WorldParameters);

void ChangeDirLightInSingleLightVolume_RenderThread(FRHICommandListImmediate& RHICmdList,
                                                    FBasicRaymarchRenderingResources Resources,
                                                    const FDirLightParameters OldLightParameters,
                                                    const FDirLightParameters NewLightParameters,
                                                    const FRaymarchWorldParameters WorldParameters);

void ClearVolumeTexture_RenderThread(FRHICommandListImmediate& RHICmdList,
                                     FRHITexture3D* ALightVolumeResource, float ClearValue);

void GenerateVolumeTextureMipLevels_RenderThread(FRHICommandListImmediate& RHICmdList,
                                                 FIntVector Dimensions,
                                                 FRHITexture3D* VolumeResource,
                                                 FRHITexture2D* TransferFunc);

void GenerateDistanceField_RenderThread(FRHICommandListImmediate& RHICmdList, FIntVector Dimensions,
                                        FRHITexture3D* VolumeResource, FRHITexture2D* TransferFunc,
                                        FRHITexture3D* DistanceFieldResource,
                                        float localSphereDiameter, float threshold);

// Compute Shader used for fast clearing of RW volume textures.
class FClearVolumeTextureShader : public FGlobalShader {
  DECLARE_SHADER_TYPE(FClearVolumeTextureShader, Global)

public:
  static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) {
    return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
  }

  FClearVolumeTextureShader(){};

  FClearVolumeTextureShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
    : FGlobalShader(Initializer) {
    Volume.Bind(Initializer.ParameterMap, TEXT("Volume"), SPF_Mandatory);
    ClearValue.Bind(Initializer.ParameterMap, TEXT("ClearValue"), SPF_Mandatory);
    ZSize.Bind(Initializer.ParameterMap, TEXT("ZSize"), SPF_Mandatory);
  }

  virtual void SetParameters(FRHICommandListImmediate& RHICmdList,
                             FUnorderedAccessViewRHIParamRef VolumeRef, float clearColor,
                             int ZSizeParam) {
    FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
    SetUAVParameter(RHICmdList, ShaderRHI, Volume, VolumeRef);
    SetShaderValue(RHICmdList, ShaderRHI, ClearValue, clearColor);
    SetShaderValue(RHICmdList, ShaderRHI, ZSize, ZSizeParam);
  }

  void UnbindUAV(FRHICommandList& RHICmdList) {
    SetUAVParameter(RHICmdList, GetComputeShader(), Volume, FUnorderedAccessViewRHIParamRef());
  }

  virtual bool Serialize(FArchive& Ar) override {
    bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
    Ar << Volume << ClearValue << ZSize;
    return bShaderHasOutdatedParameters;
  }

protected:
  // Float values to be set to the alpha volume.
  FShaderResourceParameter Volume;
  FShaderParameter ClearValue;
  FShaderParameter ZSize;
};

// Compute shader for clearing a single-channel 2D float RW texture
class FClearFloatRWTextureCS : public FGlobalShader {
  DECLARE_SHADER_TYPE(FClearFloatRWTextureCS, Global);

public:
  FClearFloatRWTextureCS() {}
  FClearFloatRWTextureCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
    : FGlobalShader(Initializer) {
    ClearValue.Bind(Initializer.ParameterMap, TEXT("ClearValue"), SPF_Mandatory);
    ClearTexture2DRW.Bind(Initializer.ParameterMap, TEXT("ClearTextureRW"), SPF_Mandatory);
  }

  // FShader interface.
  virtual bool Serialize(FArchive& Ar) override {
    bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
    Ar << ClearValue << ClearTexture2DRW;
    return bShaderHasOutdatedParameters;
  }

  void SetParameters(FRHICommandList& RHICmdList, FUnorderedAccessViewRHIParamRef TextureRW,
                     float Value) {
    SetUAVParameter(RHICmdList, GetComputeShader(), ClearTexture2DRW, TextureRW);
    SetShaderValue(RHICmdList, GetComputeShader(), ClearValue, Value);
  }

  void UnbindUAV(FRHICommandList& RHICmdList) {
    SetUAVParameter(RHICmdList, GetComputeShader(), ClearTexture2DRW,
                    FUnorderedAccessViewRHIParamRef());
  }

  static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) {
    return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
  }

  const FShaderParameter& GetClearColorParameter() { return ClearValue; }

  const FShaderResourceParameter& GetClearTextureRWParameter() { return ClearTexture2DRW; }

protected:
  FShaderResourceParameter ClearTexture2DRW;
  FShaderParameter ClearValue;
};

//
// Shaders for illumination propagation follow.
//

// Parent shader to any shader working with a raymarched volume.
// Contains Volume, Transfer Function, TF intensity domain, Clipping Parameters and StepSize.
// StepSize is needed so that we know how far through the volume we went in each step
// so we can properly calculate the opacity.
class FRaymarchVolumeShader : public FGlobalShader {
public:
  FRaymarchVolumeShader() : FGlobalShader() {}

  FRaymarchVolumeShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
    : FGlobalShader(Initializer) {
    Volume.Bind(Initializer.ParameterMap, TEXT("Volume"), SPF_Mandatory);
    VolumeSampler.Bind(Initializer.ParameterMap, TEXT("VolumeSampler"), SPF_Mandatory);

    TransferFunc.Bind(Initializer.ParameterMap, TEXT("TransferFunc"), SPF_Mandatory);
    TransferFuncSampler.Bind(Initializer.ParameterMap, TEXT("TransferFuncSampler"), SPF_Mandatory);
    LocalClippingCenter.Bind(Initializer.ParameterMap, TEXT("LocalClippingCenter"), SPF_Mandatory);
    LocalClippingDirection.Bind(Initializer.ParameterMap, TEXT("LocalClippingDirection"),
                                SPF_Mandatory);

    TFIntensityDomain.Bind(Initializer.ParameterMap, TEXT("TFIntensityDomain"), SPF_Mandatory);
    StepSize.Bind(Initializer.ParameterMap, TEXT("StepSize"), SPF_Mandatory);
  }

  void SetRaymarchResources(FRHICommandListImmediate& RHICmdList,
                            FComputeShaderRHIParamRef ShaderRHI, const FTexture3DRHIRef pVolume,
                            const FTexture2DRHIRef pTransferFunc) {
    // Create a static sampler reference and bind it together with the volume texture + TF.

    FLinearColor VolumeClearColor = FLinearColor(0.0, 0.0, 0.0, 0.0);
    const uint32 BorderColorInt = VolumeClearColor.ToFColor(false).ToPackedARGB();

    FSamplerStateRHIRef SamplerRef = RHICreateSamplerState(FSamplerStateInitializerRHI(
        SF_Trilinear, AM_Border, AM_Border, AM_Border, 0, 1, 0, 0, BorderColorInt));

    FSamplerStateRHIRef
        TFSamplerRef =  // GetBufferSamplerRef(VolumeClearColor.ToFColor(true).ToPackedARGB());
        TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
    SetTextureParameter(RHICmdList, ShaderRHI, Volume, VolumeSampler, SamplerRef, pVolume);
    SetTextureParameter(RHICmdList, ShaderRHI, TransferFunc, TransferFuncSampler, TFSamplerRef,
                        pTransferFunc);
  }

  virtual void UnbindResources(FRHICommandListImmediate& RHICmdList,
                               FComputeShaderRHIParamRef ShaderRHI) {
    SetTextureParameter(RHICmdList, ShaderRHI, Volume, FTextureRHIParamRef());
    SetTextureParameter(RHICmdList, ShaderRHI, TransferFunc, FTextureRHIParamRef());
  }

  // Sets the shader uniforms in the pipeline.
  void SetRaymarchParameters(FRHICommandListImmediate& RHICmdList,
                             FComputeShaderRHIParamRef ShaderRHI,
                             FClippingPlaneParameters LocalClippingParams, FVector2D TFDomain) {
    SetShaderValue(RHICmdList, ShaderRHI, LocalClippingCenter, LocalClippingParams.Center);
    SetShaderValue(RHICmdList, ShaderRHI, LocalClippingDirection, LocalClippingParams.Direction);
    SetShaderValue(RHICmdList, ShaderRHI, TFIntensityDomain, TFDomain);
  }

  void SetStepSize(FRHICommandListImmediate& RHICmdList, FComputeShaderRHIParamRef ShaderRHI,
                   float pStepSize) {
    SetShaderValue(RHICmdList, ShaderRHI, StepSize, pStepSize);
  }

  virtual bool Serialize(FArchive& Ar) override {
    bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
    Ar << Volume << VolumeSampler << TransferFunc << TransferFuncSampler << LocalClippingCenter
       << LocalClippingDirection << TFIntensityDomain << StepSize;
    return bShaderHasOutdatedParameters;
  }

protected:
  // Volume texture + transfer function resource parameters
  FShaderResourceParameter Volume;
  FShaderResourceParameter VolumeSampler;
  FShaderResourceParameter TransferFunc;
  FShaderResourceParameter TransferFuncSampler;
  // Clipping uniforms
  FShaderParameter LocalClippingCenter;
  FShaderParameter LocalClippingDirection;
  // TF intensity Domain
  FShaderParameter TFIntensityDomain;
  // Step size taken each iteration
  FShaderParameter StepSize;
};

// Parent Shader to shaders for propagating light as described by Sund�n and Ropinski.
// @cite - https://ieeexplore.ieee.org/abstract/document/7156382
//
// The shader only works on one layer of the Volume Texture at once. That's why we need
// the Loop variable and Permutation Matrix.
// See AddDirLightShader.usf and AddDirLightToSingleLightVolume_RenderThread
class FLightPropagationShader : public FRaymarchVolumeShader {
public:
  FLightPropagationShader() : FRaymarchVolumeShader() {}

  FLightPropagationShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
    : FRaymarchVolumeShader(Initializer) {
    Loop.Bind(Initializer.ParameterMap, TEXT("Loop"), SPF_Mandatory);
    PermutationMatrix.Bind(Initializer.ParameterMap, TEXT("PermutationMatrix"), SPF_Mandatory);

    // Read buffer and sampler.
    ReadBuffer.Bind(Initializer.ParameterMap, TEXT("ReadBuffer"), SPF_Mandatory);
    ReadBufferSampler.Bind(Initializer.ParameterMap, TEXT("ReadBufferSampler"), SPF_Mandatory);
    // Write buffer.
    WriteBuffer.Bind(Initializer.ParameterMap, TEXT("WriteBuffer"), SPF_Mandatory);
    // Actual light volume
    ALightVolume.Bind(Initializer.ParameterMap, TEXT("ALightVolume"), SPF_Mandatory);
  }

  // Sets loop-dependent uniforms in the pipeline.
  void SetLoop(FRHICommandListImmediate& RHICmdList, FComputeShaderRHIParamRef ShaderRHI,
               const unsigned loopIndex, const FTexture2DRHIRef pReadBuffer,
               const FSamplerStateRHIRef pReadBuffSampler,
               const FUnorderedAccessViewRHIRef pWriteBuffer) {
    // Update the Loop index.
    SetShaderValue(RHICmdList, ShaderRHI, Loop, loopIndex);
    // Set read/write buffers.
    SetUAVParameter(RHICmdList, ShaderRHI, WriteBuffer, pWriteBuffer);
    SetTextureParameter(RHICmdList, ShaderRHI, ReadBuffer, ReadBufferSampler, pReadBuffSampler,
                        pReadBuffer);
  }

  void SetALightVolume(FRHICommandListImmediate& RHICmdList, FComputeShaderRHIParamRef ShaderRHI,
                       FUnorderedAccessViewRHIRef pALightVolume) {
    // Set the multiplier to -1 if we're removing the light. Set to 1 if adding it.
    SetUAVParameter(RHICmdList, ShaderRHI, ALightVolume, pALightVolume);
  }

  void SetPermutationMatrix(FRHICommandListImmediate& RHICmdList,
                            FComputeShaderRHIParamRef ShaderRHI, FMatrix PermMatrix) {
    SetShaderValue(RHICmdList, ShaderRHI, PermutationMatrix, PermMatrix);
  }

  virtual void UnbindResources(FRHICommandListImmediate& RHICmdList,
                               FComputeShaderRHIParamRef ShaderRHI) override {
    // Unbind volume buffer.
    FRaymarchVolumeShader::UnbindResources(RHICmdList, ShaderRHI);
    SetUAVParameter(RHICmdList, ShaderRHI, ALightVolume, FUnorderedAccessViewRHIParamRef());
    SetUAVParameter(RHICmdList, ShaderRHI, WriteBuffer, FUnorderedAccessViewRHIParamRef());
    SetTextureParameter(RHICmdList, ShaderRHI, ReadBuffer, FTextureRHIParamRef());
  }

  virtual bool Serialize(FArchive& Ar) override {
    bool bShaderHasOutdatedParameters = FRaymarchVolumeShader::Serialize(Ar);
    Ar << Loop << PermutationMatrix << ReadBuffer << ReadBufferSampler << WriteBuffer
       << ALightVolume;
    ;
    return bShaderHasOutdatedParameters;
  }

protected:
  // Volume texture + transfer function resource parameters
  FShaderParameter Loop;
  FShaderParameter PermutationMatrix;
  // Read buffer texture and sampler.
  FShaderResourceParameter ReadBuffer;
  FShaderResourceParameter ReadBufferSampler;
  // Write buffer UAV.
  FShaderResourceParameter WriteBuffer;
  // Light volume to modify.
  FShaderResourceParameter ALightVolume;
};

// A shader implementing directional light propagation.
// Originally, spot and cone lights were supposed to be also supported, but other things were more
// important.
class FDirLightPropagationShader : public FLightPropagationShader {
public:
  FDirLightPropagationShader() : FLightPropagationShader() {}

  FDirLightPropagationShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
    : FLightPropagationShader(Initializer) {
    // Volume texture + Transfer function uniforms
    PrevPixelOffset.Bind(Initializer.ParameterMap, TEXT("PrevPixelOffset"), SPF_Mandatory);
    UVWOffset.Bind(Initializer.ParameterMap, TEXT("UVWOffset"), SPF_Mandatory);
  }

  void SetUVOffset(FRHICommandListImmediate& RHICmdList, FComputeShaderRHIParamRef ShaderRHI,
                   FVector2D PixelOffset) {
    SetShaderValue(RHICmdList, ShaderRHI, PrevPixelOffset, PixelOffset);
  }

  void SetUVWOffset(FRHICommandListImmediate& RHICmdList, FComputeShaderRHIParamRef ShaderRHI,
                    FVector pUVWOffset) {
    SetShaderValue(RHICmdList, ShaderRHI, UVWOffset, pUVWOffset);
  }

  virtual bool Serialize(FArchive& Ar) override {
    bool bShaderHasOutdatedParameters = FLightPropagationShader::Serialize(Ar);
    Ar << PrevPixelOffset << UVWOffset;
    return bShaderHasOutdatedParameters;
  }

protected:
  // Tells the shader the pixel offset for reading from the previous loop's buffer
  FShaderParameter PrevPixelOffset;
  // And the offset in the volume from the previous volume sample.
  FShaderParameter UVWOffset;
};

// A shader implementing adding or removing a single directional light.
// (As opposed to changing [e.g. add and remove at the same time] a directional light)
// Only adds the bAdded boolean for toggling adding/removing a light.
// Notice the UE macro DECLARE_SHADER_TYPE, unlike the shaders above (which are abstract)
// this one actually gets implemented.
class FAddDirLightShader : public FDirLightPropagationShader {
  DECLARE_SHADER_TYPE(FAddDirLightShader, Global)
public:
  FAddDirLightShader() : FDirLightPropagationShader() {}

  FAddDirLightShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
    : FDirLightPropagationShader(Initializer) {
    // Volume texture + Transfer function uniforms
    bAdded.Bind(Initializer.ParameterMap, TEXT("bAdded"), SPF_Mandatory);
  }

  static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) {
    return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
  }

  void SetLightAdded(FRHICommandListImmediate& RHICmdList, FComputeShaderRHIParamRef ShaderRHI,
                     bool bLightAdded) {
    // Set the multiplier to -1 if we're removing the light. Set to 1 if adding it.
    SetShaderValue(RHICmdList, ShaderRHI, bAdded, bLightAdded ? 1 : -1);
  }

  virtual bool Serialize(FArchive& Ar) override {
    bool bShaderHasOutdatedParameters = FDirLightPropagationShader::Serialize(Ar);
    Ar << bAdded;
    return bShaderHasOutdatedParameters;
  }

protected:
  // Tells the shader the pixel offset for reading from the previous loop's buffer
  FShaderParameter bAdded;
};

// A shader implementing changing a light in one pass.
// Works by subtracting the old light and adding the new one.
// Notice the UE macro DECLARE_SHADER_TYPE, unlike the shaders above (which are abstract)
// this one actually gets implemented.
class FChangeDirLightShader : public FDirLightPropagationShader {
  DECLARE_SHADER_TYPE(FChangeDirLightShader, Global)

public:
  FChangeDirLightShader() : FDirLightPropagationShader() {}

  FChangeDirLightShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
    : FDirLightPropagationShader(Initializer) {
    // Volume texture + Transfer function uniforms
    RemovedPrevPixelOffset.Bind(Initializer.ParameterMap, TEXT("RemovedPrevPixelOffset"),
                                SPF_Mandatory);
    RemovedReadBuffer.Bind(Initializer.ParameterMap, TEXT("RemovedReadBuffer"), SPF_Mandatory);
    RemovedReadBufferSampler.Bind(Initializer.ParameterMap, TEXT("RemovedReadBufferSampler"),
                                  SPF_Mandatory);
    RemovedWriteBuffer.Bind(Initializer.ParameterMap, TEXT("RemovedWriteBuffer"), SPF_Mandatory);
    RemovedUVWOffset.Bind(Initializer.ParameterMap, TEXT("RemovedUVWOffset"), SPF_Mandatory);
    RemovedStepSize.Bind(Initializer.ParameterMap, TEXT("RemovedStepSize"), SPF_Mandatory);
  }

  static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) {
    return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
  }

  // Sets loop-dependent uniforms in the pipeline.
  void SetLoop(FRHICommandListImmediate& RHICmdList, FComputeShaderRHIParamRef ShaderRHI,
               const unsigned loopIndex, const FTexture2DRHIRef pRemovedReadBuffer,
               const FSamplerStateRHIRef pRemovedReadBuffSampler,
               const FUnorderedAccessViewRHIRef pRemovedWriteBuffer,
               const FTexture2DRHIRef pAddedReadBuffer,
               const FSamplerStateRHIRef pAddedReadBuffSampler,
               const FUnorderedAccessViewRHIRef pAddedWriteBuffer) {
    // Actually sets the shader uniforms in the pipeline.
    FLightPropagationShader::SetLoop(RHICmdList, ShaderRHI, loopIndex, pAddedReadBuffer,
                                     pAddedReadBuffSampler, pAddedWriteBuffer);
    // Set read/write buffers for removed light.
    SetUAVParameter(RHICmdList, ShaderRHI, RemovedWriteBuffer, pRemovedWriteBuffer);
    SetTextureParameter(RHICmdList, ShaderRHI, RemovedReadBuffer, RemovedReadBufferSampler,
                        pRemovedReadBuffSampler, pRemovedReadBuffer);
  }

  void SetPixelOffsets(FRHICommandListImmediate& RHICmdList, FComputeShaderRHIParamRef ShaderRHI,
                       FVector2D AddedPixelOffset, FVector2D RemovedPixelOffset) {
    SetShaderValue(RHICmdList, ShaderRHI, PrevPixelOffset, AddedPixelOffset);
    SetShaderValue(RHICmdList, ShaderRHI, RemovedPrevPixelOffset, RemovedPixelOffset);
  }

  void SetUVWOffsets(FRHICommandListImmediate& RHICmdList, FComputeShaderRHIParamRef ShaderRHI,
                     FVector pAddedUVWOffset, FVector pRemovedUVWOffset) {
    SetShaderValue(RHICmdList, ShaderRHI, UVWOffset, pAddedUVWOffset);
    SetShaderValue(RHICmdList, ShaderRHI, RemovedUVWOffset, pRemovedUVWOffset);
  }

  void SetStepSizes(FRHICommandListImmediate& RHICmdList, FComputeShaderRHIParamRef ShaderRHI,
                    float pAddedStepSize, float pRemovedStepSize) {
    SetShaderValue(RHICmdList, ShaderRHI, StepSize, pAddedStepSize);
    SetShaderValue(RHICmdList, ShaderRHI, RemovedStepSize, pRemovedStepSize);
  }

  virtual void UnbindResources(FRHICommandListImmediate& RHICmdList,
                               FComputeShaderRHIParamRef ShaderRHI) override {
    // Unbind parent and also our added parameters.
    FDirLightPropagationShader::UnbindResources(RHICmdList, ShaderRHI);
    SetUAVParameter(RHICmdList, ShaderRHI, RemovedWriteBuffer, FUnorderedAccessViewRHIParamRef());
    SetTextureParameter(RHICmdList, ShaderRHI, RemovedReadBuffer, FTextureRHIParamRef());
  }

  virtual bool Serialize(FArchive& Ar) override {
    bool bShaderHasOutdatedParameters = FDirLightPropagationShader::Serialize(Ar);
    Ar << RemovedPrevPixelOffset << RemovedReadBuffer << RemovedReadBufferSampler
       << RemovedWriteBuffer << RemovedStepSize << RemovedUVWOffset;
    return bShaderHasOutdatedParameters;
  }

protected:
  // Same collection of parameters as for a "add dir light" shader, but these ones are the ones of
  // the removed light.

  // Tells the shader the pixel offset for reading from the previous loop's buffer
  FShaderParameter RemovedPrevPixelOffset;

  FShaderResourceParameter RemovedReadBuffer;
  FShaderResourceParameter RemovedReadBufferSampler;
  // Write buffer UAV.
  FShaderResourceParameter RemovedWriteBuffer;
  // Removed light step size (is different than added one's)
  FShaderParameter RemovedStepSize;
  // Removed light UVW offset
  FShaderParameter RemovedUVWOffset;
};
