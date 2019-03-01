//// Uncomment when (if) starting to develop colored light support

//// (C) Technical University of Munich - Computer Aided Medical Procedures
//// Developed by Tomas Bartipan (tomas.bartipan@tum.de)
//
//#pragma once
//
//#include "CoreMinimal.h"
//
//#include "Engine/TextureRenderTarget2D.h"
//#include "Engine/VolumeTexture.h"
//#include "Engine/World.h"
//#include "Kismet/BlueprintFunctionLibrary.h"
//#include "Engine.h"
//#include "GlobalShader.h"
//#include "Internationalization/Internationalization.h"
//#include "Logging/MessageLog.h"
//#include "PipelineStateCache.h"
//#include "RHIStaticStates.h"
//#include "SceneInterface.h"
//#include "SceneUtils.h"
//#include "ShaderParameterUtils.h"
//#include "UObject/ObjectMacros.h"
//
//#include <algorithm>  // std::sort
//#include <utility>    // std::pair, std::make_pair
//#include <vector>     // std::pair, std::make_pair
//#include "RaymarchRendering.generated.h"
  ///** Adds a light to light volumes.
  ///** Clear all light volumes.	 */
  //UFUNCTION(BlueprintCallable, Category = "RGBRaymarcher")
  //static void ClearLightVolumes(UVolumeTexture* RLightVolume,
  //                              UVolumeTexture* GLightVolume, UVolumeTexture* BLightVolume,
  //                              UVolumeTexture* ALightVolume, FVector4 ClearValues);

  ///** Sets the provided Volume Texture sizes to "Dimensions" changes them to be Float32, and sets
  // * them to all-zeros.*/
  //UFUNCTION(BlueprintCallable, Category = "RGBRaymarcher")
  //static void CreateLightVolumes(FIntVector Dimensions,
  //                               UVolumeTexture* inRTexture, UVolumeTexture* inGTexture,
  //                               UVolumeTexture* inBTexture, UVolumeTexture* inATexture);
//
//
//USTRUCT(BlueprintType) struct FColorVolumesResources {
//	GENERATED_BODY()
//
//		// Flag that these Rendering Resources have been initialized and can be used.
//		UPROPERTY(BlueprintReadOnly, Category = "Colored Lights Raymarch Rendering") bool isInitialized;
//	UVolumeTexture* RLightVolumeRef;
//	UVolumeTexture* GLightVolumeRef;
//	UVolumeTexture* BLightVolumeRef;
//	FUnorderedAccessViewRHIParamRef RLightVolumeUAVRef;
//	FUnorderedAccessViewRHIParamRef GLightVolumeUAVRef;
//	FUnorderedAccessViewRHIParamRef BLightVolumeUAVRef;
//	// Read/Write buffer structs for going along X,Y and Z axes.
//};
//
//void AddDirLightToLightVolume_RenderThread(FRHICommandListImmediate& RHICmdList,
//	FBasicRaymarchRenderingResources Resources,
//	const FColorVolumesResources ColorResources,
//	const FDirLightParameters LightParameters,
//	const bool Added,
//	const FRaymarchWorldParameters WorldParameters);
//
//void ChangeDirLightInLightVolume_RenderThread(FRHICommandListImmediate& RHICmdList,
//	FBasicRaymarchRenderingResources Resources,
//	const FColorVolumesResources ColorResources,
//	const FDirLightParameters OldLightParameters,
//	const FDirLightParameters NewLightParameters,
//	const FRaymarchWorldParameters WorldParameters);
//
//void ClearLightVolumes_RenderThread(FRHICommandListImmediate& RHICmdList,
//	FRHITexture3D* RLightVolumeResource,
//	FRHITexture3D* GLightVolumeResource,
//	FRHITexture3D* BLightVolumeResource,
//	FRHITexture3D* ALightVolumeResource, FVector4 ClearValues);
//
//
//
//// Parent shader for any shader that works on Light Volumes. Provides a way to bind and unbind the 4
//// light volumes.
//class FGenericLightVolumeShader : public FGlobalShader {
//public:
//	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) {
//		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
//	}
//
//	FGenericLightVolumeShader() {};
//
//	FGenericLightVolumeShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
//		: FGlobalShader(Initializer) {
//		// Bind Light Volume resource parameters
//		RLightVolume.Bind(Initializer.ParameterMap, TEXT("RLightVolume"));
//		GLightVolume.Bind(Initializer.ParameterMap, TEXT("GLightVolume"));
//		BLightVolume.Bind(Initializer.ParameterMap, TEXT("BLightVolume"));
//		ALightVolume.Bind(Initializer.ParameterMap, TEXT("ALightVolume"));
//	}
//
//	virtual void SetLightVolumeUAVs(FRHICommandListImmediate& RHICmdList,
//		FComputeShaderRHIParamRef ShaderRHI,
//		const FUnorderedAccessViewRHIParamRef* pVolumesArray) {
//		// Set the UAV parameter for the light volume
//		SetUAVParameter(RHICmdList, ShaderRHI, RLightVolume, pVolumesArray[0]);
//		SetUAVParameter(RHICmdList, ShaderRHI, GLightVolume, pVolumesArray[1]);
//		SetUAVParameter(RHICmdList, ShaderRHI, BLightVolume, pVolumesArray[2]);
//		SetUAVParameter(RHICmdList, ShaderRHI, ALightVolume, pVolumesArray[3]);
//	}
//
//	virtual void SetLightVolumeUAVs(FRHICommandListImmediate& RHICmdList,
//		const FUnorderedAccessViewRHIParamRef* pVolumesArray) {
//		SetLightVolumeUAVs(RHICmdList, GetComputeShader(), pVolumesArray);
//	}
//
//	virtual void UnbindLightVolumeUAVs(FRHICommandListImmediate& RHICmdList,
//		FComputeShaderRHIParamRef ShaderRHI) {
//		// Unbind the UAVs.
//		SetUAVParameter(RHICmdList, ShaderRHI, RLightVolume, FUnorderedAccessViewRHIParamRef());
//		SetUAVParameter(RHICmdList, ShaderRHI, GLightVolume, FUnorderedAccessViewRHIParamRef());
//		SetUAVParameter(RHICmdList, ShaderRHI, BLightVolume, FUnorderedAccessViewRHIParamRef());
//		SetUAVParameter(RHICmdList, ShaderRHI, ALightVolume, FUnorderedAccessViewRHIParamRef());
//	}
//
//	virtual void UnbindLightVolumeUAVs(FRHICommandListImmediate& RHICmdList) {
//		UnbindLightVolumeUAVs(RHICmdList, GetComputeShader());
//	}
//
//	bool Serialize(FArchive& Ar) {
//		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
//		Ar << RLightVolume << GLightVolume << BLightVolume << ALightVolume;
//		return bShaderHasOutdatedParameters;
//	}
//
//protected:
//	// In/Out light volume.
//	FShaderResourceParameter RLightVolume;
//	FShaderResourceParameter GLightVolume;
//	FShaderResourceParameter BLightVolume;
//	FShaderResourceParameter ALightVolume;
//};
//
//// Shader used for fast resetting of light volumes. Just fills zeros everywhere.
//class FClearLightVolumesShader : public FGenericLightVolumeShader {
//	DECLARE_SHADER_TYPE(FClearLightVolumesShader, Global)
//
//public:
//	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) {
//		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
//	}
//
//	FClearLightVolumesShader() {};
//
//	// Don't need any more bindings than the 4 volumes from generic shader, so just call parent
//	// constructor.
//	FClearLightVolumesShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
//		: FGenericLightVolumeShader(Initializer) {
//		RClearValue.Bind(Initializer.ParameterMap, TEXT("RClearValue"));
//		GClearValue.Bind(Initializer.ParameterMap, TEXT("GClearValue"));
//		BClearValue.Bind(Initializer.ParameterMap, TEXT("BClearValue"));
//		AClearValue.Bind(Initializer.ParameterMap, TEXT("AClearValue"));
//		ZSize.Bind(Initializer.ParameterMap, TEXT("ZSize"));
//	}
//
//	virtual void SetParameters(FRHICommandListImmediate& RHICmdList, FVector4 ClearParams,
//		int ZSizeParam) {
//		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
//		SetShaderValue(RHICmdList, ShaderRHI, RClearValue, ClearParams.X);
//		SetShaderValue(RHICmdList, ShaderRHI, GClearValue, ClearParams.Y);
//		SetShaderValue(RHICmdList, ShaderRHI, BClearValue, ClearParams.Z);
//		SetShaderValue(RHICmdList, ShaderRHI, AClearValue, ClearParams.W);
//		SetShaderValue(RHICmdList, ShaderRHI, ZSize, ZSizeParam);
//	}
//
//	virtual bool Serialize(FArchive& Ar) override {
//		bool bShaderHasOutdatedParameters = FGenericLightVolumeShader::Serialize(Ar);
//		Ar << RClearValue << GClearValue << BClearValue << AClearValue;
//		return bShaderHasOutdatedParameters;
//	}
//
//protected:
//	// Float values to be set to all the volumes.
//	FShaderParameter RClearValue;
//	FShaderParameter GClearValue;
//	FShaderParameter BClearValue;
//	FShaderParameter AClearValue;
//	FShaderParameter ZSize;
//};

//UFUNCTION(BlueprintCallable, Category = "RGBRaymarcher")
//static void AddDirLightToVolumes(
//	const FBasicRaymarchRenderingResources Resources,
//	const FColorVolumesResources ColorResources,
//	const FDirLightParameters LightParameters, const bool Added,
//	const FRaymarchWorldParameters WorldParameters,
//	bool& LightAdded);
//
///** Changes a light in the light volumes.	 */
//UFUNCTION(BlueprintCallable, Category = "RGBRaymarcher")
//static void ChangeDirLightInLightVolumes(const FBasicRaymarchRenderingResources Resources,
//	const FColorVolumesResources ColorResources,
//	const FDirLightParameters OldLightParameters,
//	const FDirLightParameters NewLightParameters,
//	const FRaymarchWorldParameters WorldParameters,
//	bool& LightAdded);
//
//
//void URaymarchBlueprintLibrary::AddDirLightToVolumes(
//	FBasicRaymarchRenderingResources Resources,
//	const FColorVolumesResources ColorResources, const FDirLightParameters LightParameters,
//	const bool Added, const FRaymarchWorldParameters WorldParameters, bool& LightAdded) {
//	if (!Resources.VolumeTextureRef->Resource || !Resources.TFTextureRef->Resource ||
//		!Resources.ALightVolumeRef->Resource || !Resources.VolumeTextureRef->Resource->TextureRHI ||
//		!ColorResources.RLightVolumeRef->Resource ||
//		!ColorResources.RLightVolumeRef->Resource->TextureRHI ||
//		!ColorResources.GLightVolumeRef->Resource ||
//		!ColorResources.GLightVolumeRef->Resource->TextureRHI ||
//		!ColorResources.BLightVolumeRef->Resource ||
//		!ColorResources.BLightVolumeRef->Resource->TextureRHI ||
//		!Resources.TFTextureRef->Resource->TextureRHI ||
//		!Resources.ALightVolumeRef->Resource->TextureRHI) {
//		LightAdded = false;
//		return;
//	}
//	else {
//		LightAdded = true;
//	}
//
//	// Call the actual rendering code on RenderThread.
//	ENQUEUE_RENDER_COMMAND(CaptureCommand)
//		([=](FRHICommandListImmediate& RHICmdList) {
//		AddDirLightToLightVolume_RenderThread(RHICmdList, Resources, ColorResources, LightParameters,
//			Added, WorldParameters);
//	});
//}
//
///** Changes a light in the light volumes.	 */
//void URaymarchBlueprintLibrary::ChangeDirLightInLightVolumes(
//	FBasicRaymarchRenderingResources Resources,
//	const FColorVolumesResources ColorResources, const FDirLightParameters OldLightParameters,
//	const FDirLightParameters NewLightParameters, const FRaymarchWorldParameters WorldParameters,
//	bool& LightAdded) {
//	if (!Resources.VolumeTextureRef->Resource || !Resources.TFTextureRef->Resource ||
//		!Resources.ALightVolumeRef->Resource || !Resources.VolumeTextureRef->Resource->TextureRHI ||
//		!ColorResources.RLightVolumeRef->Resource ||
//		!ColorResources.RLightVolumeRef->Resource->TextureRHI ||
//		!ColorResources.GLightVolumeRef->Resource ||
//		!ColorResources.GLightVolumeRef->Resource->TextureRHI ||
//		!ColorResources.BLightVolumeRef->Resource ||
//		!ColorResources.BLightVolumeRef->Resource->TextureRHI ||
//		!Resources.TFTextureRef->Resource->TextureRHI ||
//		!Resources.ALightVolumeRef->Resource->TextureRHI) {
//		LightAdded = false;
//		return;
//	}
//	else {
//		LightAdded = true;
//	}
//
//	// Call the actual rendering code on RenderThread.
//	ENQUEUE_RENDER_COMMAND(CaptureCommand)
//		([=](FRHICommandListImmediate& RHICmdList) {
//		ChangeDirLightInLightVolume_RenderThread(RHICmdList, Resources, ColorResources,
//			OldLightParameters, NewLightParameters,
//			WorldParameters);
//	});
//}
//
//void URaymarchBlueprintLibrary::ClearLightVolumes(
//	UVolumeTexture* RLightVolume,
//	UVolumeTexture* GLightVolume,
//	UVolumeTexture* BLightVolume,
//	UVolumeTexture* ALightVolume,
//	FVector4 ClearValues /*= FVector4(0,0,0,0)*/) {
//	FRHITexture3D* RLightVolumeResource = RLightVolume->Resource->TextureRHI->GetTexture3D();
//	FRHITexture3D* GLightVolumeResource = GLightVolume->Resource->TextureRHI->GetTexture3D();
//	FRHITexture3D* BLightVolumeResource = BLightVolume->Resource->TextureRHI->GetTexture3D();
//	FRHITexture3D* ALightVolumeResource = ALightVolume->Resource->TextureRHI->GetTexture3D();
//
//	// Call the actual rendering code on RenderThread.
//	ENQUEUE_RENDER_COMMAND(CaptureCommand)
//		([RLightVolumeResource, GLightVolumeResource, BLightVolumeResource, ALightVolumeResource,
//			ClearValues](FRHICommandListImmediate& RHICmdList) {
//		ClearLightVolumes_RenderThread(RHICmdList, RLightVolumeResource, GLightVolumeResource,
//			BLightVolumeResource, ALightVolumeResource, ClearValues);
//	});
//}
//
///** Creates light volumes with the given dimensions */
//void URaymarchBlueprintLibrary::CreateLightVolumes(
//	FIntVector Dimensions, UVolumeTexture* inRTexture,
//	UVolumeTexture* inGTexture, UVolumeTexture* inBTexture, UVolumeTexture* inATexture) {
//	UVolumeTexture* inTexture = nullptr;
//	for (int i = 0; i < 4; i++) {
//		switch (i) {
//		case 0: inTexture = inRTexture; break;
//		case 1: inTexture = inGTexture; break;
//		case 2: inTexture = inBTexture; break;
//		case 3: inTexture = inATexture; break;
//		default: return;
//		}
//
//		if (!inTexture) break;
//
//		InitLightVolume(inTexture, Dimensions);
//	}
//	return;
//}
