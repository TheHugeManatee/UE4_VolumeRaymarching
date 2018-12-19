// (C) Technical University of Munich - Computer Aided Medical Procedures
// Developed by Tomas Bartipan (tomas.bartipan@tum.de)

#pragma once

#include "CoreMinimal.h"

#include "Classes/Engine/TextureRenderTarget2D.h"
#include "Classes/Engine/VolumeTexture.h"
#include "Classes/Engine/World.h"
#include "Classes/Kismet/BlueprintFunctionLibrary.h"
#include "Engine.h"
#include "Public/GlobalShader.h"
#include "Public/Internationalization/Internationalization.h"
#include "Public/Logging/MessageLog.h"
#include "Public/PipelineStateCache.h"
#include "Public/RHIStaticStates.h"
#include "Public/SceneInterface.h"
#include "Public/SceneUtils.h"
#include "Public/ShaderParameterUtils.h"
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
  UPROPERTY(BlueprintReadWrite, Category = "DirLightParameters") FVector LightColor;
  UPROPERTY(BlueprintReadWrite, Category = "DirLightParameters") float LightIntensity;

  FDirLightParameters(FVector LightDir, FVector LightCol, float LightInt)
    : LightDirection(LightDir), LightColor(LightCol), LightIntensity(LightInt){};
  FDirLightParameters()
    : LightDirection(FVector(0, 0, 0)), LightColor(FVector(0, 0, 0)), LightIntensity(0){};
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
  UPROPERTY(BlueprintReadWrite, Category = "TransferFunctionRange") FVector2D IntensityDomain;
  // The range thet will be fully transparent - Relative to Intensity domain!
  // ( 0 to Cutoffs.X ) and ( Cutoffs.Y to 1) will be zero
  UPROPERTY(BlueprintReadWrite, Category = "TransferFunctionRange") FVector2D Cutoffs;
  UPROPERTY(BlueprintReadWrite, Category = "TransferFunctionRange")
  FTransferFunctionCutoffMode LowCutMode;
  UPROPERTY(BlueprintReadWrite, Category = "TransferFunctionRange")
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

// A structure for 2 switchable read-write buffers. Used for one axis.
struct OneAxisReadWriteBufferResources {
  FTextureRHIRef Texture1;
  FTextureRHIRef Texture2;
  FUnorderedAccessViewRHIParamRef Texture1UAV;
  FUnorderedAccessViewRHIParamRef Texture2UAV;
};

USTRUCT(BlueprintType) struct FBasicRaymarchRenderingResources {
  GENERATED_BODY()

  // Flag that these Rendering Resources have been initialized and can be used.
  UPROPERTY(BlueprintReadOnly, Category = "Basic Raymarch Rendering Resources") bool isInitialized;
  UPROPERTY(BlueprintReadWrite, Category = "Basic Raymarch Rendering Resources") UVolumeTexture* VolumeTextureRef;
  UPROPERTY(BlueprintReadWrite, Category = "Basic Raymarch Rendering Resources") UTexture2D* TFTextureRef;
  UPROPERTY(BlueprintReadWrite, Category = "Basic Raymarch Rendering Resources") UVolumeTexture* ALightVolumeRef;
  UPROPERTY(BlueprintReadWrite, Category = "Basic Raymarch Rendering Resources") FTransferFunctionRangeParameters TFRangeParameters;
  // Not visible in BPs.
  FUnorderedAccessViewRHIParamRef ALightVolumeUAVRef;
  OneAxisReadWriteBufferResources XYZReadWriteBuffers[3];
};

USTRUCT(BlueprintType) struct FRaymarchWorldParameters {
  GENERATED_BODY()

	  UPROPERTY(BlueprintReadWrite, Category = "Raymarch Rendering World Parameters") FTransform VolumeTransform;
  UPROPERTY(BlueprintReadWrite, Category = "Raymarch Rendering World Parameters") FClippingPlaneParameters ClippingPlaneParameters;
  UPROPERTY(BlueprintReadWrite, Category = "Raymarch Rendering World Parameters") FVector MeshMaxBounds;

};


USTRUCT(BlueprintType) struct FColorVolumesResources {
  GENERATED_BODY()

  // Flag that these Rendering Resources have been initialized and can be used.
  UPROPERTY(BlueprintReadOnly, Category = "Colored Lights Raymarch Rendering") bool isInitialized;
  UVolumeTexture* RLightVolumeRef;
  UVolumeTexture* GLightVolumeRef;
  UVolumeTexture* BLightVolumeRef;
  FUnorderedAccessViewRHIParamRef RLightVolumeUAVRef;
  FUnorderedAccessViewRHIParamRef GLightVolumeUAVRef;
  FUnorderedAccessViewRHIParamRef BLightVolumeUAVRef;
  // Read/Write buffer structs for going along X,Y and Z axes.
};


// Enum for indexes for cube faces - used to discern axes for light propagation shader.
enum FCubeFace : int {
  Right = 1,  // +X
  Left = 2,   // -X
  Front = 3,  // +Y
  Back = 4,   // -Y
  Top = 5,    // +Z
  Bottom = 6  // -Z
};

// Normals of corresponding cube faces in object-space.
const FVector FCubeFaceNormals[6] = {
    {1.0, 0.0, 0.0},   // right
    {-1.0, 0.0, 0.0},  // left
    {0.0, 1.0, 0.0},   // front
    {0.0, -1.0, 0.0},  // back
    {0.0, 0.0, 1.0},   // top
    {0.0, 0.0, -1.0}   // bottom
};

// Struct corresponding to the 3 major axes to propagate a light-source along with their respective
// weights.
struct FMajorAxes {
  // The 3 major axes indexes
  std::vector<std::pair<FCubeFace, float>> FaceWeight;
};

// Comparison for a Face-weight pair, to sort in Descending order.
static bool SortDescendingWeights(const std::pair<FCubeFace, float>& a,
                                  const std::pair<FCubeFace, float>& b) {
  return (a.second > b.second);
};

// Returns the weighted major axes to propagate along to add/remove a light to/from a lightvolume
// (LightPos must be converted to object-space).
static FMajorAxes GetMajorAxes(FVector LightPos) {
  FMajorAxes RetVal;
  std::vector<std::pair<FCubeFace, float>> faceVector;

  for (int i = 0; i < 6; i++) {
    // Dot of position and face normal yields cos(angle)
    float weight = FVector::DotProduct(FCubeFaceNormals[i], LightPos);

    // Dot^2 for non-negative faces will always sum up to 1.
    weight = (weight > 0 ? weight * weight : 0);
    RetVal.FaceWeight.push_back(std::make_pair(FCubeFace(i), weight));

    /*float dot = FVector::DotProduct(FCubeFaceNormals[i], LightPos);
    float weight = 1 - (2 * acos(dot) / PI);
    RetVal.FaceWeight.push_back(std::make_pair(FCubeFace(i), weight));*/
  }
  // Sort so that the 3 major axes are the first.
  std::sort(RetVal.FaceWeight.begin(), RetVal.FaceWeight.end(), SortDescendingWeights);
  return RetVal;
};

// Parent shader for any shader that works on Light Volumes. Provides a way to bind and unbind the 4
// light volumes.
class FGenericLightVolumeShader : public FGlobalShader {
public:
  static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) {
    return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
  }

  FGenericLightVolumeShader(){};

  FGenericLightVolumeShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
    : FGlobalShader(Initializer) {
    // Bind Light Volume resource parameters
    RLightVolume.Bind(Initializer.ParameterMap, TEXT("RLightVolume"));
    GLightVolume.Bind(Initializer.ParameterMap, TEXT("GLightVolume"));
    BLightVolume.Bind(Initializer.ParameterMap, TEXT("BLightVolume"));
    ALightVolume.Bind(Initializer.ParameterMap, TEXT("ALightVolume"));
  }

  virtual void SetLightVolumeUAVs(FRHICommandListImmediate& RHICmdList,
                                  FComputeShaderRHIParamRef ShaderRHI,
                                  const FUnorderedAccessViewRHIParamRef* pVolumesArray) {
    // Set the UAV parameter for the light volume
    SetUAVParameter(RHICmdList, ShaderRHI, RLightVolume, pVolumesArray[0]);
    SetUAVParameter(RHICmdList, ShaderRHI, GLightVolume, pVolumesArray[1]);
    SetUAVParameter(RHICmdList, ShaderRHI, BLightVolume, pVolumesArray[2]);
    SetUAVParameter(RHICmdList, ShaderRHI, ALightVolume, pVolumesArray[3]);
  }

  virtual void SetLightVolumeUAVs(FRHICommandListImmediate& RHICmdList,
                                  const FUnorderedAccessViewRHIParamRef* pVolumesArray) {
    SetLightVolumeUAVs(RHICmdList, GetComputeShader(), pVolumesArray);
  }

  virtual void UnbindLightVolumeUAVs(FRHICommandListImmediate& RHICmdList,
                                     FComputeShaderRHIParamRef ShaderRHI) {
    // Unbind the UAVs.
    SetUAVParameter(RHICmdList, ShaderRHI, RLightVolume, FUnorderedAccessViewRHIParamRef());
    SetUAVParameter(RHICmdList, ShaderRHI, GLightVolume, FUnorderedAccessViewRHIParamRef());
    SetUAVParameter(RHICmdList, ShaderRHI, BLightVolume, FUnorderedAccessViewRHIParamRef());
    SetUAVParameter(RHICmdList, ShaderRHI, ALightVolume, FUnorderedAccessViewRHIParamRef());
  }

  virtual void UnbindLightVolumeUAVs(FRHICommandListImmediate& RHICmdList) {
    UnbindLightVolumeUAVs(RHICmdList, GetComputeShader());
  }

  bool Serialize(FArchive& Ar) {
    bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
    Ar << RLightVolume << GLightVolume << BLightVolume << ALightVolume;
    return bShaderHasOutdatedParameters;
  }

protected:
  // In/Out light volume.
  FShaderResourceParameter RLightVolume;
  FShaderResourceParameter GLightVolume;
  FShaderResourceParameter BLightVolume;
  FShaderResourceParameter ALightVolume;
};

// Shader used for fast resetting of light volumes. Just fills zeros everywhere.
class FClearLightVolumesShader : public FGenericLightVolumeShader {
  DECLARE_SHADER_TYPE(FClearLightVolumesShader, Global)

public:
  static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) {
    return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
  }

  FClearLightVolumesShader(){};

  // Don't need any more bindings than the 4 volumes from generic shader, so just call parent
  // constructor.
  FClearLightVolumesShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
    : FGenericLightVolumeShader(Initializer) {
    RClearValue.Bind(Initializer.ParameterMap, TEXT("RClearValue"));
    GClearValue.Bind(Initializer.ParameterMap, TEXT("GClearValue"));
    BClearValue.Bind(Initializer.ParameterMap, TEXT("BClearValue"));
    AClearValue.Bind(Initializer.ParameterMap, TEXT("AClearValue"));
    ZSize.Bind(Initializer.ParameterMap, TEXT("ZSize"));
  }

  virtual void SetParameters(FRHICommandListImmediate& RHICmdList, FVector4 ClearParams,
                             int ZSizeParam) {
    FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
    SetShaderValue(RHICmdList, ShaderRHI, RClearValue, ClearParams.X);
    SetShaderValue(RHICmdList, ShaderRHI, GClearValue, ClearParams.Y);
    SetShaderValue(RHICmdList, ShaderRHI, BClearValue, ClearParams.Z);
    SetShaderValue(RHICmdList, ShaderRHI, AClearValue, ClearParams.W);
    SetShaderValue(RHICmdList, ShaderRHI, ZSize, ZSizeParam);
  }

  virtual bool Serialize(FArchive& Ar) override {
    bool bShaderHasOutdatedParameters = FGenericLightVolumeShader::Serialize(Ar);
    Ar << RClearValue << GClearValue << BClearValue << AClearValue;
    return bShaderHasOutdatedParameters;
  }

protected:
  // Float values to be set to all the volumes.
  FShaderParameter RClearValue;
  FShaderParameter GClearValue;
  FShaderParameter BClearValue;
  FShaderParameter AClearValue;
  FShaderParameter ZSize;
};

// Parent shader for shaders handling at least one directional light.
class FDirLightParentShader : public FGenericLightVolumeShader {
  // DECLARE_SHADER_TYPE(FDirLightParentShader, Global);

public:
  static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) {
    return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
  }

  FDirLightParentShader(){};

  FDirLightParentShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
    : FGenericLightVolumeShader(Initializer) {
    // Light Volumes are bound in FGenericLightVolumeShader constructor

    ReadBuffer.Bind(Initializer.ParameterMap, TEXT("ReadBuffer"));
    ReadBufferSampler.Bind(Initializer.ParameterMap, TEXT("ReadBufferSampler"));

    WriteBuffer.Bind(Initializer.ParameterMap, TEXT("WriteBuffer"));

    // Volume texture + Transfer function uniforms
    Volume.Bind(Initializer.ParameterMap, TEXT("Volume"));
    VolumeSampler.Bind(Initializer.ParameterMap, TEXT("VolumeSampler"));

    TransferFunc.Bind(Initializer.ParameterMap, TEXT("TransferFunc"));
    TransferFuncSampler.Bind(Initializer.ParameterMap, TEXT("TransferFuncSampler"));

    // Light uniforms
    LightPosition.Bind(Initializer.ParameterMap, TEXT("LightPosition"));
    LightColor.Bind(Initializer.ParameterMap, TEXT("LightColor"));
    LightIntensity.Bind(Initializer.ParameterMap, TEXT("LightIntensity"));
    Axis.Bind(Initializer.ParameterMap, TEXT("Axis"));
    Weight.Bind(Initializer.ParameterMap, TEXT("Weight"));
    Loop.Bind(Initializer.ParameterMap, TEXT("Loop"));

    LocalClippingCenter.Bind(Initializer.ParameterMap, TEXT("LocalClippingCenter"));
    LocalClippingDirection.Bind(Initializer.ParameterMap, TEXT("LocalClippingDirection"));

	TFIntensityDomain.Bind(Initializer.ParameterMap, TEXT("TFIntensityDomain"));

  }

  // Sets loop-dependent uniforms in the pipeline.
  void SetLoop(FRHICommandListImmediate& RHICmdList, FComputeShaderRHIParamRef ShaderRHI,
               const unsigned loopIndex, const FTexture2DRHIRef pReadBuffer,
               const FSamplerStateRHIRef pReadBuffSampler,
               const FUnorderedAccessViewRHIRef pWriteBuffer) {
    // Actually sets the shader uniforms in the pipeline.
    SetShaderValue(RHICmdList, ShaderRHI, Loop, loopIndex);

    // Set read/write buffers.
    SetUAVParameter(RHICmdList, ShaderRHI, WriteBuffer, pWriteBuffer);
    SetTextureParameter(RHICmdList, ShaderRHI, ReadBuffer, ReadBufferSampler, pReadBuffSampler,
                        pReadBuffer);
  }

  // Sets loop-dependent uniforms in the pipeline.
  void SetLoop(FRHICommandListImmediate& RHICmdList, const unsigned loopIndex,
               const FTexture2DRHIRef pReadBuffer, const FSamplerStateRHIRef pReadBuffSampler,
               const FUnorderedAccessViewRHIRef pWriteBuffer) {
    FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
    // Actually sets the shader uniforms in the pipeline.
    SetShaderValue(RHICmdList, ShaderRHI, Loop, loopIndex);

    // Set read/write buffers.
    SetUAVParameter(RHICmdList, ShaderRHI, WriteBuffer, pWriteBuffer);
    SetTextureParameter(RHICmdList, ShaderRHI, ReadBuffer, ReadBufferSampler, pReadBuffSampler,
                        pReadBuffer);
  }

  void SetResources(FRHICommandListImmediate& RHICmdList, const FTexture3DRHIRef pVolume,
                    const FTexture2DRHIRef pTransferFunc,
                    const FUnorderedAccessViewRHIParamRef* pVolumesArray) {
    FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
    // Create a static sampler reference and bind it together with the volume texture + TF.
    FSamplerStateRHIParamRef SamplerRef =
        TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
    SetTextureParameter(RHICmdList, ShaderRHI, Volume, VolumeSampler, SamplerRef, pVolume);
    SetTextureParameter(RHICmdList, ShaderRHI, TransferFunc, TransferFuncSampler, SamplerRef,
                        pTransferFunc);
    // Bind the light volume UAVs.
    SetLightVolumeUAVs(RHICmdList, ShaderRHI, pVolumesArray);
  }

  void UnbindUAVs(FRHICommandListImmediate& RHICmdList) {
    FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
    // Unbind write buffer.
    SetUAVParameter(RHICmdList, ShaderRHI, WriteBuffer, FUnorderedAccessViewRHIParamRef());
    // Unbind light volume uavs.
    UnbindLightVolumeUAVs(RHICmdList, ShaderRHI);
  }

  // Sets the shader uniforms in the pipeline.
  void SetParameters(FRHICommandListImmediate& RHICmdList, FComputeShaderRHIParamRef ShaderRHI,
                     const FDirLightParameters parameters,
                     FClippingPlaneParameters LocalClippingParams, const FMajorAxes MajorAxes,
                     unsigned AxisIndex, FVector2D TFDomain) {
    SetShaderValue(RHICmdList, ShaderRHI, LightPosition, -parameters.LightDirection);
    SetShaderValue(RHICmdList, ShaderRHI, LightColor, parameters.LightColor);
    SetShaderValue(RHICmdList, ShaderRHI, LightIntensity, parameters.LightIntensity);
    SetShaderValue(RHICmdList, ShaderRHI, Axis, MajorAxes.FaceWeight[AxisIndex].first);
    SetShaderValue(RHICmdList, ShaderRHI, Weight, MajorAxes.FaceWeight[AxisIndex].second);
    SetShaderValue(RHICmdList, ShaderRHI, LocalClippingCenter, LocalClippingParams.Center);
    SetShaderValue(RHICmdList, ShaderRHI, LocalClippingDirection, LocalClippingParams.Direction);
    SetShaderValue(RHICmdList, ShaderRHI, TFIntensityDomain, TFDomain);

  }

  virtual bool Serialize(FArchive& Ar) override {
    bool bShaderHasOutdatedParameters = FGenericLightVolumeShader::Serialize(Ar);
    Ar << Volume << VolumeSampler << TransferFunc << TransferFuncSampler << LightPosition
       << LightColor << LightIntensity << Axis << Weight << ReadBuffer << ReadBufferSampler
       << WriteBuffer << Loop << LocalClippingCenter << LocalClippingDirection << TFIntensityDomain;
    return bShaderHasOutdatedParameters;
  }

protected:
  // Volume texture + transfer function resource parameters
  FShaderResourceParameter Volume;
  FShaderResourceParameter VolumeSampler;

  FShaderResourceParameter TransferFunc;
  FShaderResourceParameter TransferFuncSampler;

  // Read buffer texture and sampler.
  FShaderResourceParameter ReadBuffer;
  FShaderResourceParameter ReadBufferSampler;
  // Write buffer UAV.
  FShaderResourceParameter WriteBuffer;

  // Light Direction.
  FShaderParameter LightPosition;
  // Light Color.
  FShaderParameter LightColor;
  // Light Intensity.
  FShaderParameter LightIntensity;
  // The axis along which to propagate.
  FShaderParameter Axis;
  // The weight of the current axis.
  FShaderParameter Weight;
  // Loop of the shader.
  FShaderParameter Loop;

  FShaderParameter LocalClippingCenter;

  FShaderParameter LocalClippingDirection;

  FShaderParameter TFIntensityDomain;
};

class FAddOrRemoveDirLightShader : public FDirLightParentShader {
  DECLARE_SHADER_TYPE(FAddOrRemoveDirLightShader, Global)

  using FDirLightParentShader::SetParameters;

public:
  FAddOrRemoveDirLightShader(){};

  static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) {
    return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
  }

  FAddOrRemoveDirLightShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
    : FDirLightParentShader(Initializer) {
    bAdded.Bind(Initializer.ParameterMap, TEXT("bAdded"));
  }

  virtual void SetParameters(FRHICommandListImmediate& RHICmdList,
                             const FDirLightParameters LightParameters, bool LightAdded,
                             FClippingPlaneParameters LocalClippingParams,
                             const FMajorAxes MajorAxes, unsigned AxisIndex, FVector2D TFDomain) {
    FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

    SetShaderValue(RHICmdList, ShaderRHI, bAdded, LightAdded ? 1 : -1);

    FDirLightParentShader::SetParameters(RHICmdList, ShaderRHI, LightParameters,
                                         LocalClippingParams, MajorAxes, AxisIndex, TFDomain);
  }

  virtual bool Serialize(FArchive& Ar) override {
    bool bShaderHasOutdatedParameters = FDirLightParentShader::Serialize(Ar);
    Ar << bAdded;
    return bShaderHasOutdatedParameters;
  }

private:
  // Whether the light is to be added or removed.
  FShaderParameter bAdded;
};

// Shader optimized for changing a light. It assumes the change in light direction was small (so
// that the major axes stay the same). If the difference in angles is more than 90 degrees, this
// will fail for sure.
class FChangeDirLightShader : public FDirLightParentShader {
  DECLARE_SHADER_TYPE(FChangeDirLightShader, Global)

public:
  static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) {
    return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
  }

  FChangeDirLightShader(){};

  FChangeDirLightShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
    : FDirLightParentShader(Initializer) {
    NewLightPositionParam.Bind(Initializer.ParameterMap, TEXT("NewLightPosition"));
    NewLightIntensityParam.Bind(Initializer.ParameterMap, TEXT("NewLightIntensity"));
    NewLightColorParam.Bind(Initializer.ParameterMap, TEXT("NewLightColor"));

    NewWriteBufferParam.Bind(Initializer.ParameterMap, TEXT("NewWriteBuffer"));
    NewReadBufferParam.Bind(Initializer.ParameterMap, TEXT("NewReadBuffer"));
    NewReadBufferSamplerParam.Bind(Initializer.ParameterMap, TEXT("NewReadBufferSampler"));

    NewWeightParam.Bind(Initializer.ParameterMap, TEXT("NewWeight"));
  }

  // Sets the shader uniforms in the pipeline.
  void SetParameters(FRHICommandListImmediate& RHICmdList, const FDirLightParameters OldParameters,
                     const FDirLightParameters NewParameters, const FMajorAxes OldAxes,
                     const FMajorAxes NewAxes, const FClippingPlaneParameters LocalClippingParams,
                     unsigned index, FVector2D TFDomain) {
    FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
    // Set the old light parameters.
    FDirLightParentShader::SetParameters(RHICmdList, ShaderRHI, OldParameters, LocalClippingParams,
                                         OldAxes, index, TFDomain);
    // Set the new light parameters.
    SetShaderValue(RHICmdList, ShaderRHI, NewLightPositionParam, -NewParameters.LightDirection);
    SetShaderValue(RHICmdList, ShaderRHI, NewLightColorParam, NewParameters.LightColor);
    SetShaderValue(RHICmdList, ShaderRHI, NewLightIntensityParam, NewParameters.LightIntensity);
    SetShaderValue(RHICmdList, ShaderRHI, NewWeightParam, NewAxes.FaceWeight[index].second);
  }

  virtual void SetLoop(FRHICommandListImmediate& RHICmdList, const unsigned loopIndex,
                       const FTexture2DRHIRef pReadBuffer,
                       const FSamplerStateRHIRef pReadBuffSampler,
                       const FUnorderedAccessViewRHIRef pWriteBuffer,
                       const FTexture2DRHIRef pNewReadBuffer,
                       const FSamplerStateRHIRef pNewReadBuffSampler,
                       const FUnorderedAccessViewRHIRef pNewWriteBuffer) {
    // Actually sets the shader uniforms in the pipeline.
    FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

    FDirLightParentShader::SetLoop(RHICmdList, ShaderRHI, loopIndex, pReadBuffer, pReadBuffSampler,
                                   pWriteBuffer);

    // Set new read/write buffers.
    SetUAVParameter(RHICmdList, ShaderRHI, NewWriteBufferParam, pNewWriteBuffer);
    SetTextureParameter(RHICmdList, ShaderRHI, NewReadBufferParam, NewReadBufferSamplerParam,
                        pNewReadBuffSampler, pNewReadBuffer);
  }

  virtual bool Serialize(FArchive& Ar) override {
    bool bShaderHasOutdatedParameters = FDirLightParentShader::Serialize(Ar);
    Ar << NewReadBufferParam << NewReadBufferSamplerParam << NewWriteBufferParam
       << NewLightPositionParam << NewLightColorParam << NewLightIntensityParam << NewWeightParam;
    return bShaderHasOutdatedParameters;
  }

private:
  FShaderResourceParameter NewReadBufferParam;
  FShaderResourceParameter NewReadBufferSamplerParam;
  // Write buffer for new light UAV.
  FShaderResourceParameter NewWriteBufferParam;
  // Light Direction.
  FShaderParameter NewLightPositionParam;
  // Light Color.
  FShaderParameter NewLightColorParam;
  // Light Intensity.
  FShaderParameter NewLightIntensityParam;
  // New light's weight along the axis
  FShaderParameter NewWeightParam;
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

void WriteTo3DTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FIntVector Size,
                                   UVolumeTexture* inTexture, ERHIFeatureLevel::Type FeatureLevel);

bool CreateVolumeTextureAsset(FString AssetName, EPixelFormat PixelFormat, FIntVector Dimensions,
                              uint8* BulkData, bool SaveNow = false, bool UAVCompatible = false,
                              UVolumeTexture** pOutCreatedTexture = nullptr);

ETextureSourceFormat PixelFormatToSourceFormat(EPixelFormat PixelFormat);

void AddDirLightToLightVolume_RenderThread (
    FRHICommandListImmediate& RHICmdList, const FBasicRaymarchRenderingResources Resources,
    const FColorVolumesResources ColorResources, const FDirLightParameters LightParameters,
    const bool Added, const FRaymarchWorldParameters WorldParameters, ERHIFeatureLevel::Type FeatureLevel);

void ChangeDirLightInLightVolume_RenderThread(
    FRHICommandListImmediate& RHICmdList, const FBasicRaymarchRenderingResources Resources, const FColorVolumesResources ColorResources,
    const FDirLightParameters OldLightParameters, const FDirLightParameters NewLightParameters,
    const FRaymarchWorldParameters WorldParameters, ERHIFeatureLevel::Type FeatureLevel);

void ClearLightVolumes_RenderThread(FRHICommandListImmediate& RHICmdList,
                                    FRHITexture3D* RLightVolumeResource,
                                    FRHITexture3D* GLightVolumeResource,
                                    FRHITexture3D* BLightVolumeResource,
                                    FRHITexture3D* ALightVolumeResource, FVector4 ClearValues,
                                    ERHIFeatureLevel::Type FeatureLevel);

void AddDirLightToSingleLightVolume_RenderThread(FRHICommandListImmediate& RHICmdList,
                                                 const FBasicRaymarchRenderingResources Resources,
                                                 const FDirLightParameters LightParameters,
                                                 const bool Added,
                                                 const FRaymarchWorldParameters WorldParameters,
                                                 ERHIFeatureLevel::Type FeatureLevel);

void ChangeDirLightInSingleLightVolume_RenderThread(
    FRHICommandListImmediate& RHICmdList, const FBasicRaymarchRenderingResources Resources,
    const FDirLightParameters OldLightParameters, const FDirLightParameters NewLightParameters,
    const FRaymarchWorldParameters WorldParameters,
    ERHIFeatureLevel::Type FeatureLevel);

void ClearSingleLightVolume_RenderThread(FRHICommandListImmediate& RHICmdList,
                                         FRHITexture3D* ALightVolumeResource, float ClearValue,
                                         ERHIFeatureLevel::Type FeatureLevel);

bool Create2DTextureAsset(FString AssetName, EPixelFormat PixelFormat, FIntPoint Dimensions,
                          uint8* BulkData, bool SaveNow = false, TextureAddress TilingX = TA_Clamp,
                          TextureAddress TilingY = TA_Clamp);

bool Update2DTextureAsset(UTexture2D* Texture, EPixelFormat PixelFormat, FIntPoint Dimensions,
                          uint8* BulkData, TextureAddress TilingX = TA_Clamp,
                          TextureAddress TilingY = TA_Clamp);

//
// Shaders for single (alpha) light volume follow.
//

// Parent shader for any shader that works on Light Volumes. Provides a way to bind and unbind the 4
// light volumes.
class FGenericSingleLightVolumeShader : public FGlobalShader {
public:
  static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) {
    return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
  }

  FGenericSingleLightVolumeShader(){};

  FGenericSingleLightVolumeShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
    : FGlobalShader(Initializer) {
    // Bind Light Volume resource parameters
    ALightVolume.Bind(Initializer.ParameterMap, TEXT("ALightVolume"));
  }

  virtual void SetLightVolumeUAV(FRHICommandListImmediate& RHICmdList,
                                 FComputeShaderRHIParamRef ShaderRHI,
                                 FUnorderedAccessViewRHIParamRef pLightVolume) {
    SetUAVParameter(RHICmdList, ShaderRHI, ALightVolume, pLightVolume);
  }

  virtual void SetLightVolumeUAV(FRHICommandListImmediate& RHICmdList,
                                 FUnorderedAccessViewRHIParamRef pLightVolume) {
    SetLightVolumeUAV(RHICmdList, GetComputeShader(), pLightVolume);
  }

  virtual void UnbindLightVolumeUAV(FRHICommandListImmediate& RHICmdList,
                                    FComputeShaderRHIParamRef ShaderRHI) {
    // Unbind the UAVs.
    SetUAVParameter(RHICmdList, ShaderRHI, ALightVolume, FUnorderedAccessViewRHIParamRef());
  }

  virtual void UnbindLightVolumeUAV(FRHICommandListImmediate& RHICmdList) {
    UnbindLightVolumeUAV(RHICmdList, GetComputeShader());
  }

  bool Serialize(FArchive& Ar) {
    bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
    Ar << ALightVolume;
    return bShaderHasOutdatedParameters;
  }

protected:
  // In/Out light volume.
  FShaderResourceParameter ALightVolume;
};

// Shader used for fast resetting of light volumes. Just fills zeros everywhere.
class FClearSingleLightVolumeShader : public FGenericSingleLightVolumeShader {
  DECLARE_SHADER_TYPE(FClearSingleLightVolumeShader, Global)

public:
  static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) {
    return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
  }

  FClearSingleLightVolumeShader(){};

  // Don't need any more bindings than the 4 volumes from generic shader, so just call parent
  // constructor.
  FClearSingleLightVolumeShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
    : FGenericSingleLightVolumeShader(Initializer) {
    AClearValue.Bind(Initializer.ParameterMap, TEXT("AClearValue"));  // Multibind?
    ZSize.Bind(Initializer.ParameterMap, TEXT("ZSize"));
  }

  virtual void SetParameters(FRHICommandListImmediate& RHICmdList, float clearColor,
                             int ZSizeParam) {
    FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
    SetShaderValue(RHICmdList, ShaderRHI, AClearValue, clearColor);
    SetShaderValue(RHICmdList, ShaderRHI, ZSize, ZSizeParam);
  }

  virtual bool Serialize(FArchive& Ar) override {
    bool bShaderHasOutdatedParameters = FGenericSingleLightVolumeShader::Serialize(Ar);
    Ar << AClearValue;
    return bShaderHasOutdatedParameters;
  }

protected:
  // Float values to be set to the alpha volume.
  FShaderParameter AClearValue;
  FShaderParameter ZSize;
};

// Parent shader for shaders handling at least one directional light.
class FDirLightSingleVolumeParentShader : public FGenericSingleLightVolumeShader {
  // DECLARE_SHADER_TYPE(FDirLightParentShader, Global);

public:
  static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) {
    return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
  }
  // TODO code duplication with RGB shaders!

  FDirLightSingleVolumeParentShader(){};

  FDirLightSingleVolumeParentShader(
      const ShaderMetaType::CompiledShaderInitializerType& Initializer)
    : FGenericSingleLightVolumeShader(Initializer) {
    // Light Volumes are bound in FGenericLightVolumeShader constructor
    ReadBuffer.Bind(Initializer.ParameterMap, TEXT("ReadBuffer"));
    ReadBufferSampler.Bind(Initializer.ParameterMap, TEXT("ReadBufferSampler"));

    WriteBuffer.Bind(Initializer.ParameterMap, TEXT("WriteBuffer"));

    // Volume texture + Transfer function uniforms
    Volume.Bind(Initializer.ParameterMap, TEXT("Volume"));
    VolumeSampler.Bind(Initializer.ParameterMap, TEXT("VolumeSampler"));

    TransferFunc.Bind(Initializer.ParameterMap, TEXT("TransferFunc"));
    TransferFuncSampler.Bind(Initializer.ParameterMap, TEXT("TransferFuncSampler"));

    // Light uniforms
    LightPosition.Bind(Initializer.ParameterMap, TEXT("LightPosition"));
    LightIntensity.Bind(Initializer.ParameterMap, TEXT("LightIntensity"));
    Axis.Bind(Initializer.ParameterMap, TEXT("Axis"));
    Weight.Bind(Initializer.ParameterMap, TEXT("Weight"));
    Loop.Bind(Initializer.ParameterMap, TEXT("Loop"));

    LocalClippingCenter.Bind(Initializer.ParameterMap, TEXT("LocalClippingCenter"));
    LocalClippingDirection.Bind(Initializer.ParameterMap, TEXT("LocalClippingDirection"));

	TFIntensityDomain.Bind(Initializer.ParameterMap, TEXT("TFIntensityDomain"));
  }

  // Sets loop-dependent uniforms in the pipeline.
  void SetLoop(FRHICommandListImmediate& RHICmdList, FComputeShaderRHIParamRef ShaderRHI,
               const unsigned loopIndex, const FTexture2DRHIRef pReadBuffer,
               const FSamplerStateRHIRef pReadBuffSampler,
               const FUnorderedAccessViewRHIRef pWriteBuffer) {
    // Actually sets the shader uniforms in the pipeline.
    SetShaderValue(RHICmdList, ShaderRHI, Loop, loopIndex);

    // Set read/write buffers.
    SetUAVParameter(RHICmdList, ShaderRHI, WriteBuffer, pWriteBuffer);
    SetTextureParameter(RHICmdList, ShaderRHI, ReadBuffer, ReadBufferSampler, pReadBuffSampler,
                        pReadBuffer);
  }

  // Sets loop-dependent uniforms in the pipeline.
  void SetLoop(FRHICommandListImmediate& RHICmdList, const unsigned loopIndex,
               const FTexture2DRHIRef pReadBuffer, const FSamplerStateRHIRef pReadBuffSampler,
               const FUnorderedAccessViewRHIRef pWriteBuffer) {
    FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
    // Actually sets the shader uniforms in the pipeline.
    SetShaderValue(RHICmdList, ShaderRHI, Loop, loopIndex);

    // Set read/write buffers.
    SetUAVParameter(RHICmdList, ShaderRHI, WriteBuffer, pWriteBuffer);
    SetTextureParameter(RHICmdList, ShaderRHI, ReadBuffer, ReadBufferSampler, pReadBuffSampler,
                        pReadBuffer);
  }

  void SetResources(FRHICommandListImmediate& RHICmdList, const FTexture3DRHIRef pVolume,
                    const FTexture2DRHIRef pTransferFunc,
                    const FUnorderedAccessViewRHIParamRef pLightVolume) {
    FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
    // Create a static sampler reference and bind it together with the volume texture + TF.
    FSamplerStateRHIParamRef SamplerRef =
        TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
    SetTextureParameter(RHICmdList, ShaderRHI, Volume, VolumeSampler, SamplerRef, pVolume);
    SetTextureParameter(RHICmdList, ShaderRHI, TransferFunc, TransferFuncSampler, SamplerRef,
                        pTransferFunc);
    // Bind the light volume UAVs.
    SetLightVolumeUAV(RHICmdList, ShaderRHI, pLightVolume);
  }

  void UnbindUAVs(FRHICommandListImmediate& RHICmdList) {
    FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
    // Unbind write buffer.
    SetUAVParameter(RHICmdList, ShaderRHI, WriteBuffer, FUnorderedAccessViewRHIParamRef());
    // Unbind light volume uavs.
    UnbindLightVolumeUAV(RHICmdList, ShaderRHI);
  }

  // Sets the shader uniforms in the pipeline.
  void SetParameters(FRHICommandListImmediate& RHICmdList, FComputeShaderRHIParamRef ShaderRHI,
                     const FDirLightParameters LightParameters,
                     FClippingPlaneParameters LocalClippingParams, const FMajorAxes MajorAxes,
                     unsigned AxisIndex, FVector2D TFDomain) {
    SetShaderValue(RHICmdList, ShaderRHI, LightPosition, -LightParameters.LightDirection);
    SetShaderValue(RHICmdList, ShaderRHI, LightIntensity, LightParameters.LightIntensity);
    SetShaderValue(RHICmdList, ShaderRHI, Axis, MajorAxes.FaceWeight[AxisIndex].first);
    SetShaderValue(RHICmdList, ShaderRHI, Weight, MajorAxes.FaceWeight[AxisIndex].second);
    SetShaderValue(RHICmdList, ShaderRHI, LocalClippingCenter, LocalClippingParams.Center);
    SetShaderValue(RHICmdList, ShaderRHI, LocalClippingDirection, LocalClippingParams.Direction);
    SetShaderValue(RHICmdList, ShaderRHI, TFIntensityDomain, TFDomain);

  }
  virtual bool Serialize(FArchive& Ar) override {

    bool bShaderHasOutdatedParameters = FGenericSingleLightVolumeShader::Serialize(Ar);
    Ar << Volume << VolumeSampler << TransferFunc << TransferFuncSampler << LightPosition
       << LightIntensity << Axis << Weight << ReadBuffer << ReadBufferSampler << WriteBuffer << Loop
       << LocalClippingCenter << LocalClippingDirection << TFIntensityDomain;
    return bShaderHasOutdatedParameters;
  }

protected:
  // Volume texture + transfer function resource parameters
  FShaderResourceParameter Volume;
  FShaderResourceParameter VolumeSampler;

  FShaderResourceParameter TransferFunc;
  FShaderResourceParameter TransferFuncSampler;

  // Read buffer texture and sampler.
  FShaderResourceParameter ReadBuffer;
  FShaderResourceParameter ReadBufferSampler;
  // Write buffer UAV.
  FShaderResourceParameter WriteBuffer;

  // Light Direction.
  FShaderParameter LightPosition;
  // Light Intensity.
  FShaderParameter LightIntensity;
  // The axis along which to propagate.
  FShaderParameter Axis;
  // The weight of the current axis.
  FShaderParameter Weight;
  // Loop of the shader.
  FShaderParameter Loop;
  // Clipping plane center in local space.
  FShaderParameter LocalClippingCenter;
  // Clipping plane direction in local space.
  FShaderParameter LocalClippingDirection;
  // Transfer function intensity space
  FShaderParameter TFIntensityDomain;
};

class FAddOrRemoveDirLightSingleVolumeShader : public FDirLightSingleVolumeParentShader {
  DECLARE_SHADER_TYPE(FAddOrRemoveDirLightSingleVolumeShader, Global)

  using FDirLightSingleVolumeParentShader::SetParameters;

public:
  FAddOrRemoveDirLightSingleVolumeShader(){};

  static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) {
    return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
  }

  FAddOrRemoveDirLightSingleVolumeShader(
      const ShaderMetaType::CompiledShaderInitializerType& Initializer)
    : FDirLightSingleVolumeParentShader(Initializer) {
    bAddedParam.Bind(Initializer.ParameterMap, TEXT("bAdded"));
  }

  virtual void SetParameters(FRHICommandListImmediate& RHICmdList,
                             const FDirLightParameters LocalLightParams, bool LightAdded,
                             const FClippingPlaneParameters LocalClippingParams,
                             const FMajorAxes MajorAxes, unsigned AxisIndex, FVector2D TFDomain) {
    FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

    SetShaderValue(RHICmdList, ShaderRHI, bAddedParam, LightAdded ? 1 : -1);

    FDirLightSingleVolumeParentShader::SetParameters(RHICmdList, ShaderRHI, LocalLightParams,
                                                     LocalClippingParams, MajorAxes, AxisIndex, TFDomain);
  }

  virtual bool Serialize(FArchive& Ar) override {
    bool bShaderHasOutdatedParameters = FDirLightSingleVolumeParentShader::Serialize(Ar);
    Ar << bAddedParam;
    return bShaderHasOutdatedParameters;
  }

private:
  // Whether the light is to be added or removed.
  FShaderParameter bAddedParam;
};

// Shader optimized for changing a light. It will only go through if the major axes of the light
// stay the same. Otherwise, a regular "remove and add" is performed
class FChangeDirLightSingleVolumeShader : public FDirLightSingleVolumeParentShader {
  DECLARE_SHADER_TYPE(FChangeDirLightSingleVolumeShader, Global)

public:
  static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) {
    return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
  }

  FChangeDirLightSingleVolumeShader(){};

  FChangeDirLightSingleVolumeShader(
      const ShaderMetaType::CompiledShaderInitializerType& Initializer)
    : FDirLightSingleVolumeParentShader(Initializer) {
    NewLightPosition.Bind(Initializer.ParameterMap, TEXT("NewLightPosition"));
    NewLightIntensity.Bind(Initializer.ParameterMap, TEXT("NewLightIntensity"));

    NewWriteBuffer.Bind(Initializer.ParameterMap, TEXT("NewWriteBuffer"));
    NewReadBuffer.Bind(Initializer.ParameterMap, TEXT("NewReadBuffer"));
    NewReadBufferSampler.Bind(Initializer.ParameterMap, TEXT("NewReadBufferSampler"));

    NewWeight.Bind(Initializer.ParameterMap, TEXT("NewWeight"));
  }

  // Sets the shader uniforms in the pipeline.
  void SetParameters(FRHICommandListImmediate& RHICmdList, const FDirLightParameters OldParameters,
                     const FDirLightParameters NewParameters, const FMajorAxes OldAxes,
                     const FMajorAxes NewAxes, const FClippingPlaneParameters LocalClippingParams,
                     unsigned index, FVector2D TFDomain) {
    FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
    // Set the old light parameters.
    FDirLightSingleVolumeParentShader::SetParameters(RHICmdList, ShaderRHI, OldParameters,
                                                     LocalClippingParams, OldAxes, index, TFDomain);
    // Set the new light parameters.
    SetShaderValue(RHICmdList, ShaderRHI, NewLightPosition, -NewParameters.LightDirection);
    SetShaderValue(RHICmdList, ShaderRHI, NewLightIntensity, NewParameters.LightIntensity);
    SetShaderValue(RHICmdList, ShaderRHI, NewWeight, NewAxes.FaceWeight[index].second);
  }

  virtual void SetLoop(FRHICommandListImmediate& RHICmdList, const unsigned loopIndex,
                       const FTexture2DRHIRef pReadBuffer,
                       const FSamplerStateRHIRef pReadBuffSampler,
                       const FUnorderedAccessViewRHIRef pWriteBuffer,
                       const FTexture2DRHIRef pNewReadBuffer,
                       const FSamplerStateRHIRef pNewReadBuffSampler,
                       const FUnorderedAccessViewRHIRef pNewWriteBuffer) {
    // Actually sets the shader uniforms in the pipeline.
    FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

    FDirLightSingleVolumeParentShader::SetLoop(RHICmdList, ShaderRHI, loopIndex, pReadBuffer,
                                               pReadBuffSampler, pWriteBuffer);

    // Set new read/write buffers.
    SetUAVParameter(RHICmdList, ShaderRHI, NewWriteBuffer, pNewWriteBuffer);
    SetTextureParameter(RHICmdList, ShaderRHI, NewReadBuffer, NewReadBufferSampler,
                        pNewReadBuffSampler, pNewReadBuffer);
  }

  virtual bool Serialize(FArchive& Ar) override {
    bool bShaderHasOutdatedParameters = FDirLightSingleVolumeParentShader::Serialize(Ar);
    Ar << NewReadBuffer << NewReadBufferSampler << NewWriteBuffer << NewLightPosition
       << NewLightIntensity << NewWeight;
    return bShaderHasOutdatedParameters;
  }

private:
  FShaderResourceParameter NewReadBuffer;
  FShaderResourceParameter NewReadBufferSampler;
  // Write buffer for new light UAV.
  FShaderResourceParameter NewWriteBuffer;
  // Light Direction.
  FShaderParameter NewLightPosition;
  // Light Intensity.
  FShaderParameter NewLightIntensity;
  // New light's weight along the axis
  FShaderParameter NewWeight;
};


static void CreateBasicRaymarchingResources_RenderThread(FRHICommandListImmediate& RHICmdList, 
	FBasicRaymarchRenderingResources& InParams, 
	ERHIFeatureLevel::Type FeatureLevel);