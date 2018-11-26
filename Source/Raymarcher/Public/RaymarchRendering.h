// (C) Technical University of Munich - Computer Aided Medical Procedures
// Developed by Tomas Bartipan (tomas.bartipan@tum.de)

#pragma once

#include "CoreMinimal.h"

#include "UObject/ObjectMacros.h"
#include "Classes/Kismet/BlueprintFunctionLibrary.h"
#include "Engine.h"
#include "Classes/Engine/TextureRenderTarget2D.h"
#include "Classes/Engine/World.h"
#include "Classes/Engine/VolumeTexture.h"
#include "Public/GlobalShader.h"
#include "Public/PipelineStateCache.h"
#include "Public/RHIStaticStates.h"
#include "Public/SceneUtils.h"
#include "Public/SceneInterface.h"
#include "Public/ShaderParameterUtils.h"
#include "Public/Logging/MessageLog.h"
#include "Public/Internationalization/Internationalization.h"

#include <utility>      // std::pair, std::make_pair
#include <vector>      // std::pair, std::make_pair
#include <algorithm>    // std::sort
#include "RaymarchRendering.generated.h"

#define MY_LOG(x) if(GEngine){GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TEXT(x));}

USTRUCT(BlueprintType) struct FDirLightParameters {
	GENERATED_BODY()

		UPROPERTY(BlueprintReadWrite, Category = "DirLightParameters") FVector LightDirection;
	UPROPERTY(BlueprintReadWrite, Category = "DirLightParameters") FVector LightColor;
	UPROPERTY(BlueprintReadWrite, Category = "DirLightParameters")	float LightIntensity;

	FDirLightParameters(FVector LightDir, FVector LightCol, float LightInt) : LightDirection(LightDir), LightColor(LightCol), LightIntensity(LightInt) {};
	FDirLightParameters() : LightDirection(FVector(0, 0, 0)), LightColor(FVector(0, 0, 0)), LightIntensity(0) {};
};



//struct FDirLightParameters {
//	FVector LightDirection;
//	FVector LightColor;
//	float LightIntensity;
//
//	FDirLightParameters(FVector LightDir, FVector LightCol, float LightInt) : LightDirection(LightDir), LightColor(LightCol), LightIntensity(LightInt) {};
//};

// Enum for indexes for cube faces - used to discern axes for light propagation shader.
enum FCubeFace : int {
	Right	= 1, // +X
	Left	= 2, // -X	
	Front	= 3, // +Y
	Back	= 4, // -Y
	Top		= 5, // +Z
	Bottom	= 6  // -Z
};

// Normals of corresponding cube faces in object-space.
const FVector FCubeFaceNormals[6] = {
	{ 1.0,  0.0,  0.0},  // right
	{-1.0,  0.0,  0.0},  // left
	{ 0.0,  1.0,  0.0},  // front
	{ 0.0, -1.0,  0.0},  // back
	{ 0.0,  0.0,  1.0},  // top
	{ 0.0,  0.0, -1.0}   // bottom
};

// Struct corresponding to the 3 major axes to propagate a light-source along with their respective weights.
struct FMajorAxes {
	// The 3 major axes indexes
	std::vector<std::pair<FCubeFace, float>> FaceWeight;
};

// Comparison for a Face-weight pair, to sort in Descending order.
static bool SortDescendingWeights(const std::pair<FCubeFace,float> &a,
							      const std::pair<FCubeFace,float> &b)
{
    return (a.second > b.second);
};
 
// Returns the weighted major axes to propagate along to add/remove a light to/from a lightvolume (LightPos must be converted to object-space).
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


// Parent shader for any shader that works on Light Volumes. Provides a way to bind and unbind the 4 light volumes.
class FGenericLightVolumeShader : public FGlobalShader {

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	FGenericLightVolumeShader() {};

	FGenericLightVolumeShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		// Bind Light Volume resource parameters
		RLightVolume.Bind(Initializer.ParameterMap, TEXT("RLightVolume"));
		GLightVolume.Bind(Initializer.ParameterMap, TEXT("GLightVolume"));
		BLightVolume.Bind(Initializer.ParameterMap, TEXT("BLightVolume"));
		ALightVolume.Bind(Initializer.ParameterMap, TEXT("ALightVolume"));
	}
	
	virtual void SetLightVolumeUAVs(FRHICommandListImmediate& RHICmdList, FComputeShaderRHIParamRef ShaderRHI, const FUnorderedAccessViewRHIParamRef* pVolumesArray)
	{
		// Set the UAV parameter for the light volume
		SetUAVParameter(RHICmdList, ShaderRHI, RLightVolume, pVolumesArray[0]);
		SetUAVParameter(RHICmdList, ShaderRHI, GLightVolume, pVolumesArray[1]);
		SetUAVParameter(RHICmdList, ShaderRHI, BLightVolume, pVolumesArray[2]);
		SetUAVParameter(RHICmdList, ShaderRHI, ALightVolume, pVolumesArray[3]);
	}

	virtual void SetLightVolumeUAVs(FRHICommandListImmediate& RHICmdList, const FUnorderedAccessViewRHIParamRef* pVolumesArray)
	{
		SetLightVolumeUAVs(RHICmdList, GetComputeShader(), pVolumesArray);
	}

	virtual void UnbindLightVolumeUAVs(FRHICommandListImmediate& RHICmdList, FComputeShaderRHIParamRef ShaderRHI) {
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
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	FClearLightVolumesShader() {};

	// Don't need any more bindings than the 4 volumes from generic shader, so just call parent constructor.
	FClearLightVolumesShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGenericLightVolumeShader(Initializer) {
		RClearValue.Bind(Initializer.ParameterMap, TEXT("RClearValue"));
		GClearValue.Bind(Initializer.ParameterMap, TEXT("GClearValue"));
		BClearValue.Bind(Initializer.ParameterMap, TEXT("BClearValue"));
		AClearValue.Bind(Initializer.ParameterMap, TEXT("AClearValue"));
		ZSize.Bind(Initializer.ParameterMap, TEXT("ZSize"));
	}

	virtual void SetParameters(FRHICommandListImmediate& RHICmdList,
							   FVector4 ClearParams, int ZSizeParam) {

		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		SetShaderValue(RHICmdList, ShaderRHI, RClearValue, ClearParams.X);
		SetShaderValue(RHICmdList, ShaderRHI, GClearValue, ClearParams.Y);
		SetShaderValue(RHICmdList, ShaderRHI, BClearValue, ClearParams.Z);
		SetShaderValue(RHICmdList, ShaderRHI, AClearValue, ClearParams.W);
		SetShaderValue(RHICmdList, ShaderRHI, ZSize, ZSizeParam);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
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
class FDirLightParentShader : public FGenericLightVolumeShader 
{
	//DECLARE_SHADER_TYPE(FDirLightParentShader, Global);

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	FDirLightParentShader() {};
	
	FDirLightParentShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGenericLightVolumeShader(Initializer)
	{
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
	}
		
	// Sets loop-dependent uniforms in the pipeline.
	void SetLoop(
		FRHICommandListImmediate& RHICmdList,
		FComputeShaderRHIParamRef ShaderRHI,
		const unsigned loopIndex,
		const FTexture2DRHIRef pReadBuffer,
		const FSamplerStateRHIRef pReadBuffSampler,
		const FUnorderedAccessViewRHIRef pWriteBuffer)
	{
		// Actually sets the shader uniforms in the pipeline.
		SetShaderValue(RHICmdList, ShaderRHI, Loop, loopIndex);

		// Set read/write buffers.
		SetUAVParameter(RHICmdList, ShaderRHI, WriteBuffer, pWriteBuffer);
		SetTextureParameter(RHICmdList, ShaderRHI, ReadBuffer, ReadBufferSampler, pReadBuffSampler, pReadBuffer);
	}

	// Sets loop-dependent uniforms in the pipeline.
	void SetLoop(
		FRHICommandListImmediate& RHICmdList,
		const unsigned loopIndex,
		const FTexture2DRHIRef pReadBuffer,
		const FSamplerStateRHIRef pReadBuffSampler,
		const FUnorderedAccessViewRHIRef pWriteBuffer)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		// Actually sets the shader uniforms in the pipeline.
		SetShaderValue(RHICmdList, ShaderRHI, Loop, loopIndex);

		// Set read/write buffers.
		SetUAVParameter(RHICmdList, ShaderRHI, WriteBuffer, pWriteBuffer);
		SetTextureParameter(RHICmdList, ShaderRHI, ReadBuffer, ReadBufferSampler, pReadBuffSampler, pReadBuffer);
	}
		
	void SetResources(
		FRHICommandListImmediate& RHICmdList,
		const FTexture3DRHIRef pVolume,
		const FTexture2DRHIRef pTransferFunc,
		const FUnorderedAccessViewRHIParamRef* pVolumesArray)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		// Create a static sampler reference and bind it together with the volume texture + TF.
		FSamplerStateRHIParamRef SamplerRef = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		SetTextureParameter(RHICmdList, ShaderRHI, Volume, VolumeSampler, SamplerRef, pVolume);
		SetTextureParameter(RHICmdList, ShaderRHI, TransferFunc, TransferFuncSampler, SamplerRef, pTransferFunc);
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
	void SetParameters(
		FRHICommandListImmediate& RHICmdList,
		FComputeShaderRHIParamRef ShaderRHI,
		const FDirLightParameters parameters,
		const unsigned pAxis,
		const float pWeight,
		const FVector pLocalClippingCenter,
		const FVector pLocalClippingDirection
	)
	{
		SetShaderValue(RHICmdList, ShaderRHI, LightPosition, -parameters.LightDirection);
		SetShaderValue(RHICmdList, ShaderRHI, LightColor, parameters.LightColor);
		SetShaderValue(RHICmdList, ShaderRHI, LightIntensity, parameters.LightIntensity);
		SetShaderValue(RHICmdList, ShaderRHI, Axis, pAxis);
		SetShaderValue(RHICmdList, ShaderRHI, Weight, pWeight);
		SetShaderValue(RHICmdList, ShaderRHI, LocalClippingCenter, pLocalClippingCenter);
		SetShaderValue(RHICmdList, ShaderRHI, LocalClippingDirection, pLocalClippingDirection);

	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGenericLightVolumeShader::Serialize(Ar);
		Ar << Volume << VolumeSampler << TransferFunc << TransferFuncSampler << 
			LightPosition << LightColor << LightIntensity <<  Axis << Weight << ReadBuffer << ReadBufferSampler << WriteBuffer << Loop << LocalClippingCenter << LocalClippingDirection;
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

};


class FAddOrRemoveDirLightShader : public FDirLightParentShader
{
DECLARE_SHADER_TYPE(FAddOrRemoveDirLightShader, Global)

using FDirLightParentShader::SetParameters;

public:
	


	FAddOrRemoveDirLightShader() {};

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}


	FAddOrRemoveDirLightShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FDirLightParentShader(Initializer) 
	{
		bAdded.Bind(Initializer.ParameterMap, TEXT("bAdded"));
	}


	virtual void SetParameters(FRHICommandListImmediate& RHICmdList, const FDirLightParameters parameters, bool LightAdded, const unsigned pAxis, const float pWeight, const FVector pLocalClippingCenter, const FVector pLocalClippingDirection) {
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		SetShaderValue(RHICmdList, ShaderRHI, bAdded, LightAdded ? 1 : -1);

		FDirLightParentShader::SetParameters(RHICmdList, ShaderRHI, parameters, pAxis, pWeight, pLocalClippingCenter, pLocalClippingDirection);
	}
	
	
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FDirLightParentShader::Serialize(Ar);
		Ar << bAdded;
		return bShaderHasOutdatedParameters;
	}

private:	
	// Whether the light is to be added or removed.
	FShaderParameter bAdded;
};

// Shader optimized for changing a light. It assumes the change in light direction was small (so that the major axes stay the same).
// If the difference in angles is more than 90 degrees, this will fail for sure.
class FChangeDirLightShader : public FDirLightParentShader
{
	DECLARE_SHADER_TYPE(FChangeDirLightShader, Global)


public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}


	FChangeDirLightShader() {};

	FChangeDirLightShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FDirLightParentShader(Initializer)
	{
		NewLightPosition.Bind(Initializer.ParameterMap, TEXT("NewLightPosition"));
		NewLightIntensity.Bind(Initializer.ParameterMap, TEXT("NewLightIntensity"));
		NewLightColor.Bind(Initializer.ParameterMap, TEXT("NewLightColor"));

		NewWriteBuffer.Bind(Initializer.ParameterMap, TEXT("NewWriteBuffer"));
		NewReadBuffer.Bind(Initializer.ParameterMap, TEXT("NewReadBuffer"));
		NewReadBufferSampler.Bind(Initializer.ParameterMap, TEXT("NewReadBufferSampler"));

		NewWeight.Bind(Initializer.ParameterMap, TEXT("NewWeight"));

		NewLocalClippingCenter.Bind(Initializer.ParameterMap, TEXT("NewLocalClippingCenter"));
		NewLocalClippingDirection.Bind(Initializer.ParameterMap, TEXT("NewLocalClippingDirection"));
	}


	// Sets the shader uniforms in the pipeline.
	void SetParameters (
		FRHICommandListImmediate& RHICmdList,
		const FDirLightParameters OldParameters,
		const FDirLightParameters NewParameters,
		const unsigned pAxis,
		const float pWeight,
		const float pNewWeight,
		const FVector pLocalClippingCenter,
		const FVector pLocalClippingDirection,
		const FVector pNewLocalClippingCenter,
		const FVector pNewLocalClippingDirection
	)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		// Set the old light parameters.
		FDirLightParentShader::SetParameters(RHICmdList, ShaderRHI, OldParameters, pAxis, pWeight, pLocalClippingCenter, pLocalClippingDirection);
		// Set the new light parameters.
		SetShaderValue(RHICmdList, ShaderRHI, NewLightPosition, -NewParameters.LightDirection);
		SetShaderValue(RHICmdList, ShaderRHI, NewLightColor, NewParameters.LightColor);
		SetShaderValue(RHICmdList, ShaderRHI, NewLightIntensity, NewParameters.LightIntensity);
		SetShaderValue(RHICmdList, ShaderRHI, NewWeight, pNewWeight);
		SetShaderValue(RHICmdList, ShaderRHI, NewLocalClippingCenter, pNewLocalClippingCenter);
		SetShaderValue(RHICmdList, ShaderRHI, NewLocalClippingDirection, pNewLocalClippingDirection);
	}

	virtual void SetLoop (
		FRHICommandListImmediate& RHICmdList,
		const unsigned loopIndex,
		const FTexture2DRHIRef pReadBuffer,
		const FSamplerStateRHIRef pReadBuffSampler,
		const FUnorderedAccessViewRHIRef pWriteBuffer,
		const FTexture2DRHIRef pNewReadBuffer,
		const FSamplerStateRHIRef pNewReadBuffSampler,
		const FUnorderedAccessViewRHIRef pNewWriteBuffer
	)
	{
		// Actually sets the shader uniforms in the pipeline.
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		FDirLightParentShader::SetLoop(RHICmdList, ShaderRHI, loopIndex, pReadBuffer, pReadBuffSampler, pWriteBuffer);

		// Set new read/write buffers.
		SetUAVParameter(RHICmdList, ShaderRHI, NewWriteBuffer, pNewWriteBuffer);
		SetTextureParameter(RHICmdList, ShaderRHI, NewReadBuffer, NewReadBufferSampler, pNewReadBuffSampler, pNewReadBuffer);
	} 

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FDirLightParentShader::Serialize(Ar);
		Ar << NewReadBuffer << NewReadBufferSampler << NewWriteBuffer << NewLightPosition << NewLightColor << NewLightIntensity << NewWeight << NewLocalClippingCenter << NewLocalClippingDirection;
		return bShaderHasOutdatedParameters;
	}

private:

	FShaderResourceParameter NewReadBuffer;
	FShaderResourceParameter NewReadBufferSampler;
	// Write buffer for new light UAV.
	FShaderResourceParameter NewWriteBuffer;
	// Light Direction.
	FShaderParameter NewLightPosition;
	// Light Color.
	FShaderParameter NewLightColor;
	// Light Intensity.
	FShaderParameter NewLightIntensity;
	// New light's weight along the axis
	FShaderParameter NewWeight;

	FShaderParameter NewLocalClippingCenter;

	FShaderParameter NewLocalClippingDirection;

};




// Pixel shader for writing to a Volume Texture.
class FVolumePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FVolumePS, Global);

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	FVolumePS() {}

	FVolumePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
	
	}
		
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}
};

void WriteTo3DTexture_RenderThread(
	FRHICommandListImmediate& RHICmdList,
	FIntVector Size,
	UVolumeTexture* inTexture,
	ERHIFeatureLevel::Type FeatureLevel);

bool CreateVolumeTextureAsset(FString AssetName, EPixelFormat PixelFormat, FIntVector Dimensions, uint8* BulkData, bool SaveNow = false, bool UAVCompatible = false, UVolumeTexture** pOutCreatedTexture = nullptr);

ETextureSourceFormat PixelFormatToSourceFormat(EPixelFormat PixelFormat);

void AddDirLightToLightVolume_RenderThread(
	FRHICommandListImmediate& RHICmdList,
	FRHITexture3D* RLightVolumeResource,
	FRHITexture3D* GLightVolumeResource,
	FRHITexture3D* BLightVolumeResource,
	FRHITexture3D* ALightVolumeResource,
	FRHITexture3D* VolumeResource,
	FRHITexture2D* TFResource,
	FDirLightParameters LightParams,
	bool LightAdded,
	FTransform VolumeInvTransform,
	const FVector LocalClippingCenter,
	const FVector LocalClippingDirection,
	ERHIFeatureLevel::Type FeatureLevel);

void ChangeDirLightInLightVolume_RenderThread(
	FRHICommandListImmediate& RHICmdList,
	FRHITexture3D* RLightVolumeResource,
	FRHITexture3D* GLightVolumeResource,
	FRHITexture3D* BLightVolumeResource,
	FRHITexture3D* ALightVolumeResource,
	FRHITexture3D* VolumeResource,
	FRHITexture2D* TFResource,
	FDirLightParameters LightParams,
	FDirLightParameters NewLightParams,
	FTransform VolumeInvTransform,
	const FVector LocalClippingCenter,
	const FVector LocalClippingDirection,
	const FVector NewLocalClippingCenter,
	const FVector NewLocalClippingDirection, 
	ERHIFeatureLevel::Type FeatureLevel);

void ClearLightVolumes_RenderThread(
	FRHICommandListImmediate& RHICmdList,
	FRHITexture3D* RLightVolumeResource,
	FRHITexture3D* GLightVolumeResource,
	FRHITexture3D* BLightVolumeResource,
	FRHITexture3D* ALightVolumeResource,
	FVector4 ClearValues,
	ERHIFeatureLevel::Type FeatureLevel);

bool Create2DTextureAsset(FString AssetName, EPixelFormat PixelFormat, FIntPoint Dimensions, uint8* BulkData, bool SaveNow = false, TextureAddress TilingX = TA_Clamp, TextureAddress TilingY = TA_Clamp);
