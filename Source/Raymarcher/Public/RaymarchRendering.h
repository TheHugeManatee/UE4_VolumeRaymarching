// (C) Technical University of Munich - Computer Aided Medical Procedures
// Developed by Tomas Bartipan (tomas.bartipan@tum.de)

#pragma once

#include "CoreMinimal.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Engine/VolumeTexture.h"
#include "Engine/World.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine.h"
#include "GlobalShader.h"
#include "Internationalization/Internationalization.h"
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

// A structure for 4 switchable read-write buffers. Used for one axis. Need 2 pairs for change-light
// shader.
struct OneAxisReadWriteBufferResources {
  FTexture2DRHIRef Buffers[4];
  FUnorderedAccessViewRHIParamRef UAVs[4];
};

USTRUCT(BlueprintType) struct FBasicRaymarchRenderingResources {
  GENERATED_BODY()

  // Flag that these Rendering Resources have been initialized and can be used.
  UPROPERTY(BlueprintReadOnly, Category = "Basic Raymarch Rendering Resources") bool isInitialized;
  UPROPERTY(BlueprintReadOnly, Category = "Basic Raymarch Rendering Resources") bool supportsColor;

  UPROPERTY(BlueprintReadWrite, Category = "Basic Raymarch Rendering Resources")
  UVolumeTexture* VolumeTextureRef;
  UPROPERTY(BlueprintReadWrite, Category = "Basic Raymarch Rendering Resources")
  UTexture2D* TFTextureRef;
  UPROPERTY(BlueprintReadWrite, Category = "Basic Raymarch Rendering Resources")
  UVolumeTexture* ALightVolumeRef;
  UPROPERTY(BlueprintReadWrite, Category = "Basic Raymarch Rendering Resources")
  FTransferFunctionRangeParameters TFRangeParameters;
  // Not visible in BPs.
  FUnorderedAccessViewRHIParamRef ALightVolumeUAVRef;
  OneAxisReadWriteBufferResources XYZReadWriteBuffers[3];
};

USTRUCT(BlueprintType) struct FRaymarchWorldParameters {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadWrite, Category = "Raymarch Rendering World Parameters")
  FTransform VolumeTransform;
  UPROPERTY(BlueprintReadWrite, Category = "Raymarch Rendering World Parameters")
  FClippingPlaneParameters ClippingPlaneParameters;
  UPROPERTY(BlueprintReadWrite, Category = "Raymarch Rendering World Parameters")
  FVector MeshMaxBounds;
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

void AddDirLightToLightVolume_RenderThread(FRHICommandListImmediate& RHICmdList,
                                           FBasicRaymarchRenderingResources Resources,
                                           const FColorVolumesResources ColorResources,
                                           const FDirLightParameters LightParameters,
                                           const bool Added,
                                           const FRaymarchWorldParameters WorldParameters,
                                           ERHIFeatureLevel::Type FeatureLevel);

void ChangeDirLightInLightVolume_RenderThread(FRHICommandListImmediate& RHICmdList,
                                              FBasicRaymarchRenderingResources Resources,
                                              const FColorVolumesResources ColorResources,
                                              const FDirLightParameters OldLightParameters,
                                              const FDirLightParameters NewLightParameters,
                                              const FRaymarchWorldParameters WorldParameters,
                                              ERHIFeatureLevel::Type FeatureLevel);

void ClearLightVolumes_RenderThread(FRHICommandListImmediate& RHICmdList,
                                    FRHITexture3D* RLightVolumeResource,
                                    FRHITexture3D* GLightVolumeResource,
                                    FRHITexture3D* BLightVolumeResource,
                                    FRHITexture3D* ALightVolumeResource, FVector4 ClearValues,
                                    ERHIFeatureLevel::Type FeatureLevel);

void AddDirLightToSingleLightVolume_RenderThread(FRHICommandListImmediate& RHICmdList,
                                                 FBasicRaymarchRenderingResources Resources,
                                                 const FDirLightParameters LightParameters,
                                                 const bool Added,
                                                 const FRaymarchWorldParameters WorldParameters,
                                                 ERHIFeatureLevel::Type FeatureLevel);

void ChangeDirLightInSingleLightVolume_RenderThread(
    FRHICommandListImmediate& RHICmdList, FBasicRaymarchRenderingResources Resources,
    const FDirLightParameters OldLightParameters, const FDirLightParameters NewLightParameters,
    const FRaymarchWorldParameters WorldParameters, ERHIFeatureLevel::Type FeatureLevel);

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
    AClearValue.Bind(Initializer.ParameterMap, TEXT("AClearValue"));
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
    Ar << AClearValue << ZSize;
    return bShaderHasOutdatedParameters;
  }

protected:
  // Float values to be set to the alpha volume.
  FShaderParameter AClearValue;
  FShaderParameter ZSize;
};

// Declare compute shader for clearing a single-channel float UAV texture
class FClearFloatRWTextureCS : public FGlobalShader{
	DECLARE_SHADER_TYPE(FClearFloatRWTextureCS, Global);
public:
	FClearFloatRWTextureCS() {}
	FClearFloatRWTextureCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ClearValue.Bind(Initializer.ParameterMap, TEXT("ClearValue"), SPF_Mandatory);
		ClearTexture2DRW.Bind(Initializer.ParameterMap, TEXT("ClearTextureRW"), SPF_Mandatory);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ClearValue << ClearTexture2DRW;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(FRHICommandList& RHICmdList, FUnorderedAccessViewRHIParamRef TextureRW, float Value) 
	{
		SetUAVParameter(RHICmdList, GetComputeShader(), ClearTexture2DRW, TextureRW);
		SetShaderValue(RHICmdList, GetComputeShader(), ClearValue, Value);
	}

	void UnbindUAV(FRHICommandList& RHICmdList) {
		SetUAVParameter(RHICmdList, GetComputeShader(), ClearTexture2DRW, FUnorderedAccessViewRHIParamRef());
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
	
	const FShaderParameter& GetClearColorParameter()
	{
		return ClearValue;
	}

	const FShaderResourceParameter& GetClearTextureRWParameter()
	{
		return ClearTexture2DRW;
	}

protected:
	FShaderParameter ClearValue;
	FShaderResourceParameter ClearTexture2DRW;
};



static void CreateBasicRaymarchingResources_RenderThread(
    FRHICommandListImmediate& RHICmdList, struct FBasicRaymarchRenderingResources& InParams,
    ERHIFeatureLevel::Type FeatureLevel);



class FRaymarchVolumeShader : public FGlobalShader {

public:
	FRaymarchVolumeShader() : FGlobalShader() {}

	FRaymarchVolumeShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer) {
		Volume.Bind(Initializer.ParameterMap, TEXT("Volume"));
		VolumeSampler.Bind(Initializer.ParameterMap, TEXT("VolumeSampler"));

		TransferFunc.Bind(Initializer.ParameterMap, TEXT("TransferFunc"));
		TransferFuncSampler.Bind(Initializer.ParameterMap, TEXT("TransferFuncSampler"));
		LocalClippingCenter.Bind(Initializer.ParameterMap, TEXT("LocalClippingCenter"));
		LocalClippingDirection.Bind(Initializer.ParameterMap, TEXT("LocalClippingDirection"));

		TFIntensityDomain.Bind(Initializer.ParameterMap, TEXT("TFIntensityDomain"));
	}

	void SetRaymarchResources(FRHICommandListImmediate& RHICmdList, FComputeShaderRHIParamRef ShaderRHI, const FTexture3DRHIRef pVolume,
		const FTexture2DRHIRef pTransferFunc) {
		// Create a static sampler reference and bind it together with the volume texture + TF.
		FSamplerStateRHIParamRef SamplerRef =
			TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		SetTextureParameter(RHICmdList, ShaderRHI, Volume, VolumeSampler, SamplerRef, pVolume);
		SetTextureParameter(RHICmdList, ShaderRHI, TransferFunc, TransferFuncSampler, SamplerRef,
			pTransferFunc);
	}

	virtual void UnbindResources(FRHICommandListImmediate& RHICmdList, FComputeShaderRHIParamRef ShaderRHI) {
		SetTextureParameter(RHICmdList, ShaderRHI, Volume, FTextureRHIParamRef());
		SetTextureParameter(RHICmdList, ShaderRHI, TransferFunc, FTextureRHIParamRef());
	}

	// Sets the shader uniforms in the pipeline.
	void SetRaymarchParameters(FRHICommandListImmediate& RHICmdList, FComputeShaderRHIParamRef ShaderRHI,
		FClippingPlaneParameters LocalClippingParams, FVector2D TFDomain) {
		SetShaderValue(RHICmdList, ShaderRHI, LocalClippingCenter, LocalClippingParams.Center);
		SetShaderValue(RHICmdList, ShaderRHI, LocalClippingDirection, LocalClippingParams.Direction);
		SetShaderValue(RHICmdList, ShaderRHI, TFIntensityDomain, TFDomain);
	}

	virtual bool Serialize(FArchive& Ar) override {
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << Volume << VolumeSampler << TransferFunc << TransferFuncSampler << LocalClippingCenter << LocalClippingDirection << TFIntensityDomain;
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
};

class FLightPropagationShader : public FRaymarchVolumeShader {

public:
	FLightPropagationShader() : FRaymarchVolumeShader() {}

	FLightPropagationShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FRaymarchVolumeShader(Initializer) {
		// Volume texture + Transfer function uniforms
		Loop.Bind(Initializer.ParameterMap, TEXT("Loop"), SPF_Mandatory);
		PermutationMatrix.Bind(Initializer.ParameterMap, TEXT("PermutationMatrix"), SPF_Mandatory);

		ReadBuffer.Bind(Initializer.ParameterMap, TEXT("ReadBuffer"), SPF_Mandatory);
		ReadBufferSampler.Bind(Initializer.ParameterMap, TEXT("ReadBufferSampler"), SPF_Mandatory);

		// Light uniforms
		WriteBuffer.Bind(Initializer.ParameterMap, TEXT("WriteBuffer"), SPF_Mandatory);
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

	void SetPermutationMatrix(FRHICommandListImmediate& RHICmdList, FComputeShaderRHIParamRef ShaderRHI, FMatrix PermMatrix) {
		SetShaderValue(RHICmdList, ShaderRHI, PermutationMatrix, PermMatrix);
	}

	virtual void UnbindResources(FRHICommandListImmediate& RHICmdList, FComputeShaderRHIParamRef ShaderRHI) override {
		// Unbind volume buffer.	
		FRaymarchVolumeShader::UnbindResources(RHICmdList, ShaderRHI);
		SetUAVParameter(RHICmdList, ShaderRHI, WriteBuffer, FUnorderedAccessViewRHIParamRef());
		SetTextureParameter(RHICmdList, ShaderRHI, ReadBuffer, FTextureRHIParamRef());
	}

	virtual bool Serialize(FArchive& Ar) override {
		bool bShaderHasOutdatedParameters = FRaymarchVolumeShader::Serialize(Ar);
		Ar << Loop << PermutationMatrix << ReadBuffer << ReadBufferSampler << WriteBuffer;
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
};

class FDirLightPropagationShader : public FLightPropagationShader {
public:

	FDirLightPropagationShader(): FLightPropagationShader() {}


	FDirLightPropagationShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FLightPropagationShader(Initializer) {
		// Volume texture + Transfer function uniforms
		PrevPixelOffset.Bind(Initializer.ParameterMap, TEXT("PrevPixelOffset"), SPF_Mandatory);
	}

	void SetPixelOffset(FRHICommandListImmediate& RHICmdList, FComputeShaderRHIParamRef ShaderRHI, FVector2D PixelOffset) {
		SetShaderValue(RHICmdList, ShaderRHI, PrevPixelOffset, PixelOffset);
	}

	virtual bool Serialize(FArchive& Ar) override {
		bool bShaderHasOutdatedParameters = FLightPropagationShader::Serialize(Ar);
		Ar << PrevPixelOffset;
		return bShaderHasOutdatedParameters;
	}

protected:
	// Tells the shader the pixel offset for reading from the previous loop's buffer
	FShaderParameter PrevPixelOffset;
};

class FAddDirLightShader : public FDirLightPropagationShader {

public:
	FAddDirLightShader() : FDirLightPropagationShader() {}

	FAddDirLightShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FDirLightPropagationShader(Initializer) {
		// Volume texture + Transfer function uniforms
		bAdded.Bind(Initializer.ParameterMap, TEXT("bAdded"), SPF_Mandatory);
	}

	void SetLightAdded(FRHICommandListImmediate& RHICmdList, FComputeShaderRHIParamRef ShaderRHI, bool bLightAdded) {
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


class FChangeDirLightShader : public FDirLightPropagationShader {

public:
	FChangeDirLightShader() : FDirLightPropagationShader() {}

	FChangeDirLightShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FDirLightPropagationShader(Initializer) {
		// Volume texture + Transfer function uniforms
		RemovedPrevPixelOffset.Bind(Initializer.ParameterMap, TEXT("RemovedPrevPixelOffset"), SPF_Mandatory);
		RemovedReadBuffer.Bind(Initializer.ParameterMap, TEXT("RemovedReadBuffer"), SPF_Mandatory);
		RemovedReadBufferSampler.Bind(Initializer.ParameterMap, TEXT("RemovedReadBufferSampler"), SPF_Mandatory);
		RemovedWriteBuffer.Bind(Initializer.ParameterMap, TEXT("RemovedWriteBuffer"), SPF_Mandatory);
	}

	// Sets loop-dependent uniforms in the pipeline.
	void SetLoop(FRHICommandListImmediate& RHICmdList, 
		FComputeShaderRHIParamRef ShaderRHI,
		const unsigned loopIndex, 
		const FTexture2DRHIRef pRemovedReadBuffer,
		const FSamplerStateRHIRef pRemovedReadBuffSampler,
		const FUnorderedAccessViewRHIRef pRemovedWriteBuffer,
		const FTexture2DRHIRef pAddedReadBuffer,
		const FSamplerStateRHIRef pAddedReadBuffSampler,
		const FUnorderedAccessViewRHIRef pAddedWriteBuffer) {
		// Actually sets the shader uniforms in the pipeline.
		FLightPropagationShader::SetLoop(RHICmdList, ShaderRHI, loopIndex, pAddedReadBuffer, pAddedReadBuffSampler, pAddedWriteBuffer);
		// Set read/write buffers for removed light.
		SetUAVParameter(RHICmdList, ShaderRHI, RemovedWriteBuffer, pRemovedWriteBuffer);
		SetTextureParameter(RHICmdList, ShaderRHI, RemovedReadBuffer, RemovedReadBufferSampler, pRemovedReadBuffSampler,
			pRemovedReadBuffer);
	}

	void SetRemovedPixelOffset(FRHICommandListImmediate& RHICmdList, FComputeShaderRHIParamRef ShaderRHI, FVector2D PixelOffset) {
		// Set the multiplier to -1 if we're removing the light. Set to 1 if adding it.
		SetShaderValue(RHICmdList, ShaderRHI, RemovedPrevPixelOffset, PixelOffset);
	}

	virtual void UnbindResources(FRHICommandListImmediate& RHICmdList, FComputeShaderRHIParamRef ShaderRHI) override {
		// Unbind parent and also our added parameters.
		FDirLightPropagationShader::UnbindResources(RHICmdList, ShaderRHI);
		SetUAVParameter(RHICmdList, ShaderRHI, RemovedWriteBuffer, FUnorderedAccessViewRHIParamRef());
		SetTextureParameter(RHICmdList, ShaderRHI, RemovedReadBuffer, FTextureRHIParamRef());
	}

	virtual bool Serialize(FArchive& Ar) override {
		bool bShaderHasOutdatedParameters = FDirLightPropagationShader::Serialize(Ar);
		Ar << RemovedPrevPixelOffset << RemovedReadBuffer << RemovedReadBufferSampler << RemovedWriteBuffer;
		return bShaderHasOutdatedParameters;
	}

protected:
	// Same collection of parameters as for a dir light shader, but these ones are the ones of the removed light.

	// Tells the shader the pixel offset for reading from the previous loop's buffer
	FShaderParameter RemovedPrevPixelOffset;

    FShaderResourceParameter RemovedReadBuffer;
	FShaderResourceParameter RemovedReadBufferSampler;
	// Write buffer UAV.
	FShaderResourceParameter RemovedWriteBuffer;
};

class FAddDirLightShaderSingle : public FAddDirLightShader {

	DECLARE_SHADER_TYPE(FAddDirLightShaderSingle, Global)
	public:
		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) {
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}

	FAddDirLightShaderSingle() : FAddDirLightShader() {}

	FAddDirLightShaderSingle(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FAddDirLightShader(Initializer) {
		// Volume texture + Transfer function uniforms
		ALightVolume.Bind(Initializer.ParameterMap, TEXT("ALightVolume"), SPF_Mandatory);
	}

	void SetALightVolume(FRHICommandListImmediate& RHICmdList, FComputeShaderRHIParamRef ShaderRHI, FUnorderedAccessViewRHIRef pALightVolume) {
		// Set the multiplier to -1 if we're removing the light. Set to 1 if adding it.
		SetUAVParameter(RHICmdList, ShaderRHI, ALightVolume, pALightVolume);
	}

	virtual void UnbindResources(FRHICommandListImmediate& RHICmdList, FComputeShaderRHIParamRef ShaderRHI) override {
		// Unbind parent and also our added parameter.
		FAddDirLightShader::UnbindResources(RHICmdList, ShaderRHI);
		SetUAVParameter(RHICmdList, ShaderRHI, ALightVolume, FUnorderedAccessViewRHIParamRef());
	}

	virtual bool Serialize(FArchive& Ar) override {
		bool bShaderHasOutdatedParameters = FAddDirLightShader::Serialize(Ar);
		Ar << ALightVolume;
		return bShaderHasOutdatedParameters;
	}

protected:
	// Tells the shader the pixel offset for reading from the previous loop's buffer
	FShaderResourceParameter ALightVolume;
};


class FChangeDirLightShaderSingle : public FChangeDirLightShader {
	DECLARE_SHADER_TYPE(FChangeDirLightShaderSingle, Global)
public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) {
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	FChangeDirLightShaderSingle() : FChangeDirLightShader() {}

	FChangeDirLightShaderSingle(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FChangeDirLightShader(Initializer) {
		// Volume texture + Transfer function uniforms
		ALightVolume.Bind(Initializer.ParameterMap, TEXT("ALightVolume"), SPF_Mandatory);
	}

	void SetALightVolume(FRHICommandListImmediate& RHICmdList, FComputeShaderRHIParamRef ShaderRHI, FUnorderedAccessViewRHIRef pALightVolume) {
		// Set the multiplier to -1 if we're removing the light. Set to 1 if adding it.
		SetUAVParameter(RHICmdList, ShaderRHI, ALightVolume, pALightVolume);
	}

	virtual void UnbindResources(FRHICommandListImmediate& RHICmdList, FComputeShaderRHIParamRef ShaderRHI) override {
		// Unbind parent and also our added parameter.
		FChangeDirLightShader::UnbindResources(RHICmdList, ShaderRHI);
		SetUAVParameter(RHICmdList, ShaderRHI, ALightVolume, FUnorderedAccessViewRHIParamRef());
	}

	virtual bool Serialize(FArchive& Ar) override {
		bool bShaderHasOutdatedParameters = FChangeDirLightShader::Serialize(Ar);
		Ar << ALightVolume;
		return bShaderHasOutdatedParameters;
	}

protected:
	// Tells the shader the pixel offset for reading from the previous loop's buffer
	FShaderResourceParameter ALightVolume;
};
